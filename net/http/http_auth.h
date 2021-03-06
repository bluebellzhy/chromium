// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_H_
#define NET_HTTP_HTTP_AUTH_H_

#include "net/http/http_util.h"

namespace net {

class HttpAuthHandler;
class HttpResponseHeaders;

// Utility class for http authentication.
class HttpAuth {
 public:

   // Http authentication can be done the the proxy server, origin server, 
   // or both. This enum tracks who the target is.
   enum Target {
     AUTH_PROXY = 0,
     AUTH_SERVER = 1,
   };

  // Get the name of the header containing the auth challenge
  // (either WWW-Authenticate or Proxy-Authenticate).
  static std::string GetChallengeHeaderName(Target target);

  // Get the name of the header where the credentials go
  // (either Authorization or Proxy-Authorization).
  static std::string GetAuthorizationHeaderName(Target target);

  // Create a handler to generate credentials for the challenge. If the
  // challenge is unsupported or invalid, returns NULL.
  // The caller owns the returned pointer.
  static HttpAuthHandler* CreateAuthHandler(const std::string& challenge,
                                            Target target);

  // Iterate through the challenge headers, and pick the best one that
  // we support. Returns the implementation class for handling the challenge.
  // If no supported challenge was found, returns NULL.
  // The caller owns the returned pointer.
  static HttpAuthHandler* ChooseBestChallenge(
      const HttpResponseHeaders* headers, Target target);

  // ChallengeTokenizer breaks up a challenge string into the the auth scheme
  // and parameter list, according to RFC 2617 Sec 1.2:
  //    challenge = auth-scheme 1*SP 1#auth-param
  //
  // Check valid() after each iteration step in case it was malformed.
  // Also note that value() will give whatever is to the right of the equals
  // sign, quotemarks and all. Use unquoted_value() to get the logical value.
  class ChallengeTokenizer {
   public:
    ChallengeTokenizer(std::string::const_iterator begin,
                       std::string::const_iterator end)
        : props_(begin, end, ','), valid_(true) {
      Init(begin, end);
    }

    // Get the auth scheme of the challenge.
    std::string::const_iterator scheme_begin() const { return scheme_begin_; }
    std::string::const_iterator scheme_end() const { return scheme_end_; }
    std::string scheme() const {
      return std::string(scheme_begin_, scheme_end_);
    }
    
    // Returns false if there was a parse error.
    bool valid() const {
      return valid_;
    }

    // Advances the iterator to the next name-value pair, if any.
    // Returns true if there is none to consume.
    bool GetNext();

    // The name of the current name-value pair.
    std::string::const_iterator name_begin() const { return name_begin_; }
    std::string::const_iterator name_end() const { return name_end_; }
    std::string name() const {
      return std::string(name_begin_, name_end_);
    }

    // The value of the current name-value pair.
    std::string::const_iterator value_begin() const { return value_begin_; }
    std::string::const_iterator value_end() const { return value_end_; }
    std::string value() const {
      return std::string(value_begin_, value_end_);
    }

    // If value() has quotemarks, unquote it.
    std::string unquoted_value() const;
    
    // True if the name-value pair's value has quote marks.
    bool value_is_quoted() const { return value_is_quoted_; }

   private:
    void Init(std::string::const_iterator begin,
              std::string::const_iterator end);

    HttpUtil::ValuesIterator props_;
    bool valid_;

    std::string::const_iterator scheme_begin_;
    std::string::const_iterator scheme_end_;

    std::string::const_iterator name_begin_;
    std::string::const_iterator name_end_;

    std::string::const_iterator value_begin_;
    std::string::const_iterator value_end_;

    bool value_is_quoted_;
  };
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_H_
