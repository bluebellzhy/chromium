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

#include "net/disk_cache/disk_cache_test_util.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_handle.h"
#include "net/disk_cache/backend_impl.h"

std::string GenerateKey(bool same_length) {
  char key[200];
  CacheTestFillBuffer(key, sizeof(key), same_length);

  key[199] = '\0';
  return std::string(key);
}

void CacheTestFillBuffer(char* buffer, size_t len, bool no_nulls) {
  static bool called = false;
  if (!called) {
    called = true;
    int seed = static_cast<int>(Time::Now().ToInternalValue());
    srand(seed);
  }

  for (size_t i = 0; i < len; i++) {
    buffer[i] = static_cast<char>(rand());
    if (!buffer[i] && no_nulls)
      buffer[i] = 'g';
  }
  if (len && !buffer[0])
    buffer[0] = 'g';
}

std::wstring GetCachePath() {
  std::wstring path;
  PathService::Get(base::DIR_TEMP, &path);
  file_util::AppendToPath(&path, L"cache_test");
  if (!file_util::PathExists(path))
    file_util::CreateDirectory(path);

  return path;
}

bool CreateCacheTestFile(const wchar_t* name) {
  ScopedHandle file(CreateFile(name, GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                CREATE_ALWAYS, 0, NULL));
  if (!file.IsValid())
    return false;

  SetFilePointer(file, 4 * 1024 * 1024, 0, FILE_BEGIN);
  SetEndOfFile(file);
  return true;
}

bool DeleteCache(const wchar_t* path) {
  std::wstring my_path(path);
  file_util::AppendToPath(&my_path, L"*.*");
  return file_util::Delete(my_path, false);
}

bool CheckCacheIntegrity(const std::wstring& path) {
  scoped_ptr<disk_cache::BackendImpl> cache(new disk_cache::BackendImpl(path));
  if (!cache.get())
    return false;
  if (!cache->Init())
    return false;
  return cache->SelfCheck() >= 0;
}

// -----------------------------------------------------------------------

int g_cache_tests_max_id = 0;
volatile int g_cache_tests_received = 0;
volatile bool g_cache_tests_error = 0;

// On the actual callback, increase the number of tests received and check for
// errors (an unexpected test received)
void CallbackTest::RunWithParams(const Tuple1<int>& params) {
  if (id_ > g_cache_tests_max_id) {
    NOTREACHED();
    g_cache_tests_error = true;
  } else if (reuse_) {
    DCHECK(1 == reuse_);
    if (2 == reuse_)
      g_cache_tests_error = true;
    reuse_++;
  }

  g_cache_tests_received++;
}

// -----------------------------------------------------------------------

// Quits the message loop when all callbacks are called or we've been waiting
// too long for them (2 secs without a callback).
void TimerTask::Run() {
  if (g_cache_tests_received > num_callbacks_) {
    NOTREACHED();
  } else if (g_cache_tests_received == num_callbacks_) {
    completed_ = true;
    MessageLoop::current()->Quit();
  } else {
    // Not finished yet. See if we have to abort.
    if (last_ == g_cache_tests_received)
      num_iterations_++;
    else
      last_ = g_cache_tests_received;
    if (40 == num_iterations_)
      MessageLoop::current()->Quit();
  }
}

// -----------------------------------------------------------------------

MessageLoopHelper::MessageLoopHelper() {
  message_loop_ = MessageLoop::current();
  // Create a recurrent timer of 50 mS.
  timer_ = message_loop_->timer_manager()->StartTimer(50, &timer_task_, true);
}

MessageLoopHelper::~MessageLoopHelper() {
  message_loop_->timer_manager()->StopTimer(timer_);
  delete timer_;
}

bool MessageLoopHelper::WaitUntilCacheIoFinished(int num_callbacks) {
  if (num_callbacks == g_cache_tests_received)
    return true;

  timer_task_.ExpectCallbacks(num_callbacks);
  message_loop_->Run();
  return timer_task_.GetSate();
}

