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

#include "net/base/host_resolver.h"

#include <ws2tcpip.h>
#include <wspiapi.h>  // Needed for Win2k compat.

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/worker_pool.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/winsock_init.h"

namespace net {

//-----------------------------------------------------------------------------

static int ResolveAddrInfo(const std::string& host, const std::string& port,
                           struct addrinfo** results) {
  struct addrinfo hints = {0};
  hints.ai_family = PF_UNSPEC;
  hints.ai_flags = AI_ADDRCONFIG;

  // Restrict result set to only this socket type to avoid duplicates.
  hints.ai_socktype = SOCK_STREAM;

  int err = getaddrinfo(host.c_str(), port.c_str(), &hints, results);
  return err ? ERR_NAME_NOT_RESOLVED : OK;
}

//-----------------------------------------------------------------------------

class HostResolver::Request :
    public base::RefCountedThreadSafe<HostResolver::Request> {
 public:
  Request(HostResolver* resolver,
          const std::string& host,
          const std::string& port,
          AddressList* addresses,
          CompletionCallback* callback)
      : host_(host),
        port_(port),
        resolver_(resolver),
        addresses_(addresses),
        callback_(callback),
        origin_loop_(MessageLoop::current()),
        error_(OK),
        results_(NULL) {
  }
  
  ~Request() {
    if (results_)
      freeaddrinfo(results_);
  }

  void DoLookup() {
    // Running on the worker thread
    error_ = ResolveAddrInfo(host_, port_, &results_);

    Task* reply = NewRunnableMethod(this, &Request::DoCallback);

    // The origin loop could go away while we are trying to post to it, so we
    // need to call its PostTask method inside a lock.  See ~HostResolver.
    {
      AutoLock locked(origin_loop_lock_);
      if (origin_loop_) {
        origin_loop_->PostTask(FROM_HERE, reply);
        reply = NULL;
      }
    }
    
    // Does nothing if it got posted.
    delete reply;
  }

  void DoCallback() {
    // Running on the origin thread.
    DCHECK(error_ || results_);

    // We may have been cancelled!
    if (!resolver_)
      return;

    if (!error_) {
      addresses_->Adopt(results_);
      results_ = NULL;
    }

    // Drop the resolver's reference to us.  Do this before running the
    // callback since the callback might result in the resolver being
    // destroyed.
    resolver_->request_ = NULL;

    callback_->Run(error_);
  }

  void Cancel() {
    resolver_ = NULL;

    AutoLock locked(origin_loop_lock_);
    origin_loop_ = NULL;
  }
  
 private:
  // Set on the origin thread, read on the worker thread.
  std::string host_;
  std::string port_;

  // Only used on the origin thread (where Resolve was called).
  HostResolver* resolver_;
  AddressList* addresses_;
  CompletionCallback* callback_;

  // Used to post ourselves onto the origin thread.
  Lock origin_loop_lock_;
  MessageLoop* origin_loop_;

  // Assigned on the worker thread, read on the origin thread.
  int error_;
  struct addrinfo* results_;
};

//-----------------------------------------------------------------------------

HostResolver::HostResolver() {
  EnsureWinsockInit();
}

HostResolver::~HostResolver() {
  if (request_)
    request_->Cancel();
}

int HostResolver::Resolve(const std::string& hostname, int port,
                          AddressList* addresses,
                          CompletionCallback* callback) {
  DCHECK(!request_) << "resolver already in use";

  const std::string& port_str = IntToString(port);

  // Do a synchronous resolution.
  if (!callback) {
    struct addrinfo* results;
    int rv = ResolveAddrInfo(hostname, port_str, &results);
    if (rv == OK)
      addresses->Adopt(results);
    return rv;
  }

  request_ = new Request(this, hostname, port_str, addresses, callback);

  // Dispatch to worker thread...
  if (!WorkerPool::PostTask(FROM_HERE,
          NewRunnableMethod(request_.get(), &Request::DoLookup), true)) {
    NOTREACHED();
    request_ = NULL;
    return ERR_FAILED;
  }

  return ERR_IO_PENDING;
}

}  // namespace net
