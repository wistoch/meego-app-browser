// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_service.h"

#include "base/json_writer.h"
#include "base/singleton.h"
#include "base/stl_util-inl.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

// Since we have 2 ports for every channel, we just index channels by half the
// port ID.
#define GET_CHANNEL_ID(port_id) ((port_id) / 2)

// Port1 is always even, port2 is always odd.
#define IS_PORT1_ID(port_id) (((port_id) & 1) == 0)

// Change even to odd and vice versa, to get the other side of a given channel.
#define GET_OPPOSITE_PORT_ID(source_port_id) ((source_port_id) ^ 1)

namespace {
typedef std::map<URLRequestContext*, ExtensionMessageService*> InstanceMap;
struct SingletonData {
  ~SingletonData() {
    STLDeleteContainerPairSecondPointers(map.begin(), map.end());
  }
  Lock lock;
  InstanceMap map;
};
}  // namespace

// Since ExtensionMessageService is a collection of Singletons, we don't need to
// grab a reference to it when creating Tasks involving it.
template <> struct RunnableMethodTraits<ExtensionMessageService> {
  static void RetainCallee(ExtensionMessageService*) {}
  static void ReleaseCallee(ExtensionMessageService*) {}
};

// static
ExtensionMessageService* ExtensionMessageService::GetInstance(
    URLRequestContext* context) {
  SingletonData* data = Singleton<SingletonData>::get();
  AutoLock lock(data->lock);

  ExtensionMessageService* instance = data->map[context];
  if (!instance) {
    instance = new ExtensionMessageService();
    data->map[context] = instance;
  }
  return instance;
}

ExtensionMessageService::ExtensionMessageService()
    : ui_loop_(NULL), initialized_(false), next_port_id_(0) {
}

void ExtensionMessageService::Init() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  if (initialized_)
    return;
  initialized_ = true;

  ui_loop_ = MessageLoop::current();

  // Note: we never stop observing because we live longer than
  // NotificationService.
  NotificationService::current()->AddObserver(this,
      NotificationType::RENDERER_PROCESS_TERMINATED,
      NotificationService::AllSources());
  NotificationService::current()->AddObserver(this,
      NotificationType::RENDERER_PROCESS_CLOSED,
      NotificationService::AllSources());
}

void ExtensionMessageService::RegisterExtension(
    const std::string& extension_id, int render_process_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // Make sure we're initialized.
  Init();

  AutoLock lock(process_ids_lock_);
  DCHECK(process_ids_.find(extension_id) == process_ids_.end() ||
         process_ids_[extension_id] == render_process_id);
  process_ids_[extension_id] = render_process_id;
}

void ExtensionMessageService::AddEventListener(std::string event_name,
                                               int render_process_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK(listeners_[event_name].count(render_process_id) == 0);
  listeners_[event_name].insert(render_process_id);
}

void ExtensionMessageService::RemoveEventListener(std::string event_name,
                                                  int render_process_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK(listeners_[event_name].count(render_process_id) == 1);
  listeners_[event_name].erase(render_process_id);
}

int ExtensionMessageService::GetProcessIdForExtension(
    const std::string& extension_id) {
  AutoLock lock(process_ids_lock_);
  ProcessIDMap::iterator process_id_it = process_ids_.find(
      StringToLowerASCII(extension_id));
  if (process_id_it == process_ids_.end())
    return -1;
  return process_id_it->second;
}

RenderProcessHost* ExtensionMessageService::GetProcessForExtension(
    const std::string& extension_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  int process_id = GetProcessIdForExtension(extension_id);
  if (process_id == -1)
    return NULL;

  RenderProcessHost* host = RenderProcessHost::FromID(process_id);
  DCHECK(host);

  return host;
}

int ExtensionMessageService::OpenChannelToExtension(
    int routing_id, const std::string& extension_id,
    ResourceMessageFilter* source) {
  DCHECK_EQ(MessageLoop::current(),
            ChromeThread::GetMessageLoop(ChromeThread::IO));

  // Lookup the targeted extension process.
  int process_id = GetProcessIdForExtension(extension_id);
  if (process_id == -1)
    return -1;

  DCHECK(initialized_);

  // Create a channel ID for both sides of the channel.
  // TODO(mpcomplete): what happens when this wraps?
  int port1_id = next_port_id_++;
  int port2_id = next_port_id_++;
  DCHECK(IS_PORT1_ID(port1_id));
  DCHECK(GET_OPPOSITE_PORT_ID(port1_id) == port2_id);
  DCHECK(GET_OPPOSITE_PORT_ID(port2_id) == port1_id);
  DCHECK(GET_CHANNEL_ID(port1_id) == GET_CHANNEL_ID(port2_id));

  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ExtensionMessageService::OpenChannelOnUIThread,
          routing_id, port1_id, source->GetProcessId(), port2_id, process_id));

  return port2_id;
}

void ExtensionMessageService::OpenChannelOnUIThread(
    int source_routing_id, int source_port_id, int source_process_id,
    int dest_port_id, int dest_process_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  MessageChannel channel;
  channel.port1 = RenderProcessHost::FromID(source_process_id);
  channel.port2 = RenderProcessHost::FromID(dest_process_id);
  if (!channel.port1 || !channel.port2) {
    // One of the processes could have been closed while posting this task.
    return;
  }

  channels_[GET_CHANNEL_ID(source_port_id)] = channel;

  std::string tab_json = "null";
  TabContents* contents = tab_util::GetTabContentsByID(source_process_id,
                                                       source_routing_id);
  if (contents) {
    DictionaryValue* tab_value = ExtensionTabUtil::CreateTabValue(contents);
    JSONWriter::Write(tab_value, false, &tab_json);
  }

  // Send each process the id for the opposite port.
  channel.port2->Send(new ViewMsg_ExtensionHandleConnect(source_port_id,
                                                         tab_json));
}

void ExtensionMessageService::PostMessageFromRenderer(
    int port_id, const std::string& message) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  MessageChannelMap::iterator iter =
      channels_.find(GET_CHANNEL_ID(port_id));
  if (iter == channels_.end())
    return;
  MessageChannel& channel = iter->second;

  // Figure out which port the ID corresponds to.
  RenderProcessHost* dest =
      IS_PORT1_ID(port_id) ? channel.port1 : channel.port2;

  int source_port_id = GET_OPPOSITE_PORT_ID(port_id);
  dest->Send(new ViewMsg_ExtensionHandleMessage(message, source_port_id));
}

void ExtensionMessageService::DispatchEventToRenderers(
    const std::string& event_name, const std::string& event_args) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  std::set<int>& pids = listeners_[event_name];

  // Send the event only to renderers that are listening for it.
  for (std::set<int>::iterator pid = pids.begin(); pid != pids.end(); ++pid) {
    RenderProcessHost* renderer = RenderProcessHost::FromID(*pid);
    if (!renderer)
      continue;
    renderer->Send(new ViewMsg_ExtensionHandleEvent(event_name, event_args));
  }
}

void ExtensionMessageService::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  DCHECK(type.value == NotificationType::RENDERER_PROCESS_TERMINATED ||
         type.value == NotificationType::RENDERER_PROCESS_CLOSED);

  RenderProcessHost* renderer = Source<RenderProcessHost>(source).ptr();

  {
    AutoLock lock(process_ids_lock_);
    for (ProcessIDMap::iterator it = process_ids_.begin();
         it != process_ids_.end(); ) {
      ProcessIDMap::iterator current = it++;
      if (current->second == renderer->pid()) {
        process_ids_.erase(current);
      }
    }
  }

  // Close any channels that share this renderer.
  // TODO(mpcomplete): should we notify the other side of the port?
  for (MessageChannelMap::iterator it = channels_.begin();
       it != channels_.end(); ) {
    MessageChannelMap::iterator current = it++;
    if (current->second.port1 == renderer || current->second.port2 == renderer)
      channels_.erase(current);
  }
}
