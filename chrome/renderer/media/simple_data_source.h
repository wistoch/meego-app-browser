// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An extremely simple implementation of DataSource that downloads the entire
// media resource into memory before signaling that initialization has finished.
// Primarily used to test <audio> and <video> with buffering/caching removed
// from the equation.

#ifndef CHROME_RENDERER_MEDIA_SIMPLE_DATA_SOURCE_H_
#define CHROME_RENDERER_MEDIA_SIMPLE_DATA_SOURCE_H_

#include "base/scoped_ptr.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "webkit/glue/resource_loader_bridge.h"

class MessageLoop;
class WebMediaPlayerDelegateImpl;

class SimpleDataSource :
    public media::DataSource,
    public webkit_glue::ResourceLoaderBridge::Peer {
 public:
  static media::FilterFactory* CreateFactory(int32 routing_id) {
    return new media::FilterFactoryImpl1<SimpleDataSource, int32>(routing_id);
  }

  // MediaFilter implementation.
  virtual void Stop();

  // DataSource implementation.
  virtual bool Initialize(const std::string& url);
  virtual const media::MediaFormat& media_format();
  virtual size_t Read(uint8* data, size_t size);
  virtual bool GetPosition(int64* position_out);
  virtual bool SetPosition(int64 position);
  virtual bool GetSize(int64* size_out);
  virtual bool IsSeekable();

  // webkit_glue::ResourceLoaderBridge::Peer implementation.
  virtual void OnDownloadProgress(uint64 position, uint64 size);
  virtual void OnUploadProgress(uint64 position, uint64 size);
  virtual void OnReceivedRedirect(const GURL& new_url);
  virtual void OnReceivedResponse(
      const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
      bool content_filtered);
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(const URLRequestStatus& status,
                                  const std::string& security_info);
  virtual std::string GetURLForDebugging();

 private:
  friend class media::FilterFactoryImpl1<SimpleDataSource, int32>;
  SimpleDataSource(int32 routing_id);
  virtual ~SimpleDataSource();

  // Updates |url_| and |media_format_| with the given URL.
  void SetURL(const std::string& url);

  // Start the resource loading on the render thread.
  void StartTask();

  // Passed in during construction, used when creating the bridge.
  int32 routing_id_;

  // Primarily used for asserting the bridge is loading on the render thread.
  MessageLoop* render_loop_;

  // Bridge used to load the media resource.
  scoped_ptr<webkit_glue::ResourceLoaderBridge> bridge_;

  media::MediaFormat media_format_;
  std::string url_;
  std::string data_;
  int64 size_;
  int64 position_;

  DISALLOW_COPY_AND_ASSIGN(SimpleDataSource);
};

#endif  // CHROME_RENDERER_MEDIA_SIMPLE_DATA_SOURCE_H_
