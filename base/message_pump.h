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

#ifndef BASE_MESSAGE_PUMP_H_
#define BASE_MESSAGE_PUMP_H_

#include "base/ref_counted.h"

class TimeDelta;

namespace base {

class MessagePump : public RefCountedThreadSafe<MessagePump> {
 public:
  // Please see the comments above the Run method for an illustration of how
  // these delegate methods are used.
  class Delegate {
   public:
    // Called from within Run in response to ScheduleWork or when the message
    // pump would otherwise call DoDelayedWork.  Returns true to indicate that
    // work was done.  DoDelayedWork will not be called if DoWork returns true.
    virtual bool DoWork() = 0;

    // Called from within Run in response to ScheduleDelayedWork or when the
    // message pump would otherwise sleep waiting for more work.  Returns true
    // to indicate that delayed work was done.  DoIdleWork will not be called
    // if DoDelayedWork returns true.  Upon return |next_delay| indicates the
    // next delayed work interval.  If |next_delay| is negative, then the queue
    // of future delayed work (timer events) is currently empty, and no
    // additional calls to this function need to be scheduled.
    virtual bool DoDelayedWork(TimeDelta* next_delay) = 0;

    // Called from within Run just before the message pump goes to sleep.
    // Returns true to indicate that idle work was done.
    virtual bool DoIdleWork() = 0;
  };

  virtual ~MessagePump() {}
  
  // The Run method is called to enter the message pump's run loop.
  //
  // Within the method, the message pump is responsible for processing native
  // messages as well as for giving cycles to the delegate periodically.  The
  // message pump should take care to mix delegate callbacks with native
  // message processing so neither type of event starves the other of cycles.
  //
  // The anatomy of a typical run loop:
  //
  //   for (;;) {
  //     bool did_work = DoInternalWork();
  //     if (should_quit_)
  //       break;
  //     did_work |= delegate_->DoWork();
  //     if (should_quit_)
  //       break;
  //     if (did_work)
  //       continue;
  //
  //     did_work = delegate_->DoDelayedWork();
  //     if (should_quit_)
  //       break;
  //     if (did_work)
  //       continue;
  //
  //     did_work = delegate_->DoIdleWork();
  //     if (should_quit_)
  //       break;
  //     if (did_work)
  //       continue;
  //
  //     WaitForWork();
  //   }
  //
  // Here, DoInternalWork is some private method of the message pump that is
  // responsible for dispatching the next UI message or notifying the next IO
  // completion (for example).  WaitForWork is a private method that simply
  // blocks until there is more work of any type to do.
  //
  // Notice that the run loop alternates between calling DoInternalWork and
  // calling the delegate's DoWork method.  This helps ensure that neither work
  // queue starves the other.  However, DoDelayedWork may be starved.  The
  // implementation may decide to periodically let some DoDelayedWork calls go
  // through if either DoInternalWork or DoWork is dominating the run loop.
  // This can be important for message pumps that are used to drive animations,
  // for example.
  //
  // Notice also that after each callout to foreign code, the run loop checks
  // to see if it should quit.  The Quit method is responsible for setting this
  // flag.  No further work is done once the quit flag is set.
  //
  // NOTE: Care must be taken to handle Run being called again from within any
  // of the callouts to foreign code.  Native message pumps may also need to
  // deal with other native message pumps being run outside their control
  // (e.g., the MessageBox API on Windows pumps UI messages!).  To be specific,
  // the callouts (DoWork and DoDelayedWork) MUST still be provided even in
  // nested sub-loops that are "seemingly" outside the control of this message
  // pump.  DoWork in particular must never be starved for time slices unless
  // it returns false (meaning it has run out of things to do).
  // 
  virtual void Run(Delegate* delegate) = 0;

  // Quit immediately from the most recently entered run loop.  This method may
  // only be used on the thread that called Run.
  virtual void Quit() = 0;

  // Schedule a DoWork callback to happen reasonably soon.  Does nothing if a
  // DoWork callback is already scheduled.  This method may be called from any
  // thread.  Once this call is made, DoWork should not be "starved" at least
  // until it returns a value of false.
  virtual void ScheduleWork() = 0;

  // Schedule a DoDelayedWork callback to happen after the specified delay,
  // cancelling any pending DoDelayedWork callback.  This method may only be
  // used on the thread that called Run.
  virtual void ScheduleDelayedWork(const TimeDelta& delay) = 0;
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_H_
