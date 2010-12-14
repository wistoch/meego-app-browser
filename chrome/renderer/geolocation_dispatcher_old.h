// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GEOLOCATION_DISPATCHER_OLD_H_
#define CHROME_RENDERER_GEOLOCATION_DISPATCHER_OLD_H_
#pragma once

#include "base/basictypes.h"
#include "base/id_map.h"
#include "ipc/ipc_message.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebGeolocationService.h"

class GURL;
class RenderView;
struct Geoposition;

// GeolocationDispatcherOld is a delegate for Geolocation messages used by
// WebKit.
// It's the complement of GeolocationDispatcherHostOld (owned by
// RenderViewHost).

// TODO(jknotten): Remove this class once the new client-based implementation is
// checked in (see http://crbug.com/59908).
class GeolocationDispatcherOld : public WebKit::WebGeolocationService {
 public:
  explicit GeolocationDispatcherOld(RenderView* render_view);
  virtual ~GeolocationDispatcherOld();

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled. Called in render thread.
  bool OnMessageReceived(const IPC::Message& msg);

  // WebKit::WebGeolocationService.
  virtual void requestPermissionForFrame(int bridge_id,
                                         const WebKit::WebURL& url);
  virtual void cancelPermissionRequestForFrame(
      int bridge_id, const WebKit::WebURL& url);
  virtual void startUpdating(
      int bridge_id, const WebKit::WebURL& url, bool enableHighAccuracy);
  virtual void stopUpdating(int bridge_id);
  virtual void suspend(int bridge_id);
  virtual void resume(int bridge_id);
  virtual int attachBridge(
      WebKit::WebGeolocationServiceBridge* geolocation_service);
  virtual void detachBridge(int bridge_id);

 private:
  // Permission for using geolocation has been set.
  void OnGeolocationPermissionSet(int bridge_id, bool is_allowed);

  // We have an updated geolocation position or error code.
  void OnGeolocationPositionUpdated(const Geoposition& geoposition);

  RenderView* render_view_;

  // The geolocation services attached to this dispatcher.
  IDMap<WebKit::WebGeolocationServiceBridge> bridges_map_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherOld);
};

#endif  // CHROME_RENDERER_GEOLOCATION_DISPATCHER_OLD_H_
