// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache.h"

#include "base/hash_tables.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/load_flags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

//-----------------------------------------------------------------------------
// mock disk cache (a very basic memory cache implementation)

class MockDiskEntry : public disk_cache::Entry,
                      public base::RefCounted<MockDiskEntry> {
 public:
  MockDiskEntry() : test_mode_(0), doomed_(false) {
  }

  MockDiskEntry(const std::string& key) : key_(key), doomed_(false) {
    const MockTransaction* t = FindMockTransaction(GURL(key));
    DCHECK(t);
    test_mode_ = t->test_mode;
  }

  ~MockDiskEntry() {
  }

  bool is_doomed() const { return doomed_; }

  virtual void Doom() {
    doomed_ = true;
  }

  virtual void Close() {
    Release();
  }

  virtual std::string GetKey() const {
    return key_;
  }

  virtual Time GetLastUsed() const {
    return Time::FromInternalValue(0);
  }

  virtual Time GetLastModified() const {
    return Time::FromInternalValue(0);
  }

  virtual int32 GetDataSize(int index) const {
    DCHECK(index >= 0 && index < 2);
    return static_cast<int32>(data_[index].size());
  }

  virtual int ReadData(int index, int offset, char* buf, int buf_len,
                       net::CompletionCallback* callback) {
    DCHECK(index >= 0 && index < 2);

    if (offset < 0 || offset > static_cast<int>(data_[index].size()))
      return net::ERR_FAILED;
    if (static_cast<size_t>(offset) == data_[index].size())
      return 0;

    int num = std::min(buf_len, static_cast<int>(data_[index].size()) - offset);
    memcpy(buf, &data_[index][offset], num);

    if (!callback || (test_mode_ & TEST_MODE_SYNC_CACHE_READ))
      return num;

    CallbackLater(callback, num);
    return net::ERR_IO_PENDING;
  }

  virtual int WriteData(int index, int offset, const char* buf, int buf_len,
                        net::CompletionCallback* callback, bool truncate) {
    DCHECK(index >= 0 && index < 2);
    DCHECK(truncate);

    if (offset < 0 || offset > static_cast<int>(data_[index].size()))
      return net::ERR_FAILED;

    data_[index].resize(offset + buf_len);
    if (buf_len)
      memcpy(&data_[index][offset], buf, buf_len);
    return buf_len;
  }

 private:
  // Unlike the callbacks for MockHttpTransaction, we want this one to run even
  // if the consumer called Close on the MockDiskEntry.  We achieve that by
  // leveraging the fact that this class is reference counted.
  void CallbackLater(net::CompletionCallback* callback, int result) {
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
        &MockDiskEntry::RunCallback, callback, result));
  }
  void RunCallback(net::CompletionCallback* callback, int result) {
    callback->Run(result);
  }

  std::string key_;
  std::vector<char> data_[2];
  int test_mode_;
  bool doomed_;
};

class MockDiskCache : public disk_cache::Backend {
 public:
  MockDiskCache() : open_count_(0), create_count_(0), fail_requests_(0) {
  }

  ~MockDiskCache() {
    EntryMap::iterator it = entries_.begin();
    for (; it != entries_.end(); ++it)
      it->second->Release();
  }

  virtual int32 GetEntryCount() const {
    return static_cast<int32>(entries_.size());
  }

  virtual bool OpenEntry(const std::string& key, disk_cache::Entry** entry) {
    if (fail_requests_)
      return false;

    EntryMap::iterator it = entries_.find(key);
    if (it == entries_.end())
      return false;

    if (it->second->is_doomed()) {
      it->second->Release();
      entries_.erase(it);
      return false;
    }

    open_count_++;

    it->second->AddRef();
    *entry = it->second;

    return true;
  }

  virtual bool CreateEntry(const std::string& key, disk_cache::Entry** entry) {
    if (fail_requests_)
      return false;

    EntryMap::iterator it = entries_.find(key);
    DCHECK(it == entries_.end());

    create_count_++;

    MockDiskEntry* new_entry = new MockDiskEntry(key);

    new_entry->AddRef();
    entries_[key] = new_entry;

    new_entry->AddRef();
    *entry = new_entry;

    return true;
  }

  virtual bool DoomEntry(const std::string& key) {
    EntryMap::iterator it = entries_.find(key);
    if (it != entries_.end()) {
      it->second->Release();
      entries_.erase(it);
    }
    return true;
  }

  virtual bool DoomAllEntries() {
    return false;
  }

  virtual bool DoomEntriesBetween(const Time initial_time,
                                  const Time end_time) {
    return true;
  }

  virtual bool DoomEntriesSince(const Time initial_time) {
    return true;
  }

  virtual bool OpenNextEntry(void** iter, disk_cache::Entry** next_entry) {
    return false;
  }

  virtual void EndEnumeration(void** iter) {}

  virtual void GetStats(
      std::vector<std::pair<std::string, std::string> >* stats) {
  }

  // returns number of times a cache entry was successfully opened
  int open_count() const { return open_count_; }

  // returns number of times a cache entry was successfully created
  int create_count() const { return create_count_; }

  // Fail any subsequent CreateEntry and OpenEntry.
  void set_fail_requests() { fail_requests_ = true; }

 private:
  typedef base::hash_map<std::string, MockDiskEntry*> EntryMap;
  EntryMap entries_;
  int open_count_;
  int create_count_;
  bool fail_requests_;
};

class MockHttpCache {
 public:
  MockHttpCache() : http_cache_(new MockNetworkLayer(), new MockDiskCache()) {
  }

  net::HttpCache* http_cache() { return &http_cache_; }

  MockNetworkLayer* network_layer() {
    return static_cast<MockNetworkLayer*>(http_cache_.network_layer());
  }
  MockDiskCache* disk_cache() {
    return static_cast<MockDiskCache*>(http_cache_.disk_cache());
  }

 private:
  net::HttpCache http_cache_;
};


//-----------------------------------------------------------------------------
// helpers

void ReadAndVerifyTransaction(net::HttpTransaction* trans,
                              const MockTransaction& trans_info) {
  std::string content;
  int rv = ReadTransaction(trans, &content);

  EXPECT_EQ(net::OK, rv);
  EXPECT_EQ(strlen(trans_info.data), content.size());
  EXPECT_EQ(0, memcmp(trans_info.data, content.data(), content.size()));
}

void RunTransactionTest(net::HttpCache* cache,
                        const MockTransaction& trans_info) {
  MockHttpRequest request(trans_info);
  TestCompletionCallback callback;

  // write to the cache

  scoped_ptr<net::HttpTransaction> trans(cache->CreateTransaction());
  ASSERT_TRUE(trans.get());

  int rv = trans->Start(&request, &callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response);

  ReadAndVerifyTransaction(trans.get(), trans_info);
}

}  // namespace


//-----------------------------------------------------------------------------
// tests


TEST(HttpCache, CreateThenDestroy) {
  MockHttpCache cache;

  scoped_ptr<net::HttpTransaction> trans(
      cache.http_cache()->CreateTransaction());
  ASSERT_TRUE(trans.get());
}

TEST(HttpCache, SimpleGET) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGETNoDiskCache) {
  MockHttpCache cache;

  cache.disk_cache()->set_fail_requests();

  // Read from the network, and don't use the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadOnlyFromCache_Hit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to read from the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadOnlyFromCache_Miss) {
  MockHttpCache cache;

  // force this transaction to read from the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans(
      cache.http_cache()->CreateTransaction());
  ASSERT_TRUE(trans.get());

  int rv = trans->Start(&request, &callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);

  trans.reset();

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadPreferringCache_Hit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to read from the cache if valid
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_PREFERRING_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadPreferringCache_Miss) {
  MockHttpCache cache;

  // force this transaction to read from the cache if valid
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_PREFERRING_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadBypassCache) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_BYPASS_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadBypassCache_Implicit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "pragma: no-cache";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadBypassCache_Implicit2) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "cache-control: no-cache";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadValidateCache) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadValidateCache_Implicit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "cache-control: max-age=0";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

struct Context {
  int result;
  TestCompletionCallback callback;
  scoped_ptr<net::HttpTransaction> trans;

  Context(net::HttpTransaction* t) : result(net::ERR_IO_PENDING), trans(t) {
  }
};

TEST(HttpCache, SimpleGET_ManyReaders) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(
        new Context(cache.http_cache()->CreateTransaction()));

    Context* c = context_list[i];
    int rv = c->trans->Start(&request, &c->callback);
    if (rv != net::ERR_IO_PENDING)
      c->result = rv;
  }

  // the first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // we should not have had to re-open the disk entry

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    delete c;
  }
}

TEST(HttpCache, SimpleGET_ManyWriters_CancelFirst) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(
        new Context(cache.http_cache()->CreateTransaction()));

    Context* c = context_list[i];
    int rv = c->trans->Start(&request, &c->callback);
    if (rv != net::ERR_IO_PENDING)
      c->result = rv;
  }

  // the first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    // destroy only the first transaction
    if (i == 0) {
      delete c;
      context_list[i] = NULL;
    }
  }

  // complete the rest of the transactions
  for (int i = 1; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // we should have had to re-open the disk entry

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  for (int i = 1; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    delete c;
  }
}

TEST(HttpCache, SimpleGET_AbandonedCacheRead) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  MockHttpRequest request(kSimpleGET_Transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans(
      cache.http_cache()->CreateTransaction());
  int rv = trans->Start(&request, &callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::OK, rv);

  char buf[256];
  rv = trans->Read(buf, sizeof(buf), &callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // Test that destroying the transaction while it is reading from the cache
  // works properly.
  trans.reset();

  // Make sure we pump any pending events, which should include a call to
  // HttpCache::Transaction::OnCacheReadCompleted.
  MessageLoop::current()->RunAllPending();
}

TEST(HttpCache, TypicalGET_ConditionalRequest) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kTypicalGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // get the same URL again, but this time we expect it to result
  // in a conditional request.
  RunTransactionTest(cache.http_cache(), kTypicalGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

static void ETagGet_ConditionalRequest_Handler(
    const net::HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  EXPECT_TRUE(request->extra_headers.find("If-None-Match") != std::string::npos);
  response_status->assign("HTTP/1.1 304 Not Modified");
  response_headers->assign(kETagGET_Transaction.response_headers);
  response_data->clear();
}

TEST(HttpCache, ETagGET_ConditionalRequest_304) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // write to the cache
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // get the same URL again, but this time we expect it to result
  // in a conditional request.
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.handler = ETagGet_ConditionalRequest_Handler;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimplePOST_SkipsCache) {
  MockHttpCache cache;

  // Test that we skip the cache for POST requests.  Eventually, we will want
  // to cache these, but we'll still have cases where skipping the cache makes
  // sense, so we want to make sure that it works properly.

  RunTransactionTest(cache.http_cache(), kSimplePOST_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimplePOST_LoadOnlyFromCache_Miss) {
  MockHttpCache cache;

  // Test that we skip the cache for POST requests.  Eventually, we will want
  // to cache these, but we'll still have cases where skipping the cache makes
  // sense, so we want to make sure that it works properly.

  MockTransaction transaction(kSimplePOST_Transaction);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans(
      cache.http_cache()->CreateTransaction());
  ASSERT_TRUE(trans.get());

  int rv = trans->Start(&request, &callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);

  trans.reset();

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, RangeGET_SkipsCache) {
  MockHttpCache cache;

  // Test that we skip the cache for POST requests.  Eventually, we will want
  // to cache these, but we'll still have cases where skipping the cache makes
  // sense, so we want to make sure that it works properly.

  RunTransactionTest(cache.http_cache(), kRangeGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "If-None-Match: foo";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  transaction.request_headers = "If-Modified-Since: Wed, 28 Nov 2007 00:45:20 GMT";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SyncRead) {
  MockHttpCache cache;

  // This test ensures that a read that completes synchronously does not cause
  // any problems.

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.test_mode |= (TEST_MODE_SYNC_CACHE_START |
                            TEST_MODE_SYNC_CACHE_READ);

  MockHttpRequest r1(transaction),
                  r2(transaction),
                  r3(transaction);

  TestTransactionConsumer c1(cache.http_cache()),
                          c2(cache.http_cache()),
                          c3(cache.http_cache());

  c1.Start(&r1);

  r2.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  c2.Start(&r2);

  r3.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  c3.Start(&r3);

  MessageLoop::current()->Run();

  EXPECT_TRUE(c1.is_done());
  EXPECT_TRUE(c2.is_done());
  EXPECT_TRUE(c3.is_done());

  EXPECT_EQ(net::OK, c1.error());
  EXPECT_EQ(net::OK, c2.error());
  EXPECT_EQ(net::OK, c3.error());
}

TEST(HttpCache, ValidationResultsIn200) {
  MockHttpCache cache;

  // This test ensures that a conditional request, which results in a 200
  // instead of a 304, properly truncates the existing response data.

  // write to the cache
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kETagGET_Transaction);
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;
  RunTransactionTest(cache.http_cache(), transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);
}

TEST(HttpCache, CachedRedirect) {
  MockHttpCache cache;

  ScopedMockTransaction kTestTransaction(kSimpleGET_Transaction);
  kTestTransaction.status = "HTTP/1.1 301 Moved Permanently";
  kTestTransaction.response_headers = "Location: http://www.bar.com/\n";

  MockHttpRequest request(kTestTransaction);
  TestCompletionCallback callback;

  // write to the cache
  {
    scoped_ptr<net::HttpTransaction> trans(
        cache.http_cache()->CreateTransaction());
    ASSERT_TRUE(trans.get());

    int rv = trans->Start(&request, &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    ASSERT_EQ(net::OK, rv);

    const net::HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(NULL, "Location", &location);
    EXPECT_EQ(location, "http://www.bar.com/");

    // Destroy transaction when going out of scope. We have not actually
    // read the response body -- want to test that it is still getting cached.
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // read from the cache
  {
    scoped_ptr<net::HttpTransaction> trans(
        cache.http_cache()->CreateTransaction());
    ASSERT_TRUE(trans.get());

    int rv = trans->Start(&request, &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    ASSERT_EQ(net::OK, rv);

    const net::HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(NULL, "Location", &location);
    EXPECT_EQ(location, "http://www.bar.com/");

    // Destroy transaction when going out of scope. We have not actually
    // read the response body -- want to test that it is still getting cached.
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, CacheControlNoStore) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "cache-control: no-store\n";

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  bool exists = cache.disk_cache()->OpenEntry(transaction.url, &entry);
  EXPECT_FALSE(exists);
}

TEST(HttpCache, CacheControlNoStore2) {
  // this test is similar to the above test, except that the initial response
  // is cachable, but when it is validated, no-store is received causing the
  // cached document to be deleted.
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.response_headers = "cache-control: no-store\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  bool exists = cache.disk_cache()->OpenEntry(transaction.url, &entry);
  EXPECT_FALSE(exists);
}

TEST(HttpCache, CacheControlNoStore3) {
  // this test is similar to the above test, except that the response is a 304
  // instead of a 200.  this should never happen in practice, but it seems like
  // a good thing to verify that we still destroy the cache entry.
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.response_headers = "cache-control: no-store\n";
  transaction.status = "HTTP/1.1 304 Not Modified";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  bool exists = cache.disk_cache()->OpenEntry(transaction.url, &entry);
  EXPECT_FALSE(exists);
}

// Ensure that we don't cache requests served over bad HTTPS.
TEST(HttpCache, SimpleGET_SSLError) {
  MockHttpCache cache;

  MockTransaction transaction = kSimpleGET_Transaction;
  transaction.cert_status = net::CERT_STATUS_REVOKED;
  ScopedMockTransaction scoped_transaction(transaction);

  // write to the cache
  RunTransactionTest(cache.http_cache(), transaction);

  // Test that it was not cached.
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans(
      cache.http_cache()->CreateTransaction());
  ASSERT_TRUE(trans.get());

  int rv = trans->Start(&request, &callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);
}
