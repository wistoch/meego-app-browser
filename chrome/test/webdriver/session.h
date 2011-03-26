// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_SESSION_H_
#define CHROME_TEST_WEBDRIVER_SESSION_H_

#include <map>
#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/threading/thread.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/webdriver/automation.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/frame_path.h"

class DictionaryValue;
class FilePath;
class GURL;
class ListValue;
class Value;

namespace base {
class WaitableEvent;
}

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace webdriver {

class WebElementId;

// A window ID and frame path combination that uniquely identifies a specific
// frame within a session.
struct FrameId {
  FrameId(int window_id, const FramePath& frame_path);
  FrameId& operator=(const FrameId& other);
  int window_id;
  FramePath frame_path;
};

// Every connection made by WebDriver maps to a session object.
// This object creates the chrome instance and keeps track of the
// state necessary to control the chrome browser created.
// A session manages its own lifetime.
class Session {
 public:
  // Adds this |Session| to the |SessionManager|. The session manages its own
  // lifetime. Do not call delete.
  Session();
  // Removes this |Session| from the |SessionManager|.
  ~Session();

  // Starts the session thread and a new browser, using the exe found in
  // |browser_dir|. If |browser_dir| is empty, it will search in all the default
  // locations. Returns true on success. On failure, the session will delete
  // itself and return false.
  bool Init(const FilePath& browser_dir);

  // Terminates this session and deletes itself.
  void Terminate();

  // Executes the given |script| in the context of the given frame.
  // The |script| should be in the form of a function body
  // (e.g. "return arguments[0]"), where |args| is the list of arguments to
  // pass to the function. The caller is responsible for the script result
  // |value|.
  ErrorCode ExecuteScript(const FrameId& frame_id,
                          const std::string& script,
                          const ListValue* const args,
                          Value** value);

  // Same as above, but uses the currently targeted window and frame.
  ErrorCode ExecuteScript(const std::string& script,
                          const ListValue* const args,
                          Value** value);

  // Send the given keys to the given element dictionary. This function takes
  // ownership of |element|.
  ErrorCode SendKeys(const WebElementId& element, const string16& keys);

  // Clicks the mouse at the given location using the given button.
  bool MouseClick(const gfx::Point& click, automation::MouseButton button);
  bool MouseMove(const gfx::Point& location);
  bool MouseDrag(const gfx::Point& start, const gfx::Point& end);

  bool NavigateToURL(const std::string& url);
  bool GoForward();
  bool GoBack();
  bool Reload();
  bool GetURL(GURL* url);
  bool GetURL(std::string* url);
  bool GetTabTitle(std::string* tab_title);
  bool GetCookies(const GURL& url, std::string* cookies);
  bool GetCookieByName(const GURL& url, const std::string& cookie_name,
             std::string* cookie);
  bool DeleteCookie(const GURL& url, const std::string& cookie_name);
  bool SetCookie(const GURL& url, const std::string& cookie);

  // Gets all the currently existing window IDs. Returns true on success.
  bool GetWindowIds(std::vector<int>* window_ids);

  // Switches the window used by default. |name| is either an ID returned by
  // |GetWindowIds| or the name attribute of a DOM window.
  ErrorCode SwitchToWindow(const std::string& name);

  // Switches the frame used by default. |name_or_id| is either the name or id
  // of a frame element.
  ErrorCode SwitchToFrameWithNameOrId(const std::string& name_or_id);

  // Switches the frame used by default. |index| is the zero-based frame index.
  ErrorCode SwitchToFrameWithIndex(int index);

  // Switches the frame used by default to the topmost frame.
  void SwitchToTopFrame();

  // Closes the current window. Returns true on success.
  // Note: The session will be deleted if this closes the last window in the
  // session.
  bool CloseWindow();

  // Gets the version of the running browser.
  std::string GetVersion();

  // Finds a single element in the given frame, starting at the given
  // |root_element|, using the given locator strategy. |locator| should be a
  // constant from |LocatorType|. Returns an error code. If successful,
  // |element| will be set as the found element.
  ErrorCode FindElement(const FrameId& frame_id,
                        const WebElementId& root_element,
                        const std::string& locator,
                        const std::string& query,
                        WebElementId* element);

  // Same as above, but finds multiple elements.
  ErrorCode FindElements(const FrameId& frame_id,
                         const WebElementId& root_element,
                         const std::string& locator,
                         const std::string& query,
                         std::vector<WebElementId>* elements);

  // Scroll the element into view and get its location relative to the client's
  // viewport.
  ErrorCode GetElementLocationInView(
      const WebElementId& element, gfx::Point* location);

  // Gets the size of the element from the given window and frame, even if
  // its display is none.
  ErrorCode GetElementSize(const FrameId& frame_id,
                           const WebElementId& element,
                           gfx::Size* size);

  // Gets the element's effective style for the given property.
  ErrorCode GetElementEffectiveStyle(
      const FrameId& frame_id,
      const WebElementId& element,
      const std::string& prop,
      std::string* value);

  // Gets the top and left element border widths for the given frame.
  ErrorCode GetElementBorder(const FrameId& frame_id,
                             const WebElementId& element,
                             int* border_left,
                             int* border_top);

  // Gets whether the element is currently displayed.
  ErrorCode IsElementDisplayed(const FrameId& frame_id,
                               const WebElementId& element,
                               bool* is_visible);

  // Waits for all tabs to stop loading. Returns true on success.
  bool WaitForAllTabsToStopLoading();

  inline const std::string& id() const { return id_; }

  inline int implicit_wait() const { return implicit_wait_; }
  inline void set_implicit_wait(const int& timeout) {
    implicit_wait_ = timeout > 0 ? timeout : 0;
  }

  enum Speed { kSlow, kMedium, kFast, kUnknown };
  inline Speed speed() { return speed_; }
  inline void set_speed(Speed speed) {
    speed_ = speed;
  }

  const FrameId& current_target() const;

 private:
  void RunSessionTask(Task* task);
  void RunSessionTaskOnSessionThread(
      Task* task,
      base::WaitableEvent* done_event);
  void InitOnSessionThread(const FilePath& browser_dir, bool* success);
  void TerminateOnSessionThread();
  void SendKeysOnSessionThread(const string16& keys, bool* success);
  ErrorCode SwitchToFrameWithJavaScriptLocatedFrame(
      const std::string& script,
      ListValue* args);
  ErrorCode FindElementsHelper(const FrameId& frame_id,
                               const WebElementId& root_element,
                               const std::string& locator,
                               const std::string& query,
                               bool find_one,
                               std::vector<WebElementId>* elements);
  ErrorCode GetLocationInViewHelper(const FrameId& frame_id,
                                    const WebElementId& element,
                                    const gfx::Rect& region,
                                    gfx::Point* location);

  const std::string id_;

  scoped_ptr<Automation> automation_;
  base::Thread thread_;

  int implicit_wait_;
  Speed speed_;

  FrameId current_target_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace webdriver

DISABLE_RUNNABLE_METHOD_REFCOUNT(webdriver::Session);

#endif  // CHROME_TEST_WEBDRIVER_SESSION_H_
