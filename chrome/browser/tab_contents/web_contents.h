// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_H_

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/tab_contents/tab_contents.h"

class AutofillForm;
class DOMUI;
class InterstitialPageDelegate;
class LoadNotificationDetails;
class RenderProcessHost;
class RenderViewHost;
class RenderWidgetHost;
struct ThumbnailScore;
struct ViewHostMsg_FrameNavigate_Params;
struct ViewHostMsg_DidPrintPage_Params;

namespace base {
class WaitableEvent;
}

namespace webkit_glue {
struct WebApplicationInfo;
}

namespace IPC {
class Message;
}

// WebContents represents the contents of a tab that shows web pages. It embeds
// a RenderViewHost (via RenderViewHostManager) to actually display the page.
class WebContents : public TabContents,
                    public RenderViewHostDelegate,
                    public RenderViewHostManager::Delegate,
                    public SelectFileDialog::Listener {
 public:
  // If instance is NULL, then creates a new process for this view.  Otherwise
  // initialize with a process already created for a different WebContents.
  // This will share the process between views in the same instance.
  WebContents(Profile* profile,
              SiteInstance* instance,
              int routing_id,
              base::WaitableEvent* modal_dialog_event);

  virtual ~WebContents();

  static void RegisterUserPrefs(PrefService* prefs);

  // Getters -------------------------------------------------------------------

  // Returns the AutofillManager, creating it if necessary.
  AutofillManager* GetAutofillManager();

  // Returns the PasswordManager, creating it if necessary.
  PasswordManager* GetPasswordManager();

  // Returns the PluginInstaller, creating it if necessary.
  PluginInstaller* GetPluginInstaller();

  // Returns the SavePackage which manages the page saving job. May be NULL.
  SavePackage* save_package() const { return save_package_.get(); }

  // Return the currently active RenderProcessHost and RenderViewHost. Each of
  // these may change over time.
  RenderProcessHost* process() const {
    return render_manager_.current_host()->process();
  }
  RenderViewHost* render_view_host() const {
    return render_manager_.current_host();
  }

  // The TabContentsView will never change and is guaranteed non-NULL.
  TabContentsView* view() const {
    return view_.get();
  }

#ifdef UNIT_TEST
  // Expose the render manager for testing.
  RenderViewHostManager* render_manager() { return &render_manager_; }
#endif

  // Page state getters & setters ----------------------------------------------

  bool is_starred() const { return is_starred_; }

  const std::wstring& encoding() const { return encoding_; }
  void set_encoding(const std::wstring& encoding) {
    encoding_ = encoding;
  }

  // Window stuff --------------------------------------------------------------

  // Returns true if the location bar should be focused by default rather than
  // the page contents. The view will call this function when the tab is
  // to see what it should do.
  bool FocusLocationBarByDefault();

  // TabContents (public overrides) --------------------------------------------

  virtual WebContents* AsWebContents() { return this; }
  const string16& GetTitle() const;
  virtual SiteInstance* GetSiteInstance() const;
  virtual bool ShouldDisplayURL();
  virtual bool ShouldDisplayFavIcon();
  virtual std::wstring GetStatusText() const;
  virtual bool NavigateToPendingEntry(bool reload);
  virtual void Stop();
  virtual void Cut();
  virtual void Copy();
  virtual void Paste();
  virtual void DisassociateFromPopupCount();
  virtual TabContents* Clone();
  virtual void DidBecomeSelected();
  virtual void WasHidden();
  virtual void ShowContents();
  virtual void HideContents();
  virtual bool IsBookmarkBarAlwaysVisible();
  virtual void SetDownloadShelfVisible(bool visible);
  virtual void PopupNotificationVisibilityChanged(bool visible);
  virtual void Focus();

  // Retarded pass-throughs to the view.
  // TODO(brettw) fix this, tab contents shouldn't have these methods, probably
  // it should be killed altogether.
  virtual gfx::NativeView GetNativeView() const;
  virtual gfx::NativeView GetContentNativeView();
  virtual void GetContainerBounds(gfx::Rect *out) const;

  // Web apps ------------------------------------------------------------------

  // Tell Gears to create a shortcut for the current page.
  void CreateShortcut();

  // Interstitials -------------------------------------------------------------

  // Various other systems need to know about our interstitials.
  bool showing_interstitial_page() const {
    return render_manager_.interstitial_page() != NULL;
  }

  // Sets the passed passed interstitial as the currently showing interstitial.
  // |interstitial_page| should be non NULL (use the remove_interstitial_page
  // method to unset the interstitial) and no interstitial page should be set
  // when there is already a non NULL interstitial page set.
  void set_interstitial_page(InterstitialPage* interstitial_page) {
    render_manager_.set_interstitial_page(interstitial_page);
  }

  // Unsets the currently showing interstitial.
  void remove_interstitial_page() {
    render_manager_.remove_interstitial_page();
  }

  // Returns the currently showing interstitial, NULL if no interstitial is
  // showing.
  InterstitialPage* interstitial_page() const {
    return render_manager_.interstitial_page();
  }

  // Find in Page --------------------------------------------------------------

  // Starts the Find operation by calling StartFinding on the Tab. This function
  // can be called from the outside as a result of hot-keys, so it uses the
  // last remembered search string as specified with set_find_string(). This
  // function does not block while a search is in progress. The controller will
  // receive the results through the notification mechanism. See Observe(...)
  // for details.
  void StartFinding(const string16& find_text, bool forward_direction);

  // Stops the current Find operation. If |clear_selection| is true, it will
  // also clear the selection on the focused frame.
  void StopFinding(bool clear_selection);

  // Accessors/Setters for find_ui_active_.
  bool find_ui_active() const { return find_ui_active_; }
  void set_find_ui_active(bool find_ui_active) {
      find_ui_active_ = find_ui_active;
  }

  // Setter for find_op_aborted_.
  void set_find_op_aborted(bool find_op_aborted) {
    find_op_aborted_ = find_op_aborted;
  }

  // Used _only_ by testing to set the current request ID, since it calls
  // StartFinding on the RenderViewHost directly, rather than by using
  // StartFinding's more limited API.
  void set_current_find_request_id(int current_find_request_id) {
    current_find_request_id_ = current_find_request_id;
  }

  // Accessor for find_text_. Used to determine if this WebContents has any
  // active searches.
  string16 find_text() const { return find_text_; }

  // Accessor for find_prepopulate_text_. Used to access the last search
  // string entered, whatever tab that search was performed in.
  string16 find_prepopulate_text() const { return *find_prepopulate_text_; }

  // Accessor for find_result_.
  const FindNotificationDetails& find_result() const { return find_result_; }

  // Misc state & callbacks ----------------------------------------------------

  // Set whether the contents should block javascript message boxes or not.
  // Default is not to block any message boxes.
  void set_suppress_javascript_messages(bool suppress_javascript_messages) {
    suppress_javascript_messages_ = suppress_javascript_messages;
  }

  // AppModalDialog calls this when the dialog is closed.
  void OnJavaScriptMessageBoxClosed(IPC::Message* reply_msg,
                                    bool success,
                                    const std::wstring& prompt);

  // Prepare for saving the current web page to disk.
  void OnSavePage();

  // Save page with the main HTML file path, the directory for saving resources,
  // and the save type: HTML only or complete web page.
  void SavePage(const std::wstring& main_file,
                const std::wstring& dir_path,
                SavePackage::SavePackageType save_type);

  // Displays asynchronously a print preview (generated by the renderer) if not
  // already displayed and ask the user for its preferred print settings with
  // the "Print..." dialog box. (managed by the print worker thread).
  // TODO(maruel):  Creates a snapshot of the renderer to be used for the new
  // tab for the printing facility.
  void PrintPreview();

  // Prints the current document immediately. Since the rendering is
  // asynchronous, the actual printing will not be completed on the return of
  // this function. Returns false if printing is impossible at the moment.
  bool PrintNow();

  // Returns true if the active NavigationEntry's page_id equals page_id.
  bool IsActiveEntry(int32 page_id);

  const std::string& contents_mime_type() const {
    return contents_mime_type_;
  }

  // Returns true if this WebContents will notify about disconnection.
  bool notify_disconnection() const { return notify_disconnection_; }

  // Override the encoding and reload the page by sending down
  // ViewMsg_SetPageEncoding to the renderer. |UpdateEncoding| is kinda
  // the opposite of this, by which 'browser' is notified of
  // the encoding of the current tab from 'renderer' (determined by
  // auto-detect, http header, meta, bom detection, etc).
  void override_encoding(const std::wstring& encoding) {
    set_encoding(encoding);
    render_view_host()->SetPageEncoding(encoding);
  }

  void CrossSiteNavigationCanceled() {
    render_manager_.CrossSiteNavigationCanceled();
  }

  void WindowMoveOrResizeStarted() {
    render_view_host()->WindowMoveOrResizeStarted();
  }

 protected:
  RenderWidgetHostView* render_widget_host_view() const {
    return render_manager_.current_view();
  }

  // TabContents (private overrides) -------------------------------------------

  virtual void SetInitialFocus(bool reverse);
  virtual void SetIsLoading(bool is_loading, LoadNotificationDetails* details);

  // RenderViewHostDelegate ----------------------------------------------------

  virtual RenderViewHostDelegate::View* GetViewDelegate() const;
  virtual RenderViewHostDelegate::Save* GetSaveDelegate() const;
  virtual Profile* GetProfile() const;
  virtual WebContents* GetAsWebContents() { return this; }
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void RenderViewReady(RenderViewHost* render_view_host);
  virtual void RenderViewGone(RenderViewHost* render_view_host);
  virtual void DidNavigate(RenderViewHost* render_view_host,
                           const ViewHostMsg_FrameNavigate_Params& params);
  virtual void UpdateState(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::string& state);
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::wstring& title);
  virtual void UpdateFeedList(RenderViewHost* render_view_host,
                              const ViewHostMsg_UpdateFeedList_Params& params);
  virtual void UpdateEncoding(RenderViewHost* render_view_host,
                              const std::wstring& encoding);
  virtual void UpdateTargetURL(int32 page_id, const GURL& url);
  virtual void UpdateThumbnail(const GURL& url,
                               const SkBitmap& bitmap,
                               const ThumbnailScore& score);
  virtual void Close(RenderViewHost* render_view_host);
  virtual void RequestMove(const gfx::Rect& new_bounds);
  virtual void DidStartLoading(RenderViewHost* render_view_host, int32 page_id);
  virtual void DidStopLoading(RenderViewHost* render_view_host, int32 page_id);
  virtual void DidStartProvisionalLoadForFrame(RenderViewHost* render_view_host,
                                               bool is_main_frame,
                                               const GURL& url);
  virtual void DidRedirectProvisionalLoad(int32 page_id,
                                          const GURL& source_url,
                                          const GURL& target_url);
  virtual void DidLoadResourceFromMemoryCache(
      const GURL& url,
      const std::string& frame_origin,
      const std::string& main_frame_origin,
      const std::string& security_info);
  virtual void DidFailProvisionalLoadWithError(
      RenderViewHost* render_view_host,
      bool is_main_frame,
      int error_code,
      const GURL& url,
      bool showing_repost_interstitial);
  virtual void UpdateFavIconURL(RenderViewHost* render_view_host,
                                int32 page_id, const GURL& icon_url);
  virtual void DidDownloadImage(RenderViewHost* render_view_host,
                                int id,
                                const GURL& image_url,
                                bool errored,
                                const SkBitmap& image);
  virtual void RequestOpenURL(const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition);
  virtual void DomOperationResponse(const std::string& json_string,
                                    int automation_id);
  virtual void ProcessDOMUIMessage(const std::string& message,
                                   const std::string& content);
  virtual void ProcessExternalHostMessage(const std::string& message,
                                          const std::string& origin,
                                          const std::string& target);
  virtual void GoToEntryAtOffset(int offset);
  virtual void GetHistoryListCount(int* back_list_count,
                                   int* forward_list_count);
  virtual void RunFileChooser(bool multiple_files,
                              const string16& title,
                              const FilePath& default_file);
  virtual void RunJavaScriptMessage(const std::wstring& message,
                                    const std::wstring& default_prompt,
                                    const GURL& frame_url,
                                    const int flags,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message);
  virtual void RunBeforeUnloadConfirm(const std::wstring& message,
                                      IPC::Message* reply_msg);
  virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                   const std::string& json_arguments,
                                   IPC::Message* reply_msg);
  virtual void PasswordFormsSeen(const std::vector<PasswordForm>& forms);
  virtual void AutofillFormSubmitted(const AutofillForm& form);
  virtual void GetAutofillSuggestions(const std::wstring& field_name,
      const std::wstring& user_text, int64 node_id, int request_id);
  virtual void RemoveAutofillEntry(const std::wstring& field_name,
                                   const std::wstring& value);
  virtual void PageHasOSDD(RenderViewHost* render_view_host,
                           int32 page_id, const GURL& url, bool autodetected);
  virtual void InspectElementReply(int num_resources);
  virtual void DidGetPrintedPagesCount(int cookie, int number_pages);
  virtual void DidPrintPage(const ViewHostMsg_DidPrintPage_Params& params);
  virtual GURL GetAlternateErrorPageURL() const;
  virtual WebPreferences GetWebkitPrefs();
  virtual void OnMissingPluginStatus(int status);
  virtual void OnCrashedPlugin(const FilePath& plugin_path);
  virtual void OnCrashedWorker();
  virtual void OnJSOutOfMemory();
  virtual void ShouldClosePage(bool proceed) {
    render_manager_.ShouldClosePage(proceed);
  }
  // Allows the WebContents to react when a cross-site response is ready to be
  // delivered to a pending RenderViewHost.  We must first run the onunload
  // handler of the old RenderViewHost before we can allow it to proceed.
  void OnCrossSiteResponse(int new_render_process_host_id,
                           int new_request_id) {
    render_manager_.OnCrossSiteResponse(new_render_process_host_id,
                                        new_request_id);
  }
  virtual bool CanBlur() const;
  virtual gfx::Rect GetRootWindowResizerRect() const;
  virtual void RendererUnresponsive(RenderViewHost* render_view_host,
                                    bool is_during_unload);
  virtual void RendererResponsive(RenderViewHost* render_view_host);
  virtual void LoadStateChanged(const GURL& url, net::LoadState load_state);
  virtual void OnDidGetApplicationInfo(
      int32 page_id,
      const webkit_glue::WebApplicationInfo& info);
  virtual void OnEnterOrSpace();
  virtual void OnFindReply(int request_id,
                           int number_of_matches,
                           const gfx::Rect& selection_rect,
                           int active_match_ordinal,
                           bool final_update);
  virtual bool CanTerminate() const;


  // SelectFileDialog::Listener ------------------------------------------------

  virtual void FileSelected(const FilePath& path, int index, void* params);
  virtual void MultiFilesSelected(const std::vector<FilePath>& files,
                                  void* params);
  virtual void FileSelectionCanceled(void* params);

  // RenderViewHostManager::Delegate -------------------------------------------

  virtual void BeforeUnloadFiredFromRenderManager(
      bool proceed,
      bool* proceed_to_fire_unload);
  virtual void DidStartLoadingFromRenderManager(
      RenderViewHost* render_view_host, int32 page_id) {
    DidStartLoading(render_view_host, page_id);
  }
  virtual void RenderViewGoneFromRenderManager(
      RenderViewHost* render_view_host) {
    RenderViewGone(render_view_host);
  }
  virtual void UpdateRenderViewSizeForRenderManager();
  virtual void NotifySwappedFromRenderManager() {
    NotifySwapped();
  }
  virtual NavigationController& GetControllerForRenderManager() {
    return controller();
  }
  virtual DOMUI* CreateDOMUIForRenderManager(const GURL& url);
  virtual NavigationEntry* GetLastCommittedNavigationEntryForRenderManager();

  // Initializes the given renderer if necessary and creates the view ID
  // corresponding to this view host. If this method is not called and the
  // process is not shared, then the WebContents will act as though the renderer
  // is not running (i.e., it will render "sad tab"). This method is
  // automatically called from LoadURL.
  //
  // If you are attaching to an already-existing RenderView, you should call
  // InitWithExistingID.
  virtual bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host);

 private:
  FRIEND_TEST(WebContentsTest, UpdateTitle);
  friend class TestWebContents;

  // Temporary until the view/contents separation is complete.
  friend class TabContentsView;
#if defined(OS_WIN)
  friend class TabContentsViewWin;
#elif defined(OS_MACOSX)
  friend class TabContentsViewMac;
#elif defined(OS_LINUX)
  friend class TabContentsViewGtk;
#endif

  // So InterstitialPage can access SetIsLoading.
  friend class InterstitialPage;


  // NotificationObserver ------------------------------------------------------

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Navigation helpers --------------------------------------------------------
  //
  // These functions are helpers for Navigate() and DidNavigate().

  // Handles post-navigation tasks in DidNavigate AFTER the entry has been
  // committed to the navigation controller. Note that the navigation entry is
  // not provided since it may be invalid/changed after being committed. The
  // current navigation entry is in the NavigationController at this point.
  void DidNavigateMainFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);
  void DidNavigateAnyFramePostCommit(
      RenderViewHost* render_view_host,
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);

  // Closes all child windows (constrained popups) when the domain changes.
  // Supply the new and old URLs, and this function will figure out when the
  // domain changing conditions are met.
  void MaybeCloseChildWindows(const GURL& previous_url,
                              const GURL& current_url);

  // Updates the starred state from the bookmark bar model. If the state has
  // changed, the delegate is notified.
  void UpdateStarredStateForCurrentURL();

  // Send the alternate error page URL to the renderer. This method is virtual
  // so special html pages can override this (e.g., the new tab page).
  virtual void UpdateAlternateErrorPageURL();

  // Send webkit specific settings to the renderer.
  void UpdateWebPreferences();

  // If our controller was restored and the page id is > than the site
  // instance's page id, the site instances page id is updated as well as the
  // renderers max page id.
  void UpdateMaxPageIDIfNecessary(SiteInstance* site_instance,
                                  RenderViewHost* rvh);

  // Called by OnMsgNavigate to update history state. Overridden by subclasses
  // that don't want to be added to history.
  virtual void UpdateHistoryForNavigation(const GURL& display_url,
      const ViewHostMsg_FrameNavigate_Params& params);

  // Saves the given title to the navigation entry and does associated work. It
  // will update history and the view for the new title, and also synthesize
  // titles for file URLs that have none (so we require that the URL of the
  // entry already be set).
  //
  // This is used as the backend for state updates, which include a new title,
  // or the dedicated set title message. It returns true if the new title is
  // different and was therefore updated.
  bool UpdateTitleForEntry(NavigationEntry* entry, const std::wstring& title);

  // Misc non-view stuff -------------------------------------------------------

  // Helper functions for sending notifications.
  void NotifySwapped();
  void NotifyConnected();
  void NotifyDisconnected();

  // If params has a searchable form, this tries to create a new keyword.
  void GenerateKeywordIfNecessary(
      const ViewHostMsg_FrameNavigate_Params& params);

  // Returns the DOMUI for the current state of the tab. This will either be
  // the pending DOMUI, the committed DOMUI, or NULL.
  DOMUI* GetDOMUIForCurrentState();

  DISALLOW_COPY_AND_ASSIGN(WebContents);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_H_
