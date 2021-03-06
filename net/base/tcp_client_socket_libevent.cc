// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/tcp_client_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>

#include "base/message_loop.h"
#include "net/base/net_errors.h"
#include "third_party/libevent/event.h"


namespace net {

const int kInvalidSocket = -1;

// Return 0 on success, -1 on failure.
// Too small a function to bother putting in a library?
static int SetNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (-1 == flags)
      return flags;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Convert values from <errno.h> to values from "net/base/net_errors.h"
static int MapPosixError(int err) {
  // There are numerous posix error codes, but these are the ones we thus far
  // find interesting.
  switch (err) {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
      return ERR_IO_PENDING;
    case ENETDOWN:
      return ERR_INTERNET_DISCONNECTED;
    case ETIMEDOUT:
      return ERR_TIMED_OUT;
    case ECONNRESET:
    case ENETRESET:  // Related to keep-alive
      return ERR_CONNECTION_RESET;
    case ECONNABORTED:
      return ERR_CONNECTION_ABORTED;
    case ECONNREFUSED:
      return ERR_CONNECTION_REFUSED;
    case EHOSTUNREACH:
    case ENETUNREACH:
      return ERR_ADDRESS_UNREACHABLE;
    case EADDRNOTAVAIL:
      return ERR_ADDRESS_INVALID;
    case 0:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

//-----------------------------------------------------------------------------

TCPClientSocket::TCPClientSocket(const AddressList& addresses)
  : socket_(kInvalidSocket),
    addresses_(addresses),
    current_ai_(addresses_.head()),
    waiting_connect_(false),
    event_(new event),
    write_callback_(NULL),
    callback_(NULL) {
}

TCPClientSocket::~TCPClientSocket() {
  Disconnect();
}

int TCPClientSocket::Connect(CompletionCallback* callback) {
  // If already connected, then just return OK.
  if (socket_ != kInvalidSocket)
    return OK;

  DCHECK(!waiting_connect_);

  const addrinfo* ai = current_ai_;
  DCHECK(ai);

  int rv = CreateSocket(ai);
  if (rv != OK)
    return rv;

  if (!connect(socket_, ai->ai_addr, static_cast<int>(ai->ai_addrlen))) {
    // Connected without waiting!
    return OK;
  }

  // Synchronous operation not supported
  DCHECK(callback);

  if (errno != EINPROGRESS) {
    DLOG(INFO) << "connect failed: " << errno;
    return MapPosixError(errno);
  }

  // Initialize event_ and link it to our MessagePump.
  // POLLOUT is set if the connection is established.
  // POLLIN is set if the connection fails,
  // so select for both read and write.
  MessageLoopForIO::current()->WatchSocket(
     socket_, EV_READ|EV_WRITE|EV_PERSIST, event_.get(), this);

  waiting_connect_ = true;
  callback_ = callback;
  return ERR_IO_PENDING;
}

int TCPClientSocket::ReconnectIgnoringLastError(CompletionCallback* callback) {
  // No ignorable errors!
  return ERR_UNEXPECTED;
}

void TCPClientSocket::Disconnect() {
  if (socket_ == kInvalidSocket)
    return;

  MessageLoopForIO::current()->UnwatchSocket(event_.get());
  close(socket_);
  socket_ = kInvalidSocket;
  waiting_connect_ = false;

  // Reset for next time.
  current_ai_ = addresses_.head();
}

bool TCPClientSocket::IsConnected() const {
  if (socket_ == kInvalidSocket || waiting_connect_)
    return false;

  // Check if connection is alive.
  char c;
  int rv = recv(socket_, &c, 1, MSG_PEEK);
  if (rv == 0)
    return false;
  if (rv == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    return false;

  return true;
}

int TCPClientSocket::Read(char* buf,
                          int buf_len,
                          CompletionCallback* callback) {
  DCHECK(socket_ != kInvalidSocket);
  DCHECK(!waiting_connect_);
  DCHECK(!callback_);
  // Synchronous operation not supported
  DCHECK(callback);
  DCHECK(buf_len > 0);

  int nread = read(socket_, buf, buf_len);
  if (nread >= 0) {
    return nread;
  }
  if (errno != EAGAIN && errno != EWOULDBLOCK) {
    DLOG(INFO) << "read failed, errno " << errno;
    return MapPosixError(errno);
  }

  MessageLoopForIO::current()->WatchSocket(
     socket_, EV_READ|EV_PERSIST, event_.get(), this);

  buf_ = buf;
  buf_len_ = buf_len;
  callback_ = callback;
  return ERR_IO_PENDING;
}

int TCPClientSocket::Write(const char* buf,
                           int buf_len,
                           CompletionCallback* callback) {
  DCHECK(socket_ != kInvalidSocket);
  DCHECK(!waiting_connect_);
  DCHECK(!write_callback_);
  // Synchronous operation not supported
  DCHECK(callback);
  DCHECK(buf_len > 0);

  int nwrite = write(socket_, buf, buf_len);
  if (nwrite >= 0) {
    return nwrite;
  }
  if (errno != EAGAIN && errno != EWOULDBLOCK)
    return MapPosixError(errno);

  MessageLoopForIO::current()->WatchSocket(
     socket_, EV_WRITE|EV_PERSIST, event_.get(), this);

  write_buf_ = buf;
  write_buf_len_ = buf_len;
  write_callback_ = callback;
  return ERR_IO_PENDING;
}

int TCPClientSocket::CreateSocket(const addrinfo* ai) {
  socket_ = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  if (socket_ == kInvalidSocket)
    return MapPosixError(errno);

  // All our socket I/O is nonblocking
  if (SetNonBlocking(socket_))
    return MapPosixError(errno);

  return OK;
}

void TCPClientSocket::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(callback_);

  // since Run may result in Read being called, clear callback_ up front.
  CompletionCallback* c = callback_;
  callback_ = NULL;
  c->Run(rv);
}

void TCPClientSocket::DoWriteCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(write_callback_);

  // since Run may result in Write being called, clear write_callback_ up front.
  CompletionCallback* c = write_callback_;
  write_callback_ = NULL;
  c->Run(rv);
}

void TCPClientSocket::DidCompleteConnect() {
  int result = ERR_UNEXPECTED;

  // Check to see if connect succeeded
  int error_code = 0;
  socklen_t len = sizeof(error_code);
  if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error_code, &len) < 0)
    error_code = errno;

  if (error_code == EINPROGRESS || error_code == EALREADY) {
    NOTREACHED();  // This indicates a bug in libevent or our code.
    result = ERR_IO_PENDING;
  } else if (current_ai_->ai_next && (
             error_code == EADDRNOTAVAIL ||
             error_code == EAFNOSUPPORT ||
             error_code == ECONNREFUSED ||
             error_code == ENETUNREACH ||
             error_code == EHOSTUNREACH ||
             error_code == ETIMEDOUT)) {
    // This address failed, try next one in list.
    const addrinfo* next = current_ai_->ai_next;
    Disconnect();
    current_ai_ = next;
    result = Connect(callback_);
  } else {
    result = MapPosixError(error_code);
    MessageLoopForIO::current()->UnwatchSocket(event_.get());
    waiting_connect_ = false;
  }

  if (result != ERR_IO_PENDING)
    DoCallback(result);
}

void TCPClientSocket::DidCompleteRead() {
  int bytes_transferred;
  bytes_transferred = read(socket_, buf_, buf_len_);

  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
  } else {
    result = MapPosixError(errno);
  }

  if (result != ERR_IO_PENDING) {
    buf_ = NULL;
    buf_len_ = 0;
    MessageLoopForIO::current()->UnwatchSocket(event_.get());
    DoCallback(result);
  }
}

void TCPClientSocket::DidCompleteWrite() {
  int bytes_transferred;
  bytes_transferred = write(socket_, write_buf_, write_buf_len_);

  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
  } else {
    result = MapPosixError(errno);
  }

  if (result != ERR_IO_PENDING) {
    write_buf_ = NULL;
    write_buf_len_ = 0;
    MessageLoopForIO::current()->UnwatchSocket(event_.get());
    DoWriteCallback(result);
  }
}

void TCPClientSocket::OnSocketReady(short flags) {
  // the only used bits of flags are EV_READ and EV_WRITE

  if (waiting_connect_) {
    DidCompleteConnect();
  } else {
    if ((flags & EV_WRITE) && write_callback_)
      DidCompleteWrite();
    if ((flags & EV_READ) && callback_)
      DidCompleteRead();
  }
}

int TCPClientSocket::GetPeerName(struct sockaddr *name, socklen_t *namelen) {
  return ::getpeername(socket_, name, namelen);
}

}  // namespace net

