// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/shared_memory.h"
#include "chrome/common/ipc_message_macros.h"
#include "webkit/glue/webcursor.h"

//-----------------------------------------------------------------------------
// PluginProcess messages
// These are messages sent from the browser to the plugin process.
IPC_BEGIN_MESSAGES(PluginProcess, 3)
  // Tells the plugin process to create a new channel for communication with a
  // renderer.  The channel name is returned in a
  // PluginProcessHostMsg_ChannelCreated message.
  // The renderer_handle is the handle of the renderer process requesting
  // the channel. It has to be valid in the context of the plugin process.
  IPC_MESSAGE_CONTROL2(PluginProcessMsg_CreateChannel,
                       int /* process_id */,
                       HANDLE /* renderer handle */)

  IPC_MESSAGE_CONTROL1(PluginProcessMsg_ShutdownResponse,
                       bool /* ok to shutdown */)

  // Allows a chrome plugin loaded in the browser process to send arbitrary
  // data to an instance of the same plugin loaded in a plugin process.
  IPC_MESSAGE_CONTROL1(PluginProcessMsg_PluginMessage,
                       std::vector<uint8> /* opaque data */)

  IPC_MESSAGE_CONTROL0(PluginProcessMsg_BrowserShutdown)

IPC_END_MESSAGES(PluginProcess)


//-----------------------------------------------------------------------------
// PluginProcessHost messages
// These are messages sent from the plugin process to the browser process.
IPC_BEGIN_MESSAGES(PluginProcessHost, 4)
  // Response to a PluginProcessMsg_CreateChannel message.
  IPC_MESSAGE_CONTROL2(PluginProcessHostMsg_ChannelCreated,
                       int /* process_id */,
                       std::wstring /* channel_name */)

  IPC_MESSAGE_ROUTED3(PluginProcessHostMsg_DownloadUrl,
                      std::string /* URL */,
                      int /* process id */,
                      HWND /* caller window */)

  IPC_SYNC_MESSAGE_CONTROL0_1(PluginProcessHostMsg_GetPluginFinderUrl,
                              std::string /* plugin finder URL */)

  IPC_MESSAGE_CONTROL0(PluginProcessHostMsg_ShutdownRequest)

  // Allows a chrome plugin loaded in a plugin process to send arbitrary
  // data to an instance of the same plugin loaded in the browser process.
  IPC_MESSAGE_CONTROL1(PluginProcessHostMsg_PluginMessage,
                       std::vector<uint8> /* opaque data */)

  // Retrieve the given type of info that is associated with the given
  // CPBrowsingContext.  Returns the result in a string.
  IPC_SYNC_MESSAGE_CONTROL0_1(PluginProcessHostMsg_GetPluginDataDir,
                              std::wstring /* data_dir_retval */)

  // Used to get cookies for the given URL.  The request_context is a
  // CPBrowsingContext, but is passed as int32 to avoid compilation errors.
  IPC_SYNC_MESSAGE_CONTROL2_1(PluginProcessHostMsg_GetCookies,
                              int32 /* request_context */,
                              GURL /* url */,
                              std::string /* cookies */)

IPC_END_MESSAGES(PluginProcessHost)


//-----------------------------------------------------------------------------
// Plugin messages
// These are messages sent from the renderer process to the plugin process.
IPC_BEGIN_MESSAGES(Plugin, 5)
  // Tells the plugin process to create a new plugin instance with the given
  // id.  A corresponding WebPluginDelegateStub is created which hosts the
  // WebPluginDelegateImpl.
  IPC_SYNC_MESSAGE_CONTROL1_1(PluginMsg_CreateInstance,
                              std::string /* mime_type */,
                              int /* instance_id */)

  // The WebPluginDelegateProxy sends this to the WebPluginDelegateStub in its
  // destructor, so that the stub deletes the actual WebPluginDelegateImpl
  // object that it's hosting.
  IPC_SYNC_MESSAGE_CONTROL1_0(PluginMsg_DestroyInstance,
                              int /* instance_id */)

  IPC_SYNC_MESSAGE_CONTROL0_1(PluginMsg_GenerateRouteID,
                             int /* id */)

  // The messages below all map to WebPluginDelegate methods.
  IPC_SYNC_MESSAGE_ROUTED1_1(PluginMsg_Init,
                             PluginMsg_Init_Params,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_Paint,
                             PluginMsg_Paint_Params /* params */)

  IPC_SYNC_MESSAGE_ROUTED0_1(PluginMsg_Print,
                             PluginMsg_PrintResponse_Params /* params */)

  // Returns a shared memory handle to a EMF buffer.
  IPC_SYNC_MESSAGE_ROUTED1_2(PluginMsg_PaintIntoSharedMemory,
                             PluginMsg_Paint_Params /* params */,
                             SharedMemoryHandle /* emf_buffer */,
                             size_t /* bytes */)

  IPC_SYNC_MESSAGE_ROUTED0_2(PluginMsg_GetPluginScriptableObject,
                             int /* route_id */,
                             void* /* npobject_ptr */)

  IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_DidFinishLoadWithReason,
                             int /* reason */)

  IPC_MESSAGE_ROUTED3(PluginMsg_UpdateGeometry,
                      gfx::Rect /* window_rect */,
                      gfx::Rect /* clip_rect */,
                      bool /* visible */)

  IPC_SYNC_MESSAGE_ROUTED0_0(PluginMsg_SetFocus)

  IPC_SYNC_MESSAGE_ROUTED1_2(PluginMsg_HandleEvent,
                             NPEvent /* event */,
                             bool /* handled */,
                             WebCursor /* cursor type*/)

  IPC_SYNC_MESSAGE_ROUTED2_0(PluginMsg_WillSendRequest,
                             int /* id */,
                             GURL /* url */)

  IPC_SYNC_MESSAGE_ROUTED1_1(PluginMsg_DidReceiveResponse,
                             PluginMsg_DidReceiveResponseParams,
                             bool /* cancel */)

  IPC_SYNC_MESSAGE_ROUTED2_0(PluginMsg_DidReceiveData,
                             int /* id */,
                             std::vector<char> /* buffer */)

  IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_DidFinishLoading,
                             int /* id */)

  IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_DidFail,
                             int /* id */)

  IPC_MESSAGE_ROUTED5(PluginMsg_SendJavaScriptStream,
                      std::string /* url */,
                      std::wstring /* result */,
                      bool /* success */,
                      bool /* notify required */,
                      int /* notify data */)

  IPC_MESSAGE_ROUTED2(PluginMsg_DidReceiveManualResponse,
                      std::string /* url */,
                      PluginMsg_DidReceiveResponseParams)

  IPC_MESSAGE_ROUTED1(PluginMsg_DidReceiveManualData,
                      std::vector<char> /* buffer */)

  IPC_MESSAGE_ROUTED0(PluginMsg_DidFinishManualLoading)

  IPC_MESSAGE_ROUTED0(PluginMsg_DidManualLoadFail)

  IPC_MESSAGE_ROUTED0(PluginMsg_InstallMissingPlugin)

  IPC_SYNC_MESSAGE_ROUTED1_0(PluginMsg_HandleURLRequestReply,
                             PluginMsg_URLRequestReply_Params)

  IPC_SYNC_MESSAGE_ROUTED3_0(PluginMsg_URLRequestRouted,
                             std::string /* url */,
                             bool /* notify_needed */,
                             HANDLE /* notify data */)
IPC_END_MESSAGES(Plugin)


//-----------------------------------------------------------------------------
// PluginHost messages
// These are messages sent from the plugin process to the renderer process.
// They all map to the corresponding WebPlugin methods.
IPC_BEGIN_MESSAGES(PluginHost, 6)
  // Sends the plugin window information to the renderer.
  // The window parameter is a handle to the window if the plugin is a windowed
  // plugin. It is NULL for windowless plugins.
  // The modal_loop_pump_messages_event parameter is an event handle which is
  // passed in for windowless plugins and is used to indicate if messages
  // are to be pumped in sync calls to the plugin process. Currently used
  // in HandleEvent calls.
  IPC_SYNC_MESSAGE_ROUTED2_0(PluginHostMsg_SetWindow,
                             HWND /* window */,
                             HANDLE /* modal_loop_pump_messages_event */)

  IPC_MESSAGE_ROUTED1(PluginHostMsg_URLRequest,
                      PluginHostMsg_URLRequest_Params)

  IPC_SYNC_MESSAGE_ROUTED1_0(PluginHostMsg_CancelResource,
                             int /* id */)

  IPC_MESSAGE_ROUTED0(PluginHostMsg_Invalidate)

  IPC_MESSAGE_ROUTED1(PluginHostMsg_InvalidateRect,
                      gfx::Rect /* rect */)

  IPC_SYNC_MESSAGE_ROUTED1_2(PluginHostMsg_GetWindowScriptNPObject,
                             int /* route id */,
                             bool /* success */,
                             void* /* npobject_ptr */)

  IPC_SYNC_MESSAGE_ROUTED1_2(PluginHostMsg_GetPluginElement,
                             int /* route id */,
                             bool /* success */,
                             void* /* npobject_ptr */)

  IPC_MESSAGE_ROUTED3(PluginHostMsg_SetCookie,
                      GURL /* url */,
                      GURL /* policy_url */,
                      std::string /* cookie */)

  IPC_SYNC_MESSAGE_ROUTED2_1(PluginHostMsg_GetCookies,
                             GURL /* url */,
                             GURL /* policy_url */,
                             std::string /* cookies */)

  // Asks the browser to show a modal HTML dialog.  The dialog is passed the
  // given arguments as a JSON string, and returns its result as a JSON string
  // through json_retval.
  IPC_SYNC_MESSAGE_ROUTED4_1(PluginHostMsg_ShowModalHTMLDialog,
                              GURL /* url */,
                              int /* width */,
                              int /* height */,
                              std::string /* json_arguments */,
                              std::string /* json_retval */)

  IPC_MESSAGE_ROUTED1(PluginHostMsg_MissingPluginStatus,
                      int /* status */)

  IPC_SYNC_MESSAGE_ROUTED0_1(PluginHostMsg_GetCPBrowsingContext,
                             uint32 /* context */)

IPC_END_MESSAGES(PluginHost)

//-----------------------------------------------------------------------------
// NPObject messages
// These are messages used to marshall NPObjects.  They are sent both from the
// plugin to the renderer and from the renderer to the plugin.
IPC_BEGIN_MESSAGES(NPObject, 7)
  IPC_SYNC_MESSAGE_ROUTED0_0(NPObjectMsg_Release)

  IPC_SYNC_MESSAGE_ROUTED1_1(NPObjectMsg_HasMethod,
                             NPIdentifier_Param /* name */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED3_2(NPObjectMsg_Invoke,
                             bool /* is_default */,
                             NPIdentifier_Param /* method */,
                             std::vector<NPVariant_Param> /* args */,
                             NPVariant_Param /* result_param */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_1(NPObjectMsg_HasProperty,
                             NPIdentifier_Param /* name */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_2(NPObjectMsg_GetProperty,
                             NPIdentifier_Param /* name */,
                             NPVariant_Param /* property */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED2_1(NPObjectMsg_SetProperty,
                             NPIdentifier_Param /* name */,
                             NPVariant_Param /* property */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_1(NPObjectMsg_RemoveProperty,
                             NPIdentifier_Param /* name */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED0_0(NPObjectMsg_Invalidate)

  IPC_SYNC_MESSAGE_ROUTED0_2(NPObjectMsg_Enumeration,
                             std::vector<NPIdentifier_Param> /* value */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_2(NPObjectMsg_Evaluate,
                             std::string /* script */,
                             NPVariant_Param /* result_param */,
                             bool /* result */)

  IPC_SYNC_MESSAGE_ROUTED1_0(NPObjectMsg_SetException,
                             std::string /* message */)

IPC_END_MESSAGES(NPObject)
