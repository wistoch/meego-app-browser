// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_WIN_UTIL_H__
#define BASE_WIN_UTIL_H__

#include <windows.h>
#include <aclapi.h>

#include <string>

namespace win_util {

// NOTE: Keep these in order so callers can do things like
// "if (GetWinVersion() > WINVERSION_2000) ...".  It's OK to change the values,
// though.
enum WinVersion {
  WINVERSION_PRE_2000 = 0,  // Not supported
  WINVERSION_2000 = 1,
  WINVERSION_XP = 2,
  WINVERSION_SERVER_2003 = 3,
  WINVERSION_VISTA = 4,
};

void GetNonClientMetrics(NONCLIENTMETRICS* metrics);

// Returns the running version of Windows.
WinVersion GetWinVersion();

// Adds an ACE in the DACL of the object referenced by handle. The ACE is
// granting |access| to the user |known_sid|.
// If |known_sid| is WinSelfSid, the sid of the current user will be added to
// the DACL.
bool AddAccessToKernelObject(HANDLE handle, WELL_KNOWN_SID_TYPE known_sid,
                             ACCESS_MASK access);

// Returns the string representing the current user sid.
bool GetUserSidString(std::wstring* user_sid);

// Creates a security descriptor with a DACL that has one ace giving full
// access to the current logon session.
// The security descriptor returned must be freed using LocalFree.
// The function returns true if it succeeds, false otherwise.
bool GetLogonSessionOnlyDACL(SECURITY_DESCRIPTOR** security_descriptor);

// Useful for subclassing a HWND.  Returns the previous window procedure.
WNDPROC SetWindowProc(HWND hwnd, WNDPROC wndproc);

// Subclasses a window, replacing its existing window procedure with the
// specified one. Returns true if the current window procedure was replaced,
// false if the window has already been subclassed with the specified
// subclass procedure.
bool Subclass(HWND window, WNDPROC subclass_proc);

// Unsubclasses a window subclassed using Subclass. Returns true if
// the window was subclassed with the specified |subclass_proc| and the window
// was successfully unsubclassed, false if the window's window procedure is not
// |subclass_proc|.
bool Unsubclass(HWND window, WNDPROC subclass_proc);

// Retrieves the original WNDPROC of a window subclassed using
// SubclassWindow.
WNDPROC GetSuperclassWNDPROC(HWND window);

// Pointer-friendly wrappers around Get/SetWindowLong(..., GWLP_USERDATA, ...)
// Returns the previously set value.
void* SetWindowUserData(HWND hwnd, void* user_data);
void* GetWindowUserData(HWND hwnd);

// Returns true if the shift key is currently pressed.
bool IsShiftPressed();

// Returns true if the ctrl key is currently pressed.
bool IsCtrlPressed();

// Returns true if the alt key is currently pressed.
bool IsAltPressed();

// A version of the GetClassNameW API that returns the class name in an
// std::wstring.  An empty result indicates a failure to get the class name.
std::wstring GetClassName(HWND window);

// Returns false if the computer is running Vista and the user account control
// is disabled. Returns true if user account control is enabled or the machine
// is not running vista.
bool UserAccountControlIsEnabled();

}  // namespace win_util

#endif  // BASE_WIN_UTIL_H__
