// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/view_source_contents.h"

#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/render_view_host.h"

ViewSourceContents::ViewSourceContents(Profile* profile, SiteInstance* instance)
    : WebContents(profile, instance, NULL, MSG_ROUTING_NONE, NULL) {
  type_ = TAB_CONTENTS_VIEW_SOURCE;
}

void ViewSourceContents::RendererCreated(RenderViewHost* host) {
  // Make sure the renderer is in view source mode.
  host->Send(new ViewMsg_EnableViewSourceMode(host->routing_id()));
}

