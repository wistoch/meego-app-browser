// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WIN_UTIL_H__
#define CHROME_COMMON_WIN_UTIL_H__

#include <objbase.h>

#include <string>
#include <vector>

#include "base/fix_wp64.h"
#include "base/gfx/rect.h"
#include "base/scoped_handle.h"

namespace win_util {

// Import ScopedHandle and friends into this namespace for backwards
// compatibility.  TODO(darin): clean this up!
using ::ScopedHandle;
using ::ScopedFindFileHandle;
using ::ScopedHDC;
using ::ScopedBitmap;
using ::ScopedHRGN;

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

  T** operator&() {
    return &mem_ptr_;
  }

  operator T*() {
    return mem_ptr_;
  }

 private:
  T* mem_ptr_;

  DISALLOW_EVIL_CONSTRUCTORS(CoMemReleaser);
};

// Initializes COM in the constructor, and uninitializes COM in the
// destructor.
class ScopedCOMInitializer {
 public:
  ScopedCOMInitializer() {
    CoInitialize(NULL);
  }

  ~ScopedCOMInitializer() {
    CoUninitialize();
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ScopedCOMInitializer);
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

// Use the Win32 API FormatMessage() function to generate a string, using
// Windows's default Message Compiled resources; ignoring the inserts.
std::wstring FormatMessage(unsigned messageid);

// Uses the last Win32 error to generate a human readable message string.
std::wstring FormatLastWin32Error();

// Open a Windows explorer window with the specified file highlighted.
void ShowItemInFolder(const std::wstring& full_path);

// Open or run a file via the Windows shell. In the event that there is no
// default application registered for the file specified by 'full_path',
// ask the user, via the Windows "Open With" dialog, for an application to use
// if 'ask_for_app' is true.
// Returns 'true' on successful open, 'false' otherwise.
bool OpenItemViaShell(const std::wstring& full_path, bool ask_for_app);

// The download manager now writes the alternate data stream with the
// zone on all downloads. This function is equivalent to OpenItemViaShell
// without showing the zone warning dialog.
bool OpenItemViaShellNoZoneCheck(const std::wstring& full_path,
                                 bool ask_for_app);

// Ask the user, via the Windows "Open With" dialog, for an application to use
// to open the file specified by 'full_path'.
// Returns 'true' on successful open, 'false' otherwise.
bool OpenItemWithExternalApp(const std::wstring& full_path);

// Prompt the user for location to save a file. 'suggested_name' is a full path
// that gives the dialog box a hint as to how to initialize itself.
// For example, a 'suggested_name' of:
//   "C:\Documents and Settings\jojo\My Documents\picture.png"
// will start the dialog in the "C:\Documents and Settings\jojo\My Documents\"
// directory, and filter for .png file types.
// 'owner' is the window to which the dialog box is modal, NULL for a modeless
// dialog box.
// On success,  returns true and 'final_name' contains the full path of the file
// that the user chose. On error, returns false, and 'final_name' is not
// modified.
// NOTE: DO NOT CALL THIS FUNCTION DIRECTLY! Instead use the helper objects in
//       browser/shell_dialogs.cc to do this asynchronously on a different
//       thread so that the app isn't jankified if the Windows shell dialog
//       takes a long time to display.
bool SaveFileAs(HWND owner,
                const std::wstring& suggested_name,
                std::wstring* final_name);

// Prompt the user for location to save a file.
// Callers should provide the filter string, and also a filter index.
// The parameter |index| indicates the initial index of filter description
// and filter pattern for the dialog box. If |index| is zero or greater than
// the number of total filter types, the system uses the first filter in the
// |filter| buffer. The parameter |final_name| returns the file name which
// contains the drive designator, path, file name, and extension of the user
// selected file name.
bool SaveFileAsWithFilter(HWND owner,
                          const std::wstring& suggested_name,
                          const wchar_t* filter,
                          const std::wstring& def_ext,
                          unsigned* index,
                          std::wstring* final_name);

// If the window does not fit on the default monitor, it is moved and possibly
// resized appropriately.
void AdjustWindowToFit(HWND hwnd);

// Sizes the window to have a client or window size (depending on the value of
// |pref_is_client|) of pref, then centers the window over parent, ensuring the
// window fits on screen.
void CenterAndSizeWindow(HWND parent, HWND window, const SIZE& pref,
                         bool pref_is_client);

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
// keypad (with or without Num Lock on).  |extended_key| should be set to the
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

}  // namespace win_util

#endif  // WIN_COMMON_WIN_UTIL_H__

