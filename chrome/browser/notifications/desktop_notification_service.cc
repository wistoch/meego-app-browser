// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/worker_host/worker_process_host.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNotificationPresenter.h"

using WebKit::WebNotificationPresenter;

namespace {

// Creates a data:xxxx URL which contains the full HTML for a notification
// using supplied icon, title, and text, run through a template which contains
// the standard formatting for notifications.
static string16 CreateDataUrl(const GURL& icon_url, const string16& title,
    const string16& body) {

  int resource;
  string16 line_name;
  string16 line;
  std::vector<string16> subst;
  if (icon_url.is_valid()) {
    resource = IDR_NOTIFICATION_ICON_HTML;
    subst.push_back(UTF8ToUTF16(icon_url.spec()));
    subst.push_back(UTF8ToUTF16(EscapeForHTML(UTF16ToUTF8(title))));
    subst.push_back(UTF8ToUTF16(EscapeForHTML(UTF16ToUTF8(body))));
  } else if (title.empty() || body.empty()) {
    resource = IDR_NOTIFICATION_1LINE_HTML;
    line = title.empty() ? body : title;
    // Strings are div names in the template file.
    line_name = title.empty() ? ASCIIToUTF16("description")
                              : ASCIIToUTF16("title");
    subst.push_back(UTF8ToUTF16(EscapeForHTML(UTF16ToUTF8(line_name))));
    subst.push_back(UTF8ToUTF16(EscapeForHTML(UTF16ToUTF8(line))));
  } else {
    resource = IDR_NOTIFICATION_2LINE_HTML;
    subst.push_back(UTF8ToUTF16(EscapeForHTML(UTF16ToUTF8(title))));
    subst.push_back(UTF8ToUTF16(EscapeForHTML(UTF16ToUTF8(body))));
  }

  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          resource));

  if (template_html.empty()) {
    NOTREACHED() << "unable to load template. ID: " << resource;
    return string16();
  }

  string16 format_string = ASCIIToUTF16("data:text/html;charset=utf-8,"
                                        + template_html.as_string());
  return ReplaceStringPlaceholders(format_string, subst, NULL);
}

}

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
                                        const std::wstring& display_name,
                                        int process_id,
                                        int route_id,
                                        int callback_context)
      : ConfirmInfoBarDelegate(contents),
        origin_(origin),
        display_name_(display_name),
        profile_(contents->profile()),
        process_id_(process_id),
        route_id_(route_id),
        callback_context_(callback_context),
        action_taken_(false) {
  }

  // Overridden from ConfirmInfoBarDelegate:
  virtual void InfoBarClosed() {
    if (!action_taken_)
      UMA_HISTOGRAM_COUNTS("NotificationPermissionRequest.Ignored", 1);

    ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      new NotificationPermissionCallbackTask(
          process_id_, route_id_, callback_context_));

    delete this;
  }

  virtual std::wstring GetMessageText() const {
    return l10n_util::GetStringF(IDS_NOTIFICATION_PERMISSIONS, display_name_);
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

  // The display name for the origin to be displayed.  Will be different from
  // origin_ for extensions.
  std::wstring display_name_;

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

  // Listen for new extension installations so we can cache permissions.
  notification_registrar_.Add(this,
    NotificationType::EXTENSION_LOADED,
    Source<Profile>(profile_));
}

DesktopNotificationService::~DesktopNotificationService() {
}

// Initialize the cache with the allowed and denied origins, or
// create the preferences if they don't exist yet.
void DesktopNotificationService::InitPrefs() {
  PrefService* prefs = profile_->GetPrefs();
  const ListValue* allowed_sites = NULL;
  const ListValue* denied_sites = NULL;

  if (!profile_->IsOffTheRecord()) {
    if (!prefs->FindPreference(prefs::kDesktopNotificationAllowedOrigins))
      prefs->RegisterListPref(prefs::kDesktopNotificationAllowedOrigins);
    allowed_sites = prefs->GetList(prefs::kDesktopNotificationAllowedOrigins);

    if (!prefs->FindPreference(prefs::kDesktopNotificationDeniedOrigins))
      prefs->RegisterListPref(prefs::kDesktopNotificationDeniedOrigins);
    denied_sites = prefs->GetList(prefs::kDesktopNotificationDeniedOrigins);
  }

  prefs_cache_ = new NotificationsPrefsCache(allowed_sites, denied_sites);

  ExtensionsService* ext_service = profile_->GetExtensionsService();
  if (ext_service) {
    const ExtensionList* extensions = ext_service->extensions();
    for (ExtensionList::const_iterator iter = extensions->begin();
         iter != extensions->end(); ++iter) {
      if ((*iter)->HasApiPermission(Extension::kNotificationPermission)) {
        prefs_cache_->CacheAllowedOrigin((*iter)->url());
      }
    }
  }
}

void DesktopNotificationService::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  if (type != NotificationType::EXTENSION_LOADED) {
    NOTREACHED();
    return;
  }

  Details<Extension> extension = static_cast<Details<Extension> >(details);
  if (extension->HasApiPermission(Extension::kNotificationPermission)) {
    GrantPermission(extension->url());
  }
}

void DesktopNotificationService::GrantPermission(const GURL& origin) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  PersistPermissionChange(origin, true);

  // Schedule a cache update on the IO thread.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          prefs_cache_.get(), &NotificationsPrefsCache::CacheAllowedOrigin,
          origin));
}

void DesktopNotificationService::DenyPermission(const GURL& origin) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  PersistPermissionChange(origin, false);

  // Schedule a cache update on the IO thread.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          prefs_cache_.get(), &NotificationsPrefsCache::CacheDeniedOrigin,
          origin));
}

void DesktopNotificationService::PersistPermissionChange(
    const GURL& origin, bool is_allowed) {
  // Don't persist changes when off the record.
  if (profile_->IsOffTheRecord())
    return;

  PrefService* prefs = profile_->GetPrefs();
  ListValue* allowed_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationAllowedOrigins);
  ListValue* denied_sites =
      prefs->GetMutableList(prefs::kDesktopNotificationDeniedOrigins);
  StringValue* value = new StringValue(origin.spec());
  // Remove from one list and add to the other.
  if (is_allowed) {
    allowed_sites->Append(value);
    denied_sites->Remove(*value);
  } else {
    allowed_sites->Remove(*value);
    denied_sites->Append(value);
  }
  prefs->ScheduleSavePersistentPrefs();
}

void DesktopNotificationService::RequestPermission(
    const GURL& origin, int process_id, int route_id, int callback_context) {
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

  std::wstring display_name = DisplayNameForOrigin(origin);

  tab->AddInfoBar(new NotificationPermissionInfoBarDelegate(
      tab, origin, display_name, process_id, route_id, callback_context));
}

void DesktopNotificationService::ShowNotification(
    const Notification& notification) {
  ui_manager_->Add(notification, profile_);
}

bool DesktopNotificationService::CancelDesktopNotification(
    int process_id, int route_id, int notification_id) {
  scoped_refptr<NotificationObjectProxy> proxy(
      new NotificationObjectProxy(process_id, route_id, notification_id,
                                  false));
  Notification notif(GURL(), GURL(), L"", proxy);
  return ui_manager_->Cancel(notif);
}


bool DesktopNotificationService::ShowDesktopNotification(
    const GURL& origin, const GURL& url, int process_id, int route_id,
    DesktopNotificationSource source, int notification_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  NotificationObjectProxy* proxy =
      new NotificationObjectProxy(process_id, route_id,
                                  notification_id,
                                  source == WorkerNotification);
  Notification notif(origin, url, DisplayNameForOrigin(origin), proxy);
  ShowNotification(notif);
  return true;
}

bool DesktopNotificationService::ShowDesktopNotificationText(
    const GURL& origin, const GURL& icon, const string16& title,
    const string16& text, int process_id, int route_id,
    DesktopNotificationSource source, int notification_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  NotificationObjectProxy* proxy =
      new NotificationObjectProxy(process_id, route_id,
                                  notification_id,
                                  source == WorkerNotification);
  // "upconvert" the string parameters to a data: URL.
  string16 data_url = CreateDataUrl(icon, title, text);
  Notification notif(
      origin, GURL(data_url), DisplayNameForOrigin(origin), proxy);
  ShowNotification(notif);
  return true;
}

std::wstring DesktopNotificationService::DisplayNameForOrigin(
    const GURL& origin) {
  // If the source is an extension, lookup the display name.
  if (origin.SchemeIs(chrome::kExtensionScheme)) {
    ExtensionsService* ext_service = profile_->GetExtensionsService();
    if (ext_service) {
      Extension* extension = ext_service->GetExtensionByURL(origin);
      if (extension)
        return ASCIIToWide(extension->name());
    }
  }
  return UTF8ToWide(origin.spec());
}
