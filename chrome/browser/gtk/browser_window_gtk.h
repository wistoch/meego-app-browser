// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BROWSER_WINDOW_GTK_H_
#define CHROME_BROWSER_GTK_BROWSER_WINDOW_GTK_H_

#include <gtk/gtk.h>

#include <map>

#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/x11_util.h"

class BookmarkBarGtk;
class BrowserToolbarGtk;
class CustomDrawButton;
class DownloadShelfGtk;
class FindBarGtk;
class InfoBarContainerGtk;
class LocationBar;
class NineBox;
class StatusBubbleGtk;
class TabContentsContainerGtk;
class TabStripGtk;

// An implementation of BrowserWindow for GTK.
// Cross-platform code will interact with this object when
// it needs to manipulate the window.

class BrowserWindowGtk : public BrowserWindow,
                         public NotificationObserver,
                         public TabStripModelObserver {
 public:
  explicit BrowserWindowGtk(Browser* browser);
  virtual ~BrowserWindowGtk();

  // Process a keyboard input and try to find an accelerator for it.
  void HandleAccelerator(guint keyval, GdkModifierType modifier);

  // Overridden from BrowserWindow
  virtual void Show();
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void Close();
  virtual void Activate();
  virtual bool IsActive() const;
  virtual void FlashFrame();
  virtual gfx::NativeWindow GetNativeHandle();
  virtual BrowserWindowTesting* GetBrowserWindowTesting();
  virtual StatusBubble* GetStatusBubble();
  virtual void SelectedTabToolbarSizeChanged(bool is_animating);
  virtual void UpdateTitleBar();
  virtual void UpdateLoadingAnimations(bool should_animate);
  virtual void SetStarredState(bool is_starred);
  virtual gfx::Rect GetNormalBounds() const;
  virtual bool IsMaximized() const;
  virtual void SetFullscreen(bool fullscreen);
  virtual bool IsFullscreen() const;
  virtual LocationBar* GetLocationBar() const;
  virtual void SetFocusToLocationBar();
  virtual void UpdateStopGoState(bool is_loading, bool force);
  virtual void UpdateToolbar(TabContents* contents,
                             bool should_restore_state);
  virtual void FocusToolbar();
  virtual bool IsBookmarkBarVisible() const;
  virtual gfx::Rect GetRootWindowResizerRect() const;
  virtual void ToggleBookmarkBar();
  virtual void ShowAboutChromeDialog();
  virtual void ShowBookmarkManager();
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked);
  virtual bool IsDownloadShelfVisible() const;
  virtual DownloadShelf* GetDownloadShelf();
  virtual void ShowReportBugDialog();
  virtual void ShowClearBrowsingDataDialog();
  virtual void ShowImportDialog();
  virtual void ShowSearchEnginesDialog();
  virtual void ShowPasswordManager();
  virtual void ShowSelectProfileDialog();
  virtual void ShowNewProfileDialog();
  virtual void ConfirmBrowserCloseWithPendingDownloads();
  virtual void ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window);
  virtual void UserChangedTheme();
  virtual int GetExtraRenderViewHeight() const;

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from TabStripModelObserver:
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabStripEmpty();

  // Accessor for the tab strip.
  TabStripGtk* tabstrip() const { return tabstrip_.get(); }

  void MaybeShowBookmarkBar(TabContents* contents);
  void UpdateUIForContents(TabContents* contents);

  void OnBoundsChanged(const gfx::Rect& bounds);
  void OnStateChanged(GdkWindowState state);

  // Returns false if we're not ready to close yet.  E.g., a tab may have an
  // onbeforeunload handler that prevents us from closing.
  bool CanClose() const;

  bool ShouldShowWindowIcon() const;

  // Add the find bar widget to the window hierarchy.
  void AddFindBar(FindBarGtk* findbar);

  // Sets whether a drag is active. If a drag is active the window will not
  // close.
  void set_drag_active(bool drag_active) { drag_active_ = drag_active; }

  // Returns the BrowserWindowGtk registered with |window|.
  static BrowserWindowGtk* GetBrowserWindowForNativeWindow(
      gfx::NativeWindow window);

  // Retrieves the GtkWindow associated with |xid|, which is the X Window
  // ID of the top-level X window of this object.
  static GtkWindow* GetBrowserWindowForXID(XID xid);

 protected:
  virtual void DestroyBrowser();
  // Top level window.
  GtkWindow* window_;
  // GtkAlignment that holds the interior components of the chromium window.
  GtkWidget* window_container_;
  // Box that holds the min/max/close buttons if the user turns off window
  // manager decorations.
  GtkWidget* titlebar_buttons_box_;
  // Gtk alignment that contains the tab strip.  If the user turns off window
  // manager decorations, we draw this taller.
  GtkWidget* titlebar_alignment_;
  // VBox that holds everything below the tabs.
  GtkWidget* content_vbox_;
  // VBox that holds everything below the toolbar.
  GtkWidget* render_area_vbox_;

  scoped_ptr<Browser> browser_;

  // The download shelf view (view at the bottom of the page).
  scoped_ptr<DownloadShelfGtk> download_shelf_;

 private:
  // Sets the default size for the window and the the way the user is allowed to
  // resize it.
  void SetGeometryHints();

  // Set up the window icon (potentially used in window border or alt-tab list).
  void SetWindowIcon();

  // Connect accelerators that aren't connected to menu items (like ctrl-o,
  // ctrl-l, etc.).
  void ConnectAccelerators();

  // Build the titlebar which includes the tab strip, the space above the tab
  // strip, and (maybe) the min, max, close buttons.  |container| is the gtk
  // continer that we put the widget into.
  void BuildTitlebar(GtkWidget* container);

  // Constructs a CustomDraw button given 3 image ids (IDR_), the box to place
  // the button into, and a tooltip id (IDS_).
  CustomDrawButton* BuildTitlebarButton(int image, int image_pressed,
                                        int image_hot, GtkWidget* box,
                                        int tooltip);

  // Change whether we're showing the custom blue frame.
  // Must be called once at startup.
  // Triggers relayout of the content.
  void UpdateCustomFrame();

  // Save the window position in the prefs.
  void SaveWindowPosition();

  // Callback for when the "content area" vbox needs to be redrawn.
  // The content area includes the toolbar and web page but not the tab strip.
  static gboolean OnContentAreaExpose(GtkWidget* widget, GdkEventExpose* e,
                                      BrowserWindowGtk* window);

  // Callback for when the titlebar (include the background of the tab strip)
  // needs to be redrawn.
  static gboolean OnTitlebarExpose(GtkWidget* widget, GdkEventExpose* e,
                                   BrowserWindowGtk* window);

  // Callback for min/max/close buttons.
  static void OnButtonClicked(GtkWidget* button, BrowserWindowGtk* window);

  static gboolean OnGtkAccelerator(GtkAccelGroup* accel_group,
                                   GObject* acceleratable,
                                   guint keyval,
                                   GdkModifierType modifier,
                                   BrowserWindowGtk* browser_window);

  // Maps and Unmaps the xid of |widget| to |window|.
  static void MainWindowMapped(GtkWidget* widget, BrowserWindowGtk* window);
  static void MainWindowUnMapped(GtkWidget* widget, BrowserWindowGtk* window);

  // A small shim for browser_->ExecuteCommand.
  void ExecuteBrowserCommand(int id);

  // Callback for the loading animation(s) associated with this window.
  void LoadingAnimationCallback();

  // Shows UI elements for supported window features.
  void ShowSupportedWindowFeatures();

  // Hides UI elements for unsupported window features.
  void HideUnsupportedWindowFeatures();

  bool IsTabStripSupported();

  bool IsToolbarSupported();

  NotificationRegistrar registrar_;

  gfx::Rect bounds_;
  GdkWindowState state_;

  // Whether we are full screen. Since IsFullscreen() gets called before
  // OnStateChanged(), we can't rely on |state_| & GDK_WINDOW_STATE_FULLSCREEN.
  bool full_screen_;

  // The object that manages all of the widgets in the toolbar.
  scoped_ptr<BrowserToolbarGtk> toolbar_;

  // The object that manages the bookmark bar.
  scoped_ptr<BookmarkBarGtk> bookmark_bar_;

  // The status bubble manager.  Always non-NULL.
  scoped_ptr<StatusBubbleGtk> status_bubble_;

  // A container that manages the GtkWidget*s that are the webpage display
  // (along with associated infobars, shelves, and other things that are part
  // of the content area).
  scoped_ptr<TabContentsContainerGtk> contents_container_;

  // The tab strip.  Always non-NULL.
  scoped_ptr<TabStripGtk> tabstrip_;

  // The container for info bars. Always non-NULL.
  scoped_ptr<InfoBarContainerGtk> infobar_container_;

  // Maximize and restore widgets in the titlebar.
  scoped_ptr<CustomDrawButton> minimize_button_;
  scoped_ptr<CustomDrawButton> maximize_button_;
  scoped_ptr<CustomDrawButton> restore_button_;
  scoped_ptr<CustomDrawButton> close_button_;

  // The background of the title bar and tab strip.
  scoped_ptr<NineBox> titlebar_background_;
  scoped_ptr<NineBox> titlebar_background_otr_;

  // The timer used to update frames for the Loading Animation.
  base::RepeatingTimer<BrowserWindowGtk> loading_animation_timer_;

  // Whether we're showing the custom chrome frame or the window manager
  // decorations.
  BooleanPrefMember use_custom_frame_;

  // True if a drag is active. See description above setter for details.
  bool drag_active_;

  // A map which translates an X Window ID into its respective GtkWindow.
  static std::map<XID, GtkWindow*> xid_map_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowGtk);
};

#endif  // CHROME_BROWSER_GTK_BROWSER_WINDOW_GTK_H_
