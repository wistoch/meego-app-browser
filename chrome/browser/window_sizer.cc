// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/window_sizer.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

// TODO(port): Port this to Linux.
// This requires creating window_sizer_linux.cc, creating a subclass
// of WindowSizer::MonitorInfoProvider, and providing implementations
// of GetDefaultMonitorInfoProvider() and GetDefaultPopupOrigin().

///////////////////////////////////////////////////////////////////////////////
// An implementation of WindowSizer::StateProvider that gets the last active
// and persistent state from the browser window and the user's profile.
class DefaultStateProvider : public WindowSizer::StateProvider {
 public:
  explicit DefaultStateProvider(const std::wstring& app_name, Browser* browser)
      : app_name_(app_name),
        browser_(browser) {
  }

  // Overridden from WindowSizer::StateProvider:
  virtual bool GetPersistentState(gfx::Rect* bounds, bool* maximized) const {
    DCHECK(bounds && maximized);

    std::wstring key(prefs::kBrowserWindowPlacement);
    if (!app_name_.empty()) {
      key.append(L"_");
      key.append(app_name_);
    }

    if (!g_browser_process->local_state())
      return false;

    const DictionaryValue* wp_pref =
        g_browser_process->local_state()->GetDictionary(key.c_str());
    int top = 0, left = 0, bottom = 0, right = 0;
    bool has_prefs =
        wp_pref &&
        wp_pref->GetInteger(L"top", &top) &&
        wp_pref->GetInteger(L"left", &left) &&
        wp_pref->GetInteger(L"bottom", &bottom) &&
        wp_pref->GetInteger(L"right", &right) &&
        wp_pref->GetBoolean(L"maximized", maximized);
    bounds->SetRect(left, top, std::max(0, right - left),
                    std::max(0, bottom - top));
    return has_prefs;
  }

  virtual bool GetLastActiveWindowState(gfx::Rect* bounds) const {
    // Applications are always restored with the same position.
    if (!app_name_.empty())
      return false;

    // If a reference browser is set, use its window. Otherwise find last
    // active.
    BrowserWindow* window = NULL;
    if (browser_) {
      window = browser_->window();
      DCHECK(window);
    } else {
      BrowserList::const_reverse_iterator it = BrowserList::begin_last_active();
      BrowserList::const_reverse_iterator end = BrowserList::end_last_active();
      for (; (it != end); ++it) {
        Browser* last_active = *it;
        if (last_active && last_active->type() == Browser::TYPE_NORMAL) {
          window = last_active->window();
          DCHECK(window);
          break;
        }
      }
    }

    if (window) {
      *bounds = window->GetNormalBounds();
      return true;
    }

    return false;
  }

 private:
  std::wstring app_name_;

  // If set, is used as the reference browser for GetLastActiveWindowState.
  Browser* browser_;
  DISALLOW_EVIL_CONSTRUCTORS(DefaultStateProvider);
};

///////////////////////////////////////////////////////////////////////////////
// WindowSizer, public:

WindowSizer::WindowSizer(
    StateProvider* state_provider,
    MonitorInfoProvider* monitor_info_provider) {
  Init(state_provider, monitor_info_provider);
}

WindowSizer::~WindowSizer() {
  if (state_provider_)
    delete state_provider_;
  if (monitor_info_provider_)
    delete monitor_info_provider_;
}

// static
void WindowSizer::GetBrowserWindowBounds(const std::wstring& app_name,
                                         const gfx::Rect& specified_bounds,
                                         Browser* browser,
                                         gfx::Rect* window_bounds,
                                         bool* maximized) {
  const WindowSizer sizer(new DefaultStateProvider(app_name, browser),
                          CreateDefaultMonitorInfoProvider());
  sizer.DetermineWindowBounds(specified_bounds, window_bounds, maximized);
}

///////////////////////////////////////////////////////////////////////////////
// WindowSizer, private:

WindowSizer::WindowSizer(const std::wstring& app_name) {
  Init(new DefaultStateProvider(app_name, NULL),
       CreateDefaultMonitorInfoProvider());
}

void WindowSizer::Init(StateProvider* state_provider,
                       MonitorInfoProvider* monitor_info_provider) {
  state_provider_ = state_provider;
  monitor_info_provider_ = monitor_info_provider;
}

void WindowSizer::DetermineWindowBounds(const gfx::Rect& specified_bounds,
                                        gfx::Rect* bounds,
                                        bool* maximized) const {
  *bounds = specified_bounds;
  if (bounds->IsEmpty()) {
    // See if there's saved placement information.
    if (!GetLastWindowBounds(bounds)) {
      if (!GetSavedWindowBounds(bounds, maximized)) {
        // No saved placement, figure out some sensible default size based on
        // the user's screen size.
        GetDefaultWindowBounds(bounds);
      }
    }
  }
}

bool WindowSizer::GetLastWindowBounds(gfx::Rect* bounds) const {
  DCHECK(bounds);
  if (!state_provider_ || !state_provider_->GetLastActiveWindowState(bounds))
    return false;
  gfx::Rect last_window_bounds = *bounds;
  bounds->Offset(kWindowTilePixels, kWindowTilePixels);
  AdjustBoundsToBeVisibleOnMonitorContaining(last_window_bounds, bounds);
  return true;
}

bool WindowSizer::GetSavedWindowBounds(gfx::Rect* bounds,
                                       bool* maximized) const {
  DCHECK(bounds && maximized);
  if (!state_provider_ ||
      !state_provider_->GetPersistentState(bounds, maximized))
    return false;
  const gfx::Point& taskbar_offset =
      monitor_info_provider_->GetBoundsOffsetMatching(*bounds);
  bounds->Offset(taskbar_offset.x(), taskbar_offset.y());
  AdjustBoundsToBeVisibleOnMonitorContaining(*bounds, bounds);
  return true;
}

void WindowSizer::GetDefaultWindowBounds(gfx::Rect* default_bounds) const {
  DCHECK(default_bounds);
  DCHECK(monitor_info_provider_);

  gfx::Rect work_area = monitor_info_provider_->GetPrimaryMonitorWorkArea();

  // The default size is either some reasonably wide width, or if the work
  // area is narrower, then the work area width less some aesthetic padding.
  int default_width = std::min(work_area.width() - 2 * kWindowTilePixels, 1050);
  int default_height = work_area.height() - 2 * kWindowTilePixels;

  // For wider aspect ratio displays at higher resolutions, we might size the
  // window narrower to allow two windows to easily be placed side-by-side.
  gfx::Rect screen_size = monitor_info_provider_->GetPrimaryMonitorBounds();
  double width_to_height =
    static_cast<double>(screen_size.width()) / screen_size.height();

  // The least wide a screen can be to qualify for the halving described above.
  static const int kMinScreenWidthForWindowHalving = 1600;
  // We assume 16:9/10 is a fairly standard indicator of a wide aspect ratio
  // computer display.
  if (((width_to_height * 10) >= 16) &&
      work_area.width() > kMinScreenWidthForWindowHalving) {
    // Halve the work area, subtracting aesthetic padding on either side, plus
    // some more aesthetic padding for spacing between windows.
    default_width = (work_area.width() / 2) - 3 * kWindowTilePixels;
  }
  default_bounds->SetRect(kWindowTilePixels + work_area.x(),
                          kWindowTilePixels + work_area.y(),
                          default_width, default_height);
}

bool WindowSizer::PositionIsOffscreen(int position, Edge edge) const {
  DCHECK(monitor_info_provider_);
  size_t monitor_count = monitor_info_provider_->GetMonitorCount();
  for (size_t i = 0; i < monitor_count; ++i) {
    gfx::Rect work_area = monitor_info_provider_->GetWorkAreaAt(i);
    switch (edge) {
      case TOP:
        if (position >= work_area.y())
          return false;
        break;
      case LEFT:
        if (position >= work_area.x())
          return false;
        break;
      case BOTTOM:
        if (position <= work_area.bottom())
          return false;
        break;
      case RIGHT:
        if (position <= work_area.right())
          return false;
        break;
    }
  }
  return true;
}

void WindowSizer::AdjustBoundsToBeVisibleOnMonitorContaining(
    const gfx::Rect& other_bounds, gfx::Rect* bounds) const {
  DCHECK(bounds);
  DCHECK(monitor_info_provider_);

  // Find the size of the work area of the monitor that intersects the bounds
  // of the anchor window.
  gfx::Rect work_area =
      monitor_info_provider_->GetMonitorWorkAreaMatching(other_bounds);

  // If height or width are 0, reset to the default size.
  gfx::Rect default_bounds;
  GetDefaultWindowBounds(&default_bounds);
  if (bounds->height() <= 0)
    bounds->set_height(default_bounds.height());
  if (bounds->width() <= 0)
    bounds->set_width(default_bounds.width());

  // First determine which screen edge(s) the window is offscreen on.
  monitor_info_provider_->UpdateWorkAreas();
  bool top_offscreen = PositionIsOffscreen(bounds->y(), TOP);
  bool left_offscreen = PositionIsOffscreen(bounds->x(), LEFT);
  bool bottom_offscreen = PositionIsOffscreen(bounds->bottom(), BOTTOM);
  bool right_offscreen = PositionIsOffscreen(bounds->right(), RIGHT);

  // Bump the window back onto the screen in the direction that it's offscreen.
  int min_x = work_area.x() + kWindowTilePixels;
  int min_y = work_area.y() + kWindowTilePixels;
  if (bottom_offscreen) {
    bounds->set_y(std::max(
        work_area.bottom() - kWindowTilePixels - bounds->height(), min_y));
  }
  if (right_offscreen) {
    bounds->set_x(std::max(
        work_area.right() - kWindowTilePixels - bounds->width(), min_x));
  }
  if (top_offscreen)
    bounds->set_y(min_y);
  if (left_offscreen)
    bounds->set_x(min_x);

  // Now that we've tried to correct the x/y position to something reasonable,
  // see if the window is still too tall or wide to fit, and resize it if need
  // be.
  if ((bottom_offscreen || top_offscreen) &&
      bounds->bottom() > work_area.bottom())
    bounds->set_height(work_area.height() - 2 * kWindowTilePixels);
  if ((left_offscreen || right_offscreen) &&
      bounds->right() > work_area.right())
    bounds->set_width(work_area.width() - 2 * kWindowTilePixels);
}
