// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/debug_message_handler.h"
#include "chrome/renderer/render_view.h"

////////////////////////////////////////
// methods called from the RenderThread

DebugMessageHandler::DebugMessageHandler(RenderView* view) :
    debugger_(NULL), view_(view), channel_(NULL) {
  view_loop_ = MessageLoop::current();
  view_routing_id_ = view_->routing_id();
}

DebugMessageHandler::~DebugMessageHandler() {
}

void DebugMessageHandler::EvaluateScriptUrl(const std::wstring& url) {
  DCHECK(MessageLoop::current() == view_loop_);
  // It's possible that this will get cleared out from under us.
  RenderView* view = view_;
  if (view) {
    view->EvaluateScriptUrl(L"", url);
  }
}

///////////////////////////////////////////////
// all methods below called from the IO thread

void DebugMessageHandler::DebuggerOutput(const std::wstring& out) {
  channel_->Send(new ViewHostMsg_DebuggerOutput(view_routing_id_, out));
}

void DebugMessageHandler::OnBreak(bool force) {
  debugger_->Break(force);
  if (force && view_loop_) {
    view_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &DebugMessageHandler::EvaluateScriptUrl,
        std::wstring(L"javascript:void(0)")));
  }
}

void DebugMessageHandler::OnAttach() {
  if (!debugger_) {
    debugger_ = new Debugger(this);
  }
  debugger_->Attach();
}

void DebugMessageHandler::OnCommand(const std::wstring& cmd) {
  if (!debugger_) {
    NOTREACHED();
    std::wstring msg =
        StringPrintf(L"before attach, ignored command (%S)", cmd.c_str());
    DebuggerOutput(msg);
  } else {
    debugger_->Command(cmd);
  }
}

void DebugMessageHandler::OnDetach() {
  if (debugger_)
    debugger_->Detach();
  if (view_loop_ && view_)
    view_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        view_, &RenderView::OnDebugDetach));
}

void DebugMessageHandler::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

void DebugMessageHandler::OnFilterRemoved() {
  channel_ = NULL;
  view_loop_ = NULL;
  view_ = NULL;
  // By the time this is called, the view is not in a state where it can
  // receive messages from the MessageLoop, so we need to clear those first.
  OnDetach();
}

bool DebugMessageHandler::OnMessageReceived(const IPC::Message& message) {
  DCHECK(channel_ != NULL);

  // In theory, there could be multiple debuggers running (in practice this
  // hasn't been implemented yet), so make sure we only handle messages meant
  // for the view we were initialized for.
  if (message.routing_id() != view_routing_id_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DebugMessageHandler, message)
    IPC_MESSAGE_HANDLER_GENERIC(ViewMsg_DebugAttach,
      OnAttach();
      handled = false;)
    IPC_MESSAGE_HANDLER(ViewMsg_DebugBreak, OnBreak)
    IPC_MESSAGE_HANDLER(ViewMsg_DebugCommand, OnCommand)
    IPC_MESSAGE_HANDLER_GENERIC(ViewMsg_DebugDetach,
      OnDetach();
      handled = false;)
    // If the debugger is active, then it's possible that the renderer thread
    // is suspended handling a breakpoint.  In that case, the renderer will
    // hang forever and never exit.  To avoid this, we look for close messages
    // and tell the debugger to shutdown.
    IPC_MESSAGE_HANDLER_GENERIC(ViewMsg_Close,
      if (debugger_) OnDetach();
      handled = false;)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()
  return handled;
}

