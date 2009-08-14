// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_process_manager.h"

#include "chrome/browser/browsing_instance.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/render_messages.h"

static void CreateBackgroundHosts(
    ExtensionProcessManager* manager, const ExtensionList* extensions) {
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    // Start the process for the master page, if it exists.
    if ((*extension)->background_url().is_valid())
      manager->CreateBackgroundHost(*extension, (*extension)->background_url());
  }
}

ExtensionProcessManager::ExtensionProcessManager(Profile* profile)
    : browsing_instance_(new BrowsingInstance(profile)) {
  registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSIONS_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_HOST_DESTROYED,
                 Source<Profile>(profile));
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
}

ExtensionProcessManager::~ExtensionProcessManager() {
  // Copy all_hosts_ to avoid iterator invalidation issues.
  ExtensionHostSet to_delete(background_hosts_.begin(),
                             background_hosts_.end());
  ExtensionHostSet::iterator iter;
  for (iter = to_delete.begin(); iter != to_delete.end(); ++iter)
    delete *iter;
}

ExtensionHost* ExtensionProcessManager::CreateView(Extension* extension,
                                                   const GURL& url,
                                                   Browser* browser) {
  DCHECK(extension);
  DCHECK(browser);
  ExtensionHost* host =
      new ExtensionHost(extension, GetSiteInstanceForURL(url), url,
                        ViewType::EXTENSION_TOOLSTRIP);
  host->CreateView(browser);
  OnExtensionHostCreated(host, false);
  return host;
}

ExtensionHost* ExtensionProcessManager::CreateView(const GURL& url,
                                                   Browser* browser) {
  DCHECK(browser);
  ExtensionsService* service =
    browsing_instance_->profile()->GetExtensionsService();
  if (service) {
    Extension* extension = service->GetExtensionByURL(url);
    if (extension)
      return CreateView(extension, url, browser);
  }
  return NULL;
}

ExtensionHost* ExtensionProcessManager::CreateBackgroundHost(
    Extension* extension, const GURL& url) {
  ExtensionHost* host =
      new ExtensionHost(extension, GetSiteInstanceForURL(url), url,
                        ViewType::EXTENSION_BACKGROUND_PAGE);
  host->CreateRenderView(NULL);  // create a RenderViewHost with no view
  OnExtensionHostCreated(host, true);
  return host;
}

void ExtensionProcessManager::RegisterExtensionProcess(
    const std::string& extension_id, int process_id) {
  ProcessIDMap::const_iterator it = process_ids_.find(extension_id);
  if (it != process_ids_.end() && (*it).second == process_id)
    return;

  // Extension ids should get removed from the map before the process ids get
  // reused from a dead renderer.
  DCHECK(it == process_ids_.end());
  process_ids_[extension_id] = process_id;

  ExtensionsService* extension_service =
      browsing_instance_->profile()->GetExtensionsService();

  std::vector<std::string> page_action_ids;
  Extension* extension = extension_service->GetExtensionById(extension_id);
  for (PageActionMap::const_iterator i = extension->page_actions().begin();
       i != extension->page_actions().end(); ++i) {
    page_action_ids.push_back(i->first);
  }

  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  rph->Send(new ViewMsg_Extension_UpdatePageActions(extension_id,
                                                    page_action_ids));
}

void ExtensionProcessManager::UnregisterExtensionProcess(int process_id) {
  ProcessIDMap::iterator it = process_ids_.begin();
  while (it != process_ids_.end()) {
    if (it->second == process_id)
      process_ids_.erase(it++);
    else
      ++it;
  }
}

RenderProcessHost* ExtensionProcessManager::GetExtensionProcess(
    const std::string& extension_id) {
  ProcessIDMap::const_iterator it = process_ids_.find(extension_id);
  if (it == process_ids_.end())
    return NULL;

  RenderProcessHost* rph = RenderProcessHost::FromID(it->second);
  DCHECK(rph) << "We should have unregistered this host.";
  return rph;
}

SiteInstance* ExtensionProcessManager::GetSiteInstanceForURL(const GURL& url) {
  return browsing_instance_->GetSiteInstanceForURL(url);
}

void ExtensionProcessManager::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_READY:
      CreateBackgroundHosts(this,
          Source<ExtensionsService>(source).ptr()->extensions());
      break;

    case NotificationType::EXTENSIONS_LOADED: {
      ExtensionsService* service = Source<ExtensionsService>(source).ptr();
      if (service->is_ready()) {
        const ExtensionList* extensions = Details<ExtensionList>(details).ptr();
        CreateBackgroundHosts(this, extensions);
      }
      break;
    }

    case NotificationType::EXTENSION_UNLOADED: {
      Extension* extension = Details<Extension>(details).ptr();
      for (ExtensionHostSet::iterator iter = background_hosts_.begin();
           iter != background_hosts_.end(); ++iter) {
        ExtensionHost* host = *iter;
        if (host->extension()->id() == extension->id()) {
          delete host;
          // |host| should deregister itself from our structures.
          DCHECK(background_hosts_.find(host) == background_hosts_.end());
          break;
        }
      }
      break;
    }

    case NotificationType::EXTENSION_HOST_DESTROYED: {
      ExtensionHost* host = Details<ExtensionHost>(details).ptr();
      all_hosts_.erase(host);
      background_hosts_.erase(host);
      break;
    }

    case NotificationType::RENDERER_PROCESS_TERMINATED:
    case NotificationType::RENDERER_PROCESS_CLOSED: {
      RenderProcessHost* host = Source<RenderProcessHost>(source).ptr();
      UnregisterExtensionProcess(host->pid());
      break;
    }

    default:
      NOTREACHED();
  }
}

void ExtensionProcessManager::OnExtensionHostCreated(ExtensionHost* host,
                                                     bool is_background) {
  all_hosts_.insert(host);
  if (is_background)
    background_hosts_.insert(host);
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_HOST_CREATED,
      Source<ExtensionProcessManager>(this),
      Details<ExtensionHost>(host));
}
