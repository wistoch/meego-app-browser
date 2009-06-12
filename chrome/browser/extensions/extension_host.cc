// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_host.h"

#include "app/resource_bundle.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

#include "webkit/glue/context_menu.h"

ExtensionHost::ExtensionHost(Extension* extension, SiteInstance* site_instance,
                             const GURL& url, ExtensionProcessManager* manager)
    : extension_(extension),
      manager_(manager),
      did_stop_loading_(false),
      url_(url) {
  render_view_host_ = new RenderViewHost(
      site_instance, this, MSG_ROUTING_NONE, NULL);
  render_view_host_->AllowExtensionBindings();
}

ExtensionHost::~ExtensionHost() {
  if (manager_)  // To allow passing NULL in tests.
    manager_->OnExtensionHostDestroyed(this);
  render_view_host_->Shutdown();  // deletes render_view_host
}

void ExtensionHost::CreateView(Browser* browser) {
#if defined(TOOLKIT_VIEWS)
  view_.reset(new ExtensionView(this, browser));
  // We own |view_|, so don't auto delete when it's removed from the view
  // hierarchy.
  view_->SetParentOwned(false);
#else
  // TODO(port)
  NOTREACHED();
#endif
}

RenderProcessHost* ExtensionHost::render_process_host() const {
  return render_view_host_->process();
}

SiteInstance* ExtensionHost::site_instance() const {
  return render_view_host_->site_instance();
}

void ExtensionHost::CreateRenderView(RenderWidgetHostView* host_view) {
  render_view_host_->set_view(host_view);
  render_view_host_->CreateRenderView();
  render_view_host_->NavigateToURL(url_);
}

void ExtensionHost::UpdatePreferredWidth(int pref_width) {
#if defined(OS_WIN)
  if (view_.get())
    view_->DidContentsPreferredWidthChange(pref_width);
#endif
}

WebPreferences ExtensionHost::GetWebkitPrefs() {
  PrefService* prefs = render_view_host()->process()->profile()->GetPrefs();
  const bool kIsDomUI = true;
  return RenderViewHostDelegateHelper::GetWebkitPrefs(prefs, kIsDomUI);
}

void ExtensionHost::RunJavaScriptMessage(
    const std::wstring& message,
    const std::wstring& default_prompt,
    const GURL& frame_url,
    const int flags,
    IPC::Message* reply_msg,
    bool* did_suppress_message) {
  // Automatically cancel the javascript alert (otherwise the renderer hangs
  // indefinitely).
  *did_suppress_message = true;
  render_view_host()->JavaScriptMessageBoxClosed(reply_msg, true, L"");
}

void ExtensionHost::DidStopLoading(RenderViewHost* render_view_host) {
  // TODO(aa): This is toolstrip-specific and should probably not be here.
  // ExtensionToolstrip in bookmark_bar_view.cc?
  static const StringPiece toolstrip_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_EXTENSIONS_TOOLSTRIP_CSS));
  render_view_host->InsertCSSInWebFrame(L"", toolstrip_css.as_string());

  render_view_host->WasResized();
  did_stop_loading_ = true;

#if defined(OS_WIN)
  if (view_.get())
    view_->ShowIfCompletelyLoaded();
#endif
}

ExtensionFunctionDispatcher* ExtensionHost::
    CreateExtensionFunctionDispatcher(RenderViewHost *render_view_host,
                                      const std::string& extension_id) {
  return new ExtensionFunctionDispatcher(render_view_host, this, extension_id);
}

RenderViewHostDelegate::View* ExtensionHost::GetViewDelegate() const {
  // TODO(erikkay) this is unfortunate.  The interface declares that this method
  // must be const (no good reason for it as far as I can tell) which means you
  // can't return self without doing this const_cast.  Either we need to change
  // the interface, or we need to split out the view delegate into another
  // object (which is how TabContents works).
  return const_cast<ExtensionHost*>(this);
}

void ExtensionHost::CreateNewWindow(int route_id,
                                    base::WaitableEvent* modal_dialog_event) {
  delegate_view_helper_.CreateNewWindow(
      route_id, modal_dialog_event, render_view_host()->process()->profile(),
      site_instance());
}

void ExtensionHost::CreateNewWidget(int route_id, bool activatable) {
  delegate_view_helper_.CreateNewWidget(route_id, activatable,
                                        site_instance()->GetProcess());
}

void ExtensionHost::ShowCreatedWindow(int route_id,
                                      WindowOpenDisposition disposition,
                                      const gfx::Rect& initial_pos,
                                      bool user_gesture,
                                      const GURL& creator_url) {
  TabContents* contents = delegate_view_helper_.GetCreatedWindow(route_id);
  if (contents) {
    Browser* browser = GetBrowser();
    DCHECK(browser);
    if (!browser)
      return;
    // TODO(erikkay) is it safe to pass in NULL as source?
    browser->AddTabContents(contents, disposition, initial_pos,
                            user_gesture);
  }
}

void ExtensionHost::ShowCreatedWidget(int route_id,
                                      const gfx::Rect& initial_pos) {
  RenderWidgetHostView* widget_host_view =
      delegate_view_helper_.GetCreatedWidget(route_id);
  Browser *browser = GetBrowser();
  DCHECK(browser);
  if (!browser)
    return;
  browser->BrowserRenderWidgetShowing();
  // TODO(erikkay): These two lines could be refactored with TabContentsView.
  widget_host_view->InitAsPopup(render_view_host()->view(), initial_pos);
  widget_host_view->GetRenderWidgetHost()->Init();
}

void ExtensionHost::ShowContextMenu(const ContextMenuParams& params) {
  // TODO(erikkay) - This is a temporary hack.  Show a menu here instead.
  g_browser_process->devtools_manager()->OpenDevToolsWindow(render_view_host());
}

void ExtensionHost::StartDragging(const WebDropData& drop_data) {
}

void ExtensionHost::UpdateDragCursor(bool is_drop_target) {
}

void ExtensionHost::GotFocus() {
}

void ExtensionHost::TakeFocus(bool reverse) {
}

void ExtensionHost::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
}

void ExtensionHost::HandleMouseEvent() {
#if defined(OS_WIN)
  if (view_.get())
    view_->HandleMouseEvent();
#endif
}

void ExtensionHost::HandleMouseLeave() {
#if defined(OS_WIN)
  if (view_.get())
    view_->HandleMouseLeave();
#endif
}

Browser* ExtensionHost::GetBrowser() {
#if defined(OS_WIN)
  if (view_.get())
    return view_->browser();
#endif
  Browser* browser = BrowserList::GetLastActiveWithProfile(
      render_view_host()->process()->profile());
  // NOTE(rafaelw): This can return NULL in some circumstances. In particular,
  // a toolstrip or background_page onload chrome.tabs api call can make it
  // into here before the browser is sufficiently initialized to return here.
  // A similar situation may arise during shutdown.
  // TODO(rafaelw): Delay creation of background_page until the browser
  // is available. http://code.google.com/p/chromium/issues/detail?id=13284
  return browser;
}
