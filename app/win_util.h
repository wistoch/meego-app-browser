// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_WIN_UTIL_H_
#define APP_WIN_UTIL_H_
#pragma once

#include <objbase.h>

#include <string>
#include <vector>

#include "base/fix_wp64.h"
#include "base/scoped_handle.h"
#include "gfx/font.h"
#include "gfx/rect.h"

class FilePath;

namespace win_util {

// Import ScopedHandle and friends into this namespace for backwards
// compatibility.  TODO(darin): clean this up!
using ::ScopedHandle;
using ::ScopedHDC;
using ::ScopedBitmap;

// Simple scoped memory releaser class for COM allocated memory.
// Example:
//   CoMemReleaser<ITEMIDLIST> file_item;
//   SHGetSomeInfo(&file_item, ...);
//   ...
//   return;  <-- memory released
template<typename T>
class CoMemReleaser {
 public:
  explicit CoMemReleaser() : mem_ptr_(NULL) {}

  ~CoMemReleaser() {
    if (mem_ptr_)
      CoTaskMemFree(mem_ptr_);
  }

  T** operator&() {  // NOLINT
    return &mem_ptr_;
  }

  operator T*() {
    return mem_ptr_;
  }

 private:
  T* mem_ptr_;

  DISALLOW_COPY_AND_ASSIGN(CoMemReleaser);
};

// Initializes COM in the constructor (STA), and uninitializes COM in the
// destructor.
class ScopedCOMInitializer {
 public:
  ScopedCOMInitializer() : hr_(CoInitialize(NULL)) {
  }

  ScopedCOMInitializer::~ScopedCOMInitializer() {
    if (SUCCEEDED(hr_))
      CoUninitialize();
  }

  // Returns the error code from CoInitialize(NULL)
  // (called in constructor)
  inline HRESULT error_code() const {
    return hr_;
  }

 protected:
  HRESULT hr_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedCOMInitializer);
};

// Creates a string interpretation of the time of day represented by the given
// SYSTEMTIME that's appropriate for the user's default locale.
// Format can be an empty string (for the default format), or a "format picture"
// as specified in the Windows documentation for GetTimeFormat().
std::wstring FormatSystemTime(const SYSTEMTIME& time,
                              const std::wstring& format);

// Creates a string interpretation of the date represented by the given
// SYSTEMTIME that's appropriate for the user's default locale.
// Format can be an empty string (for the default format), or a "format picture"
// as specified in the Windows documentation for GetDateFormat().
std::wstring FormatSystemDate(const SYSTEMTIME& date,
                              const std::wstring& format);

// Returns the long path name given a short path name. A short path name
// is a path that follows the 8.3 convention and has ~x in it. If the
// path is already a long path name, the function returns the current
// path without modification.
bool ConvertToLongPath(const std::wstring& short_path, std::wstring* long_path);

// Returns true if the current point is close enough to the origin point in
// space and time that it would be considered a double click.
bool IsDoubleClick(const POINT& origin,
                   const POINT& current,
                   DWORD elapsed_time);

// Returns true if the current point is far enough from the origin that it
// would be considered a drag.
bool IsDrag(const POINT& origin, const POINT& current);

// Returns true if we are on Windows Vista and composition is enabled
bool ShouldUseVistaFrame();

// Open or run a file via the Windows shell. In the event that there is no
// default application registered for the file specified by 'full_path',
// ask the user, via the Windows "Open With" dialog.
// Returns 'true' on successful open, 'false' otherwise.
bool OpenItemViaShell(const FilePath& full_path);

// The download manager now writes the alternate data stream with the
// zone on all downloads. This function is equivalent to OpenItemViaShell
// without showing the zone warning dialog.
bool OpenItemViaShellNoZoneCheck(const FilePath& full_path);

// Ask the user, via the Windows "Open With" dialog, for an application to use
// to open the file specified by 'full_path'.
// Returns 'true' on successful open, 'false' otherwise.
bool OpenItemWithExternalApp(const std::wstring& full_path);

// If the window does not fit on the default monitor, it is moved and possibly
// resized appropriately.
void AdjustWindowToFit(HWND hwnd);

// Sizes the window to have a client or window size (depending on the value of
// |pref_is_client|) of pref, then centers the window over parent, ensuring the
// window fits on screen.
void CenterAndSizeWindow(HWND parent, HWND window, const SIZE& pref,
                         bool pref_is_client);

// Returns true if edge |edge| (one of ABE_LEFT, TOP, RIGHT, or BOTTOM) of
// monitor |monitor| has an auto-hiding taskbar that's always-on-top.
bool EdgeHasTopmostAutoHideTaskbar(UINT edge, HMONITOR monitor);

// Duplicates a section handle from another process to the current process.
// Returns the new valid handle if the function succeed. NULL otherwise.
HANDLE GetSectionFromProcess(HANDLE section, HANDLE process, bool read_only);

// Returns true if the specified window is the current active top window or one
// of its children.
bool DoesWindowBelongToActiveWindow(HWND window);

// Adjusts the value of |child_rect| if necessary to ensure that it is
// completely visible within |parent_rect|.
void EnsureRectIsVisibleInRect(const gfx::Rect& parent_rect,
                               gfx::Rect* child_rect,
                               int padding);

// Ensures that the child window stays within the boundaries of the parent
// before setting its bounds. If |parent_window| is NULL, the bounds of the
// parent are assumed to be the bounds of the monitor that |child_window| is
// nearest to. If |child_window| isn't visible yet and |insert_after_window|
// is non-NULL and visible, the monitor |insert_after_window| is on is used
// as the parent bounds instead.
void SetChildBounds(HWND child_window, HWND parent_window,
                    HWND insert_after_window, const gfx::Rect& bounds,
                    int padding, unsigned long flags);

// Returns the bounds for the monitor that contains the largest area of
// intersection with the specified rectangle.
gfx::Rect GetMonitorBoundsForRect(const gfx::Rect& rect);

// Returns true if the virtual key code is a digit coming from the numeric
// keypad (with or without NumLock on).  |extended_key| should be set to the
// extended key flag specified in the WM_KEYDOWN/UP where the |key_code|
// originated.
bool IsNumPadDigit(int key_code, bool extended_key);

// Grabs a snapshot of the designated window and stores a PNG representation
// into a byte vector.
void GrabWindowSnapshot(HWND window_handle,
                        std::vector<unsigned char>* png_representation);

// Returns whether the specified window is the current active window.
bool IsWindowActive(HWND hwnd);

// Returns whether the specified file name is a reserved name on windows.
// This includes names like "com2.zip" (which correspond to devices) and
// desktop.ini and thumbs.db which have special meaning to the windows shell.
bool IsReservedName(const std::wstring& filename);

// Returns whether the specified extension is automatically integrated into the
// windows shell.
bool IsShellIntegratedExtension(const std::wstring& eextension);

// A wrapper around Windows' MessageBox function. Using a Chrome specific
// MessageBox function allows us to control certain RTL locale flags so that
// callers don't have to worry about adding these flags when running in a
// right-to-left locale.
int MessageBox(HWND hwnd,
               const std::wstring& text,
               const std::wstring& caption,
               UINT flags);

// Returns the system set window title font.
gfx::Font GetWindowTitleFont();

// The thickness of an auto-hide taskbar in pixels.
extern const int kAutoHideTaskbarThicknessPx;

// Sets the application id given as the Application Model ID for the window
// specified.  This method is used to insure that different web applications
// do not group together on the Win7 task bar.
void SetAppIdForWindow(const std::wstring& app_id, HWND hwnd);

}  // namespace win_util

#endif  // APP_WIN_UTIL_H_
