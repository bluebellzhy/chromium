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

#include "chrome/browser/base_history_model.h"

#include "base/gfx/png_decoder.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/profile.h"
#include "chrome/common/jpeg_codec.h"
#include "chrome/common/resource_bundle.h"
#include "SkBitmap.h"

// Size of the favicon and thumbnail caches.
static const int kThumbnailCacheSize = 100;

// Icon to display when one isn't found for the page.
static SkBitmap* kDefaultFavicon = NULL;

// static
const int BaseHistoryModel::kHistoryScopeMonths = 18;

BaseHistoryModel::BaseHistoryModel(Profile* profile)
    : profile_(profile),
      observer_(NULL),
      thumbnails_(kThumbnailCacheSize),
      favicons_(kThumbnailCacheSize),
      is_search_results_(false) {
  if (!kDefaultFavicon) {
    kDefaultFavicon = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_DEFAULT_FAVICON);
  }
}

BaseHistoryModel::~BaseHistoryModel() {
}

void BaseHistoryModel::SetObserver(BaseHistoryModelObserver* observer) {
  observer_ = observer;
}

BaseHistoryModelObserver* BaseHistoryModel::GetObserver() const {
  return observer_;
}

SkBitmap* BaseHistoryModel::GetThumbnail(int index) {
  return GetImage(THUMBNAIL, index);
}

SkBitmap* BaseHistoryModel::GetFavicon(int index) {
  SkBitmap* favicon = GetImage(FAVICON, index);
  return favicon ? favicon : kDefaultFavicon;
}

void BaseHistoryModel::AboutToScheduleRequest() {
  if (observer_ && cancelable_consumer_.PendingRequestCount() == 0)
    observer_->ModelBeginWork();
}

void BaseHistoryModel::RequestCompleted() {
  if (observer_ && cancelable_consumer_.PendingRequestCount() == 1)
    observer_->ModelEndWork();
}

SkBitmap* BaseHistoryModel::GetImage(ImageType image_type, int index) {
#ifndef NDEBUG
  DCHECK(IsValidIndex(index));
#endif
  history::URLID id = GetURLID(index);
  DCHECK(id);

  CacheType* cache = (image_type == THUMBNAIL) ? &thumbnails_ : &favicons_;
  CacheType::iterator iter = cache->Get(id);
  if (iter != cache->end()) {
    // Empty bitmaps indicate one that is pending a load. We still return NULL
    // in this case.
    return iter->second.getSize() ? &iter->second : NULL;
  }

  HistoryService* history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history_service)
    return NULL;

  // The entry isn't cached, ask the history service for it.

  // Add an entry to the cache. This ensures we don't request the same page
  // more than once.
  cache->Put(id, SkBitmap());

  // If this is the first request, we need to notify our delegate we're
  // beginning work.
  AboutToScheduleRequest();

  // Start the request & associate the page ID with this thumbnail request.
  HistoryService::Handle request;
  if (image_type == THUMBNAIL) {
    request = history_service->GetPageThumbnail(GetURL(index),
        &cancelable_consumer_,
        NewCallback(this, &BaseHistoryModel::OnThumbnailDataAvailable));
  } else {
    request = history_service->GetFavIconForURL(GetURL(index),
        &cancelable_consumer_,
        NewCallback(this, &BaseHistoryModel::OnFaviconDataAvailable));
  }
  cancelable_consumer_.SetClientData(history_service, request, id);
  return NULL;
}

void BaseHistoryModel::OnThumbnailDataAvailable(
    HistoryService::Handle request_handle,
    scoped_refptr<RefCountedBytes> data) {
  RequestCompleted();

  if (!data || data->data.empty()) {  // This gets called on failure too.
    return;
  }

  // The client data for this request is the page ID for the thumbnail.
  HistoryService* history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  history::URLID page = cancelable_consumer_.GetClientData(history_service,
                                                            request_handle);
  DCHECK(page > 0) << "Page ID is not found, maybe we forgot to set it";

  scoped_ptr<SkBitmap> bitmap(JPEGCodec::Decode(
      &data->data.front(), data->data.size()));
  if (!bitmap.get())
    return;

  thumbnails_.Put(page, *bitmap);

  if (observer_)
    observer_->ModelChanged(false);
}

void BaseHistoryModel::OnFaviconDataAvailable(
    HistoryService::Handle handle,
    bool know_favicon,
    scoped_refptr<RefCountedBytes> data,
    bool expired,
    GURL icon_url) {
  RequestCompleted();

  SkBitmap fav_icon;
  if (!know_favicon || !data.get() ||
      !PNGDecoder::Decode(&data->data, &fav_icon)) {
    return;
  }
  history::URLID page =
      cancelable_consumer_.GetClientData(
          profile_->GetHistoryService(Profile::EXPLICIT_ACCESS), handle);
  favicons_.Put(page, fav_icon);

  if (observer_)
    observer_->ModelChanged(false);
}
