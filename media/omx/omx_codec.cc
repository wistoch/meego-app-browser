// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "media/omx/input_buffer.h"
#include "media/omx/omx_codec.h"

namespace media {

template <typename T>
static void ResetPortHeader(const OmxCodec& dec, T* param) {
  memset(param, 0, sizeof(T));
  param->nVersion.nVersion = dec.current_omx_spec_version();
  param->nSize = sizeof(T);
}

OmxCodec::OmxCodec(MessageLoop* message_loop)
    : input_buffer_count_(0),
      input_buffer_size_(0),
      input_port_(0),
      input_eos_(false),
      output_buffer_count_(0),
      output_buffer_size_(0),
      output_port_(0),
      output_eos_(false),
      state_(kEmpty),
      next_state_(kEmpty),
      component_handle_(NULL),
      message_loop_(message_loop) {
}

OmxCodec::~OmxCodec() {
  DCHECK(state_ == kError || state_ == kEmpty);
  DCHECK_EQ(0u, input_buffers_.size());
  DCHECK_EQ(0u, output_buffers_.size());
  DCHECK(available_input_buffers_.empty());
  DCHECK(available_output_buffers_.empty());
  DCHECK(input_queue_.empty());
  DCHECK(output_queue_.empty());
}

void OmxCodec::Setup(
    const OmxCodec::OmxMediaFormat& input_format,
    const OmxCodec::OmxMediaFormat& output_format) {
  DCHECK_EQ(kEmpty, state_);
  DCHECK_NE(input_format.codec, kCodecNone);
  input_format_ = input_format;
  output_format_ = output_format;
  Codec codec = encoder() ? output_format_.codec : input_format_.codec;
  role_name_ = SelectRole(codec, encoder());
}

void OmxCodec::SetErrorCallback(Callback* callback) {
  DCHECK_EQ(kEmpty, state_);
  error_callback_.reset(callback);
}

void OmxCodec::SetFormatCallback(FormatCallback* callback) {
  DCHECK_EQ(kEmpty, state_);
  format_callback_.reset(callback);
}

void OmxCodec::Start() {
  DCHECK_NE(kCodecNone, input_format_.codec);

  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &OmxCodec::StartTask));
}

void OmxCodec::Stop(Callback* callback) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &OmxCodec::StopTask, callback));
}

void OmxCodec::Read(ReadCallback* callback) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &OmxCodec::ReadTask, callback));
}

void OmxCodec::Feed(InputBuffer* buffer, FeedCallback* callback) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &OmxCodec::FeedTask, buffer, callback));
}

void OmxCodec::Flush(Callback* callback) {
  callback->Run();
  delete callback;
}

OmxCodec::State OmxCodec::GetState() const {
  return state_;
}

void OmxCodec::SetState(State state) {
  state_ = state;
}

OmxCodec::State OmxCodec::GetNextState() const {
  return next_state_;
}

void OmxCodec::SetNextState(State state) {
  next_state_ = state;
}

void OmxCodec::StartTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  next_sample_timestamp_ = 0;

  StateTransitionTask(kLoaded);
}

void OmxCodec::StopTask(Callback* callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  stop_callback_.reset(callback);

  if (GetState() == kError) {
    DoneStop();
    return;
  }

  FreeInputQueue();
  FreeOutputQueue();

  if (GetState() == kExecuting)
    StateTransitionTask(kIdle);
  // TODO(hclam): The following two transitions may not be correct.
  else if (GetState() == kPortSettingDisable)
    StateTransitionTask(kIdle);
  else if (GetState() == kPortSettingEnable)
    StateTransitionTask(kIdle);
  else if (GetState() == kIdle)
    StateTransitionTask(kLoaded);
  else if (GetState() == kLoaded)
    StateTransitionTask(kEmpty);
}

void OmxCodec::ReadTask(ReadCallback* callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Don't accept read request on error state.
  if (!CanAcceptOutput()) {
    callback->RunWithParams(MakeTuple(static_cast<uint8*>(NULL), 0));
    delete callback;
    return;
  }

  // Queue this request.
  output_queue_.push(callback);

  // Make our best effort to serve the request and read
  // from the decoder.
  FillBufferTask();
}

void OmxCodec::FeedTask(InputBuffer* buffer, FeedCallback* callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanAcceptInput()) {
    callback->Run(buffer);
    delete callback;
    return;
  }

  // Queue this input buffer.
  input_queue_.push(std::make_pair(buffer, callback));

  // Try to feed buffers into the decoder.
  EmptyBufferTask();
}

// This method assumes OMX_AllocateBuffer() will allocate
// buffer internally. If this is not the case we need to
// call OMX_UseBuffer() to allocate buffer manually and
// assign to the headers.
bool OmxCodec::AllocateInputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  for(int i = 0; i < input_buffer_count_; ++i) {
    OMX_BUFFERHEADERTYPE* buffer;
    OMX_ERRORTYPE error =
        OMX_AllocateBuffer(component_handle_, &buffer, input_port_,
                           NULL, input_buffer_size_);
    if (error != OMX_ErrorNone)
      return false;
    input_buffers_.push_back(buffer);
    available_input_buffers_.push(buffer);
  }
  return true;
}

// This method assumes OMX_AllocateBuffer() will allocate
// buffer internally. If this is not the case we need to
// call OMX_UseBuffer() to allocate buffer manually and
// assign to the headers.
bool OmxCodec::AllocateOutputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  for(int i = 0; i < output_buffer_count_; ++i) {
    OMX_BUFFERHEADERTYPE* buffer;
    OMX_ERRORTYPE error =
        OMX_AllocateBuffer(component_handle_, &buffer, output_port_,
                           NULL, output_buffer_size_);
    if (error != OMX_ErrorNone)
      return false;
    output_buffers_.push_back(buffer);
  }
  return true;
}

void OmxCodec::FreeInputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Calls to OMX to free buffers.
  for(size_t i = 0; i < input_buffers_.size(); ++i)
    OMX_FreeBuffer(component_handle_, input_port_, input_buffers_[i]);
  input_buffers_.clear();

  // Empty available buffer queue.
  while (!available_input_buffers_.empty()) {
    available_input_buffers_.pop();
  }
}

void OmxCodec::FreeOutputBuffers() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Calls to OMX to free buffers.
  for(size_t i = 0; i < output_buffers_.size(); ++i)
    OMX_FreeBuffer(component_handle_, output_port_, output_buffers_[i]);
  output_buffers_.clear();

  // Empty available buffer queue.
  while (!available_output_buffers_.empty()) {
    available_output_buffers_.pop();
  }
}

void OmxCodec::FreeInputQueue() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  while (!input_queue_.empty()) {
    InputBuffer* buffer = input_queue_.front().first;
    FeedCallback* callback = input_queue_.front().second;
    callback->Run(buffer);
    delete callback;
    input_queue_.pop();
  }
}

void OmxCodec::FreeOutputQueue() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  while (!output_queue_.empty()) {
    ReadCallback* callback = output_queue_.front();
    callback->Run(static_cast<uint8*>(NULL), 0);
    delete callback;
    output_queue_.pop();
  }
}

// Sequence of actions in this transition:
//
// 1. Initialize OMX (To be removed.)
// 2. Map role name to component name.
// 3. Get handle of the OMX component
// 4. Get the port information.
// 5. Set role for the component.
// 6. Device specific configurations.
// 7. Input/output ports media format configuration.
// 8. Obtain the information about the input port.
// 9. Obtain the information about the output port.
void OmxCodec::Transition_EmptyToLoaded() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kEmpty, GetState());

  static OMX_CALLBACKTYPE callback = {
    &EventHandler,
    &EmptyBufferCallback,
    &FillBufferCallback
  };

  // 1. Initialize the OpenMAX Core.
  // TODO(hclam): move this out.
  OMX_ERRORTYPE omxresult = OMX_Init();
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "Failed to Init OpenMAX core";
    StateTransitionTask(kError);
    return;
  }

  // 2. Map role name to component name.
  OMX_U32 roles = 0;
  omxresult = OMX_GetComponentsOfRole(
                  const_cast<OMX_STRING>(role_name_.c_str()),
                  &roles, 0);
  if (omxresult != OMX_ErrorNone || roles == 0) {
    LOG(ERROR) << "Unsupported Role: " << role_name_.c_str();
    StateTransitionTask(kError);
    return;
  }
  const OMX_U32 kMaxRolePerComponent = 20;
  CHECK(roles < kMaxRolePerComponent);

  OMX_U8** component_names = new OMX_U8*[roles];
  const int kMaxComponentNameLength = 256;
  for (size_t i = 0; i < roles; ++i)
    component_names[i] = new OMX_U8[kMaxComponentNameLength];

  omxresult = OMX_GetComponentsOfRole(
                  const_cast<OMX_STRING>(role_name_.c_str()),
                  &roles, component_names);

  // Use first component only. Copy the name of the first component
  // so that we could free the memory.
  if (omxresult == OMX_ErrorNone)
    component_name_ = reinterpret_cast<char*>(component_names[0]);

  for (size_t i = 0; i < roles; ++i)
    delete [] component_names[i];
  delete [] component_names;

  if (omxresult != OMX_ErrorNone || roles == 0) {
    LOG(ERROR) << "Unsupported Role: " << role_name_.c_str();
    StateTransitionTask(kError);
    return;
  }

  // 3. Get the handle to the component. After OMX_GetHandle(),
  //    the component is in loaded state.
  OMX_STRING component = const_cast<OMX_STRING>(component_name_.c_str());
  OMX_HANDLETYPE handle = reinterpret_cast<OMX_HANDLETYPE>(component_handle_);
  omxresult = OMX_GetHandle(&handle, component, this, &callback);
  component_handle_ = reinterpret_cast<OMX_COMPONENTTYPE*>(handle);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "Failed to Load the component: " << component;
    StateTransitionTask(kError);
    return;
  }

  // 4. Get the port information. This will obtain information about the
  //    number of ports and index of the first port.
  OMX_PORT_PARAM_TYPE port_param;
  ResetPortHeader(*this, &port_param);
  omxresult = OMX_GetParameter(component_handle_, OMX_IndexParamVideoInit,
                               &port_param);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "Failed to get Port Param";
    StateTransitionTask(kError);
    return;
  }
  input_port_ = port_param.nStartPortNumber;
  output_port_ = input_port_ + 1;

  // 5. Set role for the component because our component could
  //    have multiple roles.
  OMX_PARAM_COMPONENTROLETYPE role_type;
  ResetPortHeader(*this, &role_type);
  base::strlcpy(reinterpret_cast<char*>(role_type.cRole),
                role_name_.c_str(),
                OMX_MAX_STRINGNAME_SIZE);
  role_type.cRole[OMX_MAX_STRINGNAME_SIZE - 1] = '\0';
  omxresult = OMX_SetParameter(component_handle_,
                               OMX_IndexParamStandardComponentRole,
                               &role_type);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "Failed to Set Role";
    StateTransitionTask(kError);
    return;
  }

  // 6. Device specific configurations.
  if (!DeviceSpecificConfig()) {
    LOG(ERROR) << "Device specific configurations failed";
    StateTransitionTask(kError);
    return;
  }

  // 7. Input/output ports media format configuration.
  if (!ConfigureIOPorts()) {
    LOG(ERROR) << "Media format configurations failed";
    StateTransitionTask(kError);
    return;
  }

  // 8. Obtain the information about the input port.
  // This will have the new mini buffer count in |port_format.nBufferCountMin|.
  // Save this value to input_buf_count.
  OMX_PARAM_PORTDEFINITIONTYPE port_format;
  ResetPortHeader(*this, &port_format);
  port_format.nPortIndex = input_port_;
  omxresult = OMX_GetParameter(component_handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamPortDefinition) failed";
    StateTransitionTask(kError);
    return;
  }
  if (OMX_DirInput != port_format.eDir) {
    LOG(ERROR) << "Expected input port";
    StateTransitionTask(kError);
    return;
  }
  input_buffer_count_ = port_format.nBufferCountMin;
  input_buffer_size_ = port_format.nBufferSize;

  // 9. Obtain the information about the output port.
  ResetPortHeader(*this, &port_format);
  port_format.nPortIndex = output_port_;
  omxresult = OMX_GetParameter(component_handle_,
                               OMX_IndexParamPortDefinition,
                               &port_format);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamPortDefinition) failed";
    StateTransitionTask(kError);
    return;
  }
  if (OMX_DirOutput != port_format.eDir) {
    LOG(ERROR) << "Expect Output Port";
    StateTransitionTask(kError);
    return;
  }
  output_buffer_count_ = port_format.nBufferCountMin;
  output_buffer_size_ = port_format.nBufferSize;

  // After we have done all the configurations, we are considered loaded.
  DoneStateTransitionTask();
}

// Sequence of actions in this transition:
//
// 1. Send command to Idle state.
// 2. Allocate buffers for input port.
// 3. Allocate buffers for output port.
void OmxCodec::Transition_LoadedToIdle() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kLoaded, GetState());

  // 1. Sets decoder to idle state.
  OMX_ERRORTYPE omxresult = OMX_SendCommand(component_handle_,
                                            OMX_CommandStateSet,
                                            OMX_StateIdle, 0);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandStateSet) failed";
    StateTransitionTask(kError);
    return;
  }

  // 2. Allocate buffer for the input port.
  if (!AllocateInputBuffers()) {
    LOG(ERROR) << "OMX_AllocateBuffer() Input buffer error";
    StateTransitionTask(kError);
    return;
  }

  // 3. Allocate buffer for the output port.
  if (!AllocateOutputBuffers()) {
    LOG(ERROR) << "OMX_AllocateBuffer() Output buffer error";
    StateTransitionTask(kError);
    return;
  }
}

// Sequence of actions in this transition:
//
// 1. Send command to Executing state.
void OmxCodec::Transition_IdleToExecuting() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kIdle, GetState());

  // Transist to executing state.
  OMX_ERRORTYPE omxresult = OMX_SendCommand(component_handle_,
                                            OMX_CommandStateSet,
                                            OMX_StateExecuting, 0);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandStateSet) failed";
    StateTransitionTask(kError);
    return;
  }
}

// Sequence of actions in this transition:
//
// 1. Send command to disable output port.
// 2. Free buffers of the output port.
void OmxCodec::Transition_ExecutingToDisable() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kExecuting, GetState());

  // Send DISABLE command.
  OMX_ERRORTYPE omxresult = OMX_SendCommand(component_handle_,
                                            OMX_CommandPortDisable,
                                            output_port_, 0);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandPortDisable) failed";
    StateTransitionTask(kError);
    return;
  }

  // Free output Buffer.
  FreeOutputBuffers();
}

// Sequence of actions in this transition:
//
// 1. Send command to enable output port.
// 2. Get parameter of the output port.
// 3. Allocate buffers for the output port.
void OmxCodec::Transition_DisableToEnable() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kPortSettingDisable, GetState());

  // Send Enable command.
  OMX_ERRORTYPE omxresult = OMX_SendCommand(component_handle_,
                                            OMX_CommandPortEnable,
                                            output_port_, 0);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandPortEnable) failed";
    StateTransitionTask(kError);
    return;
  }

  // AllocateBuffers.
  OMX_PARAM_PORTDEFINITIONTYPE port_format;
  ResetPortHeader(*this, &port_format);
  port_format.nPortIndex = output_port_;
  omxresult = OMX_GetParameter(component_handle_, OMX_IndexParamPortDefinition,
                               &port_format);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamPortDefinition) failed";
    StateTransitionTask(kError);
    return;
  }
  if (OMX_DirOutput != port_format.eDir) {
    LOG(ERROR) << "Expected Output Port";
    StateTransitionTask(kError);
    return;
  }

  // Update the output format.
  // TODO(jiesun): check if the format really change. ( we had omit some
  // information such as frame rate / bit rate / vbv buffer info now. )
  output_format_.video_header.height = port_format.format.video.nFrameHeight;
  output_format_.video_header.width = port_format.format.video.nFrameWidth;
  output_format_.video_header.stride = port_format.format.video.nStride;
  input_format_.video_header.height = output_format_.video_header.height;
  input_format_.video_header.width = output_format_.video_header.width;
  input_format_.video_header.stride = output_format_.video_header.stride;
  ReportFormatChange();

  // Update the ports in buffer.
  output_buffer_count_ = port_format.nBufferCountActual;
  output_buffer_size_ = port_format.nBufferSize;
  if (!AllocateOutputBuffers()) {
    LOG(ERROR) << "OMX_AllocateBuffer() Output buffer error";
    StateTransitionTask(kError);
    return;
  }
}

// Sequence of actions in this transition:
//
// 1. Send command to Idle state.
void OmxCodec::Transition_DisableToIdle() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kPortSettingDisable, GetState());

  OMX_ERRORTYPE omxresult = OMX_SendCommand(component_handle_,
                                            OMX_CommandStateSet,
                                            OMX_StateIdle, 0);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandStateSet) failed";
    StateTransitionTask(kError);
    return;
  }
}

// Sequence of actions in this transition:
//
// This transition does nothing.
void OmxCodec::Transition_EnableToExecuting() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kPortSettingEnable, GetState());

  // This transition is fake, nothing to do here.
  DoneStateTransitionTask();
}

void OmxCodec::Transition_EnableToIdle() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kPortSettingEnable, GetState());

  OMX_ERRORTYPE omxresult = OMX_SendCommand(component_handle_,
                                            OMX_CommandStateSet,
                                            OMX_StateIdle, 0);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandStateSet) failed";
    StateTransitionTask(kError);
    return;
  }
}

// Sequence of actions in this transition:
//
// 1. Send command to Idle state.
void OmxCodec::Transition_ExecutingToIdle() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kExecuting, GetState());

  OMX_ERRORTYPE omxresult = OMX_SendCommand(component_handle_,
                                            OMX_CommandStateSet,
                                            OMX_StateIdle, 0);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandStateSet) failed";
    StateTransitionTask(kError);
    return;
  }
}

// Sequence of actions in this transition:
//
// 1. Send command to Loaded state
// 2. Free input buffers
// 2. Free output buffers
void OmxCodec::Transition_IdleToLoaded() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kIdle, GetState());

  OMX_ERRORTYPE omxresult = OMX_SendCommand(component_handle_,
                                            OMX_CommandStateSet,
                                            OMX_StateLoaded, 0);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SendCommand(OMX_CommandStateSet) failed";
    StateTransitionTask(kError);
    return;
  }

  FreeInputBuffers();
  FreeOutputBuffers();
}

// Sequence of actions in this transition:
//
// 1. Free decoder handle
// 2. Uninitialize OMX (TODO(hclam): Remove this.)
void OmxCodec::Transition_LoadedToEmpty() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kLoaded, GetState());

  // Free the decoder handle.
  OMX_ERRORTYPE result = OMX_FreeHandle(component_handle_);
  if (result != OMX_ErrorNone) {
    LOG(ERROR) << "Terminate: OMX_FreeHandle() error. "
                  "Error code: " << result;
  }
  component_handle_ = NULL;

  // Deinit OpenMAX
  // TODO(hclam): move this out.
  OMX_Deinit();

  DoneStateTransitionTask();
}

// Sequence of actions in this transition:
//
// 1. Disable input port
// 2. Disable output port
// 3. Free input buffer
// 4. Free output buffer
// 5. Free decoder handle
// 6. Uninitialize OMX (TODO(hclam): Remove this.)
void OmxCodec::Transition_Error() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_NE(kError, GetState());

  State old_state = GetState();
  SetState(kError);

  // If we are going to error state in the following states, we need to
  // send a command to disable ports for us to free buffers.
  if (old_state == kExecuting || old_state == kIdle ||
      old_state == kPortSettingEnable || old_state == kPortSettingDisable) {
    DCHECK(component_handle_);
    OMX_SendCommand(component_handle_, OMX_CommandPortDisable, input_port_, 0);
    OMX_SendCommand(component_handle_, OMX_CommandPortDisable, output_port_, 0);
  }

  // Free input and output buffers.
  FreeInputBuffers();
  FreeOutputBuffers();

  // Free input and output queues.
  FreeInputQueue();
  FreeOutputQueue();

  // Free decoder handle.
  if (component_handle_) {
    OMX_ERRORTYPE result = OMX_FreeHandle(component_handle_);
    if (result != OMX_ErrorNone)
      LOG(ERROR) << "OMX_FreeHandle() error. Error code: " << result;
    component_handle_ = NULL;
  }

  // Deinit OpenMAX.
  OMX_Deinit();

  DoneStateTransitionTask();
}

void OmxCodec::PostStateTransitionTask(State new_state) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &OmxCodec::StateTransitionTask, new_state));
}

void OmxCodec::StateTransitionTask(State new_state) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (GetState() == kError)
    return;

  // Save the next state.
  SetNextState(new_state);

  // The following list defines all the possible state transitions
  // for this object:
  //
  // TRANSITIONS
  // 1.  Empty -> Loaded
  // 2.  Loaded -> Idle
  // 3.  Idle -> Executing
  // 4.  Executing -> Disable
  // 5.  Executing -> Idle
  // 6.  Disable -> Enable
  // 7.  Disable -> Idle
  // 8.  Enable -> Executing
  // 9.  Enable -> Idle
  // 10. Idle -> Loaded
  // 11. Loaded -> Empty  (TODO(hclam): To stopped instead.)
  // 12. *ANYTHING* -> Error
  if (GetState() == kEmpty && new_state == kLoaded)
    Transition_EmptyToLoaded();
  else if (GetState() == kLoaded && new_state == kIdle)
    Transition_LoadedToIdle();
  else if (GetState() == kIdle && new_state == kExecuting)
    Transition_IdleToExecuting();
  else if (GetState() == kExecuting && new_state == kPortSettingDisable)
    Transition_ExecutingToDisable();
  else if (GetState() == kPortSettingDisable && new_state == kPortSettingEnable)
    Transition_DisableToEnable();
  else if (GetState() == kPortSettingDisable && new_state == kIdle)
    Transition_DisableToIdle();
  else if (GetState() == kPortSettingEnable && new_state == kExecuting)
    Transition_EnableToExecuting();
  else if (GetState() == kPortSettingEnable && new_state == kIdle)
    Transition_EnableToIdle();
  else if (GetState() == kExecuting && new_state == kIdle)
    Transition_ExecutingToIdle();
  else if (GetState() == kIdle && new_state == kLoaded)
    Transition_IdleToLoaded();
  else if (GetState() == kLoaded && new_state == kEmpty)
    Transition_LoadedToEmpty();
  else if (new_state == kError)
    Transition_Error();
}

void OmxCodec::PostDoneStateTransitionTask() {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &OmxCodec::DoneStateTransitionTask));
}

void OmxCodec::DoneStateTransitionTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (GetState() == kError) {
    ReportError();
    return;
  }

  // Save the current state and completes the transition.
  State old_state = GetState();
  SetState(GetNextState());

  // The following list is to perform a state transition automatically
  // based on the last transition done:
  //
  //    LAST TRANSITION       NEXT TRANSITION
  //
  // 1. Empty -> Loaded       Laoded -> Idle
  // 2. Loaded -> Idle        Idle   -> Executing
  // 3. Idle -> Executing
  //
  // Because of the above reoute, once we kick start the transition
  // from empty to loaded, this method will automatically route it
  // executing eventually.
  //
  // The following sequence is for transition to the stopped state.
  //
  //    LAST TRANSITION       NEXT TRANSITION
  //
  // 4. Executing -> Idle     Idle -> Loaded
  // 5. Idle -> Loaded        Loaded -> Empty
  // TODO(hclam): should go to Stopped instead of Empty.
  //
  // During dynamic port seeting, the route of state transition is:
  //
  //    LAST TRANSITION       NEXT TRANSITION
  //
  // 6. Executing -> Disable  Disable -> Enable
  // 7. Disable -> Enable     Enable -> Executing
  if (old_state == kEmpty && GetState() == kLoaded)
    StateTransitionTask(kIdle);
  else if (old_state == kLoaded && GetState() == kIdle)
    StateTransitionTask(kExecuting);
  else if (old_state == kIdle && GetState() == kExecuting) {
    // TODO(hclam): It is a little too late to issue read requests.
    // This seems to introduce some latencies.
    InitialEmptyBuffer();
    InitialFillBuffer();
  }
  else if (old_state == kExecuting && GetState() == kPortSettingDisable)
    StateTransitionTask(kPortSettingEnable);
  else if (old_state == kPortSettingDisable && GetState() == kPortSettingEnable)
    StateTransitionTask(kExecuting);
  else if (old_state == kPortSettingEnable && GetState() == kExecuting) {
    InitialFillBuffer();
  }
  else if (old_state == kPortSettingDisable && GetState() == kIdle)
    StateTransitionTask(kLoaded);
  else if (old_state == kPortSettingEnable && GetState() == kIdle)
    StateTransitionTask(kLoaded);
  else if (old_state == kExecuting && GetState() == kIdle)
    StateTransitionTask(kLoaded);
  else if (old_state == kIdle && GetState() == kLoaded)
    StateTransitionTask(kEmpty);
  else if (old_state == kLoaded && GetState() == kEmpty)
    DoneStop();
  else {
    NOTREACHED() << "Invalid state transition";
  }
}

void OmxCodec::DoneStop() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!stop_callback_.get())
    return;
  stop_callback_->Run();
  stop_callback_.reset();
}

void OmxCodec::ReportError() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!error_callback_.get())
    return;
  error_callback_->Run();
  error_callback_.reset();
}

void OmxCodec::ReportFormatChange() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!format_callback_.get())
    return;
  format_callback_->Run(&input_format_, &output_format_);
}

std::string OmxCodec::SelectRole(Codec codec, bool encoder) {
  std::string role_name = encoder ? "video_encoder." : "video_decoder.";
  switch (codec) {
    case kCodecH264:
      return role_name + "avc";
    case kCodecH263:
      return role_name + "h263";
    case kCodecMpeg4:
      return role_name + "mpeg4";
    case kCodecVc1:
      return role_name + "vc1";
    default:
      break;
  }
  NOTREACHED();
  return "";
}

bool OmxCodec::ConfigureIOPorts() {
  OMX_PARAM_PORTDEFINITIONTYPE input_port_def, output_port_def;
  OMX_ERRORTYPE omxresult = OMX_ErrorNone;
  // Get default input port definition.
  ResetPortHeader(*this, &input_port_def);
  input_port_def.nPortIndex = input_port_;
  omxresult = OMX_GetParameter(component_handle_,
                               OMX_IndexParamPortDefinition,
                               &input_port_def);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamPortDefinition) "
                  "for input port failed";
    return false;
  }
  if (OMX_DirInput != input_port_def.eDir) {
    LOG(ERROR) << "Expected Input Port";
    return false;
  }

  // Get default output port definition.
  ResetPortHeader(*this, &output_port_def);
  output_port_def.nPortIndex = output_port_;
  omxresult = OMX_GetParameter(component_handle_,
                               OMX_IndexParamPortDefinition,
                               &output_port_def);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamPortDefinition) "
                  "for output port failed";
    return false;
  }
  if (OMX_DirOutput != output_port_def.eDir) {
    LOG(ERROR) << "Expected Output Port";
    return false;
  }

  return encoder() ?
         ConfigureAsEncoder(&input_port_def, &output_port_def) :
         ConfigureAsDecoder(&input_port_def, &output_port_def);
}

bool OmxCodec::ConfigureAsEncoder(
    OMX_PARAM_PORTDEFINITIONTYPE* input_port_def,
    OMX_PARAM_PORTDEFINITIONTYPE* output_port_def) {
  // TODO(jiesun): Add support for other format than MPEG4.
  DCHECK_EQ(kCodecMpeg4, output_format_.codec);
  // Configure the input port.
  input_port_def->format.video.nFrameWidth =
      input_format_.video_header.width;
  input_port_def->format.video.nFrameHeight =
      input_format_.video_header.height;
  OMX_ERRORTYPE omxresult = OMX_ErrorNone;
  omxresult = OMX_SetParameter(component_handle_,
                               OMX_IndexParamPortDefinition,
                               input_port_def);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SetParameter(OMX_IndexParamPortDefinition) "
                  "for input port failed";
    return false;
  }

  // Configure the output port
  output_port_def->format.video.nFrameWidth =
      input_format_.video_header.width;
  output_port_def->format.video.nFrameHeight =
      input_format_.video_header.height;
  omxresult = OMX_SetParameter(component_handle_,
                               OMX_IndexParamPortDefinition,
                               output_port_def);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SetParameter(OMX_IndexParamPortDefinition) "
                  "for output port failed";
    return false;
  }

  if (output_format_.codec == kCodecMpeg4) {
    OMX_VIDEO_PARAM_MPEG4TYPE mp4_type;
    omxresult = OMX_GetParameter(component_handle_,
                                 OMX_IndexParamVideoMpeg4,
                                 &mp4_type);
    if (omxresult != OMX_ErrorNone) {
      LOG(ERROR) << "GetParameter(OMX_IndexParamVideoMpeg4) failed";
      return false;
    }
    // TODO(jiesun): verify if other vendors had the same definition.
    // Specify the frame rate.
    mp4_type.nTimeIncRes = output_format_.video_header.frame_rate * 2;
    // Specify how many P frames between adjacent intra frames.
    mp4_type.nPFrames = output_format_.video_header.i_dist - 1;
    omxresult = OMX_SetParameter(component_handle_,
                                 OMX_IndexParamVideoMpeg4,
                                 &mp4_type);
    if (omxresult != OMX_ErrorNone) {
      LOG(ERROR) << "SetParameter(OMX_IndexParamVideoMpeg4) failed";
      return false;
    }
  }

  OMX_VIDEO_PARAM_BITRATETYPE bitrate;
  omxresult = OMX_GetParameter(component_handle_,
                               OMX_IndexParamVideoBitrate,
                               &bitrate);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexParamVideoBitrate) failed";
    return false;
  }
  // TODO(jiesun): expose other rate control method that matters.
  bitrate.eControlRate = OMX_Video_ControlRateConstant;
  bitrate.nTargetBitrate = output_format_.video_header.bit_rate;
  omxresult = OMX_SetParameter(component_handle_,
                               OMX_IndexParamVideoBitrate,
                               &bitrate);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SetParameter(OMX_IndexParamVideoBitrate) failed";
    return false;
  }

  OMX_CONFIG_FRAMERATETYPE framerate;
  omxresult = OMX_GetConfig(component_handle_,
                            OMX_IndexConfigVideoFramerate,
                            &framerate);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "GetParameter(OMX_IndexConfigVideoFramerate) failed";
    return false;
  }

  framerate.xEncodeFramerate =
      output_format_.video_header.frame_rate << 16;  // Q16 format.
  omxresult = OMX_SetConfig(component_handle_,
                            OMX_IndexConfigVideoFramerate,
                            &framerate);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SetParameter(OMX_IndexConfigVideoFramerate) failed";
    return false;
  }

  // Emulate a format change to conform with decoder case.
  ReportFormatChange();
  return true;
}

bool OmxCodec::ConfigureAsDecoder(
    OMX_PARAM_PORTDEFINITIONTYPE* input_port_def,
    OMX_PARAM_PORTDEFINITIONTYPE* output_port_def) {
  // Configure the input port.
  if (input_format_.codec == kCodecNone) {
    LOG(ERROR) << "Unsupported codec " << input_format_.codec;
    return false;
  }
  if (input_format_.codec == kCodecH264)
    input_port_def->format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  else if (input_format_.codec == kCodecMpeg4)
    input_port_def->format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  else if (input_format_.codec == kCodecH263)
    input_port_def->format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
  else if (input_format_.codec == kCodecVc1)
    input_port_def->format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;
  // Assume QCIF.
  // TODO(ajwong): This MUST come from the client library somehow.
  input_port_def->format.video.nFrameWidth  = 720;
  input_port_def->format.video.nFrameHeight = 480;
  OMX_ERRORTYPE omxresult = OMX_ErrorNone;
  omxresult = OMX_SetParameter(component_handle_,
                               OMX_IndexParamPortDefinition,
                               input_port_def);
  if (omxresult != OMX_ErrorNone) {
    LOG(ERROR) << "SetParameter(OMX_IndexParamPortDefinition) "
                  "for input port failed";
    return false;
  }

  // Codec specific configurations.
  // This sets the NAL length size. 0 means we are using a 3 byte start code.
  // Other values specifies number of bytes of the NAL length.
  if (input_format_.codec == kCodecH264) {
    OMX_VIDEO_CONFIG_NALSIZE naluSize;
    ResetPortHeader(*this, &naluSize);
    naluSize.nPortIndex = input_port_;
    naluSize.nNaluBytes = 0;
    omxresult = OMX_SetConfig(component_handle_,
                              OMX_IndexConfigVideoNalSize,
                              (OMX_PTR)&naluSize);
    if (omxresult != OMX_ErrorNone) {
      LOG(ERROR) << "SetConfig(OMX_IndexConfigVideoNalSize) failed";
      return false;
    }
  }
  return true;
}

bool OmxCodec::CanEmptyBuffer() {
  // We can call empty buffer while we are in executing or enabling / disabling
  // the output port.
  return (GetState() == kExecuting || GetState() == kPortSettingDisable ||
          GetState() == kPortSettingEnable) &&
      (GetNextState() == kExecuting || GetNextState() == kPortSettingDisable ||
       GetNextState() == kPortSettingEnable);
}

bool OmxCodec::CanFillBuffer() {
  // Make sure that we are staying in the executing state.
  return GetState() == kExecuting && GetState() == GetNextState();
}

bool OmxCodec::CanAcceptInput() {
  // We can't take input buffer when in error state.
  // TODO(hclam): Reject when in stopped state.
  return GetState() != kError;
}

bool OmxCodec::CanAcceptOutput() {
  // Don't output request when in error state.
  // TODO(hclam): Reject when in stopped state.
  return GetState() != kError;
}

void OmxCodec::EmptyBufferCompleteTask(OMX_BUFFERHEADERTYPE* buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanEmptyBuffer())
    return;

  // Enqueue the available buffer beacuse the decoder has consumed it.
  available_input_buffers_.push(buffer);

  // Try to feed more data into the decoder.
  EmptyBufferTask();
}

void OmxCodec::EmptyBufferTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanEmptyBuffer())
    return;

  // Loop for all available input data and input buffer for the
  // decoder. When input has reached EOS  we need to stop.
  while (!input_queue_.empty() &&
         !available_input_buffers_.empty() &&
         !input_eos_) {
    InputBuffer* buffer = input_queue_.front().first;
    FeedCallback* callback = input_queue_.front().second;
    OMX_BUFFERHEADERTYPE* omx_buffer = available_input_buffers_.front();
    available_input_buffers_.pop();

    // Read into |omx_buffer|.
    input_eos_ = buffer->IsEndOfStream();
    int filled = buffer->Read(omx_buffer->pBuffer, input_buffer_size_);
    if (buffer->Used()) {
      input_queue_.pop();
      callback->Run(buffer);
      delete callback;
    }

    omx_buffer->nInputPortIndex = input_port_;
    omx_buffer->nOffset = 0;
    omx_buffer->nFlags = 0;
    omx_buffer->nFilledLen = filled;
    omx_buffer->pAppPrivate = this;
    omx_buffer->nFlags |= input_eos_ ? OMX_BUFFERFLAG_EOS : 0;
    if (encoder()) {
      next_sample_timestamp_ += base::Time::kMicrosecondsPerSecond /
                                output_format_.video_header.frame_rate;
      omx_buffer->nTimeStamp = next_sample_timestamp_;
    }

    // Give this buffer to OMX.
    OMX_ERRORTYPE ret = OMX_EmptyThisBuffer(component_handle_, omx_buffer);
    if (ret != OMX_ErrorNone) {
      LOG(ERROR) << "OMX_EmptyThisBuffer() failed with result " << ret;
      StateTransitionTask(kError);
      return;
    }
  }
}

void OmxCodec::FillBufferCompleteTask(OMX_BUFFERHEADERTYPE* buffer) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanFillBuffer())
    return;

  // Enqueue the decoded buffer.
  available_output_buffers_.push(buffer);

  // Fulfill read requests and read more from decoder.
  FillBufferTask();
}

void OmxCodec::FillBufferTask() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanFillBuffer())
    return;

  // Loop for all available output buffers and output requests. When we hit
  // EOS then stop.
  while (!output_queue_.empty() &&
         !available_output_buffers_.empty() &&
         !output_eos_) {
    ReadCallback* callback = output_queue_.front();
    output_queue_.pop();
    OMX_BUFFERHEADERTYPE* omx_buffer = available_output_buffers_.front();
    available_output_buffers_.pop();

    // Give the output data to the callback but it doesn't own this buffer.
    callback->RunWithParams(
        MakeTuple(omx_buffer->pBuffer,
                  static_cast<int>(omx_buffer->nFilledLen)));
    delete callback;

    if (omx_buffer->nFlags & OMX_BUFFERFLAG_EOS)
      output_eos_ = true;

    omx_buffer->nOutputPortIndex = output_port_;
    omx_buffer->pAppPrivate = this;
    omx_buffer->nFlags &= ~OMX_BUFFERFLAG_EOS;
    OMX_ERRORTYPE ret = OMX_FillThisBuffer(component_handle_, omx_buffer);
    if (OMX_ErrorNone != ret) {
      LOG(ERROR) << "OMX_FillThisBuffer() failed with result " << ret;
      StateTransitionTask(kError);
      return;
    }
  }
}

void OmxCodec::InitialEmptyBuffer() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanEmptyBuffer())
    return;

  // Use EmptyBuffer() to use available input buffers to feed decoder.
  EmptyBufferTask();
}

void OmxCodec::InitialFillBuffer() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (!CanFillBuffer())
    return;

  // We'll use all the available output buffers so clear the queue
  // just to be safe.
  while (!available_output_buffers_.empty()) {
    available_output_buffers_.pop();
  }

  // Ask the decoder to fill the output buffers.
  for (size_t i = 0; i < output_buffers_.size(); ++i) {
    OMX_BUFFERHEADERTYPE* omx_buffer = output_buffers_[i];
    omx_buffer->nOutputPortIndex = output_port_;
    omx_buffer->pAppPrivate = this;
    // Need to clear the EOS flag.
    omx_buffer->nFlags &= ~OMX_BUFFERFLAG_EOS;
    OMX_ERRORTYPE ret = OMX_FillThisBuffer(component_handle_, omx_buffer);

    if (OMX_ErrorNone != ret) {
      LOG(ERROR) << "OMX_FillThisBuffer() failed with result " << ret;
      StateTransitionTask(kError);
      return;
    }
  }
}

void OmxCodec::EventHandlerInternal(OMX_HANDLETYPE component,
                                    OMX_EVENTTYPE event,
                                    OMX_U32 data1,
                                    OMX_U32 data2,
                                    OMX_PTR event_data) {
  switch(event) {
    case OMX_EventCmdComplete: {
      // If the last command was successful, we have completed
      // a state transition. So notify that we have done it
      // accordingly.
      OMX_COMMANDTYPE cmd = static_cast<OMX_COMMANDTYPE>(data1);
      if (cmd == OMX_CommandPortEnable) {
        PostDoneStateTransitionTask();
      } else if (cmd == OMX_CommandPortDisable) {
        PostDoneStateTransitionTask();
      } else if (cmd == OMX_CommandStateSet) {
        PostDoneStateTransitionTask();
      } else {
        LOG(ERROR) << "Unknown command completed\n";
      }
      break;
    }
    case OMX_EventError:
      if (OMX_ErrorInvalidState == (OMX_ERRORTYPE)data1) {
        // TODO(hclam): what to do here?
      }
      PostStateTransitionTask(kError);
      break;
    case OMX_EventPortSettingsChanged:
      PostStateTransitionTask(kPortSettingDisable);
      break;
    default:
      LOG(ERROR) << "Warning - Unknown event received\n";
      break;
  }
}

void OmxCodec::EmptyBufferCallbackInternal(
    OMX_HANDLETYPE component,
    OMX_BUFFERHEADERTYPE* buffer) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &OmxCodec::EmptyBufferCompleteTask, buffer));
}

void OmxCodec::FillBufferCallbackInternal(
    OMX_HANDLETYPE component,
    OMX_BUFFERHEADERTYPE* buffer) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &OmxCodec::FillBufferCompleteTask, buffer));
}

// static
OMX_ERRORTYPE OmxCodec::EventHandler(OMX_HANDLETYPE component,
                                            OMX_PTR priv_data,
                                            OMX_EVENTTYPE event,
                                            OMX_U32 data1,
                                            OMX_U32 data2,
                                            OMX_PTR event_data) {
  OmxCodec* decoder = static_cast<OmxCodec*>(priv_data);
  decoder->EventHandlerInternal(component, event, data1, data2, event_data);
  return OMX_ErrorNone;
}

// static
OMX_ERRORTYPE OmxCodec::EmptyBufferCallback(
    OMX_HANDLETYPE component,
    OMX_PTR priv_data,
    OMX_BUFFERHEADERTYPE* buffer) {
  OmxCodec* decoder = static_cast<OmxCodec*>(priv_data);
  decoder->EmptyBufferCallbackInternal(component, buffer);
  return OMX_ErrorNone;
}

// static
OMX_ERRORTYPE OmxCodec::FillBufferCallback(
    OMX_HANDLETYPE component,
    OMX_PTR priv_data,
    OMX_BUFFERHEADERTYPE* buffer) {
  OmxCodec* decoder = static_cast<OmxCodec*>(priv_data);
  decoder->FillBufferCallbackInternal(component, buffer);
  return OMX_ErrorNone;
}

}  // namespace media
