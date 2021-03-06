// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "KURL.h"
#include "CString.h"
#include "NotImplemented.h"
#include "TextEncoding.h"
#include "Vector.h"

#ifdef USE_GOOGLE_URL_LIBRARY

#undef LOG
#include "base/string_util.h"
#include "googleurl/src/url_canon_internal.h"
#include "googleurl/src/url_util.h"

using namespace WTF;

namespace WebCore {

namespace {

// Wraps WebKit's text encoding in a character set converter for the
// canonicalizer.
class WebCoreCharsetConverter : public url_canon::CharsetConverter {
public:
    // The encoding parameter may be NULL, but in this case the object must not
    // be called.
    WebCoreCharsetConverter(const TextEncoding* encoding)
        : m_encoding(encoding) {
    }

    virtual void ConvertFromUTF16(const url_parse::UTF16Char* input,
                                  int input_len,
                                  url_canon::CanonOutput* output) {
        CString encoded = m_encoding->encode(input, input_len, URLEncodedEntitiesForUnencodables);
        output->Append(encoded.data(), static_cast<int>(encoded.length()));
    }

private:
    const TextEncoding* m_encoding;
};

// Note that this function must be named differently than the one in KURL.cpp
// since our unit tests evilly include both files, and their local definition
// will be ambiguous.
inline void AssertProtocolIsGood(const char* protocol)
{
#ifndef NDEBUG
    const char* p = protocol;
    while (*p) {
        ASSERT(*p > ' ' && *p < 0x7F && !(*p >= 'A' && *p <= 'Z'));
        ++p;
    }
#endif
}

// Returns the characters for the given string, or a pointer to a static empty
// string if the input string is NULL. This will always ensure we have a non-
// NULL character pointer since ReplaceComponents has special meaning for NULL.
inline const url_parse::UTF16Char* CharactersOrEmpty(const String& str) {
  static const url_parse::UTF16Char zero = 0;
  return str.characters() ?
         reinterpret_cast<const url_parse::UTF16Char*>(str.characters()) :
         &zero;
}

}  // namespace

// KURL::URLString -------------------------------------------------------------

KURL::URLString::URLString()
    : m_utf8IsASCII(true)
    , m_stringIsValid(false)
{
}

// Setters for the data. Using the ASCII version when you know the
// data is ASCII will be slightly more efficient. The UTF-8 version
// will always be correct if the caller is unsure.
void KURL::URLString::setUtf8(const char* data, int data_len)
{
    // The m_utf8IsASCII must always be correct since the DeprecatedString
    // getter must create it with the proper constructor. This test can be
    // removed when DeprecatedString is gone, but it still might be a
    // performance win.
    m_utf8IsASCII = true;
    for (int i = 0; i < data_len; i++) {
        if (static_cast<unsigned char>(data[i]) >= 0x80) {
            m_utf8IsASCII = false;
            break;
        }
    }
    
    m_utf8 = CString(data, data_len);
    m_stringIsValid = false;
}

void KURL::URLString::setAscii(const char* data, int data_len) {
    m_utf8 = CString(data, data_len);
    m_utf8IsASCII = true;
    m_stringIsValid = false;
}

const String& KURL::URLString::string() const
{
    if (!m_stringIsValid) {
        // Must special case the NULL case, since constructing the
        // string like we do below will generate an empty rather than
        // a NULL string.
        if (m_utf8.isNull())
            m_string = String();
        else if (m_utf8IsASCII)
            m_string = String(m_utf8.data(), m_utf8.length());
        else
            m_string = String::fromUTF8(m_utf8.data(), m_utf8.length());
        m_stringIsValid = true;
    }
    return m_string;
}

// KURL ------------------------------------------------------------------------

// Creates with NULL-terminated string input representing an absolute URL.
// WebCore generally calls this only with hardcoded strings, so the input is
// ASCII. We treat is as UTF-8 just in case.
KURL::KURL(const char *url)
{
    // FIXME(brettw) the Mac code checks for beginning with a slash and
    // converting to a file: URL. We will want to add this as well once we
    // can compile on a system like that.
    init(KURL(), url, strlen(url), NULL);

    // The one-argument constructors should never generate a NULL string.
    // This is a funny quirk of KURL (probably a bug) which we preserve.
    if (m_url.utf8String().isNull())
          m_url.setAscii("", 0);
}

// Initializes with a string representing an absolute URL. No encoding
// information is specified. This generally happens when a KURL is converted
// to a string and then converted back. In this case, the URL is already
// canonical and in proper escaped form so needs no encoding. We treat it was
// UTF-8 just in case.
KURL::KURL(const String& url)
{
    if (!url.isNull()) {
        init(KURL(), url, NULL);
    } else {
        // WebKit expects us to preserve the nullness of strings when this
        // constructor is used. In all other cases, it expects a non-null
        // empty string, which is what init() will create.
        m_isValid = false;
    }
}

// Constructs a new URL given a base URL and a possibly relative input URL.
// This assumes UTF-8 encoding.
KURL::KURL(const KURL& base, const String& relative)
{
    init(base, relative, NULL);
}

// Constructs a new URL given a base URL and a possibly relative input URL.
// Any query portion of the relative URL will be encoded in the given encoding.
KURL::KURL(const KURL& base,
           const String& relative,
           const TextEncoding& encoding)
{
    init(base, relative, &encoding);
}

KURL::KURL(const char* canonical_spec, int canonical_spec_len,
           const url_parse::Parsed& parsed, bool is_valid)
    : m_isValid(is_valid),
      m_parsed(parsed)
{
    // We know the reference fragment is the only part that can be UTF-8, so
    // we know it's ASCII when there is no ref.
    if (parsed.ref.is_nonempty())
        m_url.setUtf8(canonical_spec, canonical_spec_len);
    else
        m_url.setAscii(canonical_spec, canonical_spec_len);
}

#if PLATFORM(CF)
KURL::KURL(CFURLRef)
{
    notImplemented();
    invalidate();
}

CFURLRef KURL::createCFURL() const {
    notImplemented();
    return NULL;
}
#endif

String KURL::componentString(const url_parse::Component& comp) const
{
    if (!m_isValid || comp.len <= 0) {
        // KURL returns a NULL string if the URL is itself a NULL string, and an
        // empty string for other nonexistant entities.
        if (isNull())
            return String();
        return String("", 0);
    }
    // begin and len are in terms of bytes which do not match
    // if urlString is UTF-16 and input contains non-ASCII characters.
    // However, the only part in urlString that can contain non-ASCII
    // characters is 'ref' at the end of the string. In that case,
    // begin will always match the actual value and len (in terms of
    // byte) will be longer than what's needed by 'mid'. However, mid
    // truncates len to avoid go past the end of a string so that we can
    // get away withtout doing anything here.
    return m_url.string().substring(comp.begin, comp.len);
}

void KURL::init(const KURL& base,
                const String& relative,
                const TextEncoding* query_encoding)
{
    init(base, relative.characters(), relative.length(), query_encoding);
}

// Note: code mostly duplicated below.
void KURL::init(const KURL& base, const char* rel, int rel_len,
                const TextEncoding* query_encoding)
{
    // As a performance optimization, we only use the charset converter if the
    // encoding is not UTF-8. The URL canonicalizer will be more efficient with
    // no charset converter object because it can do UTF-8 internally with no
    // extra copies.
    //
    // We feel free to make the charset converter object every time since it's
    // just a wrapper around a reference.
    WebCoreCharsetConverter charset_converter_object(query_encoding);
    WebCoreCharsetConverter* charset_converter =
        (!query_encoding || *query_encoding == UTF8Encoding()) ? 0 :
        &charset_converter_object;

    url_canon::RawCanonOutputT<char> output;
    const CString& baseStr = base.m_url.utf8String();
    m_isValid = url_util::ResolveRelative(baseStr.data(), baseStr.length(),
                                          base.m_parsed, rel, rel_len,
                                          charset_converter,
                                          &output, &m_parsed);

    // See TODO in URLString in the header. If canonicalization has not changed
    // the string, we can avoid an extra allocation by using assignment.
    //
    // When KURL encounters an error such that the URL is invalid and empty
    // (for example, resolving a relative URL on a non-hierarchical base), it
    // will produce an isNull URL, and calling setUtf8 will produce an empty
    // non-null URL. This is unlikely to affect anything, but we preserve this
    // just in case.
    if (m_isValid || output.length()) {
        // Without ref, the whole url is guaranteed to be ASCII-only.
        if (m_parsed.ref.is_nonempty())
            m_url.setUtf8(output.data(), output.length());
        else
            m_url.setAscii(output.data(), output.length());
    } else {
        // WebKit expects resolved URLs to be empty rather than NULL.
        m_url.setUtf8("", 0);
    }
}

// Note: code mostly duplicated above. See TODOs and comments there.
void KURL::init(const KURL& base, const UChar* rel, int rel_len,
                const TextEncoding* query_encoding)
{
    WebCoreCharsetConverter charset_converter_object(query_encoding);
    WebCoreCharsetConverter* charset_converter =
        (!query_encoding || *query_encoding == UTF8Encoding()) ? 0 :
        &charset_converter_object;

    url_canon::RawCanonOutputT<char> output;
    const CString& baseStr = base.m_url.utf8String();
    m_isValid = url_util::ResolveRelative(baseStr.data(), baseStr.length(),
                                          base.m_parsed, rel, rel_len,
                                          charset_converter,
                                          &output, &m_parsed);

    if (m_isValid || output.length()) {
        if (m_parsed.ref.is_nonempty())
            m_url.setUtf8(output.data(), output.length());
        else
            m_url.setAscii(output.data(), output.length());
    } else {
        m_url.setUtf8("", 0);
    }
}

bool KURL::hasPath() const
{
    // Note that http://www.google.com/" has a path, the path is "/". This can
    // return false only for invalid or nonstandard URLs.
    return m_parsed.path.len >= 0;
}

// We handle "parameters" separated be a semicolon, while the old KURL does
// not, which can lead to different results in some cases.
String KURL::lastPathComponent() const
{
    // When the output ends in a slash, WebKit has different expectations than
    // our library. For "/foo/bar/" the library will return the empty string,
    // but WebKit wants "bar".
    url_parse::Component path = m_parsed.path;
    if (path.len > 0 && m_url.utf8String().data()[path.end() - 1] == '/')
        path.len--;

    url_parse::Component file;
    url_parse::ExtractFileName(m_url.utf8String().data(), path, &file);

    // Bug: https://bugs.webkit.org/show_bug.cgi?id=21015 this function returns
    // a null string when the path is empty, which we duplicate here.
    if (!file.is_nonempty())
        return String();
    return componentString(file);
}

String KURL::protocol() const
{
    return componentString(m_parsed.scheme);
}

String KURL::host() const
{
    // Note: WebKit decode_string()s here.
    return componentString(m_parsed.host);
}

// Returns 0 when there is no port or it is invalid.
//
// We define invalid port numbers to be invalid URLs, and they will be
// rejected by the canonicalizer. WebKit's old one will allow them in
// parsing, and return 0 from this port() function.
unsigned short int KURL::port() const
{
    if (!m_isValid || m_parsed.port.len <= 0)
        return 0;
    int port = url_parse::ParsePort(m_url.utf8String().data(), m_parsed.port);
    if (port == url_parse::PORT_UNSPECIFIED)
        return 0;
    return static_cast<unsigned short>(port);
}

// Returns the empty string if there is no password.
String KURL::pass() const
{
    // Bug: https://bugs.webkit.org/show_bug.cgi?id=21015 this function returns
    // a null string when the password is empty, which we duplicate here.
    if (!m_parsed.password.is_nonempty())
        return String();

    // Note: WebKit decode_string()s here.
    return componentString(m_parsed.password);
}

// Returns the empty string if there is no username.
String KURL::user() const
{
    // Note: WebKit decode_string()s here.
    return componentString(m_parsed.username);
}

String KURL::ref() const
{
    // Empty but present refs ("foo.com/bar#") should result in the empty
    // string, which componentString will produce. Nonexistant refs should be
    // the NULL string.
    if (!m_parsed.ref.is_valid())
        return String();

    // Note: WebKit decode_string()s here.
    return componentString(m_parsed.ref);
}

bool KURL::hasRef() const
{
    // Note: WebKit decode_string()s here.
    // FIXME(brettw) determine if they agree about an empty ref
    return m_parsed.ref.len >= 0;
}

String KURL::query() const
{
    if (m_parsed.query.len >= 0) {
        // KURL's query() includes the question mark, even though the reference
        // doesn't. Move the query component backwards one to account for it
        // (our library doesn't count the question mark).
        url_parse::Component query_comp = m_parsed.query;
        query_comp.begin--;
        query_comp.len++;
        return componentString(query_comp);
    }

    // Bug: https://bugs.webkit.org/show_bug.cgi?id=21015 this function returns
    // an empty string when the query is empty rather than a null (not sure
    // which is right).
    return String("", 0);
}

String KURL::path() const
{
    // Note: WebKit decode_string()s here.
    return componentString(m_parsed.path);
}

void KURL::setProtocol(const String& protocol)
{
    Replacements replacements;
    replacements.SetScheme(CharactersOrEmpty(protocol),
                           url_parse::Component(0, protocol.length()));
    replaceComponents(replacements);
}

void KURL::setHost(const String& host)
{
    Replacements replacements;
    replacements.SetHost(CharactersOrEmpty(host),
                         url_parse::Component(0, host.length()));
    replaceComponents(replacements);
}

// This function is used only in the JSC build.
void KURL::setHostAndPort(const String& s) {
    String newhost = s.left(s.find(":"));
    String newport = s.substring(s.find(":") + 1);

    Replacements replacements;
    // Host can't be removed, so we always set.
    replacements.SetHost(CharactersOrEmpty(newhost),  
                         url_parse::Component(0, newhost.length()));

    if (newport.isEmpty()) {  // Port may be removed, so we support clearing.
        replacements.ClearPort();
    } else {
        replacements.SetPort(CharactersOrEmpty(newport),
                             url_parse::Component(0, newport.length()));
    }
    replaceComponents(replacements);
}

void KURL::setPort(unsigned short i)
{
    Replacements replacements;
    String portStr;
    if (i > 0) {
        portStr = String::number(static_cast<int>(i));
        replacements.SetPort(
            reinterpret_cast<const url_parse::UTF16Char*>(portStr.characters()),
            url_parse::Component(0, portStr.length()));

    } else {
        // Clear any existing port when it is set to 0.
        replacements.ClearPort();
    }
    replaceComponents(replacements);
}

void KURL::setUser(const String& user)
{
    // This function is commonly called to clear the username, which we
    // normally don't have, so we optimize this case.
    if (user.isEmpty() && !m_parsed.username.is_valid())
        return;

    // The canonicalizer will clear any usernames that are empty, so we
    // don't have to explicitly call ClearUsername() here.
    Replacements replacements;
    replacements.SetUsername(CharactersOrEmpty(user),
                             url_parse::Component(0, user.length()));
    replaceComponents(replacements);
}

void KURL::setPass(const String& pass)
{
    // This function is commonly called to clear the password, which we
    // normally don't have, so we optimize this case.
    if (pass.isEmpty() && !m_parsed.password.is_valid())
        return;

    // The canonicalizer will clear any passwords that are empty, so we
    // don't have to explicitly call ClearUsername() here.
    Replacements replacements;
    replacements.SetPassword(CharactersOrEmpty(pass),
                             url_parse::Component(0, pass.length()));
    replaceComponents(replacements);
}

void KURL::setRef(const String& ref)
{
    // This function is commonly called to clear the ref, which we
    // normally don't have, so we optimize this case.
    if (ref.isNull() && !m_parsed.ref.is_valid())
        return;

    Replacements replacements;
    if (ref.isNull()) {
        replacements.ClearRef();
    } else {
        replacements.SetRef(CharactersOrEmpty(ref),
                            url_parse::Component(0, ref.length()));
    }
    replaceComponents(replacements);
}

void KURL::removeRef()
{
    Replacements replacements;
    replacements.ClearRef();
    replaceComponents(replacements);
}

void KURL::setQuery(const String& query)
{
    Replacements replacements;
    if (query.isNull()) {
        // WebKit sets to NULL to clear any query.
        replacements.ClearQuery();
    } else if (query.length() > 0 && query[0] == '?') {
        // WebKit expects the query string to begin with a question mark, but
        // our library doesn't. So we trim off the question mark when setting.
        replacements.SetQuery(CharactersOrEmpty(query),
                              url_parse::Component(1, query.length() - 1));
    } else {
        // When set with the empty string or something that doesn't begin with
        // a question mark, WebKit will add a question mark for you. The only
        // way this isn't compatible is if you call this function with an empty
        // string. Old KURL will leave a '?' with nothing following it in the
        // URL, whereas we'll clear it.
        replacements.SetQuery(CharactersOrEmpty(query),
                              url_parse::Component(0, query.length()));
    }
    replaceComponents(replacements);
}

void KURL::setPath(const String& path)
{
    // Empty paths will be canonicalized to "/", so we don't have to worry
    // about calling ClearPath().
    Replacements replacements;
    replacements.SetPath(CharactersOrEmpty(path),
                         url_parse::Component(0, path.length()));
    replaceComponents(replacements);
}

// On Mac, this just seems to return the same URL, but with "/foo/bar" for
// file: URLs instead of file:///foo/bar. We don't bother with any of this,
// at least for now.
String KURL::prettyURL() const
{
    if (!m_isValid)
        return String();
    return m_url.string();
}

// Copy the KURL version here on Sept 12, 2008 while doing the webkit merge.
// 
// TODO(erg): Somehow share this with KURL? Like we'd theoretically merge
// with decodeURLEscapeSequences below?
#ifdef KURL_DECORATE_GLOBALS
String KURL::mimeTypeFromDataURL(const String& url)
#else
String mimeTypeFromDataURL(const String& url)
#endif
{
    ASSERT(protocolIs(url, "data"));
    int index = url.find(';');
    if (index == -1)
        index = url.find(',');
    if (index != -1) {
        int len = index - 5;
        if (len > 0)
            return url.substring(5, len);
        return "text/plain"; // Data URLs with no MIME type are considered text/plain.
    }
    return "";
}

#ifdef KURL_DECORATE_GLOBALS
String KURL::decodeURLEscapeSequences(const String& str)
#else
String decodeURLEscapeSequences(const String& str)
#endif
{
    return decodeURLEscapeSequences(str, UTF8Encoding());
}

// In WebKit's implementation, this is called by every component getter.
// It will unescape every character, including NULL. This is scary, and may
// cause security holes. We never call this function for components, and
// just return the ASCII versions instead.
//
// However, this static function is called directly in some cases. It appears
// that this only happens for javascript: URLs, so this is essentially the
// JavaScript URL decoder. It assumes UTF-8 encoding.
//
// IE doesn't unescape %00, forcing you to use \x00 in JS strings, so we do
// the same. This also eliminates NULL-related problems should a consumer
// incorrectly call this function for non-JavaScript.
//
// TODO(brettw) these should be merged to the regular KURL implementation.
#ifdef KURL_DECORATE_GLOBALS
String KURL::decodeURLEscapeSequences(const String& str, const TextEncoding& encoding)
#else
String decodeURLEscapeSequences(const String& str, const TextEncoding& encoding)
#endif
{
    // TODO(brettw) We can probably use KURL's version of this function
    // without modification. However, I'm concerned about
    // https://bugs.webkit.org/show_bug.cgi?id=20559 so am keeping this old
    // custom code for now. Using their version will also fix the bug that
    // we ignore the encoding.
    //
    // TODO(brettw) bug 1350291: This does not get called very often. We just
    // convert first to 8-bit UTF-8, then unescape, then back to 16-bit. This
    // kind of sucks, and we don't use the encoding properly, which will make
    // some obscure anchor navigations fail.
    CString cstr = str.utf8();

    const char* input = cstr.data();
    int input_length = cstr.length();
    url_canon::RawCanonOutputT<char> unescaped;
    for (int i = 0; i < input_length; i++) {
        if (input[i] == '%') {
            unsigned char ch;
            if (url_canon::DecodeEscaped(input, &i, input_length, &ch)) {
                if (ch == 0) {
                    // Never unescape NULLs.
                    unescaped.push_back('%');
                    unescaped.push_back('0');
                    unescaped.push_back('0');
                } else {
                    unescaped.push_back(ch);
                }
            } else {
                // Invalid escape sequence, copy the percent literal.
                unescaped.push_back('%');
            }
        } else {
            // Regular non-escaped 8-bit character.
            unescaped.push_back(input[i]);
        }
    }

    // Convert that 8-bit to UTF-16. It's not clear IE does this at all to
    // JavaScript URLs, but Firefox and Safari do.
    url_canon::RawCanonOutputT<url_parse::UTF16Char> utf16;
    for (int i = 0; i < unescaped.length(); i++) {
        unsigned char uch = static_cast<unsigned char>(unescaped.at(i));
        if (uch < 0x80) {
            // Non-UTF-8, just append directly
            utf16.push_back(uch);
        } else {
            // next_ch will point to the last character of the decoded
            // character.
            int next_ch = i;
            unsigned code_point;
            if (url_canon::ReadUTFChar(unescaped.data(), &next_ch,
                                       unescaped.length(), &code_point)) {
                // Valid UTF-8 character, convert to UTF-16.
                url_canon::AppendUTF16Value(code_point, &utf16);
                i = next_ch;
            } else {
                // WebKit strips any sequences that are not valid UTF-8. This
                // sounds scary. Instead, we just keep those invalid code
                // points and promote to UTF-16. We copy all characters from
                // the current position to the end of the identified sqeuqnce.
                while (i < next_ch) {
                    utf16.push_back(static_cast<unsigned char>(unescaped.at(i)));
                    i++;
                }
                utf16.push_back(static_cast<unsigned char>(unescaped.at(i)));
            }
        }
    }

    return String(reinterpret_cast<UChar*>(utf16.data()),
                            utf16.length());
}

void KURL::replaceComponents(const Replacements& replacements)
{
    url_canon::RawCanonOutputT<char> output;
    url_parse::Parsed new_parsed;

    m_isValid = url_util::ReplaceComponents(m_url.utf8String().data(),
        m_url.utf8String().length(), m_parsed, replacements, NULL, &output, &new_parsed);

    m_parsed = new_parsed;
    if (m_parsed.ref.is_nonempty())
        m_url.setUtf8(output.data(), output.length());
    else
        m_url.setAscii(output.data(), output.length());
}

bool KURL::protocolIs(const char* protocol) const
{
    AssertProtocolIsGood(protocol);
    if (m_parsed.scheme.len <= 0)
        return protocol == NULL;
    return LowerCaseEqualsASCII(
        m_url.utf8String().data() + m_parsed.scheme.begin,
        m_url.utf8String().data() + m_parsed.scheme.end(),
        protocol);
}
 
bool KURL::isLocalFile() const
{
    return protocolIs("file");
}

// This is called to escape a URL string. It is only used externally when
// constructing mailto: links to set the query section. Since our query setter
// will automatically do the correct escaping, this function does not have to
// do any work.
//
// There is a possibility that a future called may use this function in other
// ways, and may expect to get a valid URL string. The dangerous thing we want
// to protect against here is accidentally getting NULLs in a string that is
// not supposed to have NULLs. Therefore, we escape NULLs here to prevent this.
#ifdef KURL_DECORATE_GLOBALS
String KURL::encodeWithURLEscapeSequences(const String& notEncodedString)
#else
String encodeWithURLEscapeSequences(const String& notEncodedString)
#endif
{
    CString utf8 = UTF8Encoding().encode(
        reinterpret_cast<const UChar*>(notEncodedString.characters()),
        notEncodedString.length(),
        URLEncodedEntitiesForUnencodables);
    const char* input = utf8.data();
    int input_len = utf8.length();

    Vector<char, 2048> buffer;
    for (int i = 0; i < input_len; i++) {
        if (input[i] == 0)
            buffer.append("%00", 3);
        else
            buffer.append(input[i]);
    }
    return String(buffer.data(), buffer.size());
}

bool KURL::isHierarchical() const
{
    if (!m_parsed.scheme.is_nonempty())
        return false;
    return url_util::IsStandard(
        &m_url.utf8String().data()[m_parsed.scheme.begin],
        m_url.utf8String().length(),
        m_parsed.scheme);
}

#ifndef NDEBUG
void KURL::print() const
{
    printf("%s\n", m_url.utf8String().data());
}
#endif

void KURL::invalidate()
{
    // This is only called from the constructor so resetting the (automatically
    // initialized) string and parsed structure would be a waste of time.
    m_isValid = false;
}

// Equal up to reference fragments, if any.
bool equalIgnoringRef(const KURL& a, const KURL& b)
{
    // Compute the length of each URL without its ref. Note that the reference
    // begin (if it exists) points to the character *after* the '#', so we need
    // to subtract one.
    int a_len = a.m_url.utf8String().length();
    if (a.m_parsed.ref.len >= 0)
        a_len = a.m_parsed.ref.begin - 1;

    int b_len = b.m_url.utf8String().length();
    if (b.m_parsed.ref.len >= 0)
        b_len = b.m_parsed.ref.begin - 1;

    return a_len == b_len &&
        strncmp(a.m_url.utf8String().data(), b.m_url.utf8String().data(), a_len) == 0;
}

unsigned KURL::hostStart() const
{
    return m_parsed.CountCharactersBefore(url_parse::Parsed::HOST, false);
}

unsigned KURL::hostEnd() const
{
    return m_parsed.CountCharactersBefore(url_parse::Parsed::PORT, true);
}

unsigned KURL::pathStart() const
{
    return m_parsed.CountCharactersBefore(url_parse::Parsed::PATH, false);
}

unsigned KURL::pathEnd() const
{
    return m_parsed.CountCharactersBefore(url_parse::Parsed::QUERY, true);
}

unsigned KURL::pathAfterLastSlash() const
{
    // When there's no path, ask for what would be the beginning of it.
    if (!m_parsed.path.is_valid())
        return m_parsed.CountCharactersBefore(url_parse::Parsed::PATH, false);

    url_parse::Component filename;
    url_parse::ExtractFileName(m_url.utf8String().data(), m_parsed.path,
                               &filename);
    return filename.begin;
}

#ifdef KURL_DECORATE_GLOBALS
const KURL& KURL::blankURL()
#else
const KURL& blankURL()
#endif
{
    static KURL staticBlankURL("about:blank");
    return staticBlankURL;
}

#ifdef KURL_DECORATE_GLOBALS
bool KURL::protocolIs(const String& url, const char* protocol)
#else
bool protocolIs(const String& url, const char* protocol)
#endif
{
    // Do the comparison without making a new string object.
    AssertProtocolIsGood(protocol);
    for (int i = 0; ; ++i) {
        if (!protocol[i])
            return url[i] == ':';
        if (toASCIILower(url[i]) != protocol[i])
            return false;
    }
}

#ifndef KURL_DECORATE_GLOBALS
inline bool KURL::protocolIs(const String& string, const char* protocol)
{
    return WebCore::protocolIs(string, protocol);
}
#endif

}  // namespace WebCore

#endif  // USE_GOOGLE_URL_LIBRARY
