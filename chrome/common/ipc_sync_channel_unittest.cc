// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit test for SyncChannel.

#include <windows.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/common/child_process.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_sync_channel.h"
#include "chrome/common/stl_util-inl.h"
#include "testing/gtest/include/gtest/gtest.h"

#define IPC_MESSAGE_MACROS_ENUMS
#include "chrome/common/ipc_sync_channel_unittest.h"

// define the classes
#define IPC_MESSAGE_MACROS_CLASSES
#include "chrome/common/ipc_sync_channel_unittest.h"

using namespace IPC;

namespace {

class IPCSyncChannelTest : public testing::Test {
 private:
  MessageLoop message_loop_;
};

// SyncChannel should only be used in child processes as we don't want to hang
// the browser.  So in the unit test we need to have a ChildProcess object.
class TestProcess : public ChildProcess {
 public:
  explicit TestProcess(const std::wstring& channel_name) {}
  static void GlobalInit() {
    ChildProcessFactory<TestProcess> factory;
    ChildProcess::GlobalInit(L"blah", &factory);
  }
};

// Wrapper around an event handle.
class Event {
 public:
  Event() : handle_(CreateEvent(NULL, FALSE, FALSE, NULL)) { }
  ~Event() { CloseHandle(handle_); }
  void Set() { SetEvent(handle_); }
  void Wait() { WaitForSingleObject(handle_, INFINITE); }
  HANDLE handle() { return handle_; }

 private:
  HANDLE handle_;

  DISALLOW_EVIL_CONSTRUCTORS(Event);
};

// Base class for a "process" with listener and IPC threads.
class Worker : public Channel::Listener, public Message::Sender {
 public:
  // Will create a channel without a name.
  Worker(Channel::Mode mode, const std::string& thread_name)
      : channel_name_(),
        mode_(mode),
        ipc_thread_((thread_name + "_ipc").c_str()),
        listener_thread_((thread_name + "_listener").c_str()),
        overrided_thread_(NULL) { }

  // Will create a named channel and use this name for the threads' name.
  Worker(const std::wstring& channel_name, Channel::Mode mode)
      : channel_name_(channel_name),
        mode_(mode),
        ipc_thread_((WideToUTF8(channel_name) + "_ipc").c_str()),
        listener_thread_((WideToUTF8(channel_name) + "_listener").c_str()),
        overrided_thread_(NULL) { }

  // The IPC thread needs to outlive SyncChannel, so force the correct order of
  // destruction.
  virtual ~Worker() {
    Event listener_done, ipc_done;
    ListenerThread()->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &Worker::OnListenerThreadShutdown, listener_done.handle(),
        ipc_done.handle()));
    HANDLE handles[] = { listener_done.handle(), ipc_done.handle() };
    WaitForMultipleObjects(2, handles, TRUE, INFINITE);
    ipc_thread_.Stop();
    listener_thread_.Stop();
  }
  void AddRef() { }
  void Release() { }
  bool Send(Message* msg) { return channel_->Send(msg); }
  bool SendWithTimeout(Message* msg, int timeout_ms) {
    return channel_->SendWithTimeout(msg, timeout_ms);
  }
  void WaitForChannelCreation() { channel_created_.Wait(); }
  void CloseChannel() {
    DCHECK(MessageLoop::current() == ListenerThread()->message_loop());
    channel_->Close();
  }
  void Start() {
    StartThread(&listener_thread_);
    ListenerThread()->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &Worker::OnStart));
  }
  void OverrideThread(base::Thread* overrided_thread) {
    DCHECK(overrided_thread_ == NULL);
    overrided_thread_ = overrided_thread;
  }
  Channel::Mode mode() { return mode_; }
  HANDLE done_event() { return done_.handle(); }

 protected:
  // Derived classes need to call this when they've completed their part of
  // the test.
  void Done() { done_.Set(); }

  // Functions for dervied classes to implement if they wish.
  virtual void Run() { }
  virtual void OnAnswer(int* answer) { NOTREACHED(); }
  virtual void OnAnswerDelay(Message* reply_msg) {
    // The message handler map below can only take one entry for
    // SyncChannelTestMsg_AnswerToLife, so since some classes want
    // the normal version while other want the delayed reply, we
    // call the normal version if the derived class didn't override
    // this function.
    int answer;
    OnAnswer(&answer);
    SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, answer);
    Send(reply_msg);
  }
  virtual void OnDouble(int in, int* out) { NOTREACHED(); }
  virtual void OnDoubleDelay(int in, Message* reply_msg) {
    int result;
    OnDouble(in, &result);
    SyncChannelTestMsg_Double::WriteReplyParams(reply_msg, result);
    Send(reply_msg);
  }

 private:
  base::Thread* ListenerThread() {
    return overrided_thread_ ? overrided_thread_ : &listener_thread_;
  }
  // Called on the listener thread to create the sync channel.
  void OnStart() {
    // Link ipc_thread_, listener_thread_ and channel_ altogether.
    StartThread(&ipc_thread_);
    channel_.reset(new SyncChannel(
        channel_name_, mode_, this, NULL, ipc_thread_.message_loop(), true,
        TestProcess::GetShutDownEvent()));
    channel_created_.Set();
    Run();
  }

  void OnListenerThreadShutdown(HANDLE listener_event, HANDLE ipc_event) {
    // SyncChannel needs to be destructed on the thread that it was created on.
    channel_.reset();
    SetEvent(listener_event);
    
    ipc_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &Worker::OnIPCThreadShutdown, ipc_event));
  }

  void OnIPCThreadShutdown(HANDLE ipc_event) {
    SetEvent(ipc_event);
  }

  void OnMessageReceived(const Message& message) {
    IPC_BEGIN_MESSAGE_MAP(Worker, message)
     IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncChannelTestMsg_Double, OnDoubleDelay)
     IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncChannelTestMsg_AnswerToLife,
                                     OnAnswerDelay)
    IPC_END_MESSAGE_MAP()
  }

  void StartThread(base::Thread* thread) {
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    thread->StartWithOptions(options);
  }

  Event done_;
  Event channel_created_;
  std::wstring channel_name_;
  Channel::Mode mode_;
  scoped_ptr<SyncChannel> channel_;
  base::Thread ipc_thread_;
  base::Thread listener_thread_;
  base::Thread* overrided_thread_;

  DISALLOW_EVIL_CONSTRUCTORS(Worker);
};


// Starts the test with the given workers.  This function deletes the workers
// when it's done.
void RunTest(std::vector<Worker*> workers) {
  TestProcess::GlobalInit();

  // First we create the workers that are channel servers, or else the other
  // workers' channel initialization might fail because the pipe isn't created..
  for (size_t i = 0; i < workers.size(); ++i) {
    if (workers[i]->mode() == Channel::MODE_SERVER) {
      workers[i]->Start();
      workers[i]->WaitForChannelCreation();
    }
  }

  // now create the clients
  for (size_t i = 0; i < workers.size(); ++i) {
    if (workers[i]->mode() == Channel::MODE_CLIENT)
      workers[i]->Start();
  }

  // wait for all the workers to finish
  std::vector<HANDLE> done_handles;
  for (size_t i = 0; i < workers.size(); ++i)
    done_handles.push_back(workers[i]->done_event());

  int count = static_cast<int>(done_handles.size());
  WaitForMultipleObjects(count, &done_handles.front(), TRUE, INFINITE);
  STLDeleteContainerPointers(workers.begin(), workers.end());
  
  TestProcess::GlobalCleanup();
}

}  // namespace

//-----------------------------------------------------------------------------

namespace {

class SimpleServer : public Worker {
 public:
  SimpleServer(bool pump_during_send)
      : Worker(Channel::MODE_SERVER, "simpler_server"),
        pump_during_send_(pump_during_send) { }
  void Run() {
    int answer = 0;
    IPC::SyncMessage* msg = new SyncChannelTestMsg_AnswerToLife(&answer);
    if (pump_during_send_)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK(result);
    DCHECK(answer == 42);
    Done();
  }

  bool pump_during_send_;
};

class SimpleClient : public Worker {
 public:
  SimpleClient() : Worker(Channel::MODE_CLIENT, "simple_client") { }

  void OnAnswer(int* answer) {
    *answer = 42;
    Done();
  }
};

void Simple(bool pump_during_send) {
  std::vector<Worker*> workers;
  workers.push_back(new SimpleServer(pump_during_send));
  workers.push_back(new SimpleClient());
  RunTest(workers);
}

}  // namespace

// Tests basic synchronous call
TEST_F(IPCSyncChannelTest, Simple) {
  Simple(false);
  Simple(true);
}

//-----------------------------------------------------------------------------

namespace {

class DelayClient : public Worker {
 public:
  DelayClient() : Worker(Channel::MODE_CLIENT, "delay_client") { }

  void OnAnswerDelay(Message* reply_msg) {
    SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
    Send(reply_msg);
    Done();
  }
};

void DelayReply(bool pump_during_send) {
  std::vector<Worker*> workers;
  workers.push_back(new SimpleServer(pump_during_send));
  workers.push_back(new DelayClient());
  RunTest(workers);
}

}  // namespace

// Tests that asynchronous replies work
TEST_F(IPCSyncChannelTest, DelayReply) {
  DelayReply(false);
  DelayReply(true);
}

//-----------------------------------------------------------------------------

namespace {

class NoHangServer : public Worker {
 public:
  explicit NoHangServer(Event* got_first_reply, bool pump_during_send)
      : Worker(Channel::MODE_SERVER, "no_hang_server"),
        got_first_reply_(got_first_reply),
        pump_during_send_(pump_during_send) { }
  void Run() {
    int answer = 0;
    IPC::SyncMessage* msg = new SyncChannelTestMsg_AnswerToLife(&answer);
    if (pump_during_send_)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK(result);
    DCHECK(answer == 42);
    got_first_reply_->Set();

    msg = new SyncChannelTestMsg_AnswerToLife(&answer);
    if (pump_during_send_)
      msg->EnableMessagePumping();
    result = Send(msg);
    DCHECK(!result);
    Done();
  }

  Event* got_first_reply_;
  bool pump_during_send_;
};

class NoHangClient : public Worker {
 public:
  explicit NoHangClient(Event* got_first_reply)
    : Worker(Channel::MODE_CLIENT, "no_hang_client"),
      got_first_reply_(got_first_reply) { }

  virtual void OnAnswerDelay(Message* reply_msg) {
    // Use the DELAY_REPLY macro so that we can force the reply to be sent
    // before this function returns (when the channel will be reset).
    SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
    Send(reply_msg);
    got_first_reply_->Wait();
    CloseChannel();
    Done();
  }

  Event* got_first_reply_;
};

void NoHang(bool pump_during_send) {
  Event got_first_reply;
  std::vector<Worker*> workers;
  workers.push_back(new NoHangServer(&got_first_reply, pump_during_send));
  workers.push_back(new NoHangClient(&got_first_reply));
  RunTest(workers);
}

}  // namespace

// Tests that caller doesn't hang if receiver dies
TEST_F(IPCSyncChannelTest, NoHang) {
  NoHang(false);
  NoHang(true);
}

//-----------------------------------------------------------------------------

namespace {

class UnblockServer : public Worker {
 public:
  UnblockServer(bool pump_during_send)
    : Worker(Channel::MODE_SERVER, "unblock_server"),
      pump_during_send_(pump_during_send) { }
  void Run() {
    int answer = 0;
    IPC::SyncMessage* msg = new SyncChannelTestMsg_AnswerToLife(&answer);
    if (pump_during_send_)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK(result);
    DCHECK(answer == 42);
    Done();
  }

  void OnDouble(int in, int* out) {
    *out = in * 2;
  }

  bool pump_during_send_;
};

class UnblockClient : public Worker {
 public:
  UnblockClient(bool pump_during_send)
    : Worker(Channel::MODE_CLIENT, "unblock_client"),
      pump_during_send_(pump_during_send) { }

  void OnAnswer(int* answer) {
    IPC::SyncMessage* msg = new SyncChannelTestMsg_Double(21, answer);
    if (pump_during_send_)
      msg->EnableMessagePumping();
    BOOL result = Send(msg);
    DCHECK(result);
    Done();
  }

  bool pump_during_send_;
};

void Unblock(bool server_pump, bool client_pump) {
  std::vector<Worker*> workers;
  workers.push_back(new UnblockServer(server_pump));
  workers.push_back(new UnblockClient(client_pump));
  RunTest(workers);
}

}  // namespace

// Tests that the caller unblocks to answer a sync message from the receiver.
TEST_F(IPCSyncChannelTest, Unblock) {
  Unblock(false, false);
  Unblock(false, true);
  Unblock(true, false);
  Unblock(true, true);
}

//-----------------------------------------------------------------------------

namespace {

class RecursiveServer : public Worker {
 public:
  explicit RecursiveServer(
    bool expected_send_result, bool pump_first, bool pump_second)
    : Worker(Channel::MODE_SERVER, "recursive_server"),
      expected_send_result_(expected_send_result),
      pump_first_(pump_first), pump_second_(pump_second)  { }
  void Run() {
    int answer;
    IPC::SyncMessage* msg = new SyncChannelTestMsg_Double(21, &answer);
    if (pump_first_)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK(result == expected_send_result_);
    Done();
  }

  void OnDouble(int in, int* out) {
    int answer;
    IPC::SyncMessage* msg = new SyncChannelTestMsg_AnswerToLife(&answer);
    if (pump_second_)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK(result == expected_send_result_);
  }

  bool expected_send_result_, pump_first_, pump_second_;
};

class RecursiveClient : public Worker {
 public:
  explicit RecursiveClient(bool pump_during_send, bool close_channel)
    : Worker(Channel::MODE_CLIENT, "recursive_client"),
      pump_during_send_(pump_during_send), close_channel_(close_channel) { }

  void OnDoubleDelay(int in, Message* reply_msg) {
    int answer = 0;
    IPC::SyncMessage* msg = new SyncChannelTestMsg_Double(5, &answer);
    if (pump_during_send_)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK(result != close_channel_);
    if (!close_channel_) {
      SyncChannelTestMsg_Double::WriteReplyParams(reply_msg, in * 2);
      Send(reply_msg);
    }
    Done();
  }

  void OnAnswerDelay(Message* reply_msg) {
    if (close_channel_) {
      CloseChannel();
    } else {
      SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
      Send(reply_msg);
    }
  }

  bool pump_during_send_, close_channel_;
};

void Recursive(
    bool server_pump_first, bool server_pump_second, bool client_pump) {
  std::vector<Worker*> workers;
  workers.push_back(
      new RecursiveServer(true, server_pump_first, server_pump_second));
  workers.push_back(new RecursiveClient(client_pump, false));
  RunTest(workers);
}

}  // namespace

// Tests a server calling Send while another Send is pending.
TEST_F(IPCSyncChannelTest, Recursive) {
  Recursive(false, false, false);
  Recursive(false, false, true);
  Recursive(false, true, false);
  Recursive(false, true, true);
  Recursive(true, false, false);
  Recursive(true, false, true);
  Recursive(true, true, false);
  Recursive(true, true, true);
}

//-----------------------------------------------------------------------------

namespace {

void RecursiveNoHang(
    bool server_pump_first, bool server_pump_second, bool client_pump) {
  std::vector<Worker*> workers;
  workers.push_back(
      new RecursiveServer(false, server_pump_first, server_pump_second));
  workers.push_back(new RecursiveClient(client_pump, true));
  RunTest(workers);
}

}  // namespace

// Tests that if a caller makes a sync call during an existing sync call and
// the receiver dies, neither of the Send() calls hang.
TEST_F(IPCSyncChannelTest, RecursiveNoHang) {
  RecursiveNoHang(false, false, false);
  RecursiveNoHang(false, false, true);
  RecursiveNoHang(false, true, false);
  RecursiveNoHang(false, true, true);
  RecursiveNoHang(true, false, false);
  RecursiveNoHang(true, false, true);
  RecursiveNoHang(true, true, false);
  RecursiveNoHang(true, true, true);
}

//-----------------------------------------------------------------------------

namespace {

class MultipleServer1 : public Worker {
 public:
  MultipleServer1(bool pump_during_send)
    : Worker(L"test_channel1", Channel::MODE_SERVER),
      pump_during_send_(pump_during_send) { }

  void Run() {
    int answer = 0;
    IPC::SyncMessage* msg = new SyncChannelTestMsg_Double(5, &answer);
    if (pump_during_send_)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK(result);
    DCHECK(answer == 10);
    Done();
  }

  bool pump_during_send_;
};

class MultipleClient1 : public Worker {
 public:
  MultipleClient1(Event* client1_msg_received, Event* client1_can_reply) :
      Worker(L"test_channel1", Channel::MODE_CLIENT),
      client1_msg_received_(client1_msg_received),
      client1_can_reply_(client1_can_reply) { }

  void OnDouble(int in, int* out) {
    client1_msg_received_->Set();
    *out = in * 2;
    client1_can_reply_->Wait();
    Done();
  }

 private:
  Event *client1_msg_received_, *client1_can_reply_;
};

class MultipleServer2 : public Worker {
 public:
  MultipleServer2() : Worker(L"test_channel2", Channel::MODE_SERVER) { }

  void OnAnswer(int* result) {
    *result = 42;
    Done();
  }
};

class MultipleClient2 : public Worker {
 public:
  MultipleClient2(
    Event* client1_msg_received, Event* client1_can_reply,
    bool pump_during_send)
    : Worker(L"test_channel2", Channel::MODE_CLIENT),
      client1_msg_received_(client1_msg_received),
      client1_can_reply_(client1_can_reply),
      pump_during_send_(pump_during_send) { }

  void Run() {
    int answer = 0;
    client1_msg_received_->Wait();
    IPC::SyncMessage* msg = new SyncChannelTestMsg_AnswerToLife(&answer);
    if (pump_during_send_)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK(result);
    DCHECK(answer == 42);
    client1_can_reply_->Set();
    Done();
  }

 private:
  Event *client1_msg_received_, *client1_can_reply_;
  bool pump_during_send_;
};

void Multiple(bool server_pump, bool client_pump) {
  std::vector<Worker*> workers;

  // A shared worker thread so that server1 and server2 run on one thread.
  base::Thread worker_thread("Multiple");
  worker_thread.Start();

  // Server1 sends a sync msg to client1, which blocks the reply until
  // server2 (which runs on the same worker thread as server1) responds
  // to a sync msg from client2.
  Event client1_msg_received, client1_can_reply;

  Worker* worker;

  worker = new MultipleServer2();
  worker->OverrideThread(&worker_thread);
  workers.push_back(worker);

  worker = new MultipleClient2(
      &client1_msg_received, &client1_can_reply, client_pump);
  workers.push_back(worker);

  worker = new MultipleServer1(server_pump);
  worker->OverrideThread(&worker_thread);
  workers.push_back(worker);

  worker = new MultipleClient1(
      &client1_msg_received, &client1_can_reply);
  workers.push_back(worker);

  RunTest(workers);
}

}  // namespace

// Tests that multiple SyncObjects on the same listener thread can unblock each
// other.
TEST_F(IPCSyncChannelTest, Multiple) {
  Multiple(false, false);
  Multiple(false, true);
  Multiple(true, false);
  Multiple(true, true);
}

//-----------------------------------------------------------------------------

namespace {

class QueuedReplyServer1 : public Worker {
 public:
  QueuedReplyServer1(bool pump_during_send)
    : Worker(L"test_channel1", Channel::MODE_SERVER),
      pump_during_send_(pump_during_send) { }
  void Run() {
    int answer = 0;
    IPC::SyncMessage* msg = new SyncChannelTestMsg_Double(5, &answer);
    if (pump_during_send_)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK(result);
    DCHECK(answer == 10);
    Done();
  }

  bool pump_during_send_;
};

class QueuedReplyClient1 : public Worker {
 public:
  QueuedReplyClient1(Event* client1_msg_received, Event* server2_can_reply) :
      Worker(L"test_channel1", Channel::MODE_CLIENT),
      client1_msg_received_(client1_msg_received),
      server2_can_reply_(server2_can_reply) { }

  void OnDouble(int in, int* out) {
    client1_msg_received_->Set();
    *out = in * 2;
    server2_can_reply_->Wait();
    Done();
  }

 private:
  Event *client1_msg_received_, *server2_can_reply_;
};

class QueuedReplyServer2 : public Worker {
 public:
  explicit QueuedReplyServer2(Event* server2_can_reply) :
      Worker(L"test_channel2", Channel::MODE_SERVER),
      server2_can_reply_(server2_can_reply) { }

  void OnAnswer(int* result) {
    server2_can_reply_->Set();

    // give client1's reply time to reach the server listener thread
    Sleep(200);

    *result = 42;
    Done();
  }

  Event *server2_can_reply_;
};

class QueuedReplyClient2 : public Worker {
 public:
  explicit QueuedReplyClient2(
    Event* client1_msg_received, bool pump_during_send)
    : Worker(L"test_channel2", Channel::MODE_CLIENT),
      client1_msg_received_(client1_msg_received),
      pump_during_send_(pump_during_send){ }

  void Run() {
    int answer = 0;
    client1_msg_received_->Wait();
    IPC::SyncMessage* msg = new SyncChannelTestMsg_AnswerToLife(&answer);
    if (pump_during_send_)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK(result);
    DCHECK(answer == 42);
    Done();
  }

  Event *client1_msg_received_;
  bool pump_during_send_;
};

void QueuedReply(bool server_pump, bool client_pump) {
  std::vector<Worker*> workers;

  // A shared worker thread so that server1 and server2 run on one thread.
  base::Thread worker_thread("QueuedReply");
  worker_thread.Start();

  Event client1_msg_received, server2_can_reply;

  Worker* worker;

  worker = new QueuedReplyServer2(&server2_can_reply);
  worker->OverrideThread(&worker_thread);
  workers.push_back(worker);

  worker = new QueuedReplyClient2(&client1_msg_received, client_pump);
  workers.push_back(worker);

  worker = new QueuedReplyServer1(server_pump);
  worker->OverrideThread(&worker_thread);
  workers.push_back(worker);

  worker = new QueuedReplyClient1(
      &client1_msg_received, &server2_can_reply);
  workers.push_back(worker);

  RunTest(workers);
}

}  // namespace

// While a blocking send is in progress, the listener thread might answer other
// synchronous messages.  This tests that if during the response to another
// message the reply to the original messages comes, it is queued up correctly
// and the original Send is unblocked later.
TEST_F(IPCSyncChannelTest, QueuedReply) {
  QueuedReply(false, false);
  QueuedReply(false, true);
  QueuedReply(true, false);
  QueuedReply(true, true);
}

//-----------------------------------------------------------------------------

namespace {

class BadServer : public Worker {
 public:
  BadServer(bool pump_during_send)
    : Worker(Channel::MODE_SERVER, "simpler_server"),
      pump_during_send_(pump_during_send) { }
  void Run() {
    int answer = 0;

    IPC::SyncMessage* msg = new SyncMessage(
        MSG_ROUTING_CONTROL, SyncChannelTestMsg_Double::ID,
        Message::PRIORITY_NORMAL, NULL);
    if (pump_during_send_)
      msg->EnableMessagePumping();

    // Temporarily set the minimum logging very high so that the assertion
    // in ipc_message_utils doesn't fire.
    int log_level = logging::GetMinLogLevel();
    logging::SetMinLogLevel(kint32max);
    bool result = Send(msg);
    logging::SetMinLogLevel(log_level);
    DCHECK(!result);

    // Need to send another message to get the client to call Done().
    result = Send(new SyncChannelTestMsg_AnswerToLife(&answer));
    DCHECK(result);
    DCHECK(answer == 42);

    Done();
  }

  bool pump_during_send_;
};

void BadMessage(bool pump_during_send) {
  std::vector<Worker*> workers;
  workers.push_back(new BadServer(pump_during_send));
  workers.push_back(new SimpleClient());
  RunTest(workers);
}

}  // namespace

// Tests that if a message is not serialized correctly, the Send() will fail.
TEST_F(IPCSyncChannelTest, BadMessage) {
  BadMessage(false);
  BadMessage(true);
}

//-----------------------------------------------------------------------------

namespace {

class ChattyClient : public Worker {
 public:
  ChattyClient() :
      Worker(Channel::MODE_CLIENT, "chatty_client") { }

  void OnAnswer(int* answer) {
    // The PostMessage limit is 10k.  Send 20% more than that.
    const int kMessageLimit = 10000;
    const int kMessagesToSend = kMessageLimit * 120 / 100;
    for (int i = 0; i < kMessagesToSend; ++i) {
      bool result = Send(new SyncChannelTestMsg_Double(21, answer));
      DCHECK(result);
      if (!result)
        break;
    }
    Done();
  }
};

void ChattyServer(bool pump_during_send) {
  std::vector<Worker*> workers;
  workers.push_back(new UnblockServer(pump_during_send));
  workers.push_back(new ChattyClient());
  RunTest(workers);
}

}  // namespace

// Tests http://b/issue?id=1093251 - that sending lots of sync messages while
// the receiver is waiting for a sync reply does not overflow the PostMessage
// queue.
TEST_F(IPCSyncChannelTest, ChattyServer) {
  ChattyServer(false);
  ChattyServer(true);
}

//------------------------------------------------------------------------------

namespace {

class TimeoutServer : public Worker {
 public:
   TimeoutServer(int timeout_ms,
                 std::vector<bool> timeout_seq,
                 bool pump_during_send)
      : Worker(Channel::MODE_SERVER, "timeout_server"),
        timeout_ms_(timeout_ms),
        timeout_seq_(timeout_seq),
        pump_during_send_(pump_during_send) {
  }

  void Run() {
    for (std::vector<bool>::const_iterator iter = timeout_seq_.begin();
         iter != timeout_seq_.end(); ++iter) {
      int answer = 0;
      IPC::SyncMessage* msg = new SyncChannelTestMsg_AnswerToLife(&answer);
      if (pump_during_send_)
        msg->EnableMessagePumping();
      bool result = SendWithTimeout(msg, timeout_ms_);
      if (*iter) {
        // Time-out expected.
        DCHECK(!result);
        DCHECK(answer == 0);
      } else {
        DCHECK(result);
        DCHECK(answer == 42);
      }
    }
    Done();
  }

 private:
  int timeout_ms_;
  std::vector<bool> timeout_seq_;
  bool pump_during_send_;
};

class UnresponsiveClient : public Worker {
 public:
   UnresponsiveClient(std::vector<bool> timeout_seq)
      : Worker(Channel::MODE_CLIENT, "unresponsive_client"),
        timeout_seq_(timeout_seq) {
   }

  void OnAnswerDelay(Message* reply_msg) {
    DCHECK(!timeout_seq_.empty());
    if (!timeout_seq_[0]) {
      SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
      Send(reply_msg);
    } else {
      // Don't reply.
      delete reply_msg;
    }
    timeout_seq_.erase(timeout_seq_.begin());
    if (timeout_seq_.empty())
      Done();
  }

 private:
  // Whether we should time-out or respond to the various messages we receive.
  std::vector<bool> timeout_seq_;
};

void SendWithTimeoutOK(bool pump_during_send) {
  std::vector<Worker*> workers;
  std::vector<bool> timeout_seq;
  timeout_seq.push_back(false);
  timeout_seq.push_back(false);
  timeout_seq.push_back(false);
  workers.push_back(new TimeoutServer(5000, timeout_seq, pump_during_send));
  workers.push_back(new SimpleClient());
  RunTest(workers);
}

void SendWithTimeoutTimeout(bool pump_during_send) {
  std::vector<Worker*> workers;
  std::vector<bool> timeout_seq;
  timeout_seq.push_back(true);
  timeout_seq.push_back(false);
  timeout_seq.push_back(false);
  workers.push_back(new TimeoutServer(100, timeout_seq, pump_during_send));
  workers.push_back(new UnresponsiveClient(timeout_seq));
  RunTest(workers);
}

void SendWithTimeoutMixedOKAndTimeout(bool pump_during_send) {
  std::vector<Worker*> workers;
  std::vector<bool> timeout_seq;
  timeout_seq.push_back(true);
  timeout_seq.push_back(false);
  timeout_seq.push_back(false);
  timeout_seq.push_back(true);
  timeout_seq.push_back(false);
  workers.push_back(new TimeoutServer(100, timeout_seq, pump_during_send));
  workers.push_back(new UnresponsiveClient(timeout_seq));
  RunTest(workers);
}

}  // namespace

// Tests that SendWithTimeout does not time-out if the response comes back fast
// enough.
TEST_F(IPCSyncChannelTest, SendWithTimeoutOK) {
  SendWithTimeoutOK(false);
  SendWithTimeoutOK(true);
}

// Tests that SendWithTimeout does time-out.
TEST_F(IPCSyncChannelTest, SendWithTimeoutTimeout) {
  SendWithTimeoutTimeout(false);
  SendWithTimeoutTimeout(true);
}

// Sends some message that time-out and some that succeed.
TEST_F(IPCSyncChannelTest, SendWithTimeoutMixedOKAndTimeout) {
  SendWithTimeoutMixedOKAndTimeout(false);
  SendWithTimeoutMixedOKAndTimeout(true);
}
