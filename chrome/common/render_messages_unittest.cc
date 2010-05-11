// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(RenderMessagesUnittest, WebAccessibility) {
  // Test a simple case.
  webkit_glue::WebAccessibility input;
  input.id = 123;
  input.name = ASCIIToUTF16("name");
  input.value = ASCIIToUTF16("value");
  input.action = ASCIIToUTF16("action");
  input.description = ASCIIToUTF16("description");
  input.help = ASCIIToUTF16("help");
  input.shortcut = ASCIIToUTF16("shortcut");
  input.role = webkit_glue::WebAccessibility::ROLE_CHECKBUTTON;
  input.state =
      (1 << webkit_glue::WebAccessibility::STATE_CHECKED) |
      (1 << webkit_glue::WebAccessibility::STATE_FOCUSED);
  input.location = WebKit::WebRect(11, 22, 333, 444);

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  webkit_glue::WebAccessibility output;
  void* iter = NULL;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));
  EXPECT_EQ(input.id, output.id);
  EXPECT_EQ(input.name, output.name);
  EXPECT_EQ(input.value, output.value);
  EXPECT_EQ(input.action, output.action);
  EXPECT_EQ(input.description, output.description);
  EXPECT_EQ(input.help, output.help);
  EXPECT_EQ(input.shortcut, output.shortcut);
  EXPECT_EQ(input.role, output.role);
  EXPECT_EQ(input.state, output.state);
  EXPECT_EQ(input.location, output.location);
  EXPECT_EQ(input.children.size(), output.children.size());

  // Test a corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  bad_msg.WriteInt(99);
  iter = NULL;
  EXPECT_FALSE(IPC::ReadParam(&bad_msg, &iter, &output));

  // Test a recursive case.
  webkit_glue::WebAccessibility outer;
  outer.id = 1000;
  outer.name = ASCIIToUTF16("outer_name");
  outer.role = webkit_glue::WebAccessibility::ROLE_GROUPING;
  outer.state = 0;
  outer.location = WebKit::WebRect(0, 0, 1000, 1000);
  webkit_glue::WebAccessibility inner1;
  inner1.id = 1001;
  inner1.name = ASCIIToUTF16("inner1_name");
  inner1.role = webkit_glue::WebAccessibility::ROLE_RADIOBUTTON;
  inner1.state =
      (1 << webkit_glue::WebAccessibility::STATE_CHECKED) |
      (1 << webkit_glue::WebAccessibility::STATE_FOCUSED);
  inner1.location = WebKit::WebRect(10, 10, 900, 400);
  outer.children.push_back(inner1);
  webkit_glue::WebAccessibility inner2;
  inner2.id = 1002;
  inner2.name = ASCIIToUTF16("inner2_name");
  inner2.role = webkit_glue::WebAccessibility::ROLE_RADIOBUTTON;
  inner2.state = (1 << webkit_glue::WebAccessibility::STATE_CHECKED);
  inner2.location = WebKit::WebRect(10, 500, 900, 400);
  outer.children.push_back(inner2);

  IPC::Message msg2(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg2, outer);

  void* iter2 = NULL;
  EXPECT_TRUE(IPC::ReadParam(&msg2, &iter2, &output));
  EXPECT_EQ(outer.id, output.id);
  EXPECT_EQ(outer.name, output.name);
  EXPECT_EQ(outer.role, output.role);
  EXPECT_EQ(outer.state, output.state);
  EXPECT_EQ(outer.location, output.location);
  EXPECT_EQ(outer.children.size(), output.children.size());
  EXPECT_EQ(inner1.id, output.children[0].id);
  EXPECT_EQ(inner1.name, output.children[0].name);
  EXPECT_EQ(inner1.role, output.children[0].role);
  EXPECT_EQ(inner1.state, output.children[0].state);
  EXPECT_EQ(inner1.location, output.children[0].location);
  EXPECT_EQ(inner2.id, output.children[1].id);
  EXPECT_EQ(inner2.name, output.children[1].name);
  EXPECT_EQ(inner2.role, output.children[1].role);
  EXPECT_EQ(inner2.state, output.children[1].state);
  EXPECT_EQ(inner2.location, output.children[1].location);
}
