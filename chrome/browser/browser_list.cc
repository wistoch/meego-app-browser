// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_list.h"

#include "base/histogram.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/result_codes.h"

#if defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_application_mac.h"
#endif

namespace {

// This object is instantiated when the first Browser object is added to the
// list and delete when the last one is removed. It watches for loads and
// creates histograms of some global object counts.
class BrowserActivityObserver : public NotificationObserver {
 public:
  BrowserActivityObserver() {
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   NotificationService::AllSources());
  }
  ~BrowserActivityObserver() {}

 private:
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::NAV_ENTRY_COMMITTED);
    const NavigationController::LoadCommittedDetails& load =
        *Details<NavigationController::LoadCommittedDetails>(details).ptr();
    if (!load.is_main_frame || load.is_auto || load.is_in_page)
      return;  // Don't log for subframes or other trivial types.

    LogRenderProcessHostCount();
    LogBrowserTabCount();
  }

  // Counts the number of active RenderProcessHosts and logs them.
  void LogRenderProcessHostCount() const {
    int hosts_count = 0;
    for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance())
      ++hosts_count;
    UMA_HISTOGRAM_CUSTOM_COUNTS("MPArch.RPHCountPerLoad", hosts_count,
                                1, 50, 50);
  }

  // Counts the number of tabs in each browser window and logs them. This is
  // different than the number of TabContents objects since TabContents objects
  // can be used for popups and in dialog boxes. We're just counting toplevel
  // tabs here.
  void LogBrowserTabCount() const {
    int tab_count = 0;
    for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
         browser_iterator != BrowserList::end(); browser_iterator++)
      tab_count += (*browser_iterator)->tab_count();
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", tab_count, 1, 200, 50);
  }

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActivityObserver);
};

BrowserActivityObserver* activity_observer = NULL;

// Returns true if the specified |browser| has a matching profile and type to
// those specified. |type| can also be TYPE_ANY, which means only |profile|
// must be matched.
bool BrowserMatchesProfileAndType(Browser* browser,
                                  Profile* profile,
                                  Browser::Type type) {
  return (type == Browser::TYPE_ANY || browser->type() == type) &&
      browser->profile() == profile;
}

// Finds a registered Browser object matching |profile| and |type|. This
// walks the list of Browsers that have ever been activated from most recently
// activated to least. If a Browser has never been activated, such as in a test
// scenario, this function will _not_ find it. Fall back to
// FindBrowserMatching() in that case.
Browser* FindInLastActiveMatching(Profile* profile, Browser::Type type) {
  BrowserList::list_type::const_reverse_iterator browser =
      BrowserList::begin_last_active();
  for (; browser != BrowserList::end_last_active(); ++browser) {
    if (BrowserMatchesProfileAndType(*browser, profile, type))
      return *browser;
  }
  return NULL;
}

// Finds a registered Browser object matching |profile| and |type| even if that
// Browser has never been activated. This is a forward walk, and intended as a
// last ditch fallback mostly to handle tests run on machines where no window is
// ever activated. The user experience if this function is relied on is not good
// since matching browsers will be returned in registration (creation) order.
Browser* FindBrowserMatching(Profile* profile, Browser::Type type) {
  BrowserList::const_iterator browser = BrowserList::begin();
  for (; browser != BrowserList::end(); ++browser) {
    if (BrowserMatchesProfileAndType(*browser, profile, type))
      return *browser;
  }
  return NULL;
}

}  // namespace

BrowserList::list_type BrowserList::browsers_;
std::vector<BrowserList::Observer*> BrowserList::observers_;

// static
void BrowserList::AddBrowser(Browser* browser) {
  DCHECK(browser);
  browsers_.push_back(browser);

  g_browser_process->AddRefModule();

  if (!activity_observer)
    activity_observer = new BrowserActivityObserver;

  NotificationService::current()->Notify(
      NotificationType::BROWSER_OPENED,
      Source<Browser>(browser),
      NotificationService::NoDetails());

  // Send out notifications after add has occurred. Do some basic checking to
  // try to catch evil observers that change the list from under us.
  std::vector<Observer*>::size_type original_count = observers_.size();
  for (int i = 0; i < static_cast<int>(observers_.size()); i++)
    observers_[i]->OnBrowserAdded(browser);
  DCHECK_EQ(original_count, observers_.size())
      << "observer list modified during notification";
}

// static
void BrowserList::RemoveBrowser(Browser* browser) {
  RemoveBrowserFrom(browser, &last_active_browsers_);

  bool close_app = (browsers_.size() == 1);
  NotificationService::current()->Notify(
      NotificationType::BROWSER_CLOSED,
      Source<Browser>(browser), Details<bool>(&close_app));

  // Send out notifications before anything changes. Do some basic checking to
  // try to catch evil observers that change the list from under us.
  std::vector<Observer*>::size_type original_count = observers_.size();
  for (int i = 0; i < static_cast<int>(observers_.size()); i++)
    observers_[i]->OnBrowserRemoving(browser);
  DCHECK_EQ(original_count, observers_.size())
      << "observer list modified during notification";

  RemoveBrowserFrom(browser, &browsers_);

  // If the last Browser object was destroyed, make sure we try to close any
  // remaining dependent windows too.
  if (browsers_.empty()) {
    AllBrowsersClosed();

    delete activity_observer;
    activity_observer = NULL;
  }

  g_browser_process->ReleaseModule();
}

// static
void BrowserList::AddObserver(BrowserList::Observer* observer) {
  DCHECK(std::find(observers_.begin(), observers_.end(), observer)
         == observers_.end()) << "Adding an observer twice";
  observers_.push_back(observer);
}

// static
void BrowserList::RemoveObserver(BrowserList::Observer* observer) {
  std::vector<Observer*>::iterator place =
      std::find(observers_.begin(), observers_.end(), observer);
  if (place == observers_.end()) {
    NOTREACHED() << "Removing an observer that isn't registered.";
    return;
  }
  observers_.erase(place);
}

// static
void BrowserList::CloseAllBrowsers(bool use_post) {
  // Before we close the browsers shutdown all session services. That way an
  // exit can restore all browsers open before exiting.
  ProfileManager::ShutdownSessionServices();

  BrowserList::const_iterator iter;
  for (iter = BrowserList::begin(); iter != BrowserList::end();) {
    if (use_post) {
      (*iter)->window()->Close();
      ++iter;
    } else {
      // This path is hit during logoff/power-down. In this case we won't get
      // a final message and so we force the browser to be deleted.
      Browser* browser = *iter;
      browser->window()->Close();
      // Close doesn't immediately destroy the browser
      // (Browser::TabStripEmpty() uses invoke later) but when we're ending the
      // session we need to make sure the browser is destroyed now. So, invoke
      // DestroyBrowser to make sure the browser is deleted and cleanup can
      // happen.
      browser->window()->DestroyBrowser();
      iter = BrowserList::begin();
      if (iter != BrowserList::end() && browser == *iter) {
        // Destroying the browser should have removed it from the browser list.
        // We should never get here.
        NOTREACHED();
        return;
      }
    }
  }
}

// static
void BrowserList::CloseAllBrowsersAndExit() {
#if !defined(OS_MACOSX)
  // On most platforms, closing all windows causes the application to exit.
  CloseAllBrowsers(true);
#else
  // On the Mac, the application continues to run once all windows are closed.
  // Terminate will result in a CloseAllBrowsers(true) call, and additionally,
  // will cause the application to exit cleanly.
  chrome_browser_application_mac::Terminate();
#endif
}

// static
void BrowserList::WindowsSessionEnding() {
  // EndSession is invoked once per frame. Only do something the first time.
  static bool already_ended = false;
  if (already_ended)
    return;
  already_ended = true;

  browser_shutdown::OnShutdownStarting(browser_shutdown::END_SESSION);

  // Write important data first.
  g_browser_process->EndSession();

  // Close all the browsers.
  BrowserList::CloseAllBrowsers(false);

  // Send out notification. This is used during testing so that the test harness
  // can properly shutdown before we exit.
  NotificationService::current()->Notify(
      NotificationType::SESSION_END,
      NotificationService::AllSources(),
      NotificationService::NoDetails());

  // And shutdown.
  browser_shutdown::Shutdown();

#if defined(OS_WIN)
  // At this point the message loop is still running yet we've shut everything
  // down. If any messages are processed we'll likely crash. Exit now.
  ExitProcess(ResultCodes::NORMAL_EXIT);
#else
  NOTIMPLEMENTED();
#endif
}

// static
bool BrowserList::HasBrowserWithProfile(Profile* profile) {
  BrowserList::const_iterator iter;
  for (size_t i = 0; i < browsers_.size(); ++i) {
    if (BrowserMatchesProfileAndType(browsers_[i], profile, Browser::TYPE_ANY))
      return true;
  }
  return false;
}

// static
BrowserList::list_type BrowserList::last_active_browsers_;

// static
void BrowserList::SetLastActive(Browser* browser) {
  RemoveBrowserFrom(browser, &last_active_browsers_);
  last_active_browsers_.push_back(browser);

  for (int i = 0; i < static_cast<int>(observers_.size()); i++)
    observers_[i]->OnBrowserSetLastActive(browser);
}

// static
Browser* BrowserList::GetLastActive() {
  if (!last_active_browsers_.empty())
    return *(last_active_browsers_.rbegin());

  return NULL;
}

// static
Browser* BrowserList::GetLastActiveWithProfile(Profile* p) {
  // We are only interested in last active browsers, so we don't fall back to all
  // browsers like FindBrowserWith* do.
  return FindInLastActiveMatching(p, Browser::TYPE_ANY);
}

// static
Browser* BrowserList::FindBrowserWithType(Profile* p, Browser::Type t) {
  Browser* browser = FindInLastActiveMatching(p, t);
  // Fall back to a forward scan of all Browsers if no active one was found.
  return browser ? browser : FindBrowserMatching(p, t);
}

// static
Browser* BrowserList::FindBrowserWithProfile(Profile* p) {
  Browser* browser = FindInLastActiveMatching(p, Browser::TYPE_ANY);
  // Fall back to a forward scan of all Browsers if no active one was found.
  return browser ? browser : FindBrowserMatching(p, Browser::TYPE_ANY);
}

// static
Browser* BrowserList::FindBrowserWithID(SessionID::id_type desired_id) {
  BrowserList::const_iterator i;
  for (i = BrowserList::begin(); i != BrowserList::end(); ++i) {
    if ((*i)->session_id().id() == desired_id)
      return *i;
  }
  return NULL;
}

// static
size_t BrowserList::GetBrowserCountForType(Profile* p, Browser::Type type) {
  BrowserList::const_iterator i;
  size_t result = 0;
  for (i = BrowserList::begin(); i != BrowserList::end(); ++i) {
    if (BrowserMatchesProfileAndType(*i, p, type))
      ++result;
  }
  return result;
}

// static
size_t BrowserList::GetBrowserCount(Profile* p) {
  BrowserList::const_iterator i;
  size_t result = 0;
  for (i = BrowserList::begin(); i != BrowserList::end(); ++i) {
    if (BrowserMatchesProfileAndType(*i, p, Browser::TYPE_ANY))
      result++;
  }
  return result;
}

// static
bool BrowserList::IsOffTheRecordSessionActive() {
  BrowserList::const_iterator i;
  for (i = BrowserList::begin(); i != BrowserList::end(); ++i) {
    if ((*i)->profile()->IsOffTheRecord())
      return true;
  }
  return false;
}

// static
void BrowserList::RemoveBrowserFrom(Browser* browser, list_type* browser_list) {
  const iterator remove_browser =
      find(browser_list->begin(), browser_list->end(), browser);
  if (remove_browser != browser_list->end())
    browser_list->erase(remove_browser);
}

TabContentsIterator::TabContentsIterator()
    : browser_iterator_(BrowserList::begin()),
      web_view_index_(-1),
      cur_(NULL) {
    Advance();
  }

void TabContentsIterator::Advance() {
  // Unless we're at the beginning (index = -1) or end (iterator = end()),
  // then the current TabContents should be valid.
  DCHECK(web_view_index_ || browser_iterator_ == BrowserList::end() || cur_)
      << "Trying to advance past the end";

  // Update cur_ to the next TabContents in the list.
  while (browser_iterator_ != BrowserList::end()) {
    web_view_index_++;

    while (web_view_index_ >= (*browser_iterator_)->tab_count()) {
      // advance browsers
      ++browser_iterator_;
      web_view_index_ = 0;
      if (browser_iterator_ == BrowserList::end()) {
        cur_ = NULL;
        return;
      }
    }

    TabContents* next_tab =
        (*browser_iterator_)->GetTabContentsAt(web_view_index_);
    if (next_tab) {
      cur_ = next_tab;
      return;
    }
  }
}
