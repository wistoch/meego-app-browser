// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_RENDER_VIEW_HOST_H__
#define CHROME_BROWSER_RENDER_VIEW_HOST_H__

#include <string>
#include <vector>

#include "base/scoped_handle.h"
#include "chrome/browser/render_widget_host.h"
#include "webkit/glue/password_form_dom_manager.h"

enum ConsoleMessageLevel;
class NavigationEntry;
class RenderViewHostDelegate;
class SiteInstance;
class SkBitmap;
struct ViewHostMsg_ContextMenu_Params;
struct ViewHostMsg_DidPrintPage_Params;
class ViewMsg_Navigate;
struct ViewMsg_Navigate_Params;
struct ViewMsg_Print_Params;
struct ViewMsg_PrintPages_Params;
struct WebDropData;
struct WebPreferences;
enum WindowOpenDisposition;

namespace gfx {
class Point;
}

namespace net {
enum LoadState;
}

namespace text_zoom {
enum TextSize;
}

namespace webkit_glue {
struct WebApplicationInfo;
}

//
// RenderViewHost
//
//  A RenderViewHost is responsible for creating and talking to a RenderView
//  object in a child process. It exposes a high level API to users, for things
//  like loading pages, adjusting the display and other browser functionality,
//  which it translates into IPC messages sent over the IPC channel with the
//  RenderView. It responds to all IPC messages sent by that RenderView and
//  cracks them, calling a delegate object back with higher level types where
//  possible.
//
//  The intent of this class is to provide a view-agnostic communication
//  conduit with a renderer. This is so we can build HTML views not only as
//  TabContents (see WebContents for an example) but also as ChromeViews, etc.
//
//  The exact API of this object needs to be more thoroughly designed. Right
//  now it mimics what WebContents exposed, which is a fairly large API and may
//  contain things that are not relevant to a common subset of views. See also
//  the comment in render_view_host_delegate.h about the size and scope of the
//  delegate API.
//
//  Right now, the concept of page navigation (both top level and frame) exists
//  in the WebContents still, so if you instantiate one of these elsewhere, you
//  will not be able to traverse pages back and forward. We need to determine
//  if we want to bring that and other functionality down into this object so
//  it can be shared by others.
//
class RenderViewHost : public RenderWidgetHost {
 public:
  // Returns the RenderViewHost given its ID and the ID of its render process.
  // Returns NULL if the IDs do not correspond to a live RenderViewHost.
  static RenderViewHost* FromID(int render_process_id, int render_view_id);

  // routing_id could be a valid route id, or it could be MSG_ROUTING_NONE, in
  // which case RenderWidgetHost will create a new one.  modal_dialog_event is
  // the event that's set when showing a modal dialog so that the renderer and
  // plugin processes know to pump messages.  An existing event can be passed
  // in, otherwise if it's NULL a new event will be created.
  explicit RenderViewHost(SiteInstance* instance,
                          RenderViewHostDelegate* delegate,
                          int routing_id,
                          HANDLE modal_dialog_event);
  virtual ~RenderViewHost();

  SiteInstance* site_instance() const { return instance_; }
  RenderViewHostDelegate* delegate() const { return delegate_; }

  // Set up the RenderView child process.
  virtual bool CreateRenderView();
  // Returns true if the RenderView is active and has not crashed.
  virtual bool IsRenderViewLive() const;
  // Create a new RenderViewHost but recycle an existing RenderView child
  // process.
  virtual void Init();

  // Load the specified entry, optionally reloading.
  virtual void NavigateToEntry(const NavigationEntry& entry, bool is_reload);

  // Load the specified URL.
  void NavigateToURL(const GURL& url);

  // Loads the specified html (must be UTF8) in the main frame.  If
  // |new_navigation| is true, it simulates a navigation to |display_url|.
  // |security_info| is the security state that will be reported when the page
  // load commits.  It is useful for mocking SSL errors.  Provide an empty
  // string if no secure connection state should be simulated.
  // Note that if |new_navigation| is false, |display_url| and |security_info|
  // are not used.
  virtual void LoadAlternateHTMLString(const std::string& html_text,
                                       bool new_navigation,
                                       const GURL& display_url,
                                       const std::string& security_info);

  // Suspends (or unsuspends) any navigation messages from being sent from this
  // RenderViewHost.  This is called when a pending RenderViewHost is created
  // for a cross-site navigation, because we must suspend any navigations until
  // we hear back from the old renderer's onbeforeunload handler.  Note that it
  // is important that only one navigation event happen after calling this
  // method with |suspend| equal to true.  If |suspend| is false and there is
  // a suspended_nav_message_, this will send the message.
  void SetNavigationsSuspended(bool suspend);

  // Causes the renderer to invoke the onbeforeunload event handler.  The
  // result will be returned via ViewMsg_ShouldClose.
  virtual void FirePageBeforeUnload();

  // Close the page after the page has responded that it can be closed via
  // ViewMsg_ShouldClose. This is where the page itself is closed. The
  // unload handler is triggered here, which can block with a dialog, but cannot
  // cancel the close of the page.
  virtual void FirePageUnload();

  // Close the page ignoring whether it has unload events registers.
  // This is called after the beforeunload and unload events have fired
  // and the user has agreed to continue with closing the page.
  static void ClosePageIgnoringUnloadEvents(int render_process_host_id,
                                            int request_id);

  // Causes the renderer to close the current page, including running its
  // onunload event handler.  A ClosePage_ACK message will be sent to the
  // ResourceDispatcherHost when it is finished. |new_render_process_host_id|
  // and |new_request_id| will help the ResourceDispatcherHost identify which
  // response is associated with this event.
  virtual void ClosePage(int new_render_process_host_id,
                         int new_request_id);

  // Sets whether this RenderViewHost has an outstanding cross-site request,
  // for which another renderer will need to run an onunload event handler.
  // This is called before the first navigation event for this RenderViewHost,
  // and again after the corresponding OnCrossSiteResponse.
  void SetHasPendingCrossSiteRequest(bool has_pending_request);

  // Called by ResourceDispatcherHost when a response for a pending cross-site
  // request is received.  The ResourceDispatcherHost will pause the response
  // until the onunload handler of the previous renderer is run.
  void OnCrossSiteResponse(int new_render_process_host_id, int new_request_id);

  // Stops the current load.
  void Stop();


  // Retrieves the number of printed pages that would result for the current web
  // page and the specified settings. The response is a
  // ViewHostMsg_DidGetPrintedPagesCount.
  bool GetPrintedPagesCount(const ViewMsg_Print_Params& params);

  // Asks the renderer to "render" printed pages.
  bool PrintPages(const ViewMsg_PrintPages_Params& params);

  // Start looking for a string within the content of the page, with the
  // specified options.
  void StartFinding(int request_id,
                    const std::wstring& search_string,
                    bool forward,
                    bool match_case,
                    bool find_next);

  // Cancel a pending find operation. If |clear_selection| is true, it will also
  // clear the selection on the focused frame.
  void StopFinding(bool clear_selection);

  // Sends a notification to the renderer that we are ready to receive more
  // results from the scoping effort of the Find operation. The FindInPage
  // scoping is asynchronous and periodically sends results back up to the
  // browser using IPC. In an effort to not spam the browser we have the
  // browser send an ACK for each FindReply message and have the renderer
  // queue up the latest status message while waiting for this ACK.
  void SendFindReplyAck();

  // Change the text size of the page.
  void AlterTextSize(text_zoom::TextSize size);

  // Change the encoding of the page.
  void SetPageEncoding(const std::wstring& encoding_name);

  // Change the alternate error page URL.  An empty GURL disables the use of
  // alternate error pages.
  void SetAlternateErrorPageURL(const GURL& url);

  // Fill out a form within the page with the specified data.
  void FillForm(const FormData& form_data);

  // Fill out a password form and trigger DOM autocomplete in the case
  // of multiple matching logins.
  void FillPasswordForm(const PasswordFormDomManager::FillData& form_data);

  // D&d drop target messages that get sent to WebKit.
  void DragTargetDragEnter(const WebDropData& drop_data,
                           const gfx::Point& client_pt,
                           const gfx::Point& screen_pt);
  void DragTargetDragOver(const gfx::Point& client_pt,
                          const gfx::Point& screen_pt);
  void DragTargetDragLeave();
  void DragTargetDrop(const gfx::Point& client_pt,
                      const gfx::Point& screen_pt);

  // Uploads a file by automatically completing a form within the page and
  // submitting it.
  void UploadFile(const std::wstring& file_path,
                  const std::wstring& form,
                  const std::wstring& file,
                  const std::wstring& submit,
                  const std::wstring& other_values);

  // Tell the RenderView to reserve a range of page ids of the given size.
  void ReservePageIDRange(int size);

  // Runs some javascript within the context of a frame in the page.
  void ExecuteJavascriptInWebFrame(const std::wstring& frame_xpath,
                                   const std::wstring& jscript);

  // Logs a message to the console of a frame in the page.
  void AddMessageToConsole(const std::wstring& frame_xpath,
                           const std::wstring& msg,
                           ConsoleMessageLevel level);

  // Send command to the debugger
  void SendToDebugger(const std::wstring& cmd);

  // Attach to the V8 instance for debugging
  void DebugAttach();

  // Detach from the V8 instance for debugging
  void DebugDetach();

  // Cause the V8 debugger to trigger a breakpoint
  // (even if no JS code is running)
  void DebugBreak();

  // Edit operations.
  void Undo();
  void Redo();
  void Cut();
  void Copy();
  void Paste();
  void Replace(const std::wstring& text);
  void Delete();
  void SelectAll();

  // Downloads an image notifying the delegate appropriately. The returned
  // integer uniquely identifies the download for the lifetime of the browser.
  int DownloadImage(const GURL& url, int image_size);

  // Requests application info for the specified page. This is an asynchronous
  // request. The delegate is notified by way of OnDidGetApplicationInfo when
  // the data is available.
  void GetApplicationInfo(int32 page_id);

  // Captures a thumbnail representation of the page.
  void CaptureThumbnail();

  // Notifies the RenderView that the JavaScript message that was shown was
  // closed by the user.
  void JavaScriptMessageBoxClosed(IPC::Message* reply_msg,
                                  bool success,
                                  const std::wstring& prompt);

  // Notifies the RenderView that the modal html dialog has been closed.
  void ModalHTMLDialogClosed(IPC::Message* reply_msg,
                             const std::string& json_retval);

  // Copies the image at the specified point.
  void CopyImageAt(int x, int y);

  // Inspects the element at the specified point using the Web Inspector.
  void InspectElementAt(int x, int y);

  // Show the JavaScript console.
  void ShowJavaScriptConsole();

  // Notifies the renderer that a drop occurred. This is necessary because the
  // render may be the one that started the drag.
  void DragSourceEndedAt(
      int client_x, int client_y, int screen_x, int screen_y);

  // Notifies the renderer that a drag and drop operation is in progress, with
  // droppable items positioned over the renderer's view.
  void DragSourceMovedTo(
      int client_x, int client_y, int screen_x, int screen_y);

  // Notifies the renderer that we're done with the drag and drop operation.
  // This allows the renderer to reset some state.
  void DragSourceSystemDragEnded();

  // Tell the render view to expose DOM automation bindings so that the js
  // content can send JSON-encoded data back to automation in the parent
  // process.
  void AllowDomAutomationBindings();

  // Tell the render view to expose DOM bindings so that the JS content
  // can send JSON-encoded data back to the browser process.
  // This is used for HTML-based UI.
  // Must be called before CreateRenderView().
  void AllowDOMUIBindings();

  // Sets a property with the given name and value on the DOM UI binding object.
  // Must call AllowDOMUIBindings() on this renderer first.
  void SetDOMUIProperty(const std::string& name, const std::string& value);

  // Fill in a ViewMsg_Navigate_Params struct from a NavigationEntry.
  static void MakeNavigateParams(const NavigationEntry& entry,
                                 bool reload,
                                 ViewMsg_Navigate_Params* params);

  // Overridden from RenderWidgetHost: We are hosting a web page.
  virtual bool IsRenderView() { return true; }
  virtual bool CanBlur() const;

  // IPC::Channel::Listener
  virtual void OnMessageReceived(const IPC::Message& msg);

  // Override the RenderWidgetHost's Shutdown method.
  virtual void Shutdown();

  // Tells the renderer view to focus the first (last if reverse is true) node.
  void SetInitialFocus(bool reverse);

  // Update render view specific (WebKit) preferences.
  void UpdateWebPreferences(const WebPreferences& prefs);

  // Request the Renderer to ask the default plugin to start installation of
  // missing plugin.
  void InstallMissingPlugin();

  // Get all savable resource links from current webpage, include main
  // frame and sub-frame.
  void GetAllSavableResourceLinksForCurrentPage(const GURL& page_url);

  // Get html data by serializing all frames of current page with lists
  // which contain all resource links that have local copy.
  // The parameter links contain original URLs of all saved links.
  // The parameter local_paths contain corresponding local file paths of
  // all saved links, which matched with vector:links one by one.
  // The parameter local_directory_name is relative path of directory which
  // contain all saved auxiliary files included all sub frames and resouces.
  void GetSerializedHtmlDataForCurrentPageWithLocalLinks(
      const std::vector<std::wstring>& links,
      const std::vector<std::wstring>& local_paths,
      const std::wstring& local_directory_name);

  // Notifies the RenderViewHost that a file has been chosen by the user from
  // an Open File dialog for the form.
  void FileSelected(const std::wstring& path);

  // Notifies the RenderViewHost that its load state changed.
  void LoadStateChanged(const GURL& url, net::LoadState load_state);

  // Does the associated view have an onunload or onbeforeunload handler?
  bool HasUnloadListener() { return has_unload_listener_; }

  // Clears the has_unload_listener_ bit since the unload handler has fired
  // and we're necessarily leaving the page.
  void UnloadListenerHasFired() { has_unload_listener_ = false; }

  // Invoked on ui theme changes.
  void OnThemeChanged();

 protected:
  // Overridden from RenderWidgetHost:
  virtual void UnhandledInputEvent(const WebInputEvent& event);

  // IPC message handlers:
  void OnMsgCreateView(int route_id, HANDLE modal_dialog_event);
  void OnMsgCreateWidget(int route_id);
  void OnMsgShowView(int route_id,
                     WindowOpenDisposition disposition,
                     const gfx::Rect& initial_pos,
                     bool user_gesture);
  void OnMsgShowWidget(int route_id, const gfx::Rect& initial_pos);
  void OnMsgRunModal(IPC::Message* reply_msg);
  void OnMsgRendererReady();
  void OnMsgRendererGone();
  void OnMsgNavigate(const IPC::Message& msg);
  void OnMsgUpdateState(int32 page_id,
                        const GURL& url,
                        const std::wstring& title,
                        const std::string& state);
  void OnMsgUpdateTitle(int32 page_id, const std::wstring& title);
  void OnMsgUpdateEncoding(const std::wstring& encoding_name);
  void OnMsgUpdateTargetURL(int32 page_id, const GURL& url);
  void OnMsgThumbnail(const IPC::Message& msg);
  void OnMsgClose();
  void OnMsgRequestMove(const gfx::Rect& pos);
  void OnMsgDidRedirectProvisionalLoad(int32 page_id,
                                       const GURL& source_url,
                                       const GURL& target_url);
  void OnMsgDidStartLoading(int32 page_id);
  void OnMsgDidStopLoading(int32 page_id);
  void OnMsgDidLoadResourceFromMemoryCache(const GURL& url,
                                           const std::string& security_info);
  void OnMsgDidStartProvisionalLoadForFrame(bool main_frame,
                                            const GURL& url);
  void OnMsgDidFailProvisionalLoadWithError(bool main_frame,
                                            int error_code,
                                            const GURL& url,
                                            bool showing_repost_interstitial);
  void OnMsgFindReply(int request_id,
                      int number_of_matches,
                      const gfx::Rect& selection_rect,
                      int active_match_ordinal,
                      bool final_update);
  void OnMsgUpdateFavIconURL(int32 page_id, const GURL& icon_url);
  void OnMsgDidDownloadImage(int id,
                             const GURL& image_url,
                             bool errored,
                             const SkBitmap& image_data);
  void OnMsgContextMenu(const ViewHostMsg_ContextMenu_Params& params);
  void OnMsgOpenURL(const GURL& url, WindowOpenDisposition disposition);
  void OnMsgDomOperationResponse(const std::string& json_string,
                                 int automation_id);
  void OnMsgDOMUISend(const std::string& message,
                      const std::string& content);
  void OnMsgGoToEntryAtOffset(int offset);
  void OnMsgSetTooltipText(const std::wstring& tooltip_text);
  void OnMsgRunFileChooser(const std::wstring& default_file);
  void OnMsgRunJavaScriptMessage(const std::wstring& message,
                                 const std::wstring& default_prompt,
                                 const int flags,
                                 IPC::Message* reply_msg);
  void OnMsgRunBeforeUnloadConfirm(const std::wstring& message,
                                   IPC::Message* reply_msg);
  void OnMsgShowModalHTMLDialog(const GURL& url, int width, int height,
                                const std::string& json_arguments,
                                IPC::Message* reply_msg);
  void OnMsgPasswordFormsSeen(const std::vector<PasswordForm>& forms);
  void OnMsgStartDragging(const WebDropData& drop_data);
  void OnUpdateDragCursor(bool is_drop_target);
  void OnTakeFocus(bool reverse);
  void OnMsgPageHasOSDD(int32 page_id, const GURL& doc_url, bool autodetected);
  void OnMsgInspectElementReply(int num_resources);
  void DidPrintPage(const ViewHostMsg_DidPrintPage_Params& params);
  void OnDebugMessage(const std::string& message);
  void OnAddMessageToConsole(const std::wstring& message,
                             int32 line_no,
                             const std::wstring& source_id);
  void OnDebuggerOutput(const std::wstring& output);
  void DidDebugAttach();
  void OnUserMetricsRecordAction(const std::wstring& action);
  void OnMissingPluginStatus(int status);
  void OnMessageReceived(IPC::Message* msg) { }

  void OnReceivedSavableResourceLinksForCurrentPage(
      const std::vector<GURL>& resources_list,
      const std::vector<GURL>& referrers_list,
      const std::vector<GURL>& frames_list);

  void OnReceivedSerializedHtmlData(const GURL& frame_url,
                                    const std::string& data,
                                    int32 status);

  void OnDidGetApplicationInfo(int32 page_id,
                               const webkit_glue::WebApplicationInfo& info);
  void OnMsgShouldCloseACK(bool proceed);
  void OnUnloadListenerChanged(bool has_handler);

  virtual void NotifyRendererUnresponsive();
  virtual void NotifyRendererResponsive();

  // Helper function to send a navigation message.  If a cross-site request is
  // in progress, we may be suspended while waiting for the onbeforeunload
  // handler, so this function might buffer the message rather than sending it.
  void DoNavigate(ViewMsg_Navigate* nav_message);

 private:
  friend class TestRenderViewHost;

  void UpdateBackForwardListCount();

  void OnDebugDisconnect();

  // The SiteInstance associated with this RenderViewHost.  All pages drawn
  // in this RenderViewHost are part of this SiteInstance.  Should not change
  // over time.
  scoped_refptr<SiteInstance> instance_;

  // Our delegate, which wants to know about changes in the RenderView.
  RenderViewHostDelegate* delegate_;

  // true if a renderer has once been valid. We use this flag to display a sad
  // tab only when we lose our renderer and not if a paint occurs during
  // initialization.
  bool renderer_initialized_;

  // true if we are currently waiting for a response for drag context
  // information.
  bool waiting_for_drag_context_response_;

  // is the debugger attached to us or not
  bool debugger_attached_;

  // True if we've been told to set up the the Javascript bindings for
  // sending messages back to the browser.
  bool enable_dom_ui_bindings_;

  // Handle to an event that's set when the page is showing a modal dialog box
  // (or equivalent constrained window).  The renderer and plugin processes
  // check this to know if they should pump messages/tasks then.
  ScopedHandle modal_dialog_event_;

  // Multiple dialog boxes can be shown before the first one is finished,
  // so we keep a counter to know when we can reset the modal dialog event.
  int modal_dialog_count_;

  // Whether we should buffer outgoing Navigate messages rather than sending
  // them.  This will be true when a RenderViewHost is created for a cross-site
  // request, until we hear back from the onbeforeunload handler of the old
  // RenderViewHost.
  bool navigations_suspended_;

  // We only buffer a suspended navigation message while we a pending RVH for a
  // WebContents.  There will only ever be one suspended navigation, because
  // WebContents will destroy the pending RVH and create a new one if a second
  // navigation occurs.
  scoped_ptr<ViewMsg_Navigate> suspended_nav_message_;

  // If we were asked to RunModal, then this will hold the reply_msg that we
  // must return to the renderer to unblock it.
  IPC::Message* run_modal_reply_msg_;

  bool has_unload_listener_;

  bool is_waiting_for_unload_ack_;

  DISALLOW_EVIL_CONSTRUCTORS(RenderViewHost);
};

// Factory for creating RenderViewHosts.  Useful for unit tests.
class RenderViewHostFactory {
 public:
  virtual ~RenderViewHostFactory() {}

  virtual RenderViewHost* CreateRenderViewHost(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      int routing_id,
      HANDLE modal_dialog_event) = 0;
};

#endif  // CHROME_BROWSER_RENDER_VIEW_HOST_H__
