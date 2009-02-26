// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dev_tools_ui.h"

#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/render_messages.h"

// DevToolsUI is accessible from chrome-ui://devtools.
static const char kDevToolsHost[] = "devtools";

// static
GURL DevToolsUI::GetBaseURL() {
  return GURL(DOMUIContents::GetScheme() + "://" + kDevToolsHost);
}

void DevToolsUI::RenderViewCreated(RenderViewHost* render_view_host) {
  render_view_host->Send(new ViewMsg_SetupDevToolsClient(
      render_view_host->routing_id()));
}


