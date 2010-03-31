// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/controls/progress_bar.h"

namespace views {

TEST(ProgressBarTest, ProgressProperty) {
  ProgressBar bar;
  bar.SetProgress(-1);
  int progress = bar.GetProgress();
  EXPECT_EQ(0, progress);
  bar.SetProgress(300);
  progress = bar.GetProgress();
  EXPECT_EQ(100, progress);
  bar.SetProgress(62);
  progress = bar.GetProgress();
  EXPECT_EQ(62, progress);
}

TEST(ProgressBarTest, AddProgressMethod) {
  ProgressBar bar;
  bar.SetProgress(10);
  bar.AddProgress(22);
  int progress = bar.GetProgress();
  EXPECT_EQ(32, progress);
  bar.AddProgress(200);
  progress = bar.GetProgress();
  EXPECT_EQ(100, progress);
}

TEST(ProgressBarTest, TooltipTextProperty) {
  ProgressBar bar;
  std::wstring tooltip = L"Some text";
  EXPECT_FALSE(bar.GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(L"", tooltip);
  std::wstring tooltip_text = L"My progress";
  bar.SetTooltipText(tooltip_text);
  EXPECT_TRUE(bar.GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(tooltip_text, tooltip);
}

TEST(ProgressBarTest, Accessibility) {
  ProgressBar bar;
  bar.SetProgress(62);

  AccessibilityTypes::Role role;
  EXPECT_TRUE(bar.GetAccessibleRole(&role));
  EXPECT_EQ(AccessibilityTypes::ROLE_TEXT, role);

  std::wstring name;
  EXPECT_FALSE(bar.GetAccessibleName(&name));
  EXPECT_EQ(std::wstring(), name);
  std::wstring accessible_name = L"My progress bar";
  bar.SetAccessibleName(accessible_name);
  EXPECT_TRUE(bar.GetAccessibleName(&name));
  EXPECT_EQ(accessible_name, name);


  AccessibilityTypes::State state;
  EXPECT_TRUE(bar.GetAccessibleState(&state));
  EXPECT_EQ(AccessibilityTypes::STATE_READONLY, state);
}

}  // namespace views
