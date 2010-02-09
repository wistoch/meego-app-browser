// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// TODO(ajwong): Generalize this class (fix comments, API, and extract
// implemntation) so that it can be used for encoding & decoding of both
// Video and Audio.
//
// An object that works with an OpenMAX component for video decoding.
// Operations on this object are all asynchronous and this object
// requires a message loop that it works on.
//
// USAGES
//
// // Initialization.
// MessageLoop message_loop;
// OmxCodec* decoder = new OmxCodec(&message_loop);
// OmxConfigurator::MediaFormat input_format, output_format;
// input_format.codec = OmxCodec::kCodecH264;
// output_format.codec = OmxCodec::kCodecRaw;
// decoder->Setup(new OmxDecoderConfigurator(input_format, output_format));
// decoder->SetErrorCallback(NewCallback(this, &Client::ErrorCallback));
// decoder->SetFormatCallback(NewCallback(this, &Client::FormatCallback));
//
// // Start is asynchronous. But we don't need to wait for it to proceed.
// decoder->Start();
//
// // We can start giving buffer to the decoder right after start. It will
// // queue the input buffers and output requests and process them until
// // the decoder can actually process them.
// for (int i = 0; i < kInitialBuffers; ++i) {
//   OmxInputBuffer* buffer = PrepareInitialInputBuffer();
//   decoder->Feed(buffer, NewCallback(this, &Client::FeedCallback));
// }
//
// // We can also issue read requests to the decoder.
// decoder->Read(NewCallback(this, &Client::ReadCallback));
//
// // Make the following call to stop the decoder:
// decoder->Stop(NewCallback(this, &Client::StopCallback));
//
// A typical FeedCallback will look like:
// void Client::FeedCallback(OmxInputBuffer* buffer) {
//   // We have read to the end so stop feeding.
//   if (buffer->Eos())
//     return;
//   PrepareInputBuffer(buffer);
//   decoder->Feed(buffer, NewCallback(this, &Client::FeedCallback));
// }
//
// EXTERNAL STATES
//
// Client of this class will only see four states from the decoder:
//                           .........
//                          |  Error  |
//                           .........
//                              ^
//                              `-.
//          .........        .........        ........
//         |  Empty  |  ->  |  Start  |  ->  |  Stop  |
//          .........        .........        ........
//
// How to operate this object in these four states can be described by
// usage above.
//
// INTERNAL STATES
//
// There are multiple internal states to keep track of state transitions
// of the OpenMAX component. The state transitions and the task during
// the transition can be summerized by the following state diagram:
//
//   .........   ->   ..........   ->   ........   ->   .............
//  |  Empty  |      |  Loaded  |      |  Idle  |      |  Executing  |
//   .........   <-   ..........   <-   ........   <-   .............
//                                                       ^         `
//                                                       `         v
//   .........                               .............    ..............
//  |  Error  |                             | Port Enable |  | Port Disable |
//   .........                               .............    ..............
//
// We need to perform specific tasks in order to transition from one state
// to another. When an error is received, this object will transition to
// the error state.

#ifndef MEDIA_OMX_OMX_CODEC_H_
#define MEDIA_OMX_OMX_CODEC_H_

#include <queue>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "media/omx/omx_configurator.h"
#include "third_party/openmax/il/OMX_Component.h"
#include "third_party/openmax/il/OMX_Core.h"
#include "third_party/openmax/il/OMX_Video.h"

class MessageLoop;

namespace media {

class OmxInputBuffer;

class OmxCodec : public base::RefCountedThreadSafe<OmxCodec> {
 public:
  // TODO(jiesun): remove callback parameters.
  typedef Callback2<
      const OmxConfigurator::MediaFormat&,
      const OmxConfigurator::MediaFormat&>::Type FormatCallback;
  typedef Callback1<OmxInputBuffer*>::Type FeedCallback;
  typedef Callback2<uint8*, int>::Type ReadCallback;
  typedef Callback0::Type Callback;

  // Initialize an OmxCodec object that runs on |message_loop|. It is
  // guaranteed that callbacks are executed on this message loop.
  explicit OmxCodec(MessageLoop* message_loop);
  virtual ~OmxCodec();

  // Setup OmxCodec using |configurator|. Ownership of |configurator|
  // is passed to this class. It is then used for configuration.
  void Setup(OmxConfigurator* configurator);

  // Set the error callback. In case of error the callback will be called.
  void SetErrorCallback(Callback* callback);

  // Set the format change callback. In case of input stream changes.
  void SetFormatCallback(FormatCallback* callback);

  // Start the decoder, this will start the initialization asynchronously.
  // Client can start feeding to and reading from the decoder.
  void Start();

  // Stop the decoder. When the decoder is fully stopped, |callback|
  // is called.
  void Stop(Callback* callback);

  // Read decoded buffer from the decoder. When there is decoded data
  // ready to be consumed |callback| is called.
  void Read(ReadCallback* callback);

  // Feed the decoder with |buffer|. When the decoder has consumed the
  // buffer |callback| is called with |buffer| being the parameter.
  void Feed(OmxInputBuffer* buffer, FeedCallback* callback);

  // Flush the decoder and reset its end-of-stream state.
  void Flush(Callback* callback);

  // Subclass can provide a different value.
  virtual int current_omx_spec_version() const { return 0x00000101; }

 private:
  enum State {
    kEmpty,
    kLoaded,
    kIdle,
    kExecuting,
    kPortSettingEnable,
    kPortSettingDisable,
    kError,
  };

  // Getter and setter for the state.
  State GetState() const;
  void SetState(State state);
  State GetNextState() const;
  void SetNextState(State state);

  // Methods to be executed in |message_loop_|, they correspond to the
  // public methods.
  void StartTask();
  void StopTask(Callback* callback);
  void ReadTask(ReadCallback* callback);
  void FeedTask(scoped_refptr<OmxInputBuffer> buffer,
                FeedCallback* callback);

  // Helper method to perform tasks when this object is stopped.
  void DoneStop();

  // Helper method to call |error_callback_| after transition to error
  // state is done.
  void ReportError();

  // Helper method to call |format_callback_| after a format change.
  // used when decoder output port had done with port reconfigure and
  // return to enabled state.
  void ReportFormatChange(
      const OmxConfigurator::MediaFormat& input_format,
      const OmxConfigurator::MediaFormat& output_format);

  // Helper method to configure port format at LOADED state.
  bool ConfigureIOPorts();

  // Methods and free input and output buffers.
  bool AllocateInputBuffers();
  bool AllocateOutputBuffers();
  void FreeInputBuffers();
  void FreeOutputBuffers();
  void FreeInputQueue();
  void FreeOutputQueue();

  // Transition methods define the specific tasks needs to be done
  // in order transition to the next state.
  void Transition_EmptyToLoaded();
  void Transition_LoadedToIdle();
  void Transition_IdleToExecuting();
  void Transition_ExecutingToDisable();
  void Transition_DisableToEnable();
  void Transition_DisableToIdle();
  void Transition_EnableToExecuting();
  void Transition_EnableToIdle();
  void Transition_ExecutingToIdle();
  void Transition_IdleToLoaded();
  void Transition_LoadedToEmpty();
  void Transition_Error();

  // State transition routines. They control which task to perform based
  // on the current state and the next state.
  void PostStateTransitionTask(State state);
  void StateTransitionTask(State state);

  // This method does an automatic state transition after the last
  // state transition was completed. For example, after the decoder
  // has transitioned from kEmpty to kLoaded, this method will order
  // transition from kLoaded to kIdle.
  void PostDoneStateTransitionTask();
  void DoneStateTransitionTask();

  // Determine whether we can issue fill buffer or empty buffer
  // to the decoder based on the current state and next state.
  bool CanFillBuffer();
  bool CanEmptyBuffer();

  // Determine whether we can use |input_queue_| and |output_queue_|
  // based on the current state.
  bool CanAcceptInput();
  bool CanAcceptOutput();

  // Methods to handle incoming (encoded) buffers.
  void EmptyBufferCompleteTask(OMX_BUFFERHEADERTYPE* buffer);
  void EmptyBufferTask();

  // Methods to handle outgoing (decoded) buffers.
  void FillBufferCompleteTask(OMX_BUFFERHEADERTYPE* buffer);
  void FillBufferTask();

  // Methods that do initial reads to kick start the decoding process.
  void InitialFillBuffer();
  void InitialEmptyBuffer();

  // Member functions to handle events from the OMX component. They
  // are called on the thread that the OMX component runs on, thus
  // it is not safe to perform any operations on them. They simply
  // post a task on |message_loop_| to do the actual work.
  void EventHandlerInternal(OMX_HANDLETYPE component,
                            OMX_EVENTTYPE event,
                            OMX_U32 data1, OMX_U32 data2,
                            OMX_PTR event_data);

  void EmptyBufferCallbackInternal(OMX_HANDLETYPE component,
                                   OMX_BUFFERHEADERTYPE* buffer);

  void FillBufferCallbackInternal(OMX_HANDLETYPE component,
                                  OMX_BUFFERHEADERTYPE* buffer);

  // The following three methods are static callback methods
  // for the OMX component. When these callbacks are received, the
  // call is delegated to the three internal methods above.
  static OMX_ERRORTYPE EventHandler(OMX_HANDLETYPE component,
                                    OMX_PTR priv_data,
                                    OMX_EVENTTYPE event,
                                    OMX_U32 data1, OMX_U32 data2,
                                    OMX_PTR event_data);

  static OMX_ERRORTYPE EmptyBufferCallback(OMX_HANDLETYPE component,
                                           OMX_PTR priv_data,
                                           OMX_BUFFERHEADERTYPE* buffer);

  static OMX_ERRORTYPE FillBufferCallback(OMX_HANDLETYPE component,
                                          OMX_PTR priv_data,
                                          OMX_BUFFERHEADERTYPE* buffer);

  std::vector<OMX_BUFFERHEADERTYPE*> input_buffers_;
  int input_buffer_count_;
  int input_buffer_size_;
  int input_port_;
  bool input_eos_;

  std::vector<OMX_BUFFERHEADERTYPE*> output_buffers_;
  int output_buffer_count_;
  int output_buffer_size_;
  int output_port_;
  bool output_eos_;

  // |state_| records the current state. During state transition
  // |next_state_| is the next state that this machine will transition
  // to. After a state transition is completed and the state becomes
  // stable then |next_state_| equals |state_|. Inequality can be
  // used to detect a state transition.
  // These two members are read and written only on |message_loop_|.
  State state_;
  State next_state_;

  std::string role_name_;
  std::string component_name_;
  OMX_COMPONENTTYPE* component_handle_;
  scoped_ptr<OmxConfigurator> configurator_;
  MessageLoop* message_loop_;

  scoped_ptr<FormatCallback> format_callback_;
  scoped_ptr<Callback> stop_callback_;
  scoped_ptr<Callback> error_callback_;

  // Input and output queue for encoded data and decoded frames.
  typedef std::pair<scoped_refptr<OmxInputBuffer>, FeedCallback*> InputUnit;
  std::queue<InputUnit> input_queue_;
  std::queue<ReadCallback*> output_queue_;

  // Input and output buffers that we can use to feed the decoder.
  std::queue<OMX_BUFFERHEADERTYPE*> available_input_buffers_;
  std::queue<OMX_BUFFERHEADERTYPE*> available_output_buffers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmxCodec);
};

}  // namespace media

#endif  // MEDIA_OMX_OMX_CODEC_H_
