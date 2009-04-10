// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/browser_with_test_window_test.h"

#include "chrome/browser/browser.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/test_browser_window.h"
#include "chrome/test/testing_profile.h"

BrowserWithTestWindowTest::BrowserWithTestWindowTest() {
  OleInitialize(NULL);
}

void BrowserWithTestWindowTest::SetUp() {
  // NOTE: I have a feeling we're going to want virtual methods for creating
  // these, as such they're in SetUp instead of the constructor.
  profile_.reset(new TestingProfile());
  tab_contents_factory_.reset(
      TestTabContentsFactory::CreateAndRegisterFactory());
  browser_.reset(new Browser(Browser::TYPE_NORMAL, profile()));
  window_.reset(new TestBrowserWindow(browser()));
  browser_->set_window(window_.get());
}

BrowserWithTestWindowTest::~BrowserWithTestWindowTest() {
  // Make sure we close all tabs, otherwise Browser isn't happy in its
  // destructor.
  browser()->CloseAllTabs();

  // A Task is leaked if we don't destroy everything, then run the message
  // loop.
  browser_.reset(NULL);
  window_.reset(NULL);
  tab_contents_factory_.reset(NULL);
  profile_.reset(NULL);

  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  MessageLoop::current()->Run();

  OleUninitialize();
}

void BrowserWithTestWindowTest::AddTestingTab(Browser* browser) {
  TestTabContents* tab_contents = tab_contents_factory_->CreateInstanceImpl();
  tab_contents->SetupController(profile());
  browser->tabstrip_model()->AddTabContents(
      tab_contents, 0, PageTransition::TYPED, true);
}

void BrowserWithTestWindowTest::NavigateAndCommit(
    NavigationController* controller,
    const GURL& url) {
  controller->LoadURL(url, GURL(), 0);

  // Commit the load.
  // TODO(brettw) once this uses TestRenderViewHost, we should call SendNavigate
  // on it instead of doing this stuff.
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = reinterpret_cast<TestTabContents*>(
      controller->tab_contents())->GetNextPageID();
  params.url = url;
  params.transition = PageTransition::LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.is_post = false;
  NavigationController::LoadCommittedDetails details;
  controller->RendererDidNavigate(params, &details);
}

void BrowserWithTestWindowTest::NavigateAndCommitActiveTab(const GURL& url) {
  NavigateAndCommit(browser()->GetSelectedTabContents()->controller(), url);
}
