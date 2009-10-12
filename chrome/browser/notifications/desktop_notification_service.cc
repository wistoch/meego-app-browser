// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/thread.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/worker_host/worker_process_host.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "webkit/api/public/WebNotificationPresenter.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

using WebKit::WebNotificationPresenter;

// A task object which calls the renderer to inform the web page that the
// permission request has completed.
class NotificationPermissionCallbackTask : public Task {
 public:
  NotificationPermissionCallbackTask(int process_id, int route_id,
      int request_id)
      : process_id_(process_id),
        route_id_(route_id),
        request_id_(request_id) {
  }

  virtual void Run() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
    RenderViewHost* host = RenderViewHost::FromID(process_id_, route_id_);
    if (host)
      host->Send(new ViewMsg_PermissionRequestDone(route_id_, request_id_));
  }

 private:
  int process_id_;
  int route_id_;
  int request_id_;
};

// The delegate for the infobar shown when an origin requests notification
// permissions.
class NotificationPermissionInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  NotificationPermissionInfoBarDelegate(TabContents* contents,
                                        const GURL& origin,
                                        int callback_context)
      : ConfirmInfoBarDelegate(contents),
        origin_(origin),
        profile_(contents->profile()),
        process_id_(contents->process()->id()),
        route_id_(contents->render_view_host()->routing_id()),
        callback_context_(callback_context),
        action_taken_(false) {
  }

  // Overridden from ConfirmInfoBarDelegate:
  virtual void InfoBarClosed() {
    if (!action_taken_)
      UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Ignored", 1);

    base::Thread* io_thread = g_browser_process->io_thread();
    if (io_thread && io_thread->message_loop()) {
      io_thread->message_loop()->PostTask(FROM_HERE,
        new NotificationPermissionCallbackTask(process_id_, route_id_,
            callback_context_));
    }

    delete this;
  }

  virtual std::wstring GetMessageText() const {
    return l10n_util::GetStringF(IDS_NOTIFICATION_PERMISSIONS,
                                 UTF8ToWide(origin_.spec()));
  }

  virtual SkBitmap* GetIcon() const {
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(
       IDR_PRODUCT_ICON_32);
  }

  virtual int GetButtons() const {
    return BUTTON_OK | BUTTON_CANCEL | BUTTON_OK_DEFAULT;
  }

  virtual std::wstring GetButtonLabel(InfoBarButton button) const {
    return button == BUTTON_OK ?
        l10n_util::GetString(IDS_NOTIFICATION_PERMISSION_YES) :
        l10n_util::GetString(IDS_NOTIFICATION_PERMISSION_NO);
  }

  virtual bool Accept() {
    UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Allowed", 1);
    profile_->GetDesktopNotificationService()->GrantPermission(origin_);
    action_taken_ = true;
    return true;
  }

  virtual bool Cancel() {
    UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Denied", 1);
    profile_->GetDesktopNotificationService()->DenyPermission(origin_);
    action_taken_ = true;
    return true;
  }

 private:
  // The origin we are asking for permissions on.
  GURL origin_;

  // The Profile that we restore sessions from.
  Profile* profile_;

  // The callback information that tells us how to respond to javascript via
  // the correct RenderView.
  int process_id_;
  int route_id_;
  int callback_context_;

  // Whether the user clicked one of the buttons.
  bool action_taken_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPermissionInfoBarDelegate);
};

DesktopNotificationService::DesktopNotificationService(Profile* profile,
    NotificationUIManager* ui_manager)
    : profile_(profile),
      ui_manager_(ui_manager) {
  InitPrefs();
}

DesktopNotificationService::~DesktopNotificationService() {
}

// Initialize the cache with the allowed and denied origins, or
// create the preferences if they don't exist yet.
void DesktopNotificationService::InitPrefs() {
  PrefService* prefs = profile_->GetPrefs();
  ListValue* allowed_sites = NULL;
  ListValue* denied_sites = NULL;

  if (prefs->FindPreference(prefs::kDesktopNotificationAllowedOrigins))
    allowed_sites =
        prefs->GetMutableList(prefs::kDesktopNotificationAllowedOrigins);
  else
    prefs->RegisterListPref(prefs::kDesktopNotificationAllowedOrigins);

  if (prefs->FindPreference(prefs::kDesktopNotificationDeniedOrigins))
    denied_sites =
        prefs->GetMutableList(prefs::kDesktopNotificationDeniedOrigins);
  else
    prefs->RegisterListPref(prefs::kDesktopNotificationDeniedOrigins);

  prefs_cache_ = new NotificationsPrefsCache(allowed_sites, denied_sites);
}

void DesktopNotificationService::GrantPermission(const GURL& origin) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  PrefService* prefs = profile_->GetPrefs();
  ListValue* allowed_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationAllowedOrigins);
  ListValue* denied_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationDeniedOrigins);
  // Remove from the black-list and add to the white-list.
  StringValue* value = new StringValue(origin.spec());
  denied_sites->Remove(*value);
  allowed_sites->Append(value);
  prefs->ScheduleSavePersistentPrefs();

  // Schedule a cache update on the IO thread.
  base::Thread* io_thread = g_browser_process->io_thread();
  if (io_thread && io_thread->message_loop()) {
    io_thread->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(prefs_cache_.get(),
                          &NotificationsPrefsCache::CacheAllowedOrigin,
                          origin));
  }
}

void DesktopNotificationService::DenyPermission(const GURL& origin) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  PrefService* prefs = profile_->GetPrefs();
  ListValue* allowed_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationAllowedOrigins);
  ListValue* denied_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationDeniedOrigins);
  StringValue* value = new StringValue(origin.spec());
  // Remove from the white-list and add to the black-list.
  allowed_sites->Remove(*value);
  denied_sites->Append(value);
  prefs->ScheduleSavePersistentPrefs();

  // Schedule a cache update on the IO thread.
  base::Thread* io_thread = g_browser_process->io_thread();
  if (io_thread && io_thread->message_loop()) {
    io_thread->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(prefs_cache_.get(),
                          &NotificationsPrefsCache::CacheDeniedOrigin,
                          origin));
  }
}

void DesktopNotificationService::RequestPermission(
    const GURL& origin, int callback_context) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // Show an info bar requesting permission.
  Browser* browser = BrowserList::GetLastActive();
  if (!browser) {
    // Reached during ui tests.
    return;
  }
  TabContents* tab = browser->GetSelectedTabContents();
  if (!tab)
    return;
  tab->AddInfoBar(new NotificationPermissionInfoBarDelegate(tab, origin,
                                                            callback_context));
}
