// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions of this code based on Mozilla:
//   (netwerk/cookie/src/nsCookieService.cpp)
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/base/cookie_monster.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"

// #define COOKIE_LOGGING_ENABLED
#ifdef COOKIE_LOGGING_ENABLED
#define COOKIE_DLOG(severity) DLOG_IF(INFO, 1)
#else
#define COOKIE_DLOG(severity) DLOG_IF(INFO, 0)
#endif

namespace net {

// static
bool CookieMonster::enable_file_scheme_ = false;

// static
void CookieMonster::EnableFileScheme() {
  enable_file_scheme_ = true;
}

CookieMonster::CookieMonster()
    : initialized_(false),
      store_(NULL) {
}

CookieMonster::CookieMonster(PersistentCookieStore* store)
    : initialized_(false),
      store_(store) {
}

CookieMonster::~CookieMonster() {
  DeleteAll(false);
}

void CookieMonster::InitStore() {
  DCHECK(store_) << "Store must exist to initialize";

  // Initialize the store and sync in any saved persistent cookies.  We don't
  // care if it's expired, insert it so it can be garbage collected, removed,
  // and sync'd.
  std::vector<KeyedCanonicalCookie> cookies;
  store_->Load(&cookies);
  for (std::vector<KeyedCanonicalCookie>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    InternalInsertCookie(it->first, it->second, false);
  }
}

// The system resolution is not high enough, so we can have multiple
// set cookies that result in the same system time.  When this happens, we
// increment by one Time unit.  Let's hope computers don't get too fast.
Time CookieMonster::CurrentTime() {
  return std::max(Time::Now(),
      Time::FromInternalValue(last_time_seen_.ToInternalValue() + 1));
}

// Parse a cookie expiration time.  We try to be lenient, but we need to
// assume some order to distinguish the fields.  The basic rules:
//  - The month name must be present and prefix the first 3 letters of the
//    full month name (jan for January, jun for June).
//  - If the year is <= 2 digits, it must occur after the day of month.
//  - The time must be of the format hh:mm:ss.
// An average cookie expiration will look something like this:
//   Sat, 15-Apr-17 21:01:22 GMT
Time CookieMonster::ParseCookieTime(const std::string& time_string) {
  static const char* kMonths[] = { "jan", "feb", "mar", "apr", "may", "jun",
                                   "jul", "aug", "sep", "oct", "nov", "dec" };
  static const int kMonthsLen = arraysize(kMonths);
  // We want to be pretty liberal, and support most non-ascii and non-digit
  // characters as a delimiter.  We can't treat : as a delimiter, because it
  // is the delimiter for hh:mm:ss, and we want to keep this field together.
  // We make sure to include - and +, since they could prefix numbers.
  // If the cookie attribute came in in quotes (ex expires="XXX"), the quotes
  // will be preserved, and we will get them here.  So we make sure to include
  // quote characters, and also \ for anything that was internally escaped.
  static const char* kDelimiters = "\t !\"#$%&'()*+,-./;<=>?@[\\]^_`{|}~";

  Time::Exploded exploded = {0};

  StringTokenizer tokenizer(time_string, kDelimiters);

  bool found_day_of_month = false;
  bool found_month = false;
  bool found_time = false;
  bool found_year = false;

  while (tokenizer.GetNext()) {
    const std::string token = tokenizer.token();
    DCHECK(!token.empty());
    bool numerical = IsAsciiDigit(token[0]);

    // String field
    if (!numerical) {
      if (!found_month) {
        for (int i = 0; i < kMonthsLen; ++i) {
          // Match prefix, so we could match January, etc
          if (StrNCaseCmp(token.c_str(), kMonths[i], 3) == 0) {
            exploded.month = i + 1;
            found_month = true;
            break;
          }
        }
      } else {
        // If we've gotten here, it means we've already found and parsed our
        // month, and we have another string, which we would expect to be the
        // the time zone name.  According to the RFC and my experiments with
        // how sites format their expirations, we don't have much of a reason
        // to support timezones.  We don't want to ever barf on user input,
        // but this DCHECK should pass for well-formed data.
        // DCHECK(token == "GMT");
      }
    // Numeric field w/ a colon
    } else if (token.find(':') != std::string::npos) {
      if (!found_time &&
#ifdef COMPILER_MSVC
          sscanf_s(
#else
          sscanf(
#endif
                 token.c_str(), "%2u:%2u:%2u", &exploded.hour,
                 &exploded.minute, &exploded.second) == 3) {
        found_time = true;
      } else {
        // We should only ever encounter one time-like thing.  If we're here,
        // it means we've found a second, which shouldn't happen.  We keep
        // the first.  This check should be ok for well-formed input:
        // NOTREACHED();
      }
    // Numeric field
    } else {
      // Overflow with atoi() is unspecified, so we enforce a max length.
      if (!found_day_of_month && token.length() <= 2) {
        exploded.day_of_month = atoi(token.c_str());
        found_day_of_month = true;
      } else if (!found_year && token.length() <= 5) {
        exploded.year = atoi(token.c_str());
        found_year = true;
      } else {
        // If we're here, it means we've either found an extra numeric field,
        // or a numeric field which was too long.  For well-formed input, the
        // following check would be reasonable:
        // NOTREACHED();
      }
    }
  }

  if (!found_day_of_month || !found_month || !found_time || !found_year) {
    // We didn't find all of the fields we need.  For well-formed input, the
    // following check would be reasonable:
    // NOTREACHED() << "Cookie parse expiration failed: " << time_string;
    return Time();
  }

  // Normalize the year to expand abbreviated years to the full year.
  if (exploded.year >= 69 && exploded.year <= 99)
    exploded.year += 1900;
  if (exploded.year >= 0 && exploded.year <= 68)
    exploded.year += 2000;

  // If our values are within their correct ranges, we got our time.
  if (exploded.day_of_month >= 1 && exploded.day_of_month <= 31 &&
      exploded.month >= 1 && exploded.month <= 12 &&
      exploded.year >= 1601 && exploded.year <= 30827 &&
      exploded.hour <= 23 && exploded.minute <= 59 && exploded.second <= 59) {
    return Time::FromUTCExploded(exploded);
  }

  // One of our values was out of expected range.  For well-formed input,
  // the following check would be reasonable:
  // NOTREACHED() << "Cookie exploded expiration failed: " << time_string;

  return Time();
}

// Determine the cookie domain key to use for setting the specified cookie.
// On success returns true, and sets cookie_domain_key to either a
//   -host cookie key (ex: "google.com")
//   -domain cookie key (ex: ".google.com")
static bool GetCookieDomainKey(const GURL& url,
                               const CookieMonster::ParsedCookie& pc,
                               std::string* cookie_domain_key) {
  const std::string url_host(url.host());
  if (!pc.HasDomain() || pc.Domain().empty()) {
    // No domain was specified in cookie -- default to host cookie.
    *cookie_domain_key = url_host;
    DCHECK((*cookie_domain_key)[0] != '.');
    return true;
  }

  // Get the normalized domain specified in cookie line.
  // Note: The RFC says we can reject a cookie if the domain
  // attribute does not start with a dot. IE/FF/Safari however, allow a cookie
  // of the form domain=my.domain.com, treating it the same as
  // domain=.my.domain.com -- for compatibility we do the same here.  Firefox
  // also treats domain=.....my.domain.com like domain=.my.domain.com, but
  // neither IE nor Safari do this, and we don't either.
  std::string cookie_domain(net::CanonicalizeHost(pc.Domain(), NULL));
  if (cookie_domain.empty())
    return false;
  if (cookie_domain[0] != '.')
    cookie_domain = "." + cookie_domain;

  // Ensure |url| and |cookie_domain| have the same domain+registry.
  const std::string url_domain_and_registry(
      RegistryControlledDomainService::GetDomainAndRegistry(url));
  if (url_domain_and_registry.empty())
    return false;  // IP addresses/intranet hosts can't set domain cookies.
  const std::string cookie_domain_and_registry(
      RegistryControlledDomainService::GetDomainAndRegistry(cookie_domain));
  if (url_domain_and_registry != cookie_domain_and_registry)
    return false;  // Can't set a cookie on a different domain + registry.

  // Ensure |url_host| is |cookie_domain| or one of its subdomains.  Given that
  // we know the domain+registry are the same from the above checks, this is
  // basically a simple string suffix check.
  if ((url_host.length() < cookie_domain.length()) ?
      (cookie_domain != ("." + url_host)) :
      url_host.compare(url_host.length() - cookie_domain.length(),
                       cookie_domain.length(), cookie_domain))
    return false;


  *cookie_domain_key = cookie_domain;
  return true;
}

static std::string CanonPath(const GURL& url,
                             const CookieMonster::ParsedCookie& pc) {
  // The RFC says the path should be a prefix of the current URL path.
  // However, Mozilla allows you to set any path for compatibility with
  // broken websites.  We unfortunately will mimic this behavior.  We try
  // to be generous and accept cookies with an invalid path attribute, and
  // default the path to something reasonable.

  // The path was supplied in the cookie, we'll take it.
  if (pc.HasPath() && !pc.Path().empty() && pc.Path()[0] == '/')
    return pc.Path();

  // The path was not supplied in the cookie or invalid, we will default
  // to the current URL path.
  // """Defaults to the path of the request URL that generated the
  //    Set-Cookie response, up to, but not including, the
  //    right-most /."""
  // How would this work for a cookie on /?  We will include it then.
  const std::string& url_path = url.path();

  std::string::size_type idx = url_path.find_last_of('/');

  // The cookie path was invalid or a single '/'.
  if (idx == 0 || idx == std::string::npos)
    return std::string("/");

  // Return up to the rightmost '/'.
  return url_path.substr(0, idx);
}

static Time CanonExpiration(const CookieMonster::ParsedCookie& pc,
                            const Time& current) {
  // First, try the Max-Age attribute.
  uint64 max_age = 0;
  if (pc.HasMaxAge() &&
#if defined(COMPILER_MSVC)
      sscanf_s(pc.MaxAge().c_str(), " %I64u", &max_age) == 1) {

#else
      sscanf(pc.MaxAge().c_str(), " %llu", &max_age) == 1) {
#endif
    return current + TimeDelta::FromSeconds(max_age);
  }

  // Try the Expires attribute.
  if (pc.HasExpires())
    return CookieMonster::ParseCookieTime(pc.Expires());

  // Invalid or no expiration, persistent cookie.
  return Time();
}

static bool HasCookieableScheme(const GURL& url) {
  static const char* kCookieableSchemes[]  = { "http", "https", "file" };
  static const int   kCookieableSchemesLen = arraysize(kCookieableSchemes);
  static const int   kCookieableSchemesFileIndex = 2;

  // Make sure the request is on a cookie-able url scheme.
  for (int i = 0; i < kCookieableSchemesLen; ++i) {
    // We matched a scheme.
    if (url.SchemeIs(kCookieableSchemes[i])) {
      // This is file:// scheme
      if (i == kCookieableSchemesFileIndex)
        return CookieMonster::enable_file_scheme_;
      // We've matched a supported scheme.
      return true;
    }
  }

  // The scheme didn't match any in our whitelist.
  COOKIE_DLOG(WARNING) << "Unsupported cookie scheme: " << url.scheme();
  return false;
}

bool CookieMonster::SetCookie(const GURL& url,
                              const std::string& cookie_line) {
  Time creation_date = CurrentTime();
  last_time_seen_ = creation_date;
  return SetCookieWithCreationTime(url, cookie_line, creation_date);
}

bool CookieMonster::SetCookieWithCreationTime(const GURL& url,
                                              const std::string& cookie_line,
                                              const Time& creation_time) {
  DCHECK(!creation_time.is_null());

  if (!HasCookieableScheme(url)) {
    DLOG(WARNING) << "Unsupported cookie scheme: " << url.scheme();
    return false;
  }

  AutoLock autolock(lock_);
  InitIfNecessary();

  COOKIE_DLOG(INFO) << "SetCookie() line: " << cookie_line;

  // Parse the cookie.
  ParsedCookie pc(cookie_line);

  if (!pc.IsValid()) {
    COOKIE_DLOG(WARNING) << "Couldn't parse cookie";
    return false;
  }

  std::string cookie_domain;
  if (!GetCookieDomainKey(url, pc, &cookie_domain)) {
    return false;
  }

  std::string cookie_path = CanonPath(url, pc);

  scoped_ptr<CanonicalCookie> cc;
  Time cookie_expires = CanonExpiration(pc, creation_time);

  cc.reset(new CanonicalCookie(pc.Name(), pc.Value(), cookie_path,
                               pc.IsSecure(), pc.IsHttpOnly(),
                               creation_time, !cookie_expires.is_null(),
                               cookie_expires));

  if (!cc.get()) {
    COOKIE_DLOG(WARNING) << "Failed to allocate CanonicalCookie";
    return false;
  }

  // We should have only purged at most one matching cookie.
  int num_deleted = DeleteEquivalentCookies(cookie_domain, *cc);
  DCHECK(num_deleted <= 1);

  COOKIE_DLOG(INFO) << "SetCookie() cc: " << cc->DebugString();

  // Realize that we might be setting an expired cookie, and the only point
  // was to delete the cookie which we've already done.
  if (!cc->IsExpired(creation_time))
    InternalInsertCookie(cookie_domain, cc.release(), true);

  // We assume that hopefully setting a cookie will be less common than
  // querying a cookie.  Since setting a cookie can put us over our limits,
  // make sure that we garbage collect...  We can also make the assumption that
  // if a cookie was set, in the common case it will be used soon after,
  // and we will purge the expired cookies in GetCookies().
  GarbageCollect(creation_time, cookie_domain);

  return true;
}

void CookieMonster::SetCookies(const GURL& url,
                               const std::vector<std::string>& cookies) {
  for (std::vector<std::string>::const_iterator iter = cookies.begin();
       iter != cookies.end(); ++iter)
    SetCookie(url, *iter);
}

void CookieMonster::InternalInsertCookie(const std::string& key,
                                         CanonicalCookie* cc,
                                         bool sync_to_store) {
  if (cc->IsPersistent() && store_ && sync_to_store)
    store_->AddCookie(key, *cc);
  cookies_.insert(CookieMap::value_type(key, cc));
}

void CookieMonster::InternalDeleteCookie(CookieMap::iterator it,
                                         bool sync_to_store) {
  CanonicalCookie* cc = it->second;
  COOKIE_DLOG(INFO) << "InternalDeleteCookie() cc: " << cc->DebugString();
  if (cc->IsPersistent() && store_ && sync_to_store)
    store_->DeleteCookie(*cc);
  cookies_.erase(it);
  delete cc;
}

int CookieMonster::DeleteEquivalentCookies(const std::string& key,
                                           const CanonicalCookie& ecc) {
  int num_deleted = 0;
  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second; ) {
    CookieMap::iterator curit = its.first;
    CanonicalCookie* cc = curit->second;
    ++its.first;

    // TODO while we're here, we might as well purge expired cookies too.

    if (ecc.IsEquivalent(*cc)) {
      InternalDeleteCookie(curit, true);
      ++num_deleted;
#ifdef NDEBUG
      // We should only ever find a single equivalent cookie
      break;
#endif
    }
  }

  // Our internal state should be consistent, we should never have more
  // than one equivalent cookie, since they should overwrite each other.
  DCHECK(num_deleted <= 1);

  return num_deleted;
}

// TODO we should be sorting by last access time, however, right now
// we're not saving an access time, so we're sorting by creation time.
static bool OldestCookieSorter(const CookieMonster::CookieMap::iterator& it1,
                               const CookieMonster::CookieMap::iterator& it2) {
  return it1->second->CreationDate() < it2->second->CreationDate();
}

// is vector::size_type always going to be size_t?
int CookieMonster::GarbageCollectRange(const Time& current,
                                       const CookieMapItPair& itpair,
                                       size_t num_max, size_t num_purge) {
  int num_deleted = 0;

  // First, walk through and delete anything that's expired.
  // Save a list of iterators to the ones that weren't expired
  std::vector<CookieMap::iterator> cookie_its;
  for (CookieMap::iterator it = itpair.first, end = itpair.second; it != end;) {
    CookieMap::iterator curit = it;
    CanonicalCookie* cc = curit->second;
    ++it;

    if (cc->IsExpired(current)) {
      InternalDeleteCookie(curit, true);
      ++num_deleted;
    } else {
      cookie_its.push_back(curit);
    }
  }

  if (cookie_its.size() > num_max) {
    COOKIE_DLOG(INFO) << "GarbageCollectRange() Deep Garbage Collect.";
    num_purge += cookie_its.size() - num_max;
    // Sort the top N we want to purge.
    std::partial_sort(cookie_its.begin(), cookie_its.begin() + num_purge,
                      cookie_its.end(), OldestCookieSorter);

    // TODO should probably use an iterator and not an index.
    for (size_t i = 0; i < num_purge; ++i) {
      InternalDeleteCookie(cookie_its[i], true);
      ++num_deleted;
    }
  }

  return num_deleted;
}

// TODO Whenever we delete, check last_cur_utc_...
int CookieMonster::GarbageCollect(const Time& current,
                                  const std::string& key) {
  // Based off of the Mozilla defaults
  // It might seem scary to have a high purge value, but really it's not.  You
  // just make sure that you increase the max to cover the increase in purge,
  // and we would have been purging the same amount of cookies.  We're just
  // going through the garbage collection process less often.
  static const size_t kNumCookiesPerHost      = 70;  // ~50 cookies
  static const size_t kNumCookiesPerHostPurge = 20;
  static const size_t kNumCookiesTotal        = 1100;  // ~1000 cookies
  static const size_t kNumCookiesTotalPurge   = 100;

  int num_deleted = 0;

  // Collect garbage for this key.
  if (cookies_.count(key) > kNumCookiesPerHost) {
    COOKIE_DLOG(INFO) << "GarbageCollect() key: " << key;
    num_deleted += GarbageCollectRange(current, cookies_.equal_range(key),
                                       kNumCookiesPerHost,
                                       kNumCookiesPerHostPurge);
  }

  // Collect garbage for everything.
  if (cookies_.size() > kNumCookiesTotal) {
    COOKIE_DLOG(INFO) << "GarbageCollect() everything";
    num_deleted += GarbageCollectRange(current,
                                       CookieMapItPair(cookies_.begin(),
                                                       cookies_.end()),
                                       kNumCookiesTotal, kNumCookiesTotalPurge);
  }

  return num_deleted;
}

int CookieMonster::DeleteAll(bool sync_to_store) {
  AutoLock autolock(lock_);
  InitIfNecessary();

  int num_deleted = 0;
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    ++it;
    InternalDeleteCookie(curit, sync_to_store);
    ++num_deleted;
  }

  return num_deleted;
}

int CookieMonster::DeleteAllCreatedBetween(const Time& delete_begin,
                                           const Time& delete_end,
                                           bool sync_to_store) {
  AutoLock autolock(lock_);
  InitIfNecessary();

  int num_deleted = 0;
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    CanonicalCookie* cc = curit->second;
    ++it;

    if (cc->CreationDate() >= delete_begin &&
        (delete_end.is_null() || cc->CreationDate() < delete_end)) {
      InternalDeleteCookie(curit, sync_to_store);
      ++num_deleted;
    }
  }

  return num_deleted;
}

int CookieMonster::DeleteAllCreatedAfter(const Time& delete_begin,
                                         bool sync_to_store) {
  return DeleteAllCreatedBetween(delete_begin, Time(), sync_to_store);
}

bool CookieMonster::DeleteCookie(const std::string& domain,
                                 const CanonicalCookie& cookie,
                                 bool sync_to_store) {
  AutoLock autolock(lock_);
  InitIfNecessary();

  for (CookieMapItPair its = cookies_.equal_range(domain);
       its.first != its.second; ++its.first) {
    // The creation date acts as our unique index...
    if (its.first->second->CreationDate() == cookie.CreationDate()) {
      InternalDeleteCookie(its.first, sync_to_store);
      return true;
    }
  }
  return false;
}

// Mozilla sorts on the path length (longest first), and then it
// sorts by creation time (oldest first).
// The RFC says the sort order for the domain attribute is undefined.
static bool CookieSorter(CookieMonster::CanonicalCookie* cc1,
                         CookieMonster::CanonicalCookie* cc2) {
  if (cc1->Path().length() == cc2->Path().length())
    return cc1->CreationDate() < cc2->CreationDate();
  return cc1->Path().length() > cc2->Path().length();
}

std::string CookieMonster::GetCookies(const GURL& url) {
  return GetCookiesWithOptions(url, NORMAL);
}

// Currently our cookie datastructure is based on Mozilla's approach.  We have a
// hash keyed on the cookie's domain, and for any query we walk down the domain
// components and probe for cookies until we reach the TLD, where we stop.
// For example, a.b.blah.com, we would probe
//   - a.b.blah.com
//   - .a.b.blah.com (TODO should we check this first or second?)
//   - .b.blah.com
//   - .blah.com
// There are some alternative datastructures we could try, like a
// search/prefix trie, where we reverse the hostname and query for all
// keys that are a prefix of our hostname.  I think the hash probing
// should be fast and simple enough for now.
std::string CookieMonster::GetCookiesWithOptions(const GURL& url,
                                                 CookieOptions options) {
  if (!HasCookieableScheme(url)) {
    DLOG(WARNING) << "Unsupported cookie scheme: " << url.scheme();
    return std::string();
  }

  // Get the cookies for this host and its domain(s).
  std::vector<CanonicalCookie*> cookies;
  FindCookiesForHostAndDomain(url, options, &cookies);
  std::sort(cookies.begin(), cookies.end(), CookieSorter);

  std::string cookie_line;
  for (std::vector<CanonicalCookie*>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if (it != cookies.begin())
      cookie_line += "; ";
    // In Mozilla if you set a cookie like AAAA, it will have an empty token
    // and a value of AAAA.  When it sends the cookie back, it will send AAAA,
    // so we need to avoid sending =AAAA for a blank token value.
    if (!(*it)->Name().empty())
      cookie_line += (*it)->Name() + "=";
    cookie_line += (*it)->Value();
  }

  COOKIE_DLOG(INFO) << "GetCookies() result: " << cookie_line;

  return cookie_line;
}

// TODO(deanm): We could have expired cookies that haven't been purged yet,
// and exporting these would be inaccurate, for example in the cookie manager
// it might show cookies that are actually expired already.  We should do
// a full garbage collection before ...  There actually isn't a way to do
// this right now (a forceful full GC), so we'll have to live with the
// possibility of showing the user expired cookies.  This shouldn't be very
// common since most persistent cookies have a long lifetime.
CookieMonster::CookieList CookieMonster::GetAllCookies() {
  AutoLock autolock(lock_);
  InitIfNecessary();

  CookieList cookie_list;

  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end(); ++it) {
    cookie_list.push_back(CookieListPair(it->first, *it->second));
  }

  return cookie_list;
}

void CookieMonster::FindCookiesForHostAndDomain(
    const GURL& url,
    CookieOptions options,
    std::vector<CanonicalCookie*>* cookies) {
  AutoLock autolock(lock_);
  InitIfNecessary();

  const Time current_time(CurrentTime());

  // Query for the full host, For example: 'a.c.blah.com'.
  std::string key(url.host());
  FindCookiesForKey(key, url, options, current_time, cookies);

  // See if we can search for domain cookies, i.e. if the host has a TLD + 1.
  const std::string domain(
      RegistryControlledDomainService::GetDomainAndRegistry(key));
  if (domain.empty())
    return;
  DCHECK_LE(domain.length(), key.length());
  DCHECK_EQ(0, key.compare(key.length() - domain.length(), domain.length(),
                           domain));

  // Walk through the string and query at the dot points (GURL should have
  // canonicalized the dots, so this should be safe).  Stop once we reach the
  // domain + registry; we can't write cookies past this point, and with some
  // registrars other domains can, in which case we don't want to read their
  // cookies.
  for (key = "." + key; key.length() > domain.length(); ) {
    FindCookiesForKey(key, url, options, current_time, cookies);
    const size_t next_dot = key.find('.', 1);  // Skip over leading dot.
    key.erase(0, next_dot);
  }
}

void CookieMonster::FindCookiesForKey(
    const std::string& key,
    const GURL& url,
    CookieOptions options,
    const Time& current,
    std::vector<CanonicalCookie*>* cookies) {
  bool secure = url.SchemeIsSecure();

  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second; ) {
    CookieMap::iterator curit = its.first;
    CanonicalCookie* cc = curit->second;
    ++its.first;

    // If the cookie is expired, delete it.
    if (cc->IsExpired(current)) {
      InternalDeleteCookie(curit, true);
      continue;
    }

    // Filter out HttpOnly cookies unless they where explicitly requested.
    if ((options & INCLUDE_HTTPONLY) == 0 && cc->IsHttpOnly())
      continue;

    // Filter out secure cookies unless we're https.
    if (!secure && cc->IsSecure())
      continue;

    if (!cc->IsOnPath(url.path()))
      continue;

    // Congratulations Charlie, you passed the test!
    cookies->push_back(cc);
  }
}


CookieMonster::ParsedCookie::ParsedCookie(const std::string& cookie_line)
    : is_valid_(false),
      path_index_(0),
      domain_index_(0),
      expires_index_(0),
      maxage_index_(0),
      secure_index_(0),
      httponly_index_(0) {

  if (cookie_line.size() > kMaxCookieSize) {
    LOG(INFO) << "Not parsing cookie, too large: " << cookie_line.size();
    return;
  }

  ParseTokenValuePairs(cookie_line);
  if (pairs_.size() > 0) {
    is_valid_ = true;
    SetupAttributes();
  }
}

// Returns true if |c| occurs in |chars|
// TODO maybe make this take an iterator, could check for end also?
static inline bool CharIsA(const char c, const char* chars) {
  return strchr(chars, c) != NULL;
}
// Seek the iterator to the first occurrence of a character in |chars|.
// Returns true if it hit the end, false otherwise.
static inline bool SeekTo(std::string::const_iterator* it,
                          const std::string::const_iterator& end,
                          const char* chars) {
  for (; *it != end && !CharIsA(**it, chars); ++(*it));
  return *it == end;
}
// Seek the iterator to the first occurrence of a character not in |chars|.
// Returns true if it hit the end, false otherwise.
static inline bool SeekPast(std::string::const_iterator* it,
                            const std::string::const_iterator& end,
                            const char* chars) {
  for (; *it != end && CharIsA(**it, chars); ++(*it));
  return *it == end;
}
static inline bool SeekBackPast(std::string::const_iterator* it,
                                const std::string::const_iterator& end,
                                const char* chars) {
  for (; *it != end && CharIsA(**it, chars); --(*it));
  return *it == end;
}

// Parse all token/value pairs and populate pairs_.
void CookieMonster::ParsedCookie::ParseTokenValuePairs(
    const std::string& cookie_line) {
  static const char kTerminator[]      = "\n\r\0";
  static const int  kTerminatorLen     = sizeof(kTerminator) - 1;
  static const char kWhitespace[]      = " \t";
  static const char kQuoteTerminator[] = "\"";
  static const char kValueSeparator[]  = ";";
  static const char kTokenSeparator[]  = ";=";

  pairs_.clear();

  // Ok, here we go.  We should be expecting to be starting somewhere
  // before the cookie line, not including any header name...
  std::string::const_iterator start = cookie_line.begin();
  std::string::const_iterator end = cookie_line.end();
  std::string::const_iterator it = start;

  // TODO Make sure we're stripping \r\n in the network code.  Then we
  // can log any unexpected terminators.
  std::string::size_type term_pos = cookie_line.find_first_of(
      std::string(kTerminator, kTerminatorLen));
  if (term_pos != std::string::npos) {
    // We found a character we should treat as an end of string.
    end = start + term_pos;
  }

  for (int pair_num = 0; pair_num < kMaxPairs && it != end; ++pair_num) {
    TokenValuePair pair;
    std::string::const_iterator token_start, token_real_end, token_end;

    // Seek past any whitespace before the "token" (the name).
    // token_start should point at the first character in the token
    if (SeekPast(&it, end, kWhitespace))
      break;  // No token, whitespace or empty.
    token_start = it;

    // Seek over the token, to the token separator.
    // token_real_end should point at the token separator, i.e. '='.
    // If it == end after the seek, we probably have a token-value.
    SeekTo(&it, end, kTokenSeparator);
    token_real_end = it;

    // Ignore any whitespace between the token and the token separator.
    // token_end should point after the last interesting token character,
    // pointing at either whitespace, or at '=' (and equal to token_real_end).
    if (it != token_start) {  // We could have an empty token name.
      --it;  // Go back before the token separator.
      // Skip over any whitespace to the first non-whitespace character.
      SeekBackPast(&it, token_start, kWhitespace);
      // Point after it.
      ++it;
    }
    token_end = it;

    // Seek us back to the end of the token.
    it = token_real_end;

    if (it == end || *it != '=') {
      // We have a token-value, we didn't have any token name.
      if (pair_num == 0) {
        // For the first time around, we want to treat single values
        // as a value with an empty name. (Mozilla bug 169091).
        // IE seems to also have this behavior, ex "AAA", and "AAA=10" will
        // set 2 different cookies, and setting "BBB" will then replace "AAA".
        pair.first = "";
        // Rewind to the beginning of what we thought was the token name,
        // and let it get parsed as a value.
        it = token_start;
      } else {
        // Any not-first attribute we want to treat a value as a
        // name with an empty value...  This is so something like
        // "secure;" will get parsed as a Token name, and not a value.
        pair.first = std::string(token_start, token_end);
      }
    } else {
      // We have a TOKEN=VALUE.
      pair.first = std::string(token_start, token_end);
      ++it;  // Skip past the '='.
    }

    // OK, now try to parse a value.
    std::string::const_iterator value_start, value_end;

    // Seek past any whitespace that might in-between the token and value.
    SeekPast(&it, end, kWhitespace);
    // value_start should point at the first character of the value.
    value_start = it;

    // The value is double quoted, process <quoted-string>.
    if (it != end && *it == '"') {
      // Skip over the first double quote, and parse until
      // a terminating double quote or the end.
      for (++it; it != end && !CharIsA(*it, kQuoteTerminator); ++it) {
        // Allow an escaped \" in a double quoted string.
        if (*it == '\\') {
          ++it;
          if (it == end)
            break;
        }
      }

      SeekTo(&it, end, kValueSeparator);
      // We could seek to the end, that's ok.
      value_end = it;
    } else {
      // The value is non-quoted, process <token-value>.
      // Just look for ';' to terminate ('=' allowed).
      // We can hit the end, maybe they didn't terminate.
      SeekTo(&it, end, kValueSeparator);

      // Ignore any whitespace between the value and the value separator
      if (it != value_start) {  // Could have an empty value
        --it;
        SeekBackPast(&it, value_start, kWhitespace);
        ++it;
      }

      value_end = it;
    }

    // OK, we're finished with a Token/Value.
    pair.second = std::string(value_start, value_end);
    // From RFC2109: "Attributes (names) (attr) are case-insensitive."
    if (pair_num != 0)
      StringToLowerASCII(&pair.first);
    pairs_.push_back(pair);

    // We've processed a token/value pair, we're either at the end of
    // the string or a ValueSeparator like ';', which we want to skip.
    if (it != end)
      ++it;
  }
}

void CookieMonster::ParsedCookie::SetupAttributes() {
  static const char kPathTokenName[]      = "path";
  static const char kDomainTokenName[]    = "domain";
  static const char kExpiresTokenName[]   = "expires";
  static const char kMaxAgeTokenName[]    = "max-age";
  static const char kSecureTokenName[]    = "secure";
  static const char kHttpOnlyTokenName[]  = "httponly";

  // We skip over the first token/value, the user supplied one.
  for (size_t i = 1; i < pairs_.size(); ++i) {
    if (pairs_[i].first == kPathTokenName)
      path_index_ = i;
    else if (pairs_[i].first == kDomainTokenName)
      domain_index_ = i;
    else if (pairs_[i].first == kExpiresTokenName)
      expires_index_ = i;
    else if (pairs_[i].first == kMaxAgeTokenName)
      maxage_index_ = i;
    else if (pairs_[i].first == kSecureTokenName)
      secure_index_ = i;
    else if (pairs_[i].first == kHttpOnlyTokenName)
      httponly_index_ = i;
    else { /* some attribute we don't know or don't care about. */ }
  }
}

// Create a cookie-line for the cookie.  For debugging only!
// If we want to use this for something more than debugging, we
// should rewrite it better...
std::string CookieMonster::ParsedCookie::DebugString() const {
  std::string out;
  for (PairList::const_iterator it = pairs_.begin();
       it != pairs_.end(); ++it) {
    out.append(it->first);
    out.append("=");
    out.append(it->second);
    out.append("; ");
  }
  return out;
}

bool CookieMonster::CanonicalCookie::IsOnPath(
    const std::string& url_path) const {

  // A zero length would be unsafe for our trailing '/' checks, and
  // would also make no sense for our prefix match.  The code that
  // creates a CanonicalCookie should make sure the path is never zero length,
  // but we double check anyway.
  if (path_.empty())
    return false;

  // The Mozilla code broke it into 3 cases, if it's strings lengths
  // are less than, equal, or greater.  I think this is simpler:

  // Make sure the cookie path is a prefix of the url path.  If the
  // url path is shorter than the cookie path, then the cookie path
  // can't be a prefix.
  if (url_path.find(path_) != 0)
    return false;

  // Now we know that url_path is >= cookie_path, and that cookie_path
  // is a prefix of url_path.  If they are the are the same length then
  // they are identical, otherwise we need an additional check:

  // In order to avoid in correctly matching a cookie path of /blah
  // with a request path of '/blahblah/', we need to make sure that either
  // the cookie path ends in a trailing '/', or that we prefix up to a '/'
  // in the url path.  Since we know that the url path length is greater
  // than the cookie path length, it's safe to index one byte past.
  if (path_.length() != url_path.length() &&
      path_[path_.length() - 1] != '/' &&
      url_path[path_.length()] != '/')
    return false;

  return true;
}

std::string CookieMonster::CanonicalCookie::DebugString() const {
  return StringPrintf("name: %s value: %s path: %s creation: %llu",
                      name_.c_str(), value_.c_str(), path_.c_str(),
                      creation_date_.ToTimeT());
}

}  // namespace

