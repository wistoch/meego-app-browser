// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSION_PORT_CONTAINER_H_
#define CHROME_BROWSER_EXTENSION_PORT_CONTAINER_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/common/ipc_message.h"

class AutomationProvider;
class ExtensionMessageService;
class MessageLoop;
class RenderViewHost;

// This class represents an external port to an extension, opened
// through the automation interface.
class ExtensionPortContainer : public IPC::Message::Sender {
 public:

  // All external port related messages will have this origin.
  static const char kAutomationOrigin[];
  // All external port message requests should have this target.
  static const char kAutomationRequestTarget[];
  // All external port message responses have this target.
  static const char kAutomationResponseTarget[];

  static const wchar_t kAutomationRequestIdKey[];
  static const wchar_t kAutomationConnectionIdKey[];
  static const wchar_t kAutomationExtensionIdKey[];
  static const wchar_t kAutomationPortIdKey[];
  static const wchar_t kAutomationMessageDataKey[];

  // The command codes for our private message protocol.
  enum PrivateMessageCommand {
    OPEN_CHANNEL = 0,
    CHANNEL_OPENED = 1,
    POST_MESSAGE = 2,
  };

  // Intercepts and processes a message posted through the automation interface.
  // Returns true if the message was intercepted.
  static bool InterceptMessageFromExternalHost(const std::string& message,
                                               const std::string& origin,
                                               const std::string& target,
                                               AutomationProvider* automation,
                                               RenderViewHost *view_host,
                                               int tab_handle);

  ExtensionPortContainer(AutomationProvider* automation, int tab_handle);
  ~ExtensionPortContainer();

  int port_id() const { return port_id_; }
  void set_port_id(int port_id) { port_id_ = port_id; }

  // IPC implementation.
  virtual bool Send(IPC::Message* msg);

 private:
  // Posts a message to the external host.
  bool PostMessageToExternalPort(const std::string& message);
  // Posts a request response message to the external host.
  bool PostResponseToExternalPort(const std::string& message);

  // Forwards a message from the external port.
  void PostMessageFromExternalPort(const std::string& message);

  // Attempts to connect this instance to the extension id, sends
  // a response to the connecting party.
  // Returns true if the connection was successful.
  bool Connect(const std::string &extension_id,
               int process_id,
               int routing_id,
               int connection_id);
  // Sends a response to the
  void SendConnectionResponse(int connection_id, int port_id);
  void OnExtensionHandleMessage(const std::string& message, int source_port_id);

  // Our automation provider.
  AutomationProvider* automation_;

  // The extension message service.
  ExtensionMessageService* service_;

  // Our assigned port id.
  int port_id_;
  // Handle to our associated tab.
  int tab_handle_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPortContainer);
};

#endif  // CHROME_BROWSER_EXTENSION_PORT_CONTAINER_H_
