// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_

#include "base/file_util.h"
#include "net/base/completion_callback.h"
#include "net/base/file_input_stream.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

// A request job that handles reading file URLs
class URLRequestFileJob : public URLRequestJob {
 public:
  URLRequestFileJob(URLRequest* request);
  virtual ~URLRequestFileJob();

  virtual void Start();
  virtual void Kill();
  virtual bool ReadRawData(char* buf, int buf_size, int *bytes_read);
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);
  virtual bool GetMimeType(std::string* mime_type);

  static URLRequest::ProtocolFactory Factory;

 protected:
  // The OS-specific full path name of the file
  std::wstring file_path_;

 private:
  void DidResolve(bool exists, const file_util::FileInfo& file_info);
  void DidRead(int result);

  net::CompletionCallbackImpl<URLRequestFileJob> io_callback_;
  net::FileInputStream stream_;
  bool is_directory_;

#if defined(OS_WIN)
  class AsyncResolver;
  friend class AsyncResolver;
  scoped_refptr<AsyncResolver> async_resolver_;
#endif

  DISALLOW_COPY_AND_ASSIGN(URLRequestFileJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
