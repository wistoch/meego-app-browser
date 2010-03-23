// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_disabled_infobar_delegate.h"

#include "app/l10n_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/browser_list.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"

class ExtensionDisabledDialogDelegate
    : public ExtensionInstallUI::Delegate,
      public base::RefCountedThreadSafe<ExtensionDisabledDialogDelegate> {
 public:
  ExtensionDisabledDialogDelegate(Profile* profile,
                                  ExtensionsService* service,
                                  Extension* extension)
        : profile_(profile), service_(service), extension_(extension) {
    AddRef();  // Balanced in Proceed or Abort.

    // Do this now because we can't touch extension on the file loop.
    install_icon_resource_ =
        extension_->GetIconPath(Extension::EXTENSION_ICON_LARGE);

    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &ExtensionDisabledDialogDelegate::Start));
  }

  // ExtensionInstallUI::Delegate
  virtual void InstallUIProceed(bool create_app_shortcut) {
    ExtensionPrefs* prefs = service_->extension_prefs();
    prefs->SetDidExtensionEscalatePermissions(extension_, false);
    service_->EnableExtension(extension_->id());
    Release();
  }
  virtual void InstallUIAbort() {
    // Do nothing. The extension will remain disabled.
    Release();
  }

 private:
  friend class base::RefCountedThreadSafe<ExtensionDisabledDialogDelegate>;

  virtual ~ExtensionDisabledDialogDelegate() {}

  void Start() {
    // We start on the file thread so we can decode the install icon.
    FilePath install_icon_path = install_icon_resource_.GetFilePath();
    Extension::DecodeIconFromPath(
        install_icon_path, Extension::EXTENSION_ICON_LARGE, &install_icon_);
    // Then we display the UI on the UI thread.
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &ExtensionDisabledDialogDelegate::ConfirmInstall));
  }

  void ConfirmInstall() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    ExtensionInstallUI ui(profile_);
    ui.ConfirmInstall(this, extension_, install_icon_.get());
  }

  Profile* profile_;
  ExtensionsService* service_;
  Extension* extension_;
  ExtensionResource install_icon_resource_;
  scoped_ptr<SkBitmap> install_icon_;
};

class ExtensionDisabledInfobarDelegate
    : public ConfirmInfoBarDelegate,
      public NotificationObserver {
 public:
  ExtensionDisabledInfobarDelegate(TabContents* tab_contents,
                                   ExtensionsService* service,
                                   Extension* extension)
      : ConfirmInfoBarDelegate(tab_contents),
        tab_contents_(tab_contents),
        service_(service),
        extension_(extension) {
    // The user might re-enable the extension in other ways, so watch for that.
    registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                   Source<Profile>(service->profile()));
    registrar_.Add(this, NotificationType::EXTENSION_UNLOADED_DISABLED,
                   Source<Profile>(service->profile()));
  }
  virtual ~ExtensionDisabledInfobarDelegate() {
  }
  virtual std::wstring GetMessageText() const {
    return l10n_util::GetStringF(IDS_EXTENSION_DISABLED_INFOBAR_LABEL,
                                 UTF8ToWide(extension_->name()));
  }
  virtual SkBitmap* GetIcon() const {
    return NULL;
  }
  virtual int GetButtons() const {
    return BUTTON_OK;
  }
  virtual std::wstring GetButtonLabel(
      ConfirmInfoBarDelegate::InfoBarButton button) const {
    return l10n_util::GetString(IDS_EXTENSION_DISABLED_INFOBAR_ENABLE_BUTTON);
  }
  virtual bool Accept() {
    // This object manages its own lifetime.
    new ExtensionDisabledDialogDelegate(tab_contents_->profile(),
                                        service_, extension_);
    return true;
  }
  virtual void InfoBarClosed() {
    delete this;
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    // TODO(mpcomplete): RemoveInfoBar doesn't seem to always result in us
    // getting deleted.
    switch (type.value) {
      case NotificationType::EXTENSION_LOADED:
      case NotificationType::EXTENSION_UNLOADED_DISABLED: {
        Extension* extension = Details<Extension>(details).ptr();
        if (extension == extension_)
          tab_contents_->RemoveInfoBar(this);
        break;
      }
      default:
        NOTREACHED();
    }
  }

 private:
  NotificationRegistrar registrar_;
  TabContents* tab_contents_;
  ExtensionsService* service_;
  Extension* extension_;
};

void ShowExtensionDisabledUI(ExtensionsService* service, Profile* profile,
                             Extension* extension) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (!browser)
    return;

  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents)
    return;

  tab_contents->AddInfoBar(new ExtensionDisabledInfobarDelegate(
      tab_contents, service, extension));
}

void ShowExtensionDisabledDialog(ExtensionsService* service, Profile* profile,
                                 Extension* extension) {
  // This object manages its own lifetime.
  new ExtensionDisabledDialogDelegate(profile, service, extension);
}
