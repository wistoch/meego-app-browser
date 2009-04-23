// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/plugin_service.h"

#include "base/command_line.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_host.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/render_messages.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_list.h"

// static
PluginService* PluginService::GetInstance() {
  return Singleton<PluginService>::get();
}

PluginService::PluginService()
    : main_message_loop_(MessageLoop::current()),
      resource_dispatcher_host_(NULL),
      ui_locale_(g_browser_process->GetApplicationLocale()) {
  // Have the NPAPI plugin list search for Chrome plugins as well.
  ChromePluginLib::RegisterPluginsWithNPAPI();
  // Load the one specified on the command line as well.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  std::wstring path = command_line->GetSwitchValue(switches::kLoadPlugin);
  if (!path.empty())
    NPAPI::PluginList::AddExtraPluginPath(FilePath::FromWStringHack(path));

#if defined(OS_WIN)
  hkcu_key_.Create(
      HKEY_CURRENT_USER, kRegistryMozillaPlugins, KEY_NOTIFY);
  hklm_key_.Create(
      HKEY_LOCAL_MACHINE, kRegistryMozillaPlugins, KEY_NOTIFY);
  if (hkcu_key_.StartWatching()) {
    hkcu_event_.reset(new base::WaitableEvent(hkcu_key_.watch_event()));
    hkcu_watcher_.StartWatching(hkcu_event_.get(), this);
  }

  if (hklm_key_.StartWatching()) {
    hklm_event_.reset(new base::WaitableEvent(hklm_key_.watch_event()));
    hklm_watcher_.StartWatching(hklm_event_.get(), this);
  }
#endif
}

PluginService::~PluginService() {
#if defined(OS_WIN)
  // Release the events since they're owned by RegKey, not WaitableEvent.
  hkcu_watcher_.StopWatching();
  hklm_watcher_.StopWatching();
  hkcu_event_->Release();
  hklm_event_->Release();
#endif
}

void PluginService::GetPlugins(bool refresh,
                               std::vector<WebPluginInfo>* plugins) {
  AutoLock lock(lock_);
  NPAPI::PluginList::Singleton()->GetPlugins(refresh, plugins);
}

void PluginService::LoadChromePlugins(
    ResourceDispatcherHost* resource_dispatcher_host) {
  resource_dispatcher_host_ = resource_dispatcher_host;
  ChromePluginLib::LoadChromePlugins(GetCPBrowserFuncsForBrowser());
}

void PluginService::SetChromePluginDataDir(const FilePath& data_dir) {
  AutoLock lock(lock_);
  chrome_plugin_data_dir_ = data_dir;
}

const FilePath& PluginService::GetChromePluginDataDir() {
  AutoLock lock(lock_);
  return chrome_plugin_data_dir_;
}

void PluginService::AddExtraPluginDir(const FilePath& plugin_dir) {
  AutoLock lock(lock_);
  NPAPI::PluginList::ResetPluginsLoaded();
  NPAPI::PluginList::AddExtraPluginDir(plugin_dir);
}

const std::wstring& PluginService::GetUILocale() {
  return ui_locale_;
}

PluginProcessHost* PluginService::FindPluginProcess(
    const FilePath& plugin_path) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  if (plugin_path.value().empty()) {
    NOTREACHED() << "should only be called if we have a plugin to load";
    return NULL;
  }

  for (ChildProcessHost::Iterator iter(ChildProcessInfo::PLUGIN_PROCESS);
       !iter.Done(); ++iter) {
    PluginProcessHost* plugin = static_cast<PluginProcessHost*>(*iter);
    if (plugin->info().path == plugin_path)
      return plugin;
  }

  return NULL;
}

PluginProcessHost* PluginService::FindOrStartPluginProcess(
    const FilePath& plugin_path,
    const std::string& clsid) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  PluginProcessHost *plugin_host = FindPluginProcess(plugin_path);
  if (plugin_host)
    return plugin_host;

  WebPluginInfo info;
  if (!GetPluginInfoByPath(plugin_path, &info)) {
    DCHECK(false);
    return NULL;
  }

  // This plugin isn't loaded by any plugin process, so create a new process.
  plugin_host = new PluginProcessHost();
  if (!plugin_host->Init(info, clsid, ui_locale_)) {
    DCHECK(false);  // Init is not expected to fail
    delete plugin_host;
    return NULL;
  }

  return plugin_host;

  // TODO(jabdelmalek): adding a new channel means we can have one less
  // renderer process (since each child process uses one handle in the
  // IPC thread and main thread's WaitForMultipleObjects call).  Limit the
  // number of plugin processes.
}

void PluginService::OpenChannelToPlugin(
    ResourceMessageFilter* renderer_msg_filter, const GURL& url,
    const std::string& mime_type, const std::string& clsid,
    const std::wstring& locale, IPC::Message* reply_msg) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));
  FilePath plugin_path = GetPluginPath(url, mime_type, clsid, NULL);
  PluginProcessHost* plugin_host = FindOrStartPluginProcess(plugin_path, clsid);
  if (plugin_host) {
    plugin_host->OpenChannelToPlugin(renderer_msg_filter, mime_type, reply_msg);
  } else {
    PluginProcessHost::ReplyToRenderer(renderer_msg_filter,
                                       std::wstring(),
                                       FilePath(),
                                       reply_msg);
  }
}

FilePath PluginService::GetPluginPath(const GURL& url,
                                      const std::string& mime_type,
                                      const std::string& clsid,
                                      std::string* actual_mime_type) {
  AutoLock lock(lock_);
  bool allow_wildcard = true;
  WebPluginInfo info;
  NPAPI::PluginList::Singleton()->GetPluginInfo(url, mime_type, clsid,
                                                allow_wildcard, &info,
                                                actual_mime_type);
  return info.path;
}

bool PluginService::GetPluginInfoByPath(const FilePath& plugin_path,
                                        WebPluginInfo* info) {
  AutoLock lock(lock_);
  return NPAPI::PluginList::Singleton()->GetPluginInfoByPath(plugin_path, info);
}

bool PluginService::HavePluginFor(const std::string& mime_type,
                                  bool allow_wildcard) {
  AutoLock lock(lock_);

  GURL url;
  WebPluginInfo info;
  return NPAPI::PluginList::Singleton()->GetPluginInfo(url, mime_type, "",
                                                       allow_wildcard, &info,
                                                       NULL);
}

void PluginService::OnWaitableEventSignaled(base::WaitableEvent* waitable_event) {
#if defined(OS_WIN)
  if (waitable_event == hkcu_event_.get()) {
    hkcu_key_.StartWatching();
  } else {
    hklm_key_.StartWatching();
  }

  AutoLock lock(lock_);
  NPAPI::PluginList::ResetPluginsLoaded();

  for (RenderProcessHost::iterator it = RenderProcessHost::begin();
       it != RenderProcessHost::end(); ++it) {
    it->second->Send(new ViewMsg_PurgePluginListCache());
  }
#endif
}
