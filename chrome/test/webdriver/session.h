// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_SESSION_H_
#define CHROME_TEST_WEBDRIVER_SESSION_H_

#include <string>

#include "base/scoped_ptr.h"

#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/webdriver/error_codes.h"

namespace webdriver {

// Every connection made by WebDriver maps to a session object.
// This object creates the chrome instance and keeps track of the
// state necessary to control the chrome browser created.
class Session {
 public:
#if defined(OS_POSIX)
  typedef char ProfileDir[L_tmpnam + 1];  // +1 for \0
#elif defined(OS_WIN)
  typedef char ProfileDir[MAX_PATH + 1];  // +1 for \0
#endif

  Session(const std::string& id, AutomationProxy* proxy);

  bool Init(base::ProcessHandle process_handle);
  bool CreateTemporaryProfileDirectory();

  // Terminates this session and disconnects its automation proxy. After
  // invoking this method, the Session can safely be deleted.
  void Terminate();

  scoped_refptr<WindowProxy> GetWindow();
  scoped_refptr<TabProxy> ActivateTab();

  void SetBrowserAndTab(const int window_num,
                        const scoped_refptr<BrowserProxy>& browser,
                        const scoped_refptr<TabProxy>& tab);

  // Executes the given |script| in the context of the frame that is currently
  // the focus of this session. The |script| should be in the form of a
  // function body (e.g. "return arguments[0]"), where \args| is the list of
  // arguments to pass to the function. The caller is responsible for the
  // script result |value|.
  ErrorCode ExecuteScript(const std::wstring& script,
                          const ListValue* const args,
                          Value** value);

  inline const std::string& id() const { return id_; }
  inline AutomationProxy* proxy() { return proxy_.get(); }

  inline int window_num() const { return window_num_; }
  inline int implicit_wait() { return implicit_wait_; }
  inline void set_implicit_wait(const int& timeout) {
    implicit_wait_ = timeout > 0 ? timeout : 0;
  }

  inline const char* tmp_profile_dir() {
    return tmp_profile_dir_;
  };

  inline const std::wstring& current_frame_xpath() const {
    return current_frame_xpath_;
  }

  inline void set_current_frame_xpath(const std::wstring& xpath) {
    current_frame_xpath_ = xpath;
  }

 private:
  bool LoadProxies();
  bool WaitForLaunch();
  const std::string id_;
  const scoped_ptr<AutomationProxy> proxy_;

  // The chrome process that was started for this session.
  base::Process process_;

  int window_num_;
  scoped_refptr<BrowserProxy> browser_;
  scoped_refptr<TabProxy> tab_;

  int implicit_wait_;
  ProfileDir tmp_profile_dir_;

  // The XPath to the frame within this session's active tab which all
  // commands should be directed to. XPath strings can represent a frame deep
  // down the tree (across multiple frame DOMs).
  // Example, /html/body/table/tbody/tr/td/iframe\n/frameset/frame[1]
  // should break into 2 xpaths
  // /html/body/table/tbody/tr/td/iframe & /frameset/frame[1]
  std::wstring current_frame_xpath_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};
}  // namespace webdriver
#endif  // CHROME_TEST_WEBDRIVER_SESSION_H_

