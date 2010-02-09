// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/test_tab_contents.h"

#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"

TestTabContents::TestTabContents(Profile* profile, SiteInstance* instance)
    : TabContents(profile, instance, MSG_ROUTING_NONE, NULL),
      transition_cross_site(false) {
}

TestRenderViewHost* TestTabContents::pending_rvh() {
  return static_cast<TestRenderViewHost*>(
      render_manager_.pending_render_view_host_);
}

bool TestTabContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host) {
  // This will go to a TestRenderViewHost.
  render_view_host->CreateRenderView(profile()->GetRequestContext());
  return true;
}

TabContents* TestTabContents::Clone() {
  TabContents* tc = new TestTabContents(
      profile(), SiteInstance::CreateSiteInstance(profile()));
  tc->controller().CopyStateFrom(controller_);
  return tc;
}
