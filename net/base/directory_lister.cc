// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <process.h>

#include "net/base/directory_lister.h"

#include "base/message_loop.h"

namespace net {

static const int kFilesPerEvent = 8;

class DirectoryDataEvent : public Task {
 public:
  explicit DirectoryDataEvent(DirectoryLister* d)
      : lister(d), count(0), error(0) {
  }

  void Run() {
    if (count) {
      lister->OnReceivedData(data, count);
    } else {
      lister->OnDone(error);
    }
  }

  scoped_refptr<DirectoryLister> lister;
  WIN32_FIND_DATA data[kFilesPerEvent];
  int count;
  DWORD error;
};

/*static*/
unsigned __stdcall DirectoryLister::ThreadFunc(void* param) {
  DirectoryLister* self = reinterpret_cast<DirectoryLister*>(param);

  std::wstring pattern = self->directory();
  if (pattern[pattern.size()-1] != '\\') {
    pattern.append(L"\\*");
  } else {
    pattern.append(L"*");
  }

  DirectoryDataEvent* e = new DirectoryDataEvent(self);

  HANDLE handle = FindFirstFile(pattern.c_str(), &e->data[e->count]);
  if (handle == INVALID_HANDLE_VALUE) {
    e->error = GetLastError();
    self->message_loop_->PostTask(FROM_HERE, e);
    e = NULL;
  } else {
    do {
      if (++e->count == kFilesPerEvent) {
        self->message_loop_->PostTask(FROM_HERE, e);
        e = new DirectoryDataEvent(self);
      }
    } while (!self->was_canceled() && FindNextFile(handle, &e->data[e->count]));

    FindClose(handle);

    if (e->count > 0) {
      self->message_loop_->PostTask(FROM_HERE, e);
      e = NULL;
    }

    // Notify done
    e = new DirectoryDataEvent(self);
    self->message_loop_->PostTask(FROM_HERE, e);
  }

  self->Release();
  return 0;
}

DirectoryLister::DirectoryLister(const std::wstring& dir, Delegate* delegate)
    : dir_(dir),
      message_loop_(NULL),
      delegate_(delegate),
      thread_(NULL),
      canceled_(false) {
  DCHECK(!dir.empty());
}

DirectoryLister::~DirectoryLister() {
  if (thread_)
    CloseHandle(thread_);
}

bool DirectoryLister::Start() {
  // spawn a thread to enumerate the specified directory

  // pass events back to the current thread
  message_loop_ = MessageLoop::current();
  DCHECK(message_loop_) << "calling thread must have a message loop";

  AddRef();  // the thread will release us when it is done

  unsigned thread_id;
  thread_ = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL, 0, DirectoryLister::ThreadFunc, this, 0,
                     &thread_id));

  if (!thread_) {
    Release();
    return false;
  }

  return true;
}

void DirectoryLister::Cancel() {
  canceled_ = true;

  if (thread_) {
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
    thread_ = NULL;
  }
}

void DirectoryLister::OnReceivedData(const WIN32_FIND_DATA* data, int count) {
  // Since the delegate can clear itself during the OnListFile callback, we
  // need to null check it during each iteration of the loop.  Similarly, it is
  // necessary to check the canceled_ flag to avoid sending data to a delegate
  // who doesn't want anymore.
  for (int i = 0; !canceled_ && delegate_ && i < count; ++i)
    delegate_->OnListFile(data[i]);
}

void DirectoryLister::OnDone(int error) {
  // If canceled, we need to report some kind of error, but don't overwrite the
  // error condition if it is already set.
  if (!error && canceled_)
    error = ERROR_OPERATION_ABORTED;

  if (delegate_)
    delegate_->OnListDone(error);
}

}  // namespace net

