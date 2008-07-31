// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
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

#include <string>

#include "config.h"
#pragma warning(push, 0)
#include "HTTPHeaderMap.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "String.h"
#pragma warning(pop)

#undef LOG
#include "base/logging.h"
#include "base/string_util.h"
#include "webkit/glue/multipart_response_delegate.h"
#include "webkit/glue/glue_util.h"
#include "net/base/net_util.h"

MultipartResponseDelegate::MultipartResponseDelegate(
    WebCore::ResourceHandleClient* client,
    WebCore::ResourceHandle* job,
    const WebCore::ResourceResponse& response,
    const std::string& boundary)
    : client_(client),
      job_(job),
      original_response_(response),
      boundary_("--"),
      first_received_data_(true),
      processing_headers_(false),
      stop_sending_(false) {
  boundary_.append(boundary);
}

void MultipartResponseDelegate::OnReceivedData(const char* data, int data_len) {
  // stop_sending_ means that we've already received the final boundary token. 
  // The server should stop sending us data at this point, but if it does, we
  // just throw it away.
  if (stop_sending_)
    return;

  // TODO(tc): Figure out what to use for length_received.  Maybe we can just
  // pass the value on from our caller.  See note in
  // resource_handle_win.cc:ResourceHandleInternal::OnReceivedData.
  int length_received = -1;

  data_.append(data, data_len);
  if (first_received_data_) {
    // Some servers don't send a boundary token before the first chunk of
    // data.  We handle this case anyway (Gecko does too).
    first_received_data_ = false;

    // Eat leading \r\n
    int pos = PushOverLine(data_, 0);
    if (pos)
      data_ = data_.substr(pos);
    
    if (data_.length() < boundary_.length() + 2) {
      // We don't have enough data yet to make a boundary token.  Just wait
      // until the next chunk of data arrives.
      first_received_data_ = true;
      return;
    }

    if (data_.substr(0, boundary_.length()) != boundary_) {
      data_ = boundary_ + "\n" + data_;
    }
  }
  DCHECK(!first_received_data_);

  // Headers
  if (processing_headers_) {
    if (ParseHeaders()) {
      // Successfully parsed headers.
      processing_headers_ = false;
    } else {
      // Get more data before trying again.
      return;
    }
  }
  DCHECK(!processing_headers_);

  int token_line_feed = 1;
  size_t boundary_pos;
  while ((boundary_pos = FindBoundary()) != std::string::npos) {
    if (boundary_pos > 0) {
      // Send the last data chunk.
      client_->didReceiveData(job_, data_.substr(0, boundary_pos).data(),
                              static_cast<int>(boundary_pos), length_received);
    }
    size_t boundary_end_pos = boundary_pos + boundary_.length();
    if (boundary_end_pos < data_.length() && '-' == data_[boundary_end_pos]) {
      // This was the last boundary so we can stop processing.
      stop_sending_ = true;
      data_.clear();
      return;
    }

    // We can now throw out data up through the boundary
    int offset = PushOverLine(data_, boundary_end_pos);
    data_ = data_.substr(boundary_end_pos + offset);

    // Ok, back to parsing headers
    if (!ParseHeaders()) {
      processing_headers_ = true;
      break;
    }
  }
}

void MultipartResponseDelegate::OnCompletedRequest() {
  // If we have any pending data and we're not in a header, go ahead and send
  // it to WebCore.
  if (!processing_headers_ && !data_.empty()) {
    // TODO(tc): Figure out what to use for length_received.  Maybe we can just
    // pass the value on from our caller.
    int length_received = -1;
    client_->didReceiveData(job_, data_.data(),
                            static_cast<int>(data_.length()), length_received);
  }
}

int MultipartResponseDelegate::PushOverLine(const std::string& data, size_t pos) {
  int offset = 0;
  if (pos < data.length() && (data[pos] == '\r' || data[pos] == '\n')) {
    ++offset;
    if (pos + 1 < data.length() && data[pos + 1] == '\n')
      ++offset;
  }
  return offset;
}

bool MultipartResponseDelegate::ParseHeaders() {
  int line_feed_increment = 1;

  // Grab the headers being liberal about line endings.
  size_t line_start_pos = 0;
  size_t line_end_pos = data_.find('\n');
  while (line_end_pos != std::string::npos) {
    // Handle CRLF
    if (line_end_pos > line_start_pos && data_[line_end_pos - 1] == '\r') {
      line_feed_increment = 2;
      --line_end_pos;
    } else {
      line_feed_increment = 1;
    }
    if (line_start_pos == line_end_pos) {
      // A blank line, end of headers
      line_end_pos += line_feed_increment;
      break;
    }
    // Find the next header line.
    line_start_pos = line_end_pos + line_feed_increment;
    line_end_pos = data_.find('\n', line_start_pos);
  }
  // Truncated in the middle of a header, stop parsing.
  if (line_end_pos == std::string::npos)
    return false;

  // Eat headers
  std::string headers("\n");
  headers.append(data_.substr(0, line_end_pos));
  data_ = data_.substr(line_end_pos);

  // Create a ResourceResponse based on the original set of headers + the
  // replacement headers.  We only replace the same few headers that gecko
  // does.  See netwerk/streamconv/converters/nsMultiMixedConv.cpp.
  std::string mime_type = net::GetSpecificHeader(headers, "content-type");
  std::string charset = net::GetHeaderParamValue(mime_type, "charset");
  WebCore::ResourceResponse response(original_response_.url(),
      webkit_glue::StdStringToString(mime_type.c_str()),
      -1,
      charset.c_str(),
      WebCore::String());
  const WebCore::HTTPHeaderMap& orig_headers =
      original_response_.httpHeaderFields();
  for (WebCore::HTTPHeaderMap::const_iterator it = orig_headers.begin();
       it != orig_headers.end(); ++it) {
    if (!(equalIgnoringCase("content-type", it->first) ||
          equalIgnoringCase("content-length", it->first) ||
          equalIgnoringCase("content-disposition", it->first) ||
          equalIgnoringCase("content-range", it->first) ||
          equalIgnoringCase("range", it->first) ||
          equalIgnoringCase("set-cookie", it->first))) {
      response.setHTTPHeaderField(it->first, it->second);
    }
  }
  static const char* replace_headers[] = {
    "Content-Type",
    "Content-Length",
    "Content-Disposition",
    "Content-Range",
    "Range",
    "Set-Cookie"
  };
  for (int i = 0; i < arraysize(replace_headers); ++i) {
    std::string name(replace_headers[i]);
    std::string value = net::GetSpecificHeader(headers, name);
    if (!value.empty()) {
      response.setHTTPHeaderField(webkit_glue::StdStringToString(name.c_str()),
          webkit_glue::StdStringToString(value.c_str()));
    }
  }
  // Send the response!
  client_->didReceiveResponse(job_, response);
  
  return true;
}

// Boundaries are supposed to be preceeded with --, but it looks like gecko
// doesn't require the dashes to exist.  See nsMultiMixedConv::FindToken.
size_t MultipartResponseDelegate::FindBoundary() {
  size_t boundary_pos = data_.find(boundary_);
  if (boundary_pos != std::string::npos) {
    // Back up over -- for backwards compat
    // TODO(tc): Don't we only want to do this once?  Gecko code doesn't seem
    // to care.
    if (boundary_pos >= 2) {
      if ('-' == data_[boundary_pos - 1] && '-' == data_[boundary_pos - 2]) {
        boundary_pos -= 2;
        boundary_ = "--" + boundary_;
      }
    }
  }
  return boundary_pos;
}
