// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"

// Tests for the external select popup menu (Mac specific).

namespace {

const char* const kSelectID = "mySelect";

}  // namespace

class ExternalPopupMenuTest : public RenderViewTest {
 public:
  ExternalPopupMenuTest() {}

  virtual void SetUp() {
    RenderViewTest::SetUp();
    // We need to set this explictly as RenderMain is not run.
    WebKit::WebView::setUseExternalPopupMenus(true);

    std::string html = "<select id='mySelect' onchange='selectChanged(this)'>"
                       "  <option>zero</option>"
                       "  <option selected='1'>one</option>"
                       "  <option>two</option>"
                       "</select>";
    if (ShouldRemoveSelectOnChange()) {
      html += "<script>"
              "  function selectChanged(select) {"
              "    select.parentNode.removeChild(select);"
              "  }"
              "</script>";
    }

    // Load the test page.
    LoadHTML(html.c_str());

    // Set a minimum size and give focus so simulated events work.
    view_->webwidget()->resize(WebKit::WebSize(500, 500));
    view_->webwidget()->setFocus(true);
  }

  int GetSelectedIndex() {
    string16 script(ASCIIToUTF16(kSelectID));
    script.append(ASCIIToUTF16(".selectedIndex"));
    int selected_index = -1;
    ExecuteJavaScriptAndReturnIntValue(script, &selected_index);
    return selected_index;
  }

 protected:
  virtual bool ShouldRemoveSelectOnChange() const { return false; }

  DISALLOW_COPY_AND_ASSIGN(ExternalPopupMenuTest);
};

// Normal case: test showing a select popup, canceling/selecting an item.
TEST_F(ExternalPopupMenuTest, NormalCase) {
  IPC::TestSink& sink = render_thread_.sink();

  // Click the text field once.
  EXPECT_TRUE(SimulateElementClick(kSelectID));

  // We should have sent a message to the browser to show the popup menu.
  const IPC::Message* message =
      sink.GetUniqueMessageMatching(ViewHostMsg_ShowPopup::ID);
  ASSERT_TRUE(message != NULL);
  Tuple1<ViewHostMsg_ShowPopup_Params> param;
  ViewHostMsg_ShowPopup::Read(message, &param);
  ASSERT_EQ(3U, param.a.popup_items.size());
  EXPECT_EQ(1, param.a.selected_item);

  // Simulate the user canceling the popup, the index should not have changed.
  view_->OnSelectPopupMenuItem(-1);
  EXPECT_EQ(1, GetSelectedIndex());

  // Show the pop-up again and this time make a selection.
  EXPECT_TRUE(SimulateElementClick(kSelectID));
  view_->OnSelectPopupMenuItem(0);
  EXPECT_EQ(0, GetSelectedIndex());

  // Show the pop-up again and make another selection.
  sink.ClearMessages();
  EXPECT_TRUE(SimulateElementClick(kSelectID));
  message = sink.GetUniqueMessageMatching(ViewHostMsg_ShowPopup::ID);
  ASSERT_TRUE(message != NULL);
  ViewHostMsg_ShowPopup::Read(message, &param);
  ASSERT_EQ(3U, param.a.popup_items.size());
  EXPECT_EQ(0, param.a.selected_item);
}

// Page shows popup, then navigates away while popup showing, then select.
TEST_F(ExternalPopupMenuTest, ShowPopupThenNavigate) {
  // Click the text field once.
  EXPECT_TRUE(SimulateElementClick(kSelectID));

  // Now we navigate to another pager.
  LoadHTML("<blink>Awesome page!</blink>");

  // Now the user selects something, we should not crash.
  view_->OnSelectPopupMenuItem(-1);
}

class ExternalPopupMenuRemoveTest : public ExternalPopupMenuTest {
 public:
  ExternalPopupMenuRemoveTest() {}

 protected:
  virtual bool ShouldRemoveSelectOnChange() const { return true; }
};

// Tests that nothing bad happen when the page removes the select when it
// changes. (http://crbug.com/61997)
TEST_F(ExternalPopupMenuRemoveTest, RemoveOnChange) {
  // Click the text field once to show the popup.
  EXPECT_TRUE(SimulateElementClick(kSelectID));

  // Select something, it causes the select to be removed from the page.
  view_->OnSelectPopupMenuItem(0);

  // Just to check the soundness of the test, call SimulateElementClick again.
  // It should return false as the select has been removed.
  EXPECT_FALSE(SimulateElementClick(kSelectID));
}
