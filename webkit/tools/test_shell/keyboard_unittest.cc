// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  // TODO(eseidel): This should be in a SETUP call using TEST_F
  WebCore::EventNames::init();

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
  int modifiers = WebInputEvent::ALT_KEY | WebInputEvent::SHIFT_KEY;
  const char* result = InterpretNewLine(modifiers);
  EXPECT_STREQ(result, "InsertNewline");
}
