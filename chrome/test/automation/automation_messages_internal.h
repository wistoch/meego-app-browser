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

// Defines the IPC messages used by the automation interface.

// This header is meant to be included in multiple passes, hence no traditional
// header guard.
// See ipc_message_macros.h for explanation of the macros and passes.

#include <string>
#include <vector>

#include "chrome/common/ipc_message_macros.h"
#include "chrome/common/navigation_types.h"
#include "chrome/test/automation/autocomplete_edit_proxy.h"

// NOTE: All IPC messages have either a routing_id of 0 (for asynchronous
//       messages), or one that's been assigned by the proxy (for calls
//       which expect a response).  The routing_id shouldn't be used for
//       any other purpose in these message types.

// NOTE: All the new IPC messages should go at the end (before IPC_END_MESSAGES)
//       The IPC message IDs are part of an enum and hence the value
//       assumed to be constant across the builds may change.
//       The messages AutomationMsg_WindowHWND* in particular should not change
//       since the PageCyclerReferenceTest depend on the correctness of the
//       the message IDs across the builds.

// By using a start value of 0 for automation messages, we keep backward
// compatability with old builds.
IPC_BEGIN_MESSAGES(Automation, 0)

  // This message is fired when the AutomationProvider is up and running
  // in the app (the app is not fully up at this point).
  IPC_MESSAGE_ROUTED0(AutomationMsg_Hello)

  // This message is fired when the initial tab(s) are finished loading.
  IPC_MESSAGE_ROUTED0(AutomationMsg_InitialLoadsComplete)

  // This message notifies the AutomationProvider to append a new tab the window
  // with the given handle. The response contains the index of the new tab, or
  // -1 if the request failed.
  // The second parameter is the url to be loaded in the new tab.
  IPC_MESSAGE_ROUTED2(AutomationMsg_AppendTabRequest, int, GURL)
  IPC_MESSAGE_ROUTED1(AutomationMsg_AppendTabResponse, int)

  // This message requests the (zero-based) index for the currently
  // active tab in the window with the given handle. The response contains
  // the index of the active tab, or -1 if the request failed.
  IPC_MESSAGE_ROUTED1(AutomationMsg_ActiveTabIndexRequest, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_ActiveTabIndexResponse, int)

  // This message notifies the AutomationProvider to active the tab.
  // The first parameter is the handle to window resource.
  // The second parameter is the (zero-based) index to be activated
  IPC_MESSAGE_ROUTED2(AutomationMsg_ActivateTabRequest, int, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_ActivateTabResponse, int)

  // This message requests the cookie value for given url in the
  // profile of the tab identified by the second parameter.  The first
  // parameter is the URL string. The response contains the length of the cookie
  // value string. On failure, this length = -1.
  IPC_MESSAGE_ROUTED2(AutomationMsg_GetCookiesRequest, GURL, int)
  IPC_MESSAGE_ROUTED2(AutomationMsg_GetCookiesResponse, int, std::string)

  // This message notifies the AutomationProvider to set and broadcast a cookie
  // with given name and value for the given url in the profile of the tab
  // identified by the third parameter. The first parameter is the URL
  // string, and the second parameter is the cookie name and value to be set.
  // The response returns a non-negative value on success.
  IPC_MESSAGE_ROUTED3(AutomationMsg_SetCookieRequest, GURL, std::string, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_SetCookieResponse, int)

  // This message notifies the AutomationProvider to navigate to a specified url
  // in the tab with given handle. The first parameter is the handle to the tab
  // resource. The second parameter is the target url.  The response contains a
  // status code which is nonnegative on success.
  IPC_MESSAGE_ROUTED2(AutomationMsg_NavigateToURLRequest, int, GURL)
  IPC_MESSAGE_ROUTED1(AutomationMsg_NavigateToURLResponse,
                      int)  // see AutomationMsg_NavigationResponseValues

  // This message is used to implement the asynchronous version of
  // NavigateToURL.
  IPC_MESSAGE_ROUTED2(AutomationMsg_NavigationAsyncRequest,
                      int /* tab handle */,
                      GURL)
  IPC_MESSAGE_ROUTED1(AutomationMsg_NavigationAsyncResponse,
                      bool /* error value */)

  // This message notifies the AutomationProvider to navigate back in session
  // history in the tab with given handle. The first parameter is the handle
  // to the tab resource.  The response contains a status code which is
  // nonnegative on success.
  IPC_MESSAGE_ROUTED1(AutomationMsg_GoBackRequest, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_GoBackResponse,
                      int)  // see AutomationMsg_NavigationResponseValues

  // This message notifies the AutomationProvider to navigate forward in session
  // history in the tab with given handle. The first parameter is the handle
  // to the tab resource.  The response contains a status code which is
  // nonnegative on success.
  IPC_MESSAGE_ROUTED1(AutomationMsg_GoForwardRequest, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_GoForwardResponse,
                      int)  // see AutomationMsg_NavigationResponseValues

  // This message requests the number of browser windows that the app currently
  // has open.  The parameter in the response is the number of windows.
  IPC_MESSAGE_ROUTED0(AutomationMsg_BrowserWindowCountRequest)
  IPC_MESSAGE_ROUTED1(AutomationMsg_BrowserWindowCountResponse, int)

  // This message requests the handle (int64 app-unique identifier) of the
  // window with the given (zero-based) index.  On error, the returned handle
  // value is 0.
  IPC_MESSAGE_ROUTED1(AutomationMsg_BrowserWindowRequest, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_BrowserWindowResponse, int)

  // This message requests the number of tabs in the window with the given
  // handle.  The response contains the number of tabs, or -1 if the request
  // failed.
  IPC_MESSAGE_ROUTED1(AutomationMsg_TabCountRequest, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_TabCountResponse, int)

  // This message requests the handle of the tab with the given (zero-based)
  // index in the given app window. First parameter specifies the given window
  // handle, second specifies the given tab_index. On error, the returned handle
  // value is 0.
  IPC_MESSAGE_ROUTED2(AutomationMsg_TabRequest, int, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_TabResponse, int)

  // This message requests the the title of the tab with the given handle.
  // The response contains the size of the title string. On error, this value
  // should be -1 and empty string. Note that the title can be empty in which
  // case the size would be 0.
  IPC_MESSAGE_ROUTED1(AutomationMsg_TabTitleRequest, int)
  IPC_MESSAGE_ROUTED2(AutomationMsg_TabTitleResponse, int, std::wstring)

  // This message requests the url of the tab with the given handle.
  // The response contains a success flag and the URL string. The URL will
  // be empty on failure, and it still may be empty on success.
  IPC_MESSAGE_ROUTED1(AutomationMsg_TabURLRequest,
                      int /* tab handle */)
  IPC_MESSAGE_ROUTED2(AutomationMsg_TabURLResponse,
                      bool /* success flag*/,
                      GURL)

  // This message requests the HWND of the top-level window that corresponds
  // to the given automation handle.
  // The response contains the HWND value, which is 0 if the call fails.
  IPC_MESSAGE_ROUTED1(AutomationMsg_WindowHWNDRequest,
                      int /* automation handle */)
  IPC_MESSAGE_ROUTED1(AutomationMsg_WindowHWNDResponse,
                      HWND /* Win32 handle */)

  // This message notifies the AutomationProxy that a handle that it has
  // previously been given is now invalid.  (For instance, if the handle
  // represented a window which has now been closed.)  The parameter
  // value is the handle.
  IPC_MESSAGE_ROUTED1(AutomationMsg_InvalidateHandle, int)

  // This message notifies the AutomationProvider that a handle is no
  // longer being used, so it can stop paying attention to the
  // associated resource.  The parameter value is the handle.
  IPC_MESSAGE_ROUTED1(AutomationMsg_HandleUnused, int)

  // This message requests the HWND of the tab that corresponds
  // to the given automation handle.
  // The response contains the HWND value, which is 0 if the call fails.
  IPC_MESSAGE_ROUTED1(AutomationMsg_TabHWNDRequest,
                      int /* tab_handle */)
  IPC_MESSAGE_ROUTED1(AutomationMsg_TabHWNDResponse,
                      HWND /* win32 Window Handle*/)

  // This message tells the AutomationProvider to provide the given
  // authentication data to the specified tab, in response to an HTTP/FTP
  // authentication challenge.
  // The response status will be negative on error.
  IPC_MESSAGE_ROUTED3(AutomationMsg_SetAuthRequest,
                      int,  // tab handle
                      std::wstring,  // username
                      std::wstring)  // password
  IPC_MESSAGE_ROUTED1(AutomationMsg_SetAuthResponse,
                      int)  // status

  // This message tells the AutomationProvider to cancel the login in the
  // specified tab.
  // The response status will be negative on error.
  IPC_MESSAGE_ROUTED1(AutomationMsg_CancelAuthRequest,
                      int)  // tab handle
  IPC_MESSAGE_ROUTED1(AutomationMsg_CancelAuthResponse,
                      int)  // status

  // Requests that the automation provider ask history for the most recent
  // chain of redirects coming from the given URL. The response must be
  // decoded by the caller manually; it contains an integer indicating the
  // number of URLs, followed by that many wstrings indicating a chain of
  // redirects. On failure, the count will be negative.
  IPC_MESSAGE_ROUTED2(AutomationMsg_RedirectsFromRequest,
                      int,   // tab handle
                      GURL)  // source URL
  IPC_MESSAGE_EMPTY(AutomationMsg_RedirectsFromResponse)

  // This message asks the AutomationProvider whether a tab is waiting for
  // login info.
  IPC_MESSAGE_ROUTED1(AutomationMsg_NeedsAuthRequest,
                      int)  // tab handle
  IPC_MESSAGE_ROUTED1(AutomationMsg_NeedsAuthResponse,
                      bool)  // status

  // This message requests the AutomationProvider to apply a certain
  // accelerator. It is completely asynchronous with the resulting accelerator
  // action.
  IPC_MESSAGE_ROUTED2(AutomationMsg_ApplyAcceleratorRequest,
                      int,  // window handle
                      int)  // accelerator id like (IDC_BACK, IDC_FORWARD ...)
                            // The list can be found at
                            // chrome/app/chrome_dll_resource.h

  // This message requests that the AutomationProvider executes a JavaScript,
  // which is sent embedded in a 'javascript:' URL.
  // The javascript is executed in context of child frame whose xpath
  // is passed as parameter (context_frame). The execution results in
  // a serialized JSON string response.
  IPC_MESSAGE_ROUTED3(AutomationMsg_DomOperationRequest,
                      int,           // tab handle
                      std::wstring,  // context_frame
                      std::wstring)  // the javascript to be executed

  // This message is used to communicate the values received by the
  // callback binding the JS to Cpp. This message forms the second leg in
  // the communication channel. The values are originally received in the
  // renderer which are then sent to the app (wrapped as json) using
  // corresponding message in render_messages_internal.h
  // This message simply relays the json string.
  IPC_MESSAGE_ROUTED1(AutomationMsg_DomOperationResponse,
                      std::string)  // the serialized json string containing
                                    // the result of a javascript execution

  // Is the Download Shelf visible for the specified tab?
  IPC_MESSAGE_ROUTED1(AutomationMsg_ShelfVisibilityRequest,
                      int /* tab_handle */)
  IPC_MESSAGE_ROUTED1(AutomationMsg_ShelfVisibilityResponse,
                      bool /* is_visible */)

  // This message requests the number of constrained windows in the tab with
  // the given handle.  The response contains the number of constrained windows,
  // or -1 if the request failed.
  IPC_MESSAGE_ROUTED1(AutomationMsg_ConstrainedWindowCountRequest,
                      int /* tab_handle */)
  IPC_MESSAGE_ROUTED1(AutomationMsg_ConstrainedWindowCountResponse,
                      int /* constrained_window_count */)

  // This message requests the handle of the constrained window with the given
  // (zero-based) index in the given tab. First parameter specifies the given
  // tab handle, second specifies the given child_index. On error, the returned
  // handle value is 0.
  IPC_MESSAGE_ROUTED2(AutomationMsg_ConstrainedWindowRequest,
                      int, /* window_handle */
                      int) /* child_index */

  IPC_MESSAGE_ROUTED1(AutomationMsg_ConstrainedWindowResponse,
                      int) /* constrained_handle */

  // This message requests the the title of the constrained window with the
  // given handle. The response contains the size of the title string and title
  // string. On error, this value should be -1 and empty string. Note that the
  // title can be empty in which case the size would be 0.
  IPC_MESSAGE_ROUTED1(AutomationMsg_ConstrainedTitleRequest, int)
  IPC_MESSAGE_ROUTED2(AutomationMsg_ConstrainedTitleResponse, int, std::wstring)

  // This message requests the bounds of the specified View element in
  // window coordinates.
  // Request:
  //   int - the handle of the window in which the view appears
  //   int - the ID of the view, as specified in chrome/browser/view_ids.h
  //   bool - whether the bounds should be returned in the screen coordinates
  //          (if true) or in the browser coordinates (if false).
  // Response:
  //   bool - true if the view was found
  //   gfx::Rect - the bounds of the view, in window coordinates
  IPC_MESSAGE_ROUTED3(AutomationMsg_WindowViewBoundsRequest, int, int, bool)
  IPC_MESSAGE_ROUTED2(AutomationMsg_WindowViewBoundsResponse, bool, gfx::Rect)

  // This message requests that a drag be performed in window coordinate space
  // Request:
  //   int - the handle of the window that's the context for this drag
  //   std::vector<POINT> - the path of the drag in window coordinate space;
  //       it should have at least 2 points (start and end)
  //   int - the flags which identify the mouse button(s) for the drag, as
  //       defined in chrome/views/event.h
  // Response:
  //   bool - true if the drag could be performed
  IPC_MESSAGE_ROUTED3(AutomationMsg_WindowDragRequest,
                      int, std::vector<POINT>, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_WindowDragResponse, bool)

  // Similar to AutomationMsg_InitialLoadsComplete, this indicates that the
  // new tab ui has completed the initial load of its data.
  // Time is how many milliseconds the load took.
  IPC_MESSAGE_ROUTED1(AutomationMsg_InitialNewTabUILoadComplete,
                      int /* time */)

  // This message starts a find within a tab corresponding to the supplied
  // tab handle. The response contains the number of matches found on the page
  // within the tab specified. The parameter 'search_string' specifies what
  // string to search for, 'forward' specifies whether to search in forward
  // direction (1=forward, 0=back), 'match_case' specifies case sensitivity
  // (1=case sensitive, 0=case insensitive). If an error occurs, matches_found
  // will be -1.
  IPC_MESSAGE_ROUTED4(AutomationMsg_FindInPageRequest,
                      int, /* tab_handle */
                      std::wstring, /* find_request */
                      int, /* forward */
                      int /* match_case */)
  IPC_MESSAGE_ROUTED1(AutomationMsg_FindInPageResponse,
                      int /* matches_found */)

  // This message sends a inspect element request for a given tab. The response
  // contains the number of resources loaded by the inspector controller.
  IPC_MESSAGE_ROUTED3(AutomationMsg_InspectElementRequest,
                      int, /* tab_handle */
                      int, /* x */
                      int /* y */)
  IPC_MESSAGE_ROUTED1(AutomationMsg_InspectElementResponse, int)

  // This message requests the process ID of the tab that corresponds
  // to the given automation handle.
  // The response has an integer corresponding to the PID of the tab's
  // renderer, 0 if the tab currently has no renderer process, or -1 on error.
  IPC_MESSAGE_ROUTED1(AutomationMsg_TabProcessIDRequest,
                      int /* tab_handle */)
  IPC_MESSAGE_ROUTED1(AutomationMsg_TabProcessIDResponse,
                      int /* process ID */)

  // This tells the browser to enable or disable the filtered network layer.
  IPC_MESSAGE_ROUTED1(AutomationMsg_SetFilteredInet,
                      bool /* enabled */)

  // Gets the directory that downloads will occur in for the active profile.
  IPC_MESSAGE_ROUTED1(AutomationMsg_DownloadDirectoryRequest,
                      int /* tab_handle */)
  IPC_MESSAGE_ROUTED1(AutomationMsg_DownloadDirectoryResponse,
                      std::wstring /* directory */)

  // This message requests the id of the view that has the focus in the
  // specified window. If no view is focused, -1 is returned.  Note that the
  // window should either be a ViewWindow or a Browser.
  IPC_MESSAGE_ROUTED1(AutomationMsg_GetFocusedViewIDRequest,
                      int /* view_handle */)
  IPC_MESSAGE_ROUTED1(AutomationMsg_GetFocusedViewIDResponse,
                      int /* focused_view_id */)

  // This message shows/hides the window.
  IPC_MESSAGE_ROUTED2(AutomationMsg_SetWindowVisibleRequest,
                      int /* view_handle */, bool /* visible */)
  IPC_MESSAGE_ROUTED1(AutomationMsg_SetWindowVisibleResponse,
                      bool /* success */)

  // Gets the active status of a window.
  IPC_MESSAGE_ROUTED1(AutomationMsg_IsWindowActiveRequest,
                      int /* view_handle */)
  IPC_MESSAGE_ROUTED2(AutomationMsg_IsWindowActiveResponse,
                      bool /* success */, bool /* active */)

  // Makes the specified window the active window.
  IPC_MESSAGE_ROUTED1(AutomationMsg_ActivateWindow, int /* view_handle */)

  // Opens a new browser window.
  IPC_MESSAGE_ROUTED1(AutomationMsg_OpenNewBrowserWindow,
                      int /* show_command*/ )

  // This message requests the handle (int64 app-unique identifier) of the
  // current active top window.  On error, the returned handle value is 0.
  IPC_MESSAGE_ROUTED0(AutomationMsg_ActiveWindowRequest)
  IPC_MESSAGE_ROUTED1(AutomationMsg_ActiveWindowResponse, int)

  // This message requests the browser associated with the specified window
  // handle.
  // The response contains a success flag and the handle of the browser.
  IPC_MESSAGE_ROUTED1(AutomationMsg_BrowserForWindowRequest,
                      int /* window handle */)
  IPC_MESSAGE_ROUTED2(AutomationMsg_BrowserForWindowResponse,
                      bool /* success flag */,
                      int /* browser handle */)

  // This message requests the window associated with the specified browser
  // handle.
  // The response contains a success flag and the handle of the window.
  IPC_MESSAGE_ROUTED1(AutomationMsg_WindowForBrowserRequest,
                      int /* browser handle */)
  IPC_MESSAGE_ROUTED2(AutomationMsg_WindowForBrowserResponse,
                      bool /* success flag */,
                      int /* window handle */)

  // This message requests the AutocompleteEdit associated with the specified
  // browser handle.
  // The response contains a success flag and the handle of the omnibox.
  IPC_MESSAGE_ROUTED1(AutomationMsg_AutocompleteEditForBrowserRequest,
                      int /* browser handle */)
  IPC_MESSAGE_ROUTED2(AutomationMsg_AutocompleteEditForBrowserResponse,
                      bool /* success flag */,
                      int /* AutocompleteEdit handle */)

  // This message requests that a mouse click be performed in window coordinate
  // space.
  // Request:
  //   int - the handle of the window that's the context for this click
  //   POINT - the point to click
  //   int - the flags which identify the mouse button(s) for the click, as
  //       defined in chrome/views/event.h
  IPC_MESSAGE_ROUTED3(AutomationMsg_WindowClickRequest, int, POINT, int)

  // This message requests that a key press be performed.
  // Request:
  //   int - the handle of the window that's the context for this click
  //   wchar_t - char of the key that was pressed.
  //   int - the flags which identify the modifiers (shift, ctrl, alt)
  //         associated for, as defined in chrome/views/event.h
  IPC_MESSAGE_ROUTED3(AutomationMsg_WindowKeyPressRequest, int, wchar_t, int)

  // This message notifies the AutomationProvider to create a tab which is
  // hosted by an external process. The response contains the HWND of the
  // window that contains the external tab and the handle to the newly
  // created tab
  // The second parameter is the url to be loaded in the new tab.
  IPC_MESSAGE_ROUTED0(AutomationMsg_CreateExternalTab)
  IPC_MESSAGE_ROUTED2(AutomationMsg_CreateExternalTabResponse, HWND, int)

  // This message notifies the AutomationProvider to navigate to a specified
  // url in the external tab with given handle. The first parameter is the
  // handle to the tab resource. The second parameter is the target url.
  // The response contains a status code which is nonnegative on success.
  IPC_MESSAGE_ROUTED2(AutomationMsg_NavigateInExternalTabRequest, int, GURL)
  IPC_MESSAGE_ROUTED1(AutomationMsg_NavigateInExternalTabResponse,
                      int)  // see AutomationMsg_NavigationResponseValues

  // This message is an outgoing message from Chrome to an external host.
  // It is a notification that the NavigationState was changed
  // Request:
  //   -int: The flags specifying what changed
  //         (see TabContents::InvalidateTypes)
  // Response:
  //   None expected
  IPC_MESSAGE_ROUTED1(AutomationMsg_NavigationStateChanged, int)

  // This message is an outgoing message from Chrome to an external host.
  // It is a notification that the target URL has changed (the target URL
  // is the URL of the link that the user is hovering on)
  // Request:
  //   -std::wstring: The new target URL
  // Response:
  //   None expected
  IPC_MESSAGE_ROUTED1(AutomationMsg_UpdateTargetUrl, std::wstring)

  // This message notifies the AutomationProvider to show the specified html
  // text in an interstitial page in the tab with given handle. The first
  // parameter is the handle to the tab resource. The second parameter is the
  // html text to be displayed.
  // The response contains a success flag.
  IPC_MESSAGE_ROUTED2(AutomationMsg_ShowInterstitialPageRequest,
                      int,
                      std::string)
  IPC_MESSAGE_ROUTED1(AutomationMsg_ShowInterstitialPageResponse, bool)

  // This message notifies the AutomationProvider to hide the current
  // interstitial page in the tab with given handle. The parameter is the handle
  // to the tab resource.
  // The response contains a success flag.
  IPC_MESSAGE_ROUTED1(AutomationMsg_HideInterstitialPageRequest, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_HideInterstitialPageResponse, bool)

  // This message requests that a tab be closed.
  // Request:
  //   - int: handle of the tab to close
  //   - bool: if true the proxy blocks until the tab has completely closed,
  //           otherwise the proxy only blocks until it initiates the close.
  IPC_MESSAGE_ROUTED2(AutomationMsg_CloseTabRequest, int, bool)
  IPC_MESSAGE_ROUTED1(AutomationMsg_CloseTabResponse, bool)

  // This message requests that the browser be closed.
  // Request:
  //   - int: handle of the browser which contains the tab
  // Response:
  //  - bool: whether the operation was successfull.
  //  - bool: whether the browser process will be terminated as a result (if
  //          this was the last closed browser window).
  IPC_MESSAGE_ROUTED1(AutomationMsg_CloseBrowserRequest, int)
  IPC_MESSAGE_ROUTED2(AutomationMsg_CloseBrowserResponse, bool, bool)

  // This message sets the keyboard accelarators to be used by an externally
  // hosted tab. This call is not valid on a regular tab hosted within
  // Chrome.
  // Request:
  //   - int: handle of the tab
  //   - HACCEL: The accelerator table to be set
  //   - int: The number of entries in the accelerator table
  // Response:
  //   -bool: whether the operation was successful.
  IPC_MESSAGE_ROUTED3(AutomationMsg_SetAcceleratorsForTab, int, HACCEL, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_SetAcceleratorsForTabResponse, bool)

  // This message is an outgoing message from Chrome to an external host.
  // It is a request to process a keyboard accelerator.
  // Request:
  //   -MSG: The keyboard message
  // Response:
  //   None expected
  // TODO(sanjeevr): Ideally we need to add a response from the external
  // host saying whether it processed the accelerator
  IPC_MESSAGE_ROUTED1(AutomationMsg_HandleAccelerator, MSG)

  // This message is an outgoing message from Chrome to an external host.
  // It is a request to open a url
  // Request:
  //   -GURL: The URL to open
  //   -int: The WindowOpenDisposition that specifies where the URL should
  //         be opened (new tab, new window etc).
  // Response:
  //   None expected
  IPC_MESSAGE_ROUTED2(AutomationMsg_OpenURL, GURL, int)

  // This message is sent by the container of an externally hosted tab to
  // reflect any accelerator keys that it did not process. This gives the
  // tab a chance to handle the keys
  // Request:
  //   - int: handle of the tab
  //   -MSG: The keyboard message that the container did not handle
  // Response:
  //   None expected
  IPC_MESSAGE_ROUTED2(AutomationMsg_ProcessUnhandledAccelerator, int, MSG)

  // This message requests the provider to wait until the specified tab has
  // finished restoring after session restore.
  // Request:
  //   - int: handle of the tab
  // Response:
  //  - bool: whether the operation was successful.
  IPC_MESSAGE_ROUTED1(AutomationMsg_WaitForTabToBeRestored, int)

  // Sent in response to AutomationMsg_WaitForTabToBeRestored once the tab has
  // finished loading.
  IPC_MESSAGE_ROUTED0(AutomationMsg_TabFinishedRestoring)

  // This message is an outgoing message from Chrome to an external host.
  // It is a notification that a navigation happened
  // Request:
  //   -int : Indicates the type of navigation (see the NavigationType enum)
  //   -int:  If this was not a new navigation, then this value indicates the
  //          relative offset of the navigation. A positive offset means a
  //          forward navigation, a negative value means a backward navigation
  //          and 0 means this was a redirect
  // Response:
  //   None expected
  IPC_MESSAGE_ROUTED2(AutomationMsg_DidNavigate, int, int)

  // This message requests the different security states of the page displayed
  // in the specified tab.
  // Request:
  //   - int: handle of the tab
  // Response:
  //  - bool: whether the operation was successful.
  //  - int: the security style of the tab (enum SecurityStyle see
  //         security_style.h)).
  //  - int: the status of the server's ssl cert (0 means no errors or no ssl
  //         was used).
  //  - int: the mixed content state, 0 means no mixed/unsafe contents.
  IPC_MESSAGE_ROUTED1(AutomationMsg_GetSecurityState, int)
  IPC_MESSAGE_ROUTED4(AutomationMsg_GetSecurityStateResponse,
                      bool,
                      int,
                      int,
                      int)

  // This message requests the page type of the page displayed in the specified
  // tab (normal, error or interstitial).
  // Request:
  //   - int: handle of the tab
  // Response:
  //  - bool: whether the operation was successful.
  //  - int: the type of the page currently displayed (enum PageType see
  //         entry_navigation.h).
  IPC_MESSAGE_ROUTED1(AutomationMsg_GetPageType, int)
  IPC_MESSAGE_ROUTED2(AutomationMsg_GetPageTypeResponse, bool, int)

  // This message simulates the user action on the SSL blocking page showing in
  // the specified tab.  This message is only effective if an interstitial page
  // is showing in the tab.
  // Request:
  //   - int: handle of the tab
  //   - bool: whether to proceed or abort the navigation
  // Response:
  //  - bool: whether the operation was successful.
  IPC_MESSAGE_ROUTED2(AutomationMsg_ActionOnSSLBlockingPage, int, bool)
  IPC_MESSAGE_ROUTED1(AutomationMsg_ActionOnSSLBlockingPageResponse, bool)

  // Message to request that a browser window is brought to the front and activated.
  // Request:
  //   - int: handle of the browser window.
  // Response:
  //   - bool: True if the browser is brought to the front.
  IPC_MESSAGE_ROUTED1(AutomationMsg_BringBrowserToFront, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_BringBrowserToFrontResponse, bool)

  // Message to request whether a certain item is enabled of disabled in the "Page"
  // menu in the browser window
  //
  // Request:
  //   - int: handle of the browser window.
  //   - int: IDC message identifier to query if enabled
  // Response:
  //   - bool: True if the command is enabled on the Page menu
  IPC_MESSAGE_ROUTED2(AutomationMsg_IsPageMenuCommandEnabled, int, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_IsPageMenuCommandEnabledResponse, bool)

  // This message notifies the AutomationProvider to print the tab with given
  // handle. The first parameter is the handle to the tab resource.  The
  // response contains a bool which is true on success.
  IPC_MESSAGE_ROUTED1(AutomationMsg_PrintNowRequest, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_PrintNowResponse, bool)

  // This message notifies the AutomationProvider to reload the current page in
  // the tab with given handle. The first parameter is the handle to the tab
  // resource.  The response contains a status code which is nonnegative on
  // success.
  IPC_MESSAGE_ROUTED1(AutomationMsg_ReloadRequest, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_ReloadResponse,
                      int)  // see AutomationMsg_NavigationResponseValues

  // This message requests the handle (int64 app-unique identifier) of the
  // last active browser window, or the browser at index 0 if there is no last
  // active browser, or it no longer exists. Returns 0 if no browser windows
  // exist.
  IPC_MESSAGE_ROUTED0(AutomationMsg_LastActiveBrowserWindowRequest)
  IPC_MESSAGE_ROUTED1(AutomationMsg_LastActiveBrowserWindowResponse, int)

  // This message requests the bounds of a constrained window (relative to its
  // containing TabContents). On an internal error, the boolean in the result will
  // be set to false.
  IPC_MESSAGE_ROUTED1(AutomationMsg_ConstrainedWindowBoundsRequest,
                      int /* tab_handle */)
  IPC_MESSAGE_ROUTED2(AutomationMsg_ConstrainedWindowBoundsResponse,
                      bool /* the requested window exists */,
                      gfx::Rect /* constrained_window_count */)

  // This message notifies the AutomationProvider to save the page with given
  // handle. The first parameter is the handle to the tab resource. The second
  // parameter is the main HTML file name. The third parameter is the directory
  // for saving resources. The fourth parameter is the saving type: 0 for HTML
  // only; 1 for complete web page.
  // The response contains a bool which is true on success.
  IPC_MESSAGE_ROUTED4(AutomationMsg_SavePageRequest, int, std::wstring,
                      std::wstring, int)
  IPC_MESSAGE_ROUTED1(AutomationMsg_SavePageResponse, bool)


  // This message requests the text currently being displayed in the
  // AutocompleteEdit.  The parameter is the handle to the AutocompleteEdit.
  // The response is a string indicating the text in the AutocompleteEdit.
  IPC_MESSAGE_ROUTED1(AutomationMsg_AutocompleteEditGetTextRequest,
                      int /* autocomplete edit handle */)
  IPC_MESSAGE_ROUTED2(AutomationMsg_AutocompleteEditGetTextResponse,
                      bool /* the requested autocomplete edit exists */,
                      std::wstring /* omnibox text */)

  // This message sets the text being displayed in the AutocompleteEdit.  The
  // first parameter is the handle to the omnibox and the second parameter is
  // the text to be displayed in the AutocompleteEdit.
  // The response has no parameters and is returned when the operation has
  // completed.
  IPC_MESSAGE_ROUTED2(AutomationMsg_AutocompleteEditSetTextRequest,
                      int /* autocomplete edit handle */,
                      std::wstring /* text to set */)
  IPC_MESSAGE_ROUTED1(AutomationMsg_AutocompleteEditSetTextResponse,
                      bool /* the requested autocomplete edit exists */)

  // This message requests if a query to a autocomplete provider is still in
  // progress.  The first parameter in the request is the handle to the
  // autocomplete edit.
  // The first parameter in the response indicates if the request succeeded.
  // The second parameter in indicates if a query is still in progress.
  IPC_MESSAGE_ROUTED1(AutomationMsg_AutocompleteEditIsQueryInProgressRequest,
                      int /* autocomplete edit handle*/)
  IPC_MESSAGE_ROUTED2(AutomationMsg_AutocompleteEditIsQueryInProgressResponse,
                      bool /* the requested autocomplete edit exists */,
                      bool /* indicates if a query is in progress */)

  // This message requests a list of the autocomplete messages currently being
  // displayed by the popup.  The parameter in the request is a handle to the
  // autocomplete edit.
  // The first parameter in the response indicates if the request was
  // successful while the second parameter is the actual list of matches.
  IPC_MESSAGE_ROUTED1(AutomationMsg_AutocompleteEditGetMatchesRequest,
                      int /* autocomplete edit handle*/)
  IPC_MESSAGE_ROUTED2(AutomationMsg_AutocompleteEditGetMatchesResponse,
                      bool /* the requested autocomplete edit exists */,
                      std::vector<AutocompleteMatchData> /* matches */)

  // This message requests the execution of a browser command in the browser
  // for which the handle is specified.
  // The response contains a boolean, whether the command execution was
  // successful.
  IPC_MESSAGE_ROUTED2(AutomationMsg_WindowExecuteCommandRequest,
                      int /* automation handle */,
                      int /* browser command */)
  IPC_MESSAGE_ROUTED1(AutomationMsg_WindowExecuteCommandResponse,
                      bool /* success flag */)

IPC_END_MESSAGES(Automation)
