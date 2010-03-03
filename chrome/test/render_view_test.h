// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_RENDER_VIEW_TEST_H_
#define CHROME_TEST_RENDER_VIEW_TEST_H_

#include <string>

#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/sandbox_init_wrapper.h"
#include "chrome/renderer/mock_keyboard.h"
#include "chrome/renderer/mock_render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/renderer_main_platform_delegate.h"
#include "chrome/renderer/renderer_webkitclient_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"

class MockRenderProcess;

class RenderViewTest : public testing::Test {
 public:
  RenderViewTest();
  ~RenderViewTest();

 protected:
  // Spins the message loop to process all messages that are currently pending.
  void ProcessPendingMessages();

  // Returns a pointer to the main frame.
  WebKit::WebFrame* GetMainFrame();

  // Executes the given JavaScript in the context of the main frame. The input
  // is a NULL-terminated UTF-8 string.
  void ExecuteJavaScript(const char* js);

  // Loads the given HTML into the main frame as a data: URL.
  void LoadHTML(const char* html);

  // Sends IPC messages that emulates a key-press event.
  int SendKeyEvent(MockKeyboard::Layout layout,
                   int key_code,
                   MockKeyboard::Modifiers key_modifiers,
                   std::wstring* output);

  // Sends one native key event over IPC.
  void SendNativeKeyEvent(const NativeWebKeyboardEvent& key_event);

  // testing::Test
  virtual void SetUp();

  virtual void TearDown();

  MessageLoop msg_loop_;
  MockRenderThread render_thread_;
  scoped_ptr<MockRenderProcess> mock_process_;
  scoped_refptr<RenderView> view_;
  RendererWebKitClientImpl webkitclient_;
  scoped_ptr<MockKeyboard> mock_keyboard_;

  // Used to setup the process so renderers can run.
  scoped_ptr<RendererMainPlatformDelegate> platform_;
  scoped_ptr<MainFunctionParams> params_;
  scoped_ptr<CommandLine> command_line_;
  scoped_ptr<SandboxInitWrapper> sandbox_init_wrapper_;
};

#endif  // CHROME_TEST_RENDER_VIEW_TEST_H_
