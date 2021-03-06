// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides global database of differential decompression dictionaries for the
// SDCH filter (processes sdch enconded content).

// Exactly one instance of SdchManager is built, and all references are made
// into that collection.
//
// The SdchManager maintains a collection of memory resident dictionaries.  It
// can find a dictionary (based on a server specification of a hash), store a
// dictionary, and make judgements about what URLs can use, set, etc. a
// dictionary.

// These dictionaries are acquired over the net, and include a header
// (containing metadata) as well as a VCDIFF dictionary (for use by a VCDIFF
// module) to decompress data.

#ifndef NET_BASE_SDCH_MANAGER_H_
#define NET_BASE_SDCH_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/ref_counted.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"


//------------------------------------------------------------------------------
// Create a public interface to help us load SDCH dictionaries.
// The SdchManager class allows registration to support this interface.
// A browser may register a fetcher that is used by the dictionary managers to
// get data from a specified URL.  This allows us to use very high level browser
// functionality in this base (when the functionaity can be provided).
class SdchFetcher {
 public:
  SdchFetcher() {}
  virtual ~SdchFetcher() {}

  // The Schedule() method is called when there is a need to get a dictionary
  // from a server.  The callee is responsible for getting that dictionary_text,
  // and then calling back to AddSdchDictionary() to the SdchManager instance.
  virtual void Schedule(const GURL& dictionary_url) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(SdchFetcher);
};
//------------------------------------------------------------------------------

class SdchManager {
 public:
  // A list of errors that appeared and were either resolved, or used to turn
  // off sdch encoding.
  enum ProblemCodes {
    MIN_PROBLEM_CODE,

    // Content Decode problems.
    ADDED_CONTENT_ENCODING,
    FIXED_CONTENT_ENCODING,
    FIXED_CONTENT_ENCODINGS,

    // Content decoding errors.
    DECODE_HEADER_ERROR,
    DECODE_BODY_ERROR,

    // Dictionary selection for use problems.
    DICTIONARY_NOT_FOUND_FOR_HASH = 10,
    DICTIONARY_FOUND_HAS_WRONG_DOMAIN,
    DICTIONARY_FOUND_HAS_WRONG_PORT_LIST,
    DICTIONARY_FOUND_HAS_WRONG_PATH,
    DICTIONARY_FOUND_HAS_WRONG_SCHEME,
    DICTIONARY_HASH_NOT_FOUND,
    DICTIONARY_HASH_MALFORMED,

    // Decode recovery methods.
    META_REFRESH_RECOVERY,
    PASSING_THROUGH_NON_SDCH,
    UNRECOVERABLE_ERROR,

    // Dictionary saving problems.
    DICTIONARY_HAS_NO_HEADER = 20,
    DICTIONARY_HEADER_LINE_MISSING_COLON,
    DICTIONARY_MISSING_DOMAIN_SPECIFIER,
    DICTIONARY_SPECIFIES_TOP_LEVEL_DOMAIN,
    DICTIONARY_DOMAIN_NOT_MATCHING_SOURCE_URL,
    DICTIONARY_PORT_NOT_MATCHING_SOURCE_URL,

    // Dictionary loading problems.
    DICTIONARY_LOAD_ATTEMPT_FROM_DIFFERENT_HOST = 30,
    DICTIONARY_SELECTED_FOR_SSL,
    DICTIONARY_ALREADY_LOADED,

    MAX_PROBLEM_CODE  // Used to bound histogram
  };

  // There is one instance of |Dictionary| for each memory-cached SDCH
  // dictionary.
  class Dictionary : public base::RefCounted<Dictionary> {
   public:
    // Sdch filters can get our text to use in decoding compressed data.
    const std::string& text() const { return text_; }

   private:
    friend class SdchManager;  // Only manager can construct an instance.

    // Construct a vc-diff usable dictionary from the dictionary_text starting
    // at the given offset.  The supplied client_hash should be used to
    // advertise the dictionary's availability relative to the suppplied URL.
    Dictionary(const std::string& dictionary_text, size_t offset,
               const std::string& client_hash, const GURL& url,
               const std::string& domain, const std::string& path,
               const Time& expiration, const std::set<int> ports);

    const GURL& url() const { return url_; }
    const std::string& client_hash() const { return client_hash_; }

    // Security method to check if we can advertise this dictionary for use
    // if the |target_url| returns SDCH compressed data.
    bool CanAdvertise(const GURL& target_url);

    // Security methods to check if we can establish a new dictionary with the
    // given data, that arrived in response to get of dictionary_url.
    static bool CanSet(const std::string& domain, const std::string& path,
                       const std::set<int> ports, const GURL& dictionary_url);

    // Security method to check if we can use a dictionary to decompress a
    // target that arrived with a reference to this dictionary.
    bool CanUse(const GURL referring_url);

    // Compare paths to see if they "match" for dictionary use.
    static bool PathMatch(const std::string& path,
                          const std::string& restriction);

    // Compare domains to see if the "match" for dictionary use.
    static bool DomainMatch(const GURL& url, const std::string& restriction);


    // The actual text of the dictionary.
    std::string text_;

    // Part of the hash of text_ that the client uses to advertise the fact that
    // it has a specific dictionary pre-cached.
    std::string client_hash_;

    // The GURL that arrived with the text_ in a URL request to specify where
    // this dictionary may be used.
    const GURL url_;

    // Metadate "headers" in before dictionary text contained the following:
    // Each dictionary payload consists of several headers, followed by the text
    // of the dictionary.  The following are the known headers.
    const std::string domain_;
    const std::string path_;
    const Time expiration_;  // Implied by max-age.
    const std::set<int> ports_;

    DISALLOW_COPY_AND_ASSIGN(Dictionary);
  };

  SdchManager();
  ~SdchManager();

  // Provide access to the single instance of this class.
  static SdchManager* Global();

  // Record stats on various errors.
  static void SdchErrorRecovery(ProblemCodes problem);

  // Register a fetcher that this class can use to obtain dictionaries.
  void set_sdch_fetcher(SdchFetcher* fetcher) { fetcher_.reset(fetcher); }

  // If called with an empty string, advertise and support sdch on all domains.
  // If called with a specific string, advertise and support only the specified
  // domain.  Function assumes the existence of a global SdchManager instance.
  void EnableSdchSupport(const std::string& domain);

  static bool sdch_enabled() { return global_ && global_->sdch_enabled_; }

  // Prevent further advertising of SDCH on this domain (if SDCH is enabled).
  // Used when filter errors are found from a given domain, to prevent further
  // use of SDCH on that domain.
  static bool BlacklistDomain(const GURL& url);

  // For testing only, tihs function resets enabling of sdch, and clears the
  // blacklist.
  static void ClearBlacklistings();

  // Check to see if SDCH is enabled (globally), and the given URL is in a
  // supported domain (i.e., not blacklisted, and either the specific supported
  // domain, or all domains were assumed supported).
  const bool IsInSupportedDomain(const GURL& url) const;

  // Schedule the URL fetching to load a dictionary. This will generally return
  // long before the dictionary is actually loaded and added.
  // After the implied task does completes, the dictionary will have been
  // cached in memory.
  void FetchDictionary(const GURL& referring_url, const GURL& dictionary_url);

  // Add an SDCH dictionary to our list of availible dictionaries. This addition
  // will fail (return false) if addition is illegal (data in the dictionary is
  // not acceptable from the dictionary_url; dictionary already added, etc.).
  bool AddSdchDictionary(const std::string& dictionary_text,
                         const GURL& dictionary_url);

  // Find the vcdiff dictionary (the body of the sdch dictionary that appears
  // after the meta-data headers like Domain:...) with the given |server_hash|
  // to use to decompreses data that arrived as SDCH encoded content.  Check to
  // be sure the returned |dictionary| can be used for decoding content supplied
  // in response to a request for |referring_url|.
  // Caller is responsible for AddRef()ing the dictionary, and Release()ing it
  // when done.
  // Return null in |dictionary| if there is no matching legal dictionary.
  void GetVcdiffDictionary(const std::string& server_hash,
                           const GURL& referring_url,
                           Dictionary** dictionary);

  // Get list of available (pre-cached) dictionaries that we have already loaded
  // into memory.  The list is a comma separated list of (client) hashes per
  // the SDCH spec.
  void GetAvailDictionaryList(const GURL& target_url, std::string* list);

  // Construct the pair of hashes for client and server to identify an SDCH
  // dictionary.  This is only made public to facilitate unit testing, but is
  // otherwise private
  static void GenerateHash(const std::string& dictionary_text,
                           std::string* client_hash, std::string* server_hash);

 private:
  // A map of dictionaries info indexed by the hash that the server provides.
  typedef std::map<std::string, Dictionary*> DictionaryMap;

  // The one global instance of that holds all the data.
  static SdchManager* global_;

  // A simple implementation of a RFC 3548 "URL safe" base64 encoder.
  static void UrlSafeBase64Encode(const std::string& input,
                                  std::string* output);
  DictionaryMap dictionaries_;

  // An instance that can fetch a dictionary given a URL.
  scoped_ptr<SdchFetcher> fetcher_;

  // Support SDCH compression, by advertising in headers.
  bool sdch_enabled_;

  // Empty string means all domains.  Non-empty means support only the given
  // domain is supported.
  std::string supported_domain_;

  // List domains where decode failures have required disabling sdch.
  std::set<std::string> blacklisted_domains_;

  DISALLOW_COPY_AND_ASSIGN(SdchManager);
};

#endif  // NET_BASE_SDCH_MANAGER_H_
