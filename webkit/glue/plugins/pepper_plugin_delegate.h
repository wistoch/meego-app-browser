// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_DELEGATE_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_DELEGATE_H_

#include <string>

#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "third_party/ppapi/c/pp_completion_callback.h"
#include "third_party/ppapi/c/pp_errors.h"
#include "third_party/ppapi/c/pp_stdint.h"
#include "third_party/ppapi/c/pp_video.h"

class AudioMessageFilter;

namespace skia {
class PlatformCanvas;
}

namespace WebKit {
class WebFileChooserCompletion;
struct WebFileChooserParams;
}

namespace pepper {

class PluginInstance;

// Virtual interface that the browser implements to implement features for
// Pepper plugins.
class PluginDelegate {
 public:
  // Represents an image. This is to allow the browser layer to supply a correct
  // image representation. In Chrome, this will be a TransportDIB.
  class PlatformImage2D {
   public:
    virtual ~PlatformImage2D() {}

    // Caller will own the returned pointer, returns NULL on failure.
    virtual skia::PlatformCanvas* Map() = 0;

    // Returns the platform-specific shared memory handle of the data backing
    // this image. This is used by NativeClient to send the image to the
    // out-of-process plugin. Returns 0 on failure.
    virtual intptr_t GetSharedMemoryHandle() const = 0;
  };

  class PlatformAudio {
   public:
    class Client {
     public:
      virtual ~Client() {}

      // Called when the stream is created.
      virtual void StreamCreated(base::SharedMemoryHandle shared_memory_handle,
                                 size_t shared_memory_size,
                                 base::SyncSocket::Handle socket) = 0;
    };

    virtual ~PlatformAudio() {}

    // Starts the playback. Returns false on error or if called before the
    // stream is created or after the stream is closed.
    virtual bool StartPlayback() = 0;

    // Stops the playback. Returns false on error or if called before the stream
    // is created or after the stream is closed.
    virtual bool StopPlayback() = 0;

    // Closes the stream. Make sure to call this before the object is
    // destructed.
    virtual void ShutDown() = 0;
  };

  class PlatformVideoDecoder {
   public:
    virtual ~PlatformVideoDecoder() {}

    // Returns false on failure.
    virtual bool Decode(PP_VideoCompressedDataBuffer& input_buffer) = 0;
    virtual int32_t Flush(PP_CompletionCallback& callback) = 0;
    virtual bool ReturnUncompressedDataBuffer(
        PP_VideoUncompressedDataBuffer& buffer) = 0;
  };

  // Indicates that the given instance has been created.
  virtual void InstanceCreated(pepper::PluginInstance* instance) = 0;

  // Indicates that the given instance is being destroyed. This is called from
  // the destructor, so it's important that the instance is not dereferenced
  // from this call.
  virtual void InstanceDeleted(pepper::PluginInstance* instance) = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformImage2D* CreateImage2D(int width, int height) = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformVideoDecoder* CreateVideoDecoder(
      const PP_VideoDecoderConfig& decoder_config) = 0;

  // Notifies that the number of find results has changed.
  virtual void DidChangeNumberOfFindResults(int identifier,
                                           int total,
                                           bool final_result) = 0;

  // Notifies that the index of the currently selected item has been updated.
  virtual void DidChangeSelectedFindResult(int identifier, int index) = 0;

  // The caller will own the pointer returned from this.
  virtual PlatformAudio* CreateAudio(uint32_t sample_rate,
                                     uint32_t sample_count,
                                     PlatformAudio::Client* client) = 0;

  // Runs a file chooser.
  virtual bool RunFileChooser(
      const WebKit::WebFileChooserParams& params,
      WebKit::WebFileChooserCompletion* chooser_completion) = 0;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_DELEGATE_H_
