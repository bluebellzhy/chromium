// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/sdch_dictionary_fetcher.h"
#include "chrome/browser/profile.h"

void SdchDictionaryFetcher::Schedule(const GURL& dictionary_url) {
  fetch_queue_.push(dictionary_url);
  ScheduleDelayedRun();
}

// TODO(jar): If QOS low priority is supported, switch to using that instead of
// just waiting to do the fetch.
void SdchDictionaryFetcher::ScheduleDelayedRun() {
  if (fetch_queue_.empty() || current_fetch_.get() || task_is_pending_)
    return;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&SdchDictionaryFetcher::StartFetching),
      kMsDelayFromRequestTillDownload);
  task_is_pending_ = true;
}

void SdchDictionaryFetcher::StartFetching() {
  DCHECK(task_is_pending_);
  task_is_pending_ = false;

  current_fetch_.reset(new URLFetcher(fetch_queue_.front(), URLFetcher::GET,
                                      this));
  fetch_queue_.pop();
  current_fetch_->set_request_context(Profile::GetDefaultRequestContext());
  current_fetch_->Start();
}

void SdchDictionaryFetcher::OnURLFetchComplete(const URLFetcher* source,
                                               const GURL& url,
                                               const URLRequestStatus& status,
                                               int response_code,
                                               const ResponseCookies& cookies,
                                               const std::string& data) {
  if ((200 == response_code) && (status.status() == URLRequestStatus::SUCCESS))
    SdchManager::Global()->AddSdchDictionary(data, url);
  current_fetch_.reset(NULL);
  ScheduleDelayedRun();
}
