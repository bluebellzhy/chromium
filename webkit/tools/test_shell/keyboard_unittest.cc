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

#include "config.h"

#pragma warning(push, 0)
#include "EventNames.h"
#include "EventTarget.h"
#include "KeyboardEvent.h"
#pragma warning(pop)

#include "webkit/glue/editor_client_impl.h"
#include "webkit/glue/event_conversion.h"
#include "webkit/glue/webinputevent.h"

#include "testing/gtest/include/gtest/gtest.h"

using WebCore::PlatformKeyboardEvent;
using WebCore::KeyboardEvent;

static inline const char* InterpretKeyEvent(
	const WebKeyboardEvent& keyboard_event,
	PlatformKeyboardEvent::Type key_type) {
  EditorClientImpl editor_impl(NULL);

  MakePlatformKeyboardEvent evt(keyboard_event);
  evt.SetKeyType(key_type);
  RefPtr<KeyboardEvent> keyboardEvent = KeyboardEvent::create(evt, NULL);
  return editor_impl.interpretKeyEvent(keyboardEvent.get());
}

static inline void SetupKeyDownEvent(WebKeyboardEvent& keyboard_event,
                                     char key_code,
                                     int modifiers) {
  keyboard_event.key_code = key_code;
  keyboard_event.key_data = key_code;
  keyboard_event.modifiers = modifiers;
  keyboard_event.type = WebInputEvent::KEY_DOWN;
}

static inline const char* InterpretCtrlKeyPress(char key_code) {
  WebKeyboardEvent keyboard_event;
  SetupKeyDownEvent(keyboard_event, 0xD, WebInputEvent::CTRL_KEY);

  return InterpretKeyEvent(keyboard_event, PlatformKeyboardEvent::RawKeyDown);
}

static const int no_modifiers = 0;

TEST(KeyboardUnitTestKeyDown, TestCtrlReturn) {
  WebCore::EventNames::init(); // FIXME: This should be in a SETUP call using TEST_F

  EXPECT_STREQ(InterpretCtrlKeyPress(0xD), "InsertNewline");
}

TEST(KeyboardUnitTestKeyDown, TestCtrlZ) {
  EXPECT_STREQ(InterpretCtrlKeyPress('Z'), "Undo");
}

TEST(KeyboardUnitTestKeyDown, TestCtrlA) {
  EXPECT_STREQ(InterpretCtrlKeyPress('A'), "SelectAll");
}

TEST(KeyboardUnitTestKeyDown, TestCtrlX) {
  EXPECT_STREQ(InterpretCtrlKeyPress('X'), "Cut");
}

TEST(KeyboardUnitTestKeyDown, TestCtrlC) {
  EXPECT_STREQ(InterpretCtrlKeyPress('C'), "Copy");
}

TEST(KeyboardUnitTestKeyDown, TestCtrlV) {
  EXPECT_STREQ(InterpretCtrlKeyPress('V'), "Paste");
}

TEST(KeyboardUnitTestKeyDown, TestEscape) {
  WebKeyboardEvent keyboard_event;
  SetupKeyDownEvent(keyboard_event, VK_ESCAPE, no_modifiers);

  const char* result = InterpretKeyEvent(keyboard_event,
                                         PlatformKeyboardEvent::RawKeyDown);
  EXPECT_STREQ(result, "Cancel");
}

TEST(KeyboardUnitTestKeyDown, TestRedo) {
  EXPECT_STREQ(InterpretCtrlKeyPress('Y'), "Redo");
}

static inline const char* InterpretTab(int modifiers) {
  WebKeyboardEvent keyboard_event;
  SetupKeyDownEvent(keyboard_event, '\t', modifiers);
  return InterpretKeyEvent(keyboard_event, PlatformKeyboardEvent::Char);
}

TEST(KeyboardUnitTestKeyPress, TestInsertTab) {
  EXPECT_STREQ(InterpretTab(no_modifiers), "InsertTab");
}

TEST(KeyboardUnitTestKeyPress, TestInsertBackTab) {
  EXPECT_STREQ(InterpretTab(WebInputEvent::SHIFT_KEY), "InsertBacktab");
}

static inline const char* InterpretNewLine(int modifiers) {
  WebKeyboardEvent keyboard_event;
  SetupKeyDownEvent(keyboard_event, '\r', modifiers);
  return InterpretKeyEvent(keyboard_event, PlatformKeyboardEvent::Char);
}

TEST(KeyboardUnitTestKeyPress, TestInsertNewline) {
  EXPECT_STREQ(InterpretNewLine(no_modifiers), "InsertNewline");
}

TEST(KeyboardUnitTestKeyPress, TestInsertNewline2) {
  EXPECT_STREQ(InterpretNewLine(WebInputEvent::CTRL_KEY), "InsertNewline");
}

TEST(KeyboardUnitTestKeyPress, TestInsertlinebreak) {
  EXPECT_STREQ(InterpretNewLine(WebInputEvent::SHIFT_KEY), "InsertNewline");
}

TEST(KeyboardUnitTestKeyPress, TestInsertNewline3) {
  EXPECT_STREQ(InterpretNewLine(WebInputEvent::ALT_KEY), "InsertNewline");
}

TEST(KeyboardUnitTestKeyPress, TestInsertNewline4) {
  const char* result = InterpretNewLine(WebInputEvent::ALT_KEY | WebInputEvent::SHIFT_KEY);
  EXPECT_STREQ(result, "InsertNewline");
}