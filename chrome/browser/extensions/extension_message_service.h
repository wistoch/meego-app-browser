// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_

#include <map>
#include <set>
#include <string>

#include "base/linked_ptr.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/notification_registrar.h"

class MessageLoop;
class Profile;
class RenderProcessHost;
class ResourceMessageFilter;
class URLRequestContext;

// This class manages message and event passing between renderer processes.
// It maintains a list of processes that are listening to events and a set of
// open channels.
//
// Messaging works this way:
// - An extension-owned script context (like a toolstrip or a content script)
// adds an event listener to the "onConnect" event.
// - Another context calls "extension.connect()" to open a channel to the
// extension process, or an extension context calls "tabs.connect(tabId)" to
// open a channel to the content scripts for the given tab.  The EMS notifies
// the target process/tab, which then calls the onConnect event in every
// context owned by the connecting extension in that process/tab.
// - Once the channel is established, either side can call postMessage to send
// a message to the opposite side of the channel, which may have multiple
// listeners.
//
// Terminology:
// channel: connection between two ports
// port: an IPC::Message::Sender interface and an optional routing_id (in the
// case that the port is a tab).  The Sender is usually either a
// RenderProcessHost or a RenderViewHost.
class ExtensionMessageService :
    public base::RefCountedThreadSafe<ExtensionMessageService>,
    public NotificationObserver {
 public:
  // Javascript function name constants.
  static const char kDispatchOnConnect[];
  static const char kDispatchOnDisconnect[];
  static const char kDispatchOnMessage[];
  static const char kDispatchEvent[];
  static const char kDispatchError[];

  // A messaging channel.  Note that the opening port can be the same as the
  // receiver, if an extension toolstrip wants to talk to its tab (for example).
  struct MessageChannel;
  struct MessagePort;

  // --- UI thread only:

  ExtensionMessageService(Profile* profile);
  ~ExtensionMessageService();

  // Notification that our owning profile is going away.
  void ProfileDestroyed();

  // Add or remove |render_process_pid| as a listener for |event_name|.
  void AddEventListener(std::string event_name, int render_process_id);
  void RemoveEventListener(std::string event_name, int render_process_id);

  // Closes the message channel associated with the given port, and notifies
  // the other side.
  void CloseChannel(int port_id);

  // Sends a message from a renderer to the given port.
  void PostMessageFromRenderer(int port_id, const std::string& message);

  // Send an event to every registered extension renderer.
  void DispatchEventToRenderers(
      const std::string& event_name, const std::string& event_args);

  // Given an extension's ID, opens a channel between the given automation
  // "port" and that extension.  Returns a channel ID to be used for posting
  // messages between the processes, or -1 if the extension doesn't exist.
  int OpenAutomationChannelToExtension(int source_process_id,
                                       int routing_id,
                                       const std::string& extension_id,
                                       IPC::Message::Sender* source);

  // --- IO thread only:

  // Given an extension's ID, opens a channel between the given renderer "port"
  // and every listening context owned by that extension.  Returns a port ID
  // to be used for posting messages between the processes.  |channel_name| is
  // an optional identifier for use by extension developers.
  // This runs on the IO thread so that it can be used in a synchronous IPC
  // message.
  int OpenChannelToExtension(int routing_id, const std::string& extension_id,
                             const std::string& channel_name,
                             ResourceMessageFilter* source);

  // Same as above, but opens a channel to the tab with the given ID.  Messages
  // are restricted to that tab, so if there are multiple tabs in that process,
  // only the targeted tab will receive messages.
  int OpenChannelToTab(int routing_id, int tab_id, 
                       const std::string& extension_id,
                       const std::string& channel_name,
                       ResourceMessageFilter* source);

 private:
  // A map of channel ID to its channel object.
  typedef std::map<int, linked_ptr<MessageChannel> > MessageChannelMap;

  // Allocates a pair of port ids.
  // NOTE: this can be called from any thread.
  void AllocatePortIdPair(int* port1, int* port2);

  void CloseChannelImpl(MessageChannelMap::iterator channel_iter, int port_id);

  // The UI message loop, used for posting tasks.
  MessageLoop* ui_loop_;

  // --- UI thread only:

  // Handles channel creation and notifies the destinations that a channel was
  // opened.
  void OpenChannelToExtensionOnUIThread(
    int source_process_id, int source_routing_id, int receiver_port_id,
    const std::string& extension_id, const std::string& channel_name);

  void OpenChannelToTabOnUIThread(
    int source_process_id, int source_routing_id, int receiver_port_id,
    int tab_id, const std::string& extension_id,
    const std::string& channel_name);

  // Common between OpenChannelOnUIThread and OpenAutomationChannelToExtension.
  void OpenChannelOnUIThreadImpl(
    IPC::Message::Sender* source, int source_process_id, int source_routing_id,
    const MessagePort& receiver, int receiver_port_id,
    const std::string& extension_id, const std::string& channel_name);

  // NotificationObserver interface.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // An IPC sender that might be in our list of channels has closed.
  void OnSenderClosed(IPC::Message::Sender* sender);

  Profile* profile_;

  NotificationRegistrar registrar_;

  MessageChannelMap channels_;

  // A map between an event name and a set of process id's that are listening
  // to that event.
  typedef std::map<std::string, std::set<int> > ListenerMap;
  ListenerMap listeners_;

  // --- UI or IO thread:

  // For generating unique channel IDs.
  int next_port_id_;

  // Protects the next_port_id_ variable, since it can be
  // used on the IO thread or the UI thread.
  Lock next_port_id_lock_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
