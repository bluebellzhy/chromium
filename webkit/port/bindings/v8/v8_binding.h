// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BINDING_H__
#define V8_BINDING_H__

#include "base/compiler_specific.h"
#include "build/build_config.h"

#include <v8.h>
#include "KURL.h"
#include "PlatformString.h"
#include "MathExtras.h"
#include "StringBuffer.h"

// Suppress warnings in CString of converting size_t to unsigned int.
// TODO(fqian): fix CString.h.
MSVC_PUSH_WARNING_LEVEL(0);
#include "CString.h"
MSVC_POP_WARNING();

#if defined(OS_LINUX)
// Use the platform.h for linux.
#include "unicode/plinux.h"
#elif defined(OS_WIN) || defined(OS_MACOSX)
// WebKit ships a hacked up version of one of the ICU header files, with all
// options set for OSX.
#include "platform.h"
#endif

namespace WebCore {

// The string returned by this function is still owned by the argument
// and will be deallocated when the argument is deallocated.
inline const uint16_t* FromWebCoreString(const String& str) {
  return reinterpret_cast<const uint16_t*>(str.characters());
}

// WebCoreStringResource is a helper class for v8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class WebCoreStringResource: public v8::String::ExternalStringResource {
 public:
  explicit WebCoreStringResource(const String& str)
  : external_string_(str) {
  }

  ~WebCoreStringResource() {}

  const uint16_t* data() const { return FromWebCoreString(external_string_); }

  size_t length() const { return external_string_.length(); }

  String webcore_string() { return external_string_; }

 private:
  // A shallow copy of the string.
  // Keeps the string buffer alive until the V8 engine garbage collects it.
  String external_string_;
};

// TODO: converting between WebCore::String and V8 string is expensive.
// Optimize it !!!
inline String ToWebCoreString(v8::Handle<v8::Value> obj) {
  v8::TryCatch block;
  v8::Local<v8::String> v8_str = obj->ToString();
  if (v8_str.IsEmpty())
    return "";

  if (v8_str->IsExternal()) {
    WebCoreStringResource* str_resource = static_cast<WebCoreStringResource*>(
        v8_str->GetExternalStringResource());
    return str_resource->webcore_string();
  }

  int length = v8_str->Length();
  StringBuffer buf(length);
  v8_str->Write(reinterpret_cast<uint16_t*>(buf.characters()), 0, length);
  return String::adopt(buf);
}

inline String valueToStringWithNullCheck(v8::Handle<v8::Value> value) {
  if (value->IsNull()) return String();
  return ToWebCoreString(value);
}

inline String valueToStringWithNullOrUndefinedCheck(
    v8::Handle<v8::Value> value) {
  if (value->IsNull() || value->IsUndefined()) return String();
  return ToWebCoreString(value);
}

// Convert a value to a 32-bit integer.  The conversion fails if the
// value cannot be converted to an integer or converts to nan or to an
// infinity.
inline int ToInt32(v8::Handle<v8::Value> value, bool& ok) {
  ok = true;

  // Fast case.  The value is already a 32-bit integer.
  if (value->IsInt32()) {
    return value->Int32Value();
  }

  // Can the value be converted to a number?
  v8::Local<v8::Number> number_object = value->ToNumber();
  if (number_object.IsEmpty()) {
    ok = false;
    return 0;
  }

  // Does the value convert to nan or to an infinity?
  double number_value = number_object->Value();
  if (isnan(number_value) || isinf(number_value)) {
    ok = false;
    return 0;
  }

  // Can the value be converted to a 32-bit integer?
  v8::Local<v8::Int32> int_value = value->ToInt32();
  if (int_value.IsEmpty()) {
    ok = false;
    return 0;
  }

  // Return the result of the int32 conversion.
  return int_value->Value();
}

// Convert a value to a 32-bit integer assuming the conversion cannot fail.
inline int ToInt32(v8::Handle<v8::Value> value) {
  bool ok;
  return ToInt32(value, ok);
}

inline String ToString(const String& string) {
  return string;
}

inline String ToString(const KURL& url) {
  return url.string();
}

// If a WebCore string length is greater than the threshold,
// v8String creates an external string to avoid allocating
// the string in the large object space (which has a high memory overhead).
static const unsigned int kV8ExternalStringThreshold = 2048;

// Convert a string to a V8 string.
inline v8::Handle<v8::String> v8String(const String& str) {
  if (str.length() <= kV8ExternalStringThreshold) {
    return v8::String::New(FromWebCoreString(str), str.length());
  } else {
    return v8::String::NewExternal(new WebCoreStringResource(str));
  }
}

inline v8::Handle<v8::String> v8UndetectableString(const String& str) {
  return v8::String::NewUndetectable(FromWebCoreString(str), str.length());
}

// Return a V8 external string that shares the underlying buffer with the given
// WebCore string. The reference counting mechanism is used to keep the
// underlying buffer alive while the string is still live in the V8 engine.
inline v8::Local<v8::String> v8ExternalString(const String& str) {
  return v8::String::NewExternal(new WebCoreStringResource(str));
}

inline v8::Handle<v8::Value> v8StringOrNull(const String& str) {
  return str.isNull()
    ? v8::Handle<v8::Value>(v8::Null())
    : v8::Handle<v8::Value>(v8String(str));
}

inline v8::Handle<v8::Value> v8StringOrUndefined(const String& str) {
  return str.isNull()
    ? v8::Handle<v8::Value>(v8::Undefined())
    : v8::Handle<v8::Value>(v8String(str));
}

inline v8::Handle<v8::Value> v8StringOrFalse(const String& str) {
  return str.isNull()
    ? v8::Handle<v8::Value>(v8::False())
    : v8::Handle<v8::Value>(v8String(str));
}

}  // namespace WebCore

#endif  // V8_BINDING_H__

