// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests common functionality used by the Chrome Extensions webNavigation API
// implementation.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/testing_profile.h"

class FrameNavigationStateTest : public RenderViewHostTestHarness {
};

// Test that a frame is correctly tracked, and removed once the tab contents
// goes away.
TEST_F(FrameNavigationStateTest, TrackFrame) {
  FrameNavigationState navigation_state;
  const long long frame_id1 = 23;
  const long long frame_id2 = 42;
  const GURL url("http://www.google.com/");

  // Create a main frame.
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id1));
  navigation_state.TrackFrame(frame_id1, url, true, contents());
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));

  // Add a sub frame.
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id2));
  navigation_state.TrackFrame(frame_id2, url, false, contents());
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id2));

  // Removing the tab contents should also remove all state of its frames.
  navigation_state.RemoveTabContentsState(contents());
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id2));
}

// Test that no events can be sent for a frame after an error occurred, but
// before a new navigation happened in this frame.
TEST_F(FrameNavigationStateTest, ErrorState) {
  FrameNavigationState navigation_state;
  TestTabContents* tab_contents =
      new TestTabContents(profile(), contents()->GetSiteInstance());
  const long long frame_id = 42;
  const GURL url("http://www.google.com/");

  navigation_state.TrackFrame(frame_id, url, true, tab_contents);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id));

  // After an error occurred, no further events should be sent.
  navigation_state.ErrorOccurredInFrame(frame_id);
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id));

  // Navigations to the "unreachable web data" URL should be ignored.
  navigation_state.TrackFrame(
      frame_id, GURL(chrome::kUnreachableWebDataURL), true, tab_contents);
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id));

  // However, when the frame navigates again, it should send events again.
  navigation_state.TrackFrame(frame_id, url, true, tab_contents);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id));
}

// Tests that for a sub frame, no events are send after an error occurred, but
// before a new navigation happened in this frame.
TEST_F(FrameNavigationStateTest, ErrorStateFrame) {
  FrameNavigationState navigation_state;
  TestTabContents* tab_contents =
      new TestTabContents(profile(), contents()->GetSiteInstance());
  const long long frame_id1 = 23;
  const long long frame_id2 = 42;
  const GURL url("http://www.google.com/");

  navigation_state.TrackFrame(frame_id1, url, true, tab_contents);
  navigation_state.TrackFrame(frame_id2, url, false, tab_contents);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id2));

  // After an error occurred, no further events should be sent.
  navigation_state.ErrorOccurredInFrame(frame_id2);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id2));

  // Navigations to the "unreachable web data" URL should be ignored.
  navigation_state.TrackFrame(
      frame_id2, GURL(chrome::kUnreachableWebDataURL), false, tab_contents);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_FALSE(navigation_state.CanSendEvents(frame_id2));

  // However, when the frame navigates again, it should send events again.
  navigation_state.TrackFrame(frame_id2, url, false, tab_contents);
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id1));
  EXPECT_TRUE(navigation_state.CanSendEvents(frame_id2));
}
