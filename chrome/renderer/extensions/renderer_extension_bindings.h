// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_

#include "v8/include/v8.h"

#include <string>

class RenderThreadBase;

// This class adds extension-related javascript bindings to a renderer.  It is
// used by both web renderers and extension processes.
class RendererExtensionBindings {
 public:
  // Name of extension, for dependencies.
  static const char* kName;

  // Creates an instance of the extension.
  static v8::Extension* Get(RenderThreadBase* render_thread);

  // Notify any listeners that a message channel has been opened to this
  // process.
  static void HandleConnect(int port_id);

  // Dispatch the given message sent on this channel.
  static void HandleMessage(const std::string& message, int port_id);
};

#endif  // CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_
