// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBVIEW_IMPL_H__
#define WEBKIT_GLUE_WEBVIEW_IMPL_H__

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gfx/platform_canvas.h"
#include "base/gfx/point.h"
#include "base/gfx/size.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webview.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "BackForwardList.h"
MSVC_POP_WARNING();

namespace WebCore {
class Frame;
class HistoryItem;
class KeyboardEvent;
class Page;
class PlatformKeyboardEvent;
class PopupContainer;
class Range;
class Widget;
}

class AutocompletePopupMenuClient;
class ImageResourceFetcher;
class SearchableFormData;
struct WebDropData;
class WebHistoryItemImpl;
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebViewDelegate;

class WebViewImpl : public WebView, public WebCore::BackForwardListClient {
 public:
  // WebView
  virtual bool ShouldClose();
  virtual void Close();
  virtual WebViewDelegate* GetDelegate();
  virtual void SetUseEditorDelegate(bool value);
  virtual void SetTabKeyCyclesThroughElements(bool value);
  virtual WebFrame* GetMainFrame();
  virtual WebFrame* GetFocusedFrame();
  virtual void SetFocusedFrame(WebFrame* frame);
  virtual WebFrame* GetFrameWithName(const std::wstring& name);
  virtual WebFrame* GetPreviousFrameBefore(WebFrame* frame, bool wrap);
  virtual WebFrame* GetNextFrameAfter(WebFrame* frame, bool wrap);
  virtual void Resize(const gfx::Size& new_size);
  virtual gfx::Size GetSize() { return size(); }
  virtual void Layout();
  virtual void Paint(gfx::PlatformCanvas* canvas, const gfx::Rect& rect);
  virtual bool HandleInputEvent(const WebInputEvent* input_event);
  virtual void MouseCaptureLost();
  virtual void SetFocus(bool enable);
  virtual void StoreFocusForFrame(WebFrame* frame);
  virtual void ImeSetComposition(int string_type, int cursor_position,
                                 int target_start, int target_end,
                                 int string_length,
                                 const wchar_t *string_data);
  virtual bool ImeUpdateStatus(bool* enable_ime, const void** id,
                               int* x, int* y);
  virtual void StopLoading();
  virtual void SetBackForwardListSize(int size);
  virtual void RestoreFocus();
  virtual void SetInitialFocus(bool reverse);
  virtual bool FocusedFrameNeedsSpellchecking();
  virtual bool DownloadImage(int id, const GURL& image_url, int image_size);
  virtual void SetPreferences(const WebPreferences& preferences);
  virtual const WebPreferences& GetPreferences();
  virtual void SetPageEncoding(const std::wstring& encoding_name);
  virtual std::wstring GetMainFrameEncodingName();
  virtual void ZoomIn(bool text_only);
  virtual void ZoomOut(bool text_only);
  virtual void ResetZoom();
  virtual void CopyImageAt(int x, int y);
  virtual void InspectElement(int x, int y);
  virtual void ShowJavaScriptConsole();
  virtual void DragSourceEndedAt(
      int client_x, int client_y, int screen_x, int screen_y);
  virtual void DragSourceMovedTo(
      int client_x, int client_y, int screen_x, int screen_y);
  virtual void DragSourceSystemDragEnded();
  virtual bool DragTargetDragEnter(const WebDropData& drop_data,
      int client_x, int client_y, int screen_x, int screen_y);
  virtual bool DragTargetDragOver(
      int client_x, int client_y, int screen_x, int screen_y);
  virtual void DragTargetDragLeave();
  virtual void DragTargetDrop(
      int client_x, int client_y, int screen_x, int screen_y);
  virtual void AutofillSuggestionsForNode(
      int64 node_id,
      const std::vector<std::wstring>& suggestions,
      int default_suggestion_index);

  // WebViewImpl

  const gfx::Size& size() const { return size_; }

  const gfx::Point& last_mouse_down_point() const {
      return last_mouse_down_point_;
  }

  WebCore::Frame* GetFocusedWebCoreFrame();

  static WebViewImpl* FromPage(WebCore::Page* page);

  WebFrameImpl* main_frame() {
    return main_frame_;
  }

  WebViewDelegate* delegate() {
    return delegate_.get();
  }

  // Returns the page object associated with this view. This may be NULL when
  // the page is shutting down, but will be valid all other times.
  WebCore::Page* page() const {
    return page_.get();
  }

  WebHistoryItemImpl* pending_history_item() const {
    return pending_history_item_;
  }

  void MouseMove(const WebMouseEvent& mouse_event);
  void MouseLeave(const WebMouseEvent& mouse_event);
  void MouseDown(const WebMouseEvent& mouse_event);
  void MouseUp(const WebMouseEvent& mouse_event);
  void MouseContextMenu(const WebMouseEvent& mouse_event);
  void MouseDoubleClick(const WebMouseEvent& mouse_event);
  void MouseWheel(const WebMouseWheelEvent& wheel_event);
  bool KeyEvent(const WebKeyboardEvent& key_event);
  bool CharEvent(const WebKeyboardEvent& key_event);

  // Handles context menu events orignated via the the keyboard. These
  // include the VK_APPS virtual key and the Shift+F10 combine.
  // Code is based on the Webkit function
  // bool WebView::handleContextMenuEvent(WPARAM wParam, LPARAM lParam) in
  // webkit\webkit\win\WebView.cpp. The only significant change in this
  // function is the code to convert from a Keyboard event to the Right
  // Mouse button down event.
  bool SendContextMenuEvent(const WebKeyboardEvent& event);

  // Releases references used to restore focus.
  void ReleaseFocusReferences();

  // Notifies the WebView that a load has been committed.
  // is_new_navigation will be true if a new session history item should be
  // created for that load.
  void DidCommitLoad(bool* is_new_navigation);

  bool context_menu_allowed() const {
    return context_menu_allowed_;
  }

  // Set the disposition for how this webview is to be initially shown.
  void set_window_open_disposition(WindowOpenDisposition disp) {
    window_open_disposition_ = disp;
  }
  WindowOpenDisposition window_open_disposition() const {
    return window_open_disposition_;
  }

  // Start a system drag and drop operation.
  void StartDragging(const WebDropData& drop_data);

  virtual const WebCore::Node* getInspectedNode(WebCore::Frame* frame);

  // ImageResourceFetcher callback.
  void ImageResourceDownloadDone(ImageResourceFetcher* fetcher,
                                 bool errored,
                                 const SkBitmap& image);

  // Hides the autocomplete popup if it is showing.
  void HideAutoCompletePopup();

 protected:
  friend class WebView;  // So WebView::Create can call our constructor

  WebViewImpl();
  ~WebViewImpl();

  void ModifySelection(uint32 message,
                       WebCore::Frame* frame,
                       const WebCore::PlatformKeyboardEvent& e);

  // TODO(darin): Figure out what happens to these methods.
#if 0
  // WebCore::WidgetClientWin
  virtual const SkBitmap* getPreloadedResourceBitmap(int resource_id);
  virtual void onScrollPositionChanged(WebCore::Widget* widget);
  virtual const WTF::Vector<RefPtr<WebCore::Range> >* getTickmarks(
      WebCore::Frame* frame);
  virtual size_t getActiveTickmarkIndex(WebCore::Frame* frame);
  virtual bool isHidden();
#endif

  // WebCore::BackForwardListClient
  virtual void didAddHistoryItem(WebCore::HistoryItem* item);
  virtual void willGoToHistoryItem(WebCore::HistoryItem* item);
  virtual WebCore::HistoryItem* itemAtIndex(int index);
  virtual int backListCount();
  virtual int forwardListCount();

  // Creates and returns a new SearchableFormData for the focused node.
  // It's up to the caller to free the returned SearchableFormData.
  // This returns NULL if the focused node is NULL, or not in a valid form.
  SearchableFormData* CreateSearchableFormDataForFocusedNode();

  scoped_refptr<WebViewDelegate> delegate_;
  gfx::Size size_;

  scoped_refptr<WebFrameImpl> main_frame_;
  gfx::Point last_mouse_position_;
  // Reference to the Frame that last had focus. This is set once when
  // we lose focus, and used when focus is gained to reinstall focus to
  // the correct element.
  RefPtr<WebCore::Frame> last_focused_frame_;
  // Reference to the node that last had focus.
  RefPtr<WebCore::Node> last_focused_node_;
  scoped_ptr<WebCore::Page> page_;

  // The last history item that was accessed via itemAtIndex().  We keep track
  // of this until willGoToHistoryItem() is called, so we can track the
  // navigation.
  scoped_refptr<WebHistoryItemImpl> pending_history_item_;

  // This flag is set when a new navigation is detected.  It is used to satisfy
  // the corresponding argument to WebViewDelegate::DidCommitLoadForFrame.
  bool observed_new_navigation_;
#ifndef NDEBUG
  // Used to assert that the new navigation we observed is the same navigation
  // when we make use of observed_new_navigation_.
  const WebCore::DocumentLoader* new_navigation_loader_;
#endif

  // A copy of the WebPreferences object we receive from the browser.
  WebPreferences webprefs_;

  // A copy of the web drop data object we received from the browser.
  scoped_ptr<WebDropData> current_drop_data_;

 private:
  // Returns true if the event was actually processed.
  bool KeyEventDefault(const WebKeyboardEvent& event);

  // Returns true if the view was scrolled.
  bool ScrollViewWithKeyboard(int key_code);

  // Removes fetcher from the set of pending image fetchers and deletes it.
  // This is invoked after the download is completed (or fails).
  void DeleteImageResourceFetcher(ImageResourceFetcher* fetcher);

  // ImageResourceFetchers schedule via DownloadImage.
  std::set<ImageResourceFetcher*> image_fetchers_;

  // The point relative to the client area where the mouse was last pressed
  // down. This is used by the drag client to determine what was under the
  // mouse when the drag was initiated. We need to track this here in
  // WebViewImpl since DragClient::startDrag does not pass the position the
  // mouse was at when the drag was initiated, only the current point, which
  // can be misleading as it is usually not over the element the user actually
  // dragged by the time a drag is initiated.
  gfx::Point last_mouse_down_point_;

  // Keeps track of the current text zoom level.  0 means no zoom, positive
  // values mean larger text, negative numbers mean smaller.
  int zoom_level_;

  bool context_menu_allowed_;

  bool doing_drag_and_drop_;

  // Webkit expects keyPress events to be suppressed if the associated keyDown
  // event was handled. Safari implements this behavior by peeking out the
  // associated WM_CHAR event if the keydown was handled. We emulate
  // this behavior by setting this flag if the keyDown was handled.
  bool suppress_next_keypress_event_;

  // The disposition for how this webview is to be initially shown.
  WindowOpenDisposition window_open_disposition_;

  // Represents whether or not this object should process incoming IME events.
  bool ime_accept_events_;

  // The currently shown autocomplete popup.
  RefPtr<WebCore::PopupContainer> autocomplete_popup_;

  // The popup client of the currently shown autocomplete popup.  Necessary for
  // managing the life of the client.
  RefPtr<AutocompletePopupMenuClient> autocomplete_popup_client_;

  // HACK: current_input_event is for ChromeClientImpl::show(), until we can fix
  // WebKit to pass enough information up into ChromeClient::show() so we can
  // decide if the window.open event was caused by a middle-mouse click
public:
  static const WebInputEvent* current_input_event() {
    return g_current_input_event;
  }
private:
  static const WebInputEvent* g_current_input_event;

  DISALLOW_EVIL_CONSTRUCTORS(WebViewImpl);
};

#endif  // WEBKIT_GLUE_WEBVIEW_IMPL_H__

