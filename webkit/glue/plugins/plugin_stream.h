// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_STREAM_H__
#define WEBKIT_GLUE_PLUGIN_PLUGIN_STREAM_H__

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "third_party/npapi/bindings/npapi.h"


namespace NPAPI {

class PluginInstance;

// Base class for a NPAPI stream.  Tracks basic elements
// of a stream for NPAPI notifications and stream position.
class PluginStream : public base::RefCounted<PluginStream> {
 public:
  // Create a new PluginStream object.  If needNotify is true, then the
  // plugin will be notified when the stream has been fully sent.
  PluginStream(PluginInstance *instance,
               const char *url,
               bool need_notify,
               void *notify_data);
  virtual ~PluginStream();

  // In case of a redirect, this can be called to update the url.  But it must
  // be called before Open().
  void UpdateUrl(const char* url);

  // Opens the stream to the Plugin.
  // If the mime-type is not specified, we'll try to find one based on the
  // mime-types table and the extension (if any) in the URL.
  // If the size of the stream is known, use length to set the size.  If
  // not known, set length to 0.
  bool Open(const std::string &mime_type,
            const std::string &headers,
            uint32 length,
            uint32 last_modified);

  // Writes to the stream.
  int Write(const char *buf, const int len);

  // Write the result as a file.
  void WriteAsFile();

  // Notify the plugin that a stream is complete.
  void Notify(NPReason reason);

  // Close the stream.
  virtual bool Close(NPReason reason);

  const NPStream* stream() const {
    return &stream_;
  }

 protected:
  PluginInstance* instance() { return instance_.get(); }
  // Check if the stream is open.
  bool open() { return opened_; }

 private:
  // Open a temporary file for this stream.  
  // If successful, will set temp_file_name_, temp_file_handle_, and
  // return true.
  bool OpenTempFile();

  // Closes the temporary file if it is open.
  void CloseTempFile();

  // Closes the temporary file if it is open and deletes the file.
  void CleanupTempFile();

  // Sends the data to the file if it's open.
  bool WriteToFile(const char *buf, const int length);

  // Sends the data to the plugin.  If it's not ready, handles buffering it
  // and retrying later.
  bool WriteToPlugin(const char *buf, const int length);

  // Send the data to the plugin, returning how many bytes it accepted, or -1
  // if an error occurred.
  int TryWriteToPlugin(const char *buf, const int length);

  // The callback which calls TryWriteToPlugin.
  void OnDelayDelivery();

 private:
  NPStream                      stream_;
  std::string                   headers_;
  scoped_refptr<PluginInstance> instance_;
  int                           bytes_sent_;
  bool                          notify_needed_;
  void *                        notify_data_;
  bool                          close_on_write_data_;
  uint16                        requested_plugin_mode_;
  bool                          opened_;
  char                          temp_file_name_[MAX_PATH];
  HANDLE                        temp_file_handle_;
  std::vector<char>             delivery_data_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginStream);
};

} // namespace NPAPI

#endif // WEBKIT_GLUE_PLUGIN_PLUGIN_STREAM_H__

