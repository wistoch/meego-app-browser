// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of TestWebViewDelegate, which serves
// as the WebViewDelegate for the TestShellWebHost.  The host is expected to
// have initialized a MessageLoop before these methods are called.

#include "config.h"

#undef LOG

#include "webkit/tools/test_shell/test_webview_delegate.h"

#include "base/file_util.h"
#include "base/gfx/point.h"
#include "base/gfx/native_widget_types.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "net/base/net_errors.h"
#include "webkit/api/public/WebConsoleMessage.h"
#include "webkit/api/public/WebCString.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebDataSource.h"
#include "webkit/api/public/WebDragData.h"
#include "webkit/api/public/WebHistoryItem.h"
#include "webkit/api/public/WebFrame.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebNode.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebPopupMenu.h"
#include "webkit/api/public/WebRange.h"
#include "webkit/api/public/WebScreenInfo.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLError.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/api/public/WebURLResponse.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/media/buffered_data_source.h"
#include "webkit/glue/media/media_resource_loader_bridge_factory.h"
#include "webkit/glue/media/simple_data_source.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webplugin_impl.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/glue/webmediaplayer_impl.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_web_worker.h"

#if defined(OS_WIN)
// TODO(port): make these files work everywhere.
#include "webkit/tools/test_shell/drag_delegate.h"
#include "webkit/tools/test_shell/drop_delegate.h"
#endif

using WebKit::WebConsoleMessage;
using WebKit::WebData;
using WebKit::WebDataSource;
using WebKit::WebDragData;
using WebKit::WebDragOperationsMask;
using WebKit::WebEditingAction;
using WebKit::WebForm;
using WebKit::WebFrame;
using WebKit::WebHistoryItem;
using WebKit::WebMediaPlayer;
using WebKit::WebMediaPlayerClient;
using WebKit::WebNavigationType;
using WebKit::WebNavigationPolicy;
using WebKit::WebNode;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebPopupMenu;
using WebKit::WebRange;
using WebKit::WebRect;
using WebKit::WebScreenInfo;
using WebKit::WebSecurityOrigin;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebTextAffinity;
using WebKit::WebTextDirection;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebWidget;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;

namespace {

// WebNavigationType debugging strings taken from PolicyDelegate.mm.
const char* kLinkClickedString = "link clicked";
const char* kFormSubmittedString = "form submitted";
const char* kBackForwardString = "back/forward";
const char* kReloadString = "reload";
const char* kFormResubmittedString = "form resubmitted";
const char* kOtherString = "other";
const char* kIllegalString = "illegal value";

int next_page_id_ = 1;

// Used to write a platform neutral file:/// URL by only taking the filename
// (e.g., converts "file:///tmp/foo.txt" to just "foo.txt").
std::string UrlSuitableForTestResult(const std::string& url) {
  if (url.empty() || std::string::npos == url.find("file://"))
    return url;

  std::string filename =
      WideToUTF8(file_util::GetFilenameFromPath(UTF8ToWide(url)));
  if (filename.empty())
    return "file:";  // A WebKit test has this in its expected output.
  return filename;
}

// Adds a file called "DRTFakeFile" to |data_object| (CF_HDROP).  Use to fake
// dragging a file.
void AddDRTFakeFileToDataObject(WebDragData* drag_data) {
  drag_data->appendToFileNames(WebString::fromUTF8("DRTFakeFile"));
}

// Get a debugging string from a WebNavigationType.
const char* WebNavigationTypeToString(WebNavigationType type) {
  switch (type) {
    case WebKit::WebNavigationTypeLinkClicked:
      return kLinkClickedString;
    case WebKit::WebNavigationTypeFormSubmitted:
      return kFormSubmittedString;
    case WebKit::WebNavigationTypeBackForward:
      return kBackForwardString;
    case WebKit::WebNavigationTypeReload:
      return kReloadString;
    case WebKit::WebNavigationTypeFormResubmitted:
      return kFormResubmittedString;
    case WebKit::WebNavigationTypeOther:
      return kOtherString;
  }
  return kIllegalString;
}

std::string GetURLDescription(const GURL& url) {
  if (url.SchemeIs("file"))
    return url.ExtractFileName();

  return url.possibly_invalid_spec();
}

std::string GetResponseDescription(const WebURLResponse& response) {
  if (response.isNull())
    return "(null)";

  return StringPrintf("<NSURLResponse %s, http status code %d>",
      GURL(response.url()).possibly_invalid_spec().c_str(),
      response.httpStatusCode());
}

std::string GetErrorDescription(const WebURLError& error) {
  std::string domain = UTF16ToASCII(error.domain);
  int code = error.reason;

  if (domain == net::kErrorDomain) {
    domain = "NSURLErrorDomain";
    switch (error.reason) {
      case net::ERR_ABORTED:
        code = -999;
        break;
      case net::ERR_UNSAFE_PORT:
        // Our unsafe port checking happens at the network stack level, but we
        // make this translation here to match the behavior of stock WebKit.
        domain = "WebKitErrorDomain";
        code = 103;
        break;
      case net::ERR_ADDRESS_INVALID:
      case net::ERR_ADDRESS_UNREACHABLE:
        code = -1004;
        break;
    }
  } else {
    DLOG(WARNING) << "Unknown error domain";
  }

  return StringPrintf("<NSError domain %s, code %d, failing URL \"%s\">",
      domain.c_str(), code, error.unreachableURL.spec().data());
}

std::string GetNodeDescription(const WebNode& node, int exception) {
  if (exception)
    return "ERROR";
  if (node.isNull())
    return "(null)";
  std::string str = node.nodeName().utf8();
  const WebNode& parent = node.parentNode();
  if (!parent.isNull()) {
    str.append(" > ");
    str.append(GetNodeDescription(parent, 0));
  }
  return str;
}

std::string GetRangeDescription(const WebRange& range) {
  if (range.isNull())
    return "(null)";
  int exception = 0;
  std::string str = "range from ";
  int offset = range.startOffset();
  str.append(IntToString(offset));
  str.append(" of ");
  WebNode container = range.startContainer(exception);
  str.append(GetNodeDescription(container, exception));
  str.append(" to ");
  offset = range.endOffset();
  str.append(IntToString(offset));
  str.append(" of ");
  container = range.endContainer(exception);
  str.append(GetNodeDescription(container, exception));
  return str;
}

std::string GetEditingActionDescription(WebEditingAction action) {
  switch (action) {
    case WebKit::WebEditingActionTyped:
      return "WebViewInsertActionTyped";
    case WebKit::WebEditingActionPasted:
      return "WebViewInsertActionPasted";
    case WebKit::WebEditingActionDropped:
      return "WebViewInsertActionDropped";
  }
  return "(UNKNOWN ACTION)";
}

std::string GetTextAffinityDescription(WebTextAffinity affinity) {
  switch (affinity) {
    case WebKit::WebTextAffinityUpstream:
      return "NSSelectionAffinityUpstream";
    case WebKit::WebTextAffinityDownstream:
      return "NSSelectionAffinityDownstream";
  }
  return "(UNKNOWN AFFINITY)";
}

}  // namespace

// WebViewDelegate -----------------------------------------------------------

std::string TestWebViewDelegate::GetResourceDescription(uint32 identifier) {
  ResourceMap::iterator it = resource_identifier_map_.find(identifier);
  return it != resource_identifier_map_.end() ? it->second : "<unknown>";
}

void TestWebViewDelegate::ShowContextMenu(
    WebView* webview,
    ContextNodeType node_type,
    int x,
    int y,
    const GURL& link_url,
    const GURL& image_url,
    const GURL& page_url,
    const GURL& frame_url,
    const ContextMenuMediaParams& media_params,
    const std::wstring& selection_text,
    const std::wstring& misspelled_word,
    int edit_flags,
    const std::string& security_info,
    const std::string& frame_charset) {
  CapturedContextMenuEvent context(node_type, x, y);
  captured_context_menu_events_.push_back(context);
}

void TestWebViewDelegate::SetUserStyleSheetEnabled(bool is_enabled) {
  WebPreferences* prefs = shell_->GetWebPreferences();
  prefs->user_style_sheet_enabled = is_enabled;
  prefs->Apply(shell_->webView());
}

void TestWebViewDelegate::SetUserStyleSheetLocation(const GURL& location) {
  WebPreferences* prefs = shell_->GetWebPreferences();
  prefs->user_style_sheet_enabled = true;
  prefs->user_style_sheet_location = location;
  prefs->Apply(shell_->webView());
}

// WebViewClient -------------------------------------------------------------

WebView* TestWebViewDelegate::createView(WebFrame* creator) {
  return shell_->CreateWebView();
}

WebWidget* TestWebViewDelegate::createPopupMenu(
    bool activatable) {
  // TODO(darin): Should we honor activatable?
  return shell_->CreatePopupWidget();
}

void TestWebViewDelegate::didAddMessageToConsole(
    const WebConsoleMessage& message, const WebString& source_name,
    unsigned source_line) {
  if (!shell_->layout_test_mode()) {
    logging::LogMessage("CONSOLE", 0).stream() << "\""
                                               << message.text.utf8().data()
                                               << ",\" source: "
                                               << source_name.utf8().data()
                                               << "("
                                               << source_line
                                               << ")";
  } else {
    // This matches win DumpRenderTree's UIDelegate.cpp.
    std::string new_message;
    if (!message.text.isEmpty()) {
      new_message = message.text.utf8();
      size_t file_protocol = new_message.find("file://");
      if (file_protocol != std::string::npos) {
        new_message = new_message.substr(0, file_protocol) +
            UrlSuitableForTestResult(new_message.substr(file_protocol));
      }
    }

    printf("CONSOLE MESSAGE: line %d: %s\n", source_line, new_message.data());
  }
}

// The output from these methods in layout test mode should match that
// expected by the layout tests.  See EditingDelegate.m in DumpRenderTree.

bool TestWebViewDelegate::shouldBeginEditing(const WebRange& range) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldBeginEditingInDOMRange:%s\n",
           GetRangeDescription(range).c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldEndEditing(const WebRange& range) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldEndEditingInDOMRange:%s\n",
           GetRangeDescription(range).c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldInsertNode(const WebNode& node,
                                           const WebRange& range,
                                           WebEditingAction action) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldInsertNode:%s "
           "replacingDOMRange:%s givenAction:%s\n",
           GetNodeDescription(node, 0).c_str(),
           GetRangeDescription(range).c_str(),
           GetEditingActionDescription(action).c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldInsertText(const WebString& text,
                                           const WebRange& range,
                                           WebEditingAction action) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldInsertText:%s "
           "replacingDOMRange:%s givenAction:%s\n",
           text.utf8().data(),
           GetRangeDescription(range).c_str(),
           GetEditingActionDescription(action).c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldChangeSelectedRange(const WebRange& from_range,
                                                    const WebRange& to_range,
                                                    WebTextAffinity affinity,
                                                    bool still_selecting) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldChangeSelectedDOMRange:%s "
           "toDOMRange:%s affinity:%s stillSelecting:%s\n",
           GetRangeDescription(from_range).c_str(),
           GetRangeDescription(to_range).c_str(),
           GetTextAffinityDescription(affinity).c_str(),
           (still_selecting ? "TRUE" : "FALSE"));
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldDeleteRange(const WebRange& range) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldDeleteDOMRange:%s\n",
           GetRangeDescription(range).c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldApplyStyle(const WebString& style,
                                           const WebRange& range) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldApplyStyle:%s toElementsInDOMRange:%s\n",
           style.utf8().data(),
           GetRangeDescription(range).c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::isSmartInsertDeleteEnabled() {
  return smart_insert_delete_enabled_;
}

bool TestWebViewDelegate::isSelectTrailingWhitespaceEnabled() {
  return select_trailing_whitespace_enabled_;
}

void TestWebViewDelegate::didBeginEditing() {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: "
           "webViewDidBeginEditing:WebViewDidBeginEditingNotification\n");
  }
}

void TestWebViewDelegate::didChangeSelection(bool is_empty_selection) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: "
    "webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
  }
  UpdateSelectionClipboard(is_empty_selection);
}

void TestWebViewDelegate::didChangeContents() {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: "
           "webViewDidChange:WebViewDidChangeNotification\n");
  }
}

void TestWebViewDelegate::didEndEditing() {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: "
           "webViewDidEndEditing:WebViewDidEndEditingNotification\n");
  }
}

void TestWebViewDelegate::runModalAlertDialog(
    WebFrame* frame, const WebString& message) {
  if (!shell_->layout_test_mode()) {
    ShowJavaScriptAlert(UTF16ToWideHack(message));
  } else {
    printf("ALERT: %s\n", message.utf8().data());
  }
}

bool TestWebViewDelegate::runModalConfirmDialog(
    WebFrame* frame, const WebString& message) {
  if (shell_->layout_test_mode()) {
    // When running tests, write to stdout.
    printf("CONFIRM: %s\n", message.utf8().data());
    return true;
  }
  return false;
}

bool TestWebViewDelegate::runModalPromptDialog(
    WebFrame* frame, const WebString& message, const WebString& default_value,
    WebString* actual_value) {
  if (shell_->layout_test_mode()) {
    // When running tests, write to stdout.
    printf("PROMPT: %s, default text: %s\n",
           message.utf8().data(),
           default_value.utf8().data());
    return true;
  }
  return false;
}

bool TestWebViewDelegate::runModalBeforeUnloadDialog(
    WebFrame* frame, const WebString& message) {
  return true;  // Allow window closure.
}

void TestWebViewDelegate::setStatusText(const WebString& text) {
  if (WebKit::layoutTestMode() &&
      shell_->layout_test_controller()->ShouldDumpStatusCallbacks()) {
    // When running tests, write to stdout.
    printf("UI DELEGATE STATUS CALLBACK: setStatusText:%s\n", text.utf8().data());
  }
}

void TestWebViewDelegate::startDragging(
    const WebPoint& mouse_coords, const WebDragData& data,
    WebDragOperationsMask mask) {
  if (WebKit::layoutTestMode()) {
    WebDragData mutable_drag_data = data;
    if (shell_->layout_test_controller()->ShouldAddFileToPasteboard()) {
      // Add a file called DRTFakeFile to the drag&drop clipboard.
      AddDRTFakeFileToDataObject(&mutable_drag_data);
    }

    // When running a test, we need to fake a drag drop operation otherwise
    // Windows waits for real mouse events to know when the drag is over.
    EventSendingController::DoDragDrop(mouse_coords, mutable_drag_data, mask);
  } else {
    // TODO(tc): Drag and drop is disabled in the test shell because we need
    // to be able to convert from WebDragData to an IDataObject.
    //if (!drag_delegate_)
    //  drag_delegate_ = new TestDragDelegate(shell_->webViewWnd(),
    //                                        shell_->webView());
    //const DWORD ok_effect = DROPEFFECT_COPY | DROPEFFECT_LINK | DROPEFFECT_MOVE;
    //DWORD effect;
    //HRESULT res = DoDragDrop(drop_data.data_object, drag_delegate_.get(),
    //                         ok_effect, &effect);
    //DCHECK(DRAGDROP_S_DROP == res || DRAGDROP_S_CANCEL == res);
  }
  shell_->webView()->DragSourceSystemDragEnded();
}

void TestWebViewDelegate::navigateBackForwardSoon(int offset) {
  shell_->navigation_controller()->GoToOffset(offset);
}

int TestWebViewDelegate::historyBackListCount() {
  int current_index =
      shell_->navigation_controller()->GetLastCommittedEntryIndex();
  return current_index;
}

int TestWebViewDelegate::historyForwardListCount() {
  int current_index =
      shell_->navigation_controller()->GetLastCommittedEntryIndex();
  return shell_->navigation_controller()->GetEntryCount() - current_index - 1;
}

// WebWidgetClient -----------------------------------------------------------

void TestWebViewDelegate::didInvalidateRect(const WebRect& rect) {
  if (WebWidgetHost* host = GetWidgetHost())
    host->DidInvalidateRect(rect);
}

void TestWebViewDelegate::didScrollRect(int dx, int dy,
                                        const WebRect& clip_rect) {
  if (WebWidgetHost* host = GetWidgetHost())
    host->DidScrollRect(dx, dy, clip_rect);
}

void TestWebViewDelegate::didFocus() {
  if (WebWidgetHost* host = GetWidgetHost())
    shell_->SetFocus(host, true);
}

void TestWebViewDelegate::didBlur() {
  if (WebWidgetHost* host = GetWidgetHost())
    shell_->SetFocus(host, false);
}

WebScreenInfo TestWebViewDelegate::screenInfo() {
  if (WebWidgetHost* host = GetWidgetHost())
    return host->GetScreenInfo();

  return WebScreenInfo();
}

// WebFrameClient ------------------------------------------------------------

WebPlugin* TestWebViewDelegate::createPlugin(
    WebFrame* frame, const WebPluginParams& params) {
  return new webkit_glue::WebPluginImpl(frame, params, AsWeakPtr());
}

WebWorker* TestWebViewDelegate::createWorker(
    WebFrame* frame, WebWorkerClient* client) {
#if ENABLE(WORKERS)
  return new TestWebWorker();
#else
  return NULL;
#endif
}

WebMediaPlayer* TestWebViewDelegate::createMediaPlayer(
    WebFrame* frame, WebMediaPlayerClient* client) {
  scoped_refptr<media::FilterFactoryCollection> factory =
      new media::FilterFactoryCollection();

  // TODO(hclam): this is the same piece of code as in RenderView, maybe they
  // should be grouped together.
  webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory =
      new webkit_glue::MediaResourceLoaderBridgeFactory(
          GURL::EmptyGURL(),  // referrer
          "null",             // frame origin
          "null",             // main_frame_origin
          base::GetCurrentProcId(),
          appcache::kNoHostId,
          0);
  factory->AddFactory(webkit_glue::BufferedDataSource::CreateFactory(
      MessageLoop::current(), bridge_factory));
  // TODO(hclam): Use command line switch to determine which data source to use.
  return new webkit_glue::WebMediaPlayerImpl(client, factory);
}

void TestWebViewDelegate::loadURLExternally(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationPolicy policy) {
  DCHECK_NE(policy, WebKit::WebNavigationPolicyCurrentTab);
  TestShell* shell = NULL;
  if (TestShell::CreateNewWindow(request.url(), &shell))
    shell->Show(policy);
}

WebNavigationPolicy TestWebViewDelegate::decidePolicyForNavigation(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationType type, WebNavigationPolicy default_policy,
    bool is_redirect) {
  WebNavigationPolicy result;
  if (policy_delegate_enabled_) {
    printf("Policy delegate: attempt to load %s with navigation type '%s'\n",
           GetURLDescription(request.url()).c_str(),
           WebNavigationTypeToString(type));
    if (policy_delegate_is_permissive_) {
      result = WebKit::WebNavigationPolicyCurrentTab;
    } else {
      result = WebKit::WebNavigationPolicyIgnore;
    }
    if (policy_delegate_should_notify_done_)
      shell_->layout_test_controller()->PolicyDelegateDone();
  } else {
    result = default_policy;
  }
  return result;
}

void TestWebViewDelegate::willPerformClientRedirect(
    WebFrame* frame, const WebURL& from, const WebURL& to,
    double interval, double fire_time) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - willPerformClientRedirectToURL: %s \n",
           GetFrameDescription(frame).c_str(),
           to.spec().data());
  }
}

void TestWebViewDelegate::didCancelClientRedirect(WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didCancelClientRedirectForFrame\n",
           GetFrameDescription(frame).c_str());
  }
}

void TestWebViewDelegate::didCreateDataSource(
    WebFrame* frame, WebDataSource* ds) {
  ds->setExtraData(pending_extra_data_.release());
}

void TestWebViewDelegate::didStartProvisionalLoad(WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didStartProvisionalLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  }

  if (!top_loading_frame_) {
    top_loading_frame_ = frame;
  }

  if (shell_->layout_test_controller()->StopProvisionalFrameLoads()) {
    printf("%S - stopping load in didStartProvisionalLoadForFrame callback\n",
           GetFrameDescription(frame).c_str());
    frame->stopLoading();
  }
  UpdateAddressBar(frame->view());
}

void TestWebViewDelegate::didReceiveServerRedirectForProvisionalLoad(
    WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didReceiveServerRedirectForProvisionalLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  }
  UpdateAddressBar(frame->view());
}

void TestWebViewDelegate::didFailProvisionalLoad(
    WebFrame* frame, const WebURLError& error) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didFailProvisionalLoadWithError\n",
           GetFrameDescription(frame).c_str());
  }

  LocationChangeDone(frame);

  // Don't display an error page if we're running layout tests, because
  // DumpRenderTree doesn't.
  if (shell_->layout_test_mode())
    return;

  // Don't display an error page if this is simply a cancelled load.  Aside
  // from being dumb, WebCore doesn't expect it and it will cause a crash.
  if (error.reason == net::ERR_ABORTED)
    return;

  const WebDataSource* failed_ds = frame->provisionalDataSource();

  TestShellExtraData* extra_data =
      static_cast<TestShellExtraData*>(failed_ds->extraData());
  bool replace = extra_data && extra_data->pending_page_id != -1;

  const std::string& error_text =
      StringPrintf("Error %d when loading url %s", error.reason,
      failed_ds->request().url().spec().data());

  // Make sure we never show errors in view source mode.
  frame->enableViewSourceMode(false);

  frame->loadHTMLString(
      error_text, GURL("testshell-error:"), error.unreachableURL, replace);
}

void TestWebViewDelegate::didCommitProvisionalLoad(
    WebFrame* frame, bool is_new_navigation) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didCommitLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  }
  UpdateForCommittedLoad(frame, is_new_navigation);
}

void TestWebViewDelegate::didClearWindowObject(WebFrame* frame) {
  shell_->BindJSObjectsToWindow(frame);
}

void TestWebViewDelegate::didReceiveTitle(
    WebFrame* frame, const WebString& title) {
  std::wstring wtitle = UTF16ToWideHack(title);

  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didReceiveTitle\n",
           GetFrameDescription(frame).c_str());
  }

  if (shell_->ShouldDumpTitleChanges()) {
    printf("TITLE CHANGED: %S\n", wtitle.c_str());
  }

  SetPageTitle(wtitle);
}

void TestWebViewDelegate::didFinishDocumentLoad(WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didFinishDocumentLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  } else {
    unsigned pending_unload_events = frame->unloadListenerCount();
    if (pending_unload_events) {
      printf("%S - has %u onunload handler(s)\n",
          GetFrameDescription(frame).c_str(), pending_unload_events);
    }
  }
}

void TestWebViewDelegate::didHandleOnloadEvents(WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didHandleOnloadEventsForFrame\n",
           GetFrameDescription(frame).c_str());
  }
}

void TestWebViewDelegate::didFailLoad(
    WebFrame* frame, const WebURLError& error) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didFailLoadWithError\n",
           GetFrameDescription(frame).c_str());
  }
  LocationChangeDone(frame);
}

void TestWebViewDelegate::didFinishLoad(WebFrame* frame) {
  TRACE_EVENT_END("frame.load", this, frame->url().spec());
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didFinishLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  }
  UpdateAddressBar(frame->view());
  LocationChangeDone(frame);
}

void TestWebViewDelegate::didChangeLocationWithinPage(
    WebFrame* frame, bool is_new_navigation) {
  frame->dataSource()->setExtraData(pending_extra_data_.release());

  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didChangeLocationWithinPageForFrame\n",
           GetFrameDescription(frame).c_str());
  }

  UpdateForCommittedLoad(frame, is_new_navigation);
}

void TestWebViewDelegate::assignIdentifierToRequest(
    WebFrame* frame, unsigned identifier, const WebURLRequest& request) {
  if (shell_->ShouldDumpResourceLoadCallbacks())
    resource_identifier_map_[identifier] = request.url().spec();
}

void TestWebViewDelegate::willSendRequest(
    WebFrame* frame, unsigned identifier, WebURLRequest& request,
    const WebURLResponse& redirect_response) {
  GURL url = request.url();
  std::string request_url = url.possibly_invalid_spec();

  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    GURL main_document_url = request.firstPartyForCookies();
    printf("%s - willSendRequest <NSURLRequest URL %s, main document URL %s,"
           " http method %s> redirectResponse %s\n",
           GetResourceDescription(identifier).c_str(),
           request_url.c_str(),
           GetURLDescription(main_document_url).c_str(),
           request.httpMethod().utf8().data(),
           GetResponseDescription(redirect_response).c_str());
  }

  if (!redirect_response.isNull() && block_redirects_) {
    printf("Returning null for this redirect\n");

    // To block the request, we set its URL to an empty one.
    request.setURL(WebURL());
    return;
  }

  std::string host = url.host();
  if (TestShell::layout_test_mode() && !host.empty() &&
      (url.SchemeIs("http") || url.SchemeIs("https")) &&
       host != "127.0.0.1" &&
       host != "255.255.255.255" &&  // Used in some tests that expect to get
                                     // back an error.
       host != "localhost") {
    printf("Blocked access to external URL %s\n", request_url.c_str());

    // To block the request, we set its URL to an empty one.
    request.setURL(WebURL());
    return;
  }

  TRACE_EVENT_BEGIN("url.load", identifier, request_url);
  // Set the new substituted URL.
  request.setURL(GURL(TestShell::RewriteLocalUrl(request_url)));
}

void TestWebViewDelegate::didReceiveResponse(
    WebFrame* frame, unsigned identifier, const WebURLResponse& response) {
  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    printf("%s - didReceiveResponse %s\n",
           GetResourceDescription(identifier).c_str(),
           GetResponseDescription(response).c_str());
  }
}

void TestWebViewDelegate::didFinishResourceLoad(
    WebFrame* frame, unsigned identifier) {
  TRACE_EVENT_END("url.load", identifier, "");
  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    printf("%s - didFinishLoading\n",
           GetResourceDescription(identifier).c_str());
  }
  resource_identifier_map_.erase(identifier);
}

void TestWebViewDelegate::didFailResourceLoad(
    WebFrame* frame, unsigned identifier, const WebURLError& error) {
  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    printf("%s - didFailLoadingWithError: %s\n",
           GetResourceDescription(identifier).c_str(),
           GetErrorDescription(error).c_str());
  }
  resource_identifier_map_.erase(identifier);
}

void TestWebViewDelegate::didDisplayInsecureContent(WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks())
    printf("didDisplayInsecureContent\n");
}

void TestWebViewDelegate::didRunInsecureContent(
    WebFrame* frame, const WebSecurityOrigin& origin) {
  if (shell_->ShouldDumpFrameLoadCallbacks())
    printf("didRunInsecureContent\n");
}

// Public methods ------------------------------------------------------------

TestWebViewDelegate::TestWebViewDelegate(TestShell* shell)
    : policy_delegate_enabled_(false),
      policy_delegate_is_permissive_(false),
      policy_delegate_should_notify_done_(false),
      shell_(shell),
      top_loading_frame_(NULL),
      page_id_(-1),
      last_page_id_updated_(-1),
#if defined(OS_LINUX)
      cursor_type_(GDK_X_CURSOR),
#endif
      smart_insert_delete_enabled_(true),
#if defined(OS_WIN)
      select_trailing_whitespace_enabled_(true),
#else
      select_trailing_whitespace_enabled_(false),
#endif
      block_redirects_(false) {
}

TestWebViewDelegate::~TestWebViewDelegate() {
}

void TestWebViewDelegate::Reset() {
  // Do a little placement new dance...
  TestShell* shell = shell_;
  this->~TestWebViewDelegate();
  new (this) TestWebViewDelegate(shell);
}

void TestWebViewDelegate::SetSmartInsertDeleteEnabled(bool enabled) {
  smart_insert_delete_enabled_ = enabled;
  // In upstream WebKit, smart insert/delete is mutually exclusive with select
  // trailing whitespace, however, we allow both because Chromium on Windows
  // allows both.
}

void TestWebViewDelegate::SetSelectTrailingWhitespaceEnabled(bool enabled) {
  select_trailing_whitespace_enabled_ = enabled;
  // In upstream WebKit, smart insert/delete is mutually exclusive with select
  // trailing whitespace, however, we allow both because Chromium on Windows
  // allows both.
}

void TestWebViewDelegate::RegisterDragDrop() {
#if defined(OS_WIN)
  // TODO(port): add me once drag and drop works.
  DCHECK(!drop_delegate_);
  drop_delegate_ = new TestDropDelegate(shell_->webViewWnd(),
                                        shell_->webView());
#endif
}

void TestWebViewDelegate::RevokeDragDrop() {
#if defined(OS_WIN)
  ::RevokeDragDrop(shell_->webViewWnd());
#endif
}

void TestWebViewDelegate::SetCustomPolicyDelegate(bool is_custom,
                                                  bool is_permissive) {
  policy_delegate_enabled_ = is_custom;
  policy_delegate_is_permissive_ = is_permissive;
}

void TestWebViewDelegate::WaitForPolicyDelegate() {
  policy_delegate_enabled_ = true;
  policy_delegate_should_notify_done_ = true;
}

// Private methods -----------------------------------------------------------

void TestWebViewDelegate::UpdateAddressBar(WebView* webView) {
  WebFrame* mainFrame = webView->GetMainFrame();

  WebDataSource* dataSource = mainFrame->dataSource();
  if (!dataSource)
    dataSource = mainFrame->provisionalDataSource();
  if (!dataSource)
    return;

  // TODO(abarth): This is wrong!
  SetAddressBarURL(dataSource->request().firstPartyForCookies());
}

void TestWebViewDelegate::LocationChangeDone(WebFrame* frame) {
  if (frame == top_loading_frame_) {
    top_loading_frame_ = NULL;

    if (shell_->layout_test_mode())
      shell_->layout_test_controller()->LocationChangeDone();
  }
}

WebWidgetHost* TestWebViewDelegate::GetWidgetHost() {
  if (this == shell_->delegate())
    return shell_->webViewHost();
  if (this == shell_->popup_delegate())
    return shell_->popupHost();
  return NULL;
}

void TestWebViewDelegate::UpdateForCommittedLoad(WebFrame* frame,
                                                 bool is_new_navigation) {
  // Code duplicated from RenderView::DidCommitLoadForFrame.
  TestShellExtraData* extra_data = static_cast<TestShellExtraData*>(
      frame->dataSource()->extraData());

  if (is_new_navigation) {
    // New navigation.
    UpdateSessionHistory(frame);
    page_id_ = next_page_id_++;
  } else if (extra_data && extra_data->pending_page_id != -1 &&
             !extra_data->request_committed) {
    // This is a successful session history navigation!
    UpdateSessionHistory(frame);
    page_id_ = extra_data->pending_page_id;
  }

  // Don't update session history multiple times.
  if (extra_data)
    extra_data->request_committed = true;

  UpdateURL(frame);
}

void TestWebViewDelegate::UpdateURL(WebFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  DCHECK(ds);

  const WebURLRequest& request = ds->request();

  // Type is unused.
  scoped_ptr<TestNavigationEntry> entry(new TestNavigationEntry);

  // Bug 654101: the referrer will be empty on https->http transitions. It
  // would be nice if we could get the real referrer from somewhere.
  entry->SetPageID(page_id_);
  if (ds->hasUnreachableURL()) {
    entry->SetURL(ds->unreachableURL());
  } else {
    entry->SetURL(request.url());
  }

  const WebHistoryItem& history_item = frame->currentHistoryItem();
  if (!history_item.isNull())
    entry->SetContentState(webkit_glue::HistoryItemToString(history_item));

  shell_->navigation_controller()->DidNavigateToEntry(entry.release());

  last_page_id_updated_ = std::max(last_page_id_updated_, page_id_);
}

void TestWebViewDelegate::UpdateSessionHistory(WebFrame* frame) {
  // If we have a valid page ID at this point, then it corresponds to the page
  // we are navigating away from.  Otherwise, this is the first navigation, so
  // there is no past session history to record.
  if (page_id_ == -1)
    return;

  TestNavigationEntry* entry = static_cast<TestNavigationEntry*>(
      shell_->navigation_controller()->GetEntryWithPageID(page_id_));
  if (!entry)
    return;

  const WebHistoryItem& history_item =
      shell_->webView()->GetMainFrame()->previousHistoryItem();
  if (history_item.isNull())
    return;

  entry->SetContentState(webkit_glue::HistoryItemToString(history_item));
}

std::wstring TestWebViewDelegate::GetFrameDescription(WebFrame* webframe) {
  std::wstring name = UTF16ToWideHack(webframe->name());

  if (webframe == shell_->webView()->GetMainFrame()) {
    if (name.length())
      return L"main frame \"" + name + L"\"";
    else
      return L"main frame";
  } else {
    if (name.length())
      return L"frame \"" + name + L"\"";
    else
      return L"frame (anonymous)";
  }
}
