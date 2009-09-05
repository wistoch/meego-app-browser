// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_PROFILE_IMPL_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_PROFILE_IMPL_H_

#include "chrome/browser/profile.h"
#include "net/url_request/url_request_context.h"

namespace net {
class CookieStore;
}

// Automation overrides for profile settings.
class AutomationProfileImpl : public Profile {
 public:
  AutomationProfileImpl() : original_profile_(NULL),
                            tab_handle_(0) {
  }

  void Initialize(Profile* original_profile,
                  IPC::Message::Sender* automation_client);

  void set_tab_handle(int tab_handle) {
    tab_handle_ = tab_handle;
  }
  int tab_handle() const {
    return tab_handle_;
  }

  // Profile implementation.
  virtual FilePath GetPath() {
    return original_profile_->GetPath();
  }
  virtual bool IsOffTheRecord() {
    return original_profile_->IsOffTheRecord();
  }
  virtual Profile* GetOffTheRecordProfile() {
    return original_profile_->GetOffTheRecordProfile();
  }
  virtual void DestroyOffTheRecordProfile() {
    return original_profile_->DestroyOffTheRecordProfile();
  }
  virtual Profile* GetOriginalProfile() {
    return original_profile_->GetOriginalProfile();
  }
  virtual ChromeAppCacheService* GetAppCacheService() {
    return original_profile_->GetAppCacheService();
  }
  virtual VisitedLinkMaster* GetVisitedLinkMaster() {
    return original_profile_->GetVisitedLinkMaster();
  }
  virtual ExtensionsService* GetExtensionsService() {
    return original_profile_->GetExtensionsService();
  }
  virtual UserScriptMaster* GetUserScriptMaster() {
    return original_profile_->GetUserScriptMaster();
  }
  virtual ExtensionDevToolsManager* GetExtensionDevToolsManager() {
    return original_profile_->GetExtensionDevToolsManager();
  }
  virtual ExtensionProcessManager* GetExtensionProcessManager() {
    return original_profile_->GetExtensionProcessManager();
  }
  virtual ExtensionMessageService* GetExtensionMessageService() {
    return original_profile_->GetExtensionMessageService();
  }
  virtual SSLHostState* GetSSLHostState() {
    return original_profile_->GetSSLHostState();
  }
  virtual net::ForceTLSState* GetForceTLSState() {
    return original_profile_->GetForceTLSState();
  }
  virtual FaviconService* GetFaviconService(ServiceAccessType access) {
    return original_profile_->GetFaviconService(access);
  }
  virtual HistoryService* GetHistoryService(ServiceAccessType access) {
    return original_profile_->GetHistoryService(access);
  }
  virtual WebDataService* GetWebDataService(ServiceAccessType access) {
    return original_profile_->GetWebDataService(access);
  }
  virtual PasswordStore* GetPasswordStore(ServiceAccessType access) {
    return original_profile_->GetPasswordStore(access);
  }
  virtual PrefService* GetPrefs() {
    return original_profile_->GetPrefs();
  }
  virtual TemplateURLModel* GetTemplateURLModel() {
    return original_profile_->GetTemplateURLModel();
  }
  virtual TemplateURLFetcher* GetTemplateURLFetcher() {
    return original_profile_->GetTemplateURLFetcher();
  }
  virtual DownloadManager* GetDownloadManager() {
    return original_profile_->GetDownloadManager();
  }
  virtual bool HasCreatedDownloadManager() const {
    return original_profile_->HasCreatedDownloadManager();
  }
  virtual void InitThemes() {
    return original_profile_->InitThemes();
  }
  virtual void SetTheme(Extension* extension) {
    return original_profile_->SetTheme(extension);
  }
  virtual void SetNativeTheme() {
    return original_profile_->SetNativeTheme();
  }
  virtual void ClearTheme() {
    return original_profile_->ClearTheme();
  }
  virtual Extension* GetTheme() {
    return original_profile_->GetTheme();
  }
  virtual ThemeProvider* GetThemeProvider() {
    return original_profile_->GetThemeProvider();
  }
  virtual ThumbnailStore* GetThumbnailStore() {
    return original_profile_->GetThumbnailStore();
  }
  virtual URLRequestContext* GetRequestContext() {
    return alternate_reqeust_context_;
  }
  virtual URLRequestContext* GetRequestContextForMedia() {
    return original_profile_->GetRequestContextForMedia();
  }
  virtual URLRequestContext* GetRequestContextForExtensions() {
    return original_profile_->GetRequestContextForExtensions();
  }
  virtual net::SSLConfigService* GetSSLConfigService() {
    return original_profile_->GetSSLConfigService();
  }
  virtual Blacklist* GetBlacklist() {
    return original_profile_->GetBlacklist();
  }
  virtual SessionService* GetSessionService() {
    return original_profile_->GetSessionService();
  }
  virtual void ShutdownSessionService() {
    return original_profile_->ShutdownSessionService();
  }
  virtual bool HasSessionService() const {
    return original_profile_->HasSessionService();
  }
  virtual std::wstring GetName() {
    return original_profile_->GetName();
  }
  virtual void SetName(const std::wstring& name) {
    return original_profile_->SetName(name);
  }
  virtual std::wstring GetID() {
    return original_profile_->GetID();
  }
  virtual void SetID(const std::wstring& id) {
    return original_profile_->SetID(id);
  }
  virtual bool DidLastSessionExitCleanly() {
    return original_profile_->DidLastSessionExitCleanly();
  }
  virtual BookmarkModel* GetBookmarkModel() {
    return original_profile_->GetBookmarkModel();
  }

#ifdef CHROME_PERSONALIZATION
  virtual ProfileSyncService* GetProfileSyncService() {
    return original_profile_->GetProfileSyncService();
  }
#endif

  virtual bool IsSameProfile(Profile* profile) {
    return original_profile_->IsSameProfile(profile);
  }
  virtual base::Time GetStartTime() const {
    return original_profile_->GetStartTime();
  }
  virtual TabRestoreService* GetTabRestoreService() {
    return original_profile_->GetTabRestoreService();
  }
  virtual void ResetTabRestoreService() {
    return original_profile_->ResetTabRestoreService();
  }
  virtual void ReinitializeSpellChecker() {
    return original_profile_->ReinitializeSpellChecker();
  }
  virtual SpellChecker* GetSpellChecker() {
    return original_profile_->GetSpellChecker();
  }
  virtual WebKitContext* GetWebKitContext() {
    return original_profile_->GetWebKitContext();
  }
  virtual void MarkAsCleanShutdown() {
    return original_profile_->MarkAsCleanShutdown();
  }
  virtual void InitExtensions() {
    return original_profile_->InitExtensions();
  }
  virtual void InitWebResources() {
    return original_profile_->InitWebResources();
  }

 protected:
  Profile* original_profile_;
  scoped_refptr<net::CookieStore> alternate_cookie_store_;
  scoped_refptr<URLRequestContext> alternate_reqeust_context_;
  int tab_handle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomationProfileImpl);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_PROFILE_IMPL_H_
