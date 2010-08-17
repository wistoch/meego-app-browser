// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_dispatcher_host.h"

#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/render_messages.h"

namespace speech_input {

SpeechInputManager::FactoryMethod*
    SpeechInputDispatcherHost::manager_factory_ = &SpeechInputManager::Create;

SpeechInputDispatcherHost::SpeechInputDispatcherHost(
    int resource_message_filter_process_id)
    : resource_message_filter_process_id_(resource_message_filter_process_id) {
  // This is initialized by ResourceMessageFilter. Do not add any non-trivial
  // initialization here, instead do it lazily when required (e.g. see the
  // method |manager()|) or add an Init() method.
}

SpeechInputDispatcherHost::~SpeechInputDispatcherHost() {
}

SpeechInputManager* SpeechInputDispatcherHost::manager() {
  if (!manager_.get()) {
    manager_.reset((*manager_factory_)(this));
    DCHECK(manager_.get());
  }
  return manager_.get();
}

bool SpeechInputDispatcherHost::OnMessageReceived(
    const IPC::Message& msg, bool* msg_was_ok) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SpeechInputDispatcherHost, msg, *msg_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SpeechInput_StartRecognition,
                        OnStartRecognition)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SpeechInput_CancelRecognition,
                        OnCancelRecognition)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SpeechInput_StopRecording,
                        OnStopRecording)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SpeechInputDispatcherHost::OnStartRecognition(int render_view_id,
                                                   int request_id) {
  LOG(INFO) << "SpeechInputDispatcherHost: start recognition"
            << render_view_id;
  manager()->StartRecognition(SpeechInputCallerId(render_view_id, request_id));
}

void SpeechInputDispatcherHost::OnCancelRecognition(int render_view_id,
                                                    int request_id) {
  LOG(INFO) << "SpeechInputDispatcherHost: cancel recognition"
            << render_view_id;
  manager()->CancelRecognition(SpeechInputCallerId(render_view_id, request_id));
}

void SpeechInputDispatcherHost::OnStopRecording(int render_view_id,
                                                int request_id) {
  LOG(INFO) << "SpeechInputDispatcherHost: stop recording"
            << render_view_id;
  manager()->StopRecording(SpeechInputCallerId(render_view_id, request_id));
}

void SpeechInputDispatcherHost::SendMessageToRenderView(IPC::Message* message,
                                                        int render_view_id) {
  CallRenderViewHost(
      resource_message_filter_process_id_, render_view_id,
      &RenderViewHost::Send, message);
}


void SpeechInputDispatcherHost::SetRecognitionResult(
    const SpeechInputCallerId& caller_id, const string16& result) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  int caller_render_view_id = caller_id.first;
  int caller_request_id = caller_id.second;
  SendMessageToRenderView(
      new ViewMsg_SpeechInput_SetRecognitionResult(caller_render_view_id,
                                                   caller_request_id,
                                                   result),
      caller_render_view_id);
}

void SpeechInputDispatcherHost::DidCompleteRecording(
    const SpeechInputCallerId& caller_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  int caller_render_view_id = caller_id.first;
  int caller_request_id = caller_id.second;
  SendMessageToRenderView(
      new ViewMsg_SpeechInput_RecordingComplete(caller_render_view_id,
                                                caller_request_id),
      caller_render_view_id);
}

void SpeechInputDispatcherHost::DidCompleteRecognition(
    const SpeechInputCallerId& caller_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  int caller_render_view_id = caller_id.first;
  int caller_request_id = caller_id.second;
  SendMessageToRenderView(
      new ViewMsg_SpeechInput_RecognitionComplete(caller_render_view_id,
                                                  caller_request_id),
      caller_render_view_id);
}

}  // namespace speech_input
