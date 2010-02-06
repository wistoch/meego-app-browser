// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/side_tab_strip.h"

#include "app/gfx/canvas.h"
#include "base/command_line.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

namespace {
const int kVerticalTabSpacing = 2;
const int kTabStripWidth = 127;
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, public:

SideTabStrip::SideTabStrip() {
  SetID(VIEW_ID_TAB_STRIP);
}

SideTabStrip::~SideTabStrip() {
}

void SideTabStrip::SetModel(SideTabStripModel* model) {
  model_.reset(model);
}

// static
bool SideTabStrip::Available() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVerticalTabs);
}

// static
bool SideTabStrip::Visible(Profile* profile) {
  return Available() &&
      profile->GetPrefs()->GetBoolean(prefs::kUseVerticalTabs);
}

void SideTabStrip::AddTabAt(int index) {
  SideTab* tab = new SideTab(this);
  AddChildView(tab);
  Layout();
}

void SideTabStrip::RemoveTabAt(int index) {
  View* v = GetChildViewAt(index);
  RemoveChildView(v);
  delete v;
  Layout();
}

void SideTabStrip::SelectTabAt(int index) {
  GetChildViewAt(index)->SchedulePaint();
}

void SideTabStrip::UpdateTabAt(int index) {
  GetChildViewAt(index)->SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, SideTabModel implementation:

std::wstring SideTabStrip::GetTitle(SideTab* tab) const {
  return model_->GetTitle(GetIndexOfSideTab(tab));
}

SkBitmap SideTabStrip::GetIcon(SideTab* tab) const {
  return model_->GetIcon(GetIndexOfSideTab(tab));
}

bool SideTabStrip::IsSelected(SideTab* tab) const {
  return model_->IsSelected(GetIndexOfSideTab(tab));
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, BaseTabStrip implementation:

int SideTabStrip::GetPreferredHeight() {
  return 0;
}

void SideTabStrip::SetBackgroundOffset(const gfx::Point& offset) {
}

bool SideTabStrip::IsPositionInWindowCaption(const gfx::Point& point) {
  return true;
}

void SideTabStrip::SetDraggedTabBounds(int tab_index,
                                       const gfx::Rect& tab_bounds) {
}

bool SideTabStrip::IsDragSessionActive() const {
  return false;
}

void SideTabStrip::UpdateLoadingAnimations() {
}

bool SideTabStrip::IsAnimating() const {
  return false;
}

TabStrip* SideTabStrip::AsTabStrip() {
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, views::View overrides:

void SideTabStrip::Layout() {
  int y = 0;
  for (int c = GetChildViewCount(), i = 0; i < c; ++i) {
    views::View* child = GetChildViewAt(i);
    child->SetBounds(0, y, width(), child->GetPreferredSize().height());
    y = child->bounds().bottom() + kVerticalTabSpacing;
  }
}

void SideTabStrip::Paint(gfx::Canvas* canvas) {
  // canvas->FillRectInt(SK_ColorGREEN, 0, 0, width(), height());
}

gfx::Size SideTabStrip::GetPreferredSize() {
  return gfx::Size(kTabStripWidth, 0);
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, private:

int SideTabStrip::GetIndexOfSideTab(SideTab* tab) const {
  return GetChildIndex(tab);
}

