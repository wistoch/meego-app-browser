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

#ifndef CHROME_BROWSER_TABS_TAB_STRIP_H__
#define CHROME_BROWSER_TABS_TAB_STRIP_H__

#include "base/gfx/point.h"
#include "base/task.h"
#include "chrome/browser/tabs/tab.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/views/button.h"
#include "chrome/views/hwnd_view_container.h"
#include "chrome/views/menu.h"
#include "chrome/views/view.h"

class DraggedTabController;
class ScopedMouseCloseWidthCalculator;
class TabStripModel;
class Timer;

namespace ChromeViews {
class ImageView;
}

///////////////////////////////////////////////////////////////////////////////
//
// TabStrip
//
//  A View that represents the TabStripModel. The TabStrip has the
//  following responsibilities:
//    - It implements the TabStripModelObserver interface, and acts as a
//      container for Tabs, and is also responsible for creating them.
//    - It takes part in Tab Drag & Drop with Tab, TabDragHelper and
//      DraggedTab, focusing on tasks that require reshuffling other tabs
//      in response to dragged tabs.
//
///////////////////////////////////////////////////////////////////////////////
class TabStrip : public ChromeViews::View,
                 public TabStripModelObserver,
                 public Tab::TabDelegate,
                 public ChromeViews::Button::ButtonListener,
                 public Task,
                 public MessageLoop::Observer {
 public:
  TabStrip(TabStripModel* model);
  virtual ~TabStrip();

  // Returns the preferred height of this TabStrip. This is based on the
  // typical height of its constituent tabs.
  int GetPreferredHeight();

  // Returns true if the associated TabStrip's delegate supports tab moving or
  // detaching. Used by the Frame to determine if dragging on the Tab
  // itself should move the window in cases where there's only one
  // non drag-able Tab.
  bool HasAvailableDragActions() const;

  // Ask the delegate to show the application menu at the provided point.
  // The point is in screen coordinate system.
  void ShowApplicationMenu(const gfx::Point& p);

  // Returns true if the TabStrip can accept input events. This returns false
  // when the TabStrip is animating to a new state and as such the user should
  // not be allowed to interact with the TabStrip.
  bool CanProcessInputEvents() const;

  // Return true if this tab strip is compatible with the provided tab strip.
  // Compatible tab strips can transfer tabs during drag and drop.
  bool IsCompatibleWith(TabStrip* other);

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  bool IsAnimating() const;

  // Accessors for the model and individual Tabs.
  TabStripModel* model() { return model_; }

  // Returns true if there is an active drag session.
  bool IsDragSessionActive() const { return drag_controller_.get() != NULL; }

  // Aborts any active drag session. This is called from XP/VistaFrame's
  // end session handler to make sure there are no drag sessions in flight that
  // could prevent the frame from being closed right away.
  void AbortActiveDragSession() { EndDrag(true); }

  // Destroys the active drag controller.
  void DestroyDragController();

  // Retrieve the ideal bounds for the Tab at the specified index.
  gfx::Rect GetIdealBounds(int index);

  // ChromeViews::View overrides:
  virtual void PaintChildren(ChromeCanvas* canvas);
  virtual void DidChangeBounds(const CRect& previous, const CRect& current);
  virtual ChromeViews::View* GetViewByID(int id) const;
  virtual void Layout();
  virtual void GetPreferredSize(CSize* preferred_size);
  // NOTE: the drag and drop methods are invoked from FrameView. This is done to
  // allow for a drop region that extends outside the bounds of the TabStrip.
  virtual void OnDragEntered(const ChromeViews::DropTargetEvent& event);
  virtual int OnDragUpdated(const ChromeViews::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const ChromeViews::DropTargetEvent& event);
  virtual bool GetAccessibleRole(VARIANT* role);
  virtual bool GetAccessibleName(std::wstring* name);
  virtual void SetAccessibleName(const std::wstring& name);

 protected:
  // TabStripModelObserver implementation:
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContents* contents, int from_index, int to_index);
  virtual void TabChangedAt(TabContents* contents, int index);
  virtual void TabValidateAnimations();

  // Tab::Delegate implementation:
  virtual bool IsTabSelected(const Tab* tab) const;
  virtual void SelectTab(Tab* tab);
  virtual void CloseTab(Tab* tab);
  virtual bool IsCommandEnabledForTab(
      TabStripModel::ContextMenuCommand command_id, const Tab* tab) const;
  virtual void ExecuteCommandForTab(
      TabStripModel::ContextMenuCommand command_id, Tab* tab);
  virtual void MaybeStartDrag(Tab* tab,
                              const ChromeViews::MouseEvent& event);
  virtual void ContinueDrag(const ChromeViews::MouseEvent& event);
  virtual void EndDrag(bool canceled);

  // ChromeViews::Button::ButtonListener implementation:
  virtual void ButtonPressed(ChromeViews::BaseButton* sender);

  // Task implementation:
  virtual void Run();

  // MessageLoop::Observer implementation:
  virtual void WillProcessMessage(const MSG& msg);
  virtual void DidProcessMessage(const MSG& msg);

 private:
  friend class DraggedTabController;
  friend class InsertTabAnimation;
  friend class MoveTabAnimation;
  friend class RemoveTabAnimation;
  friend class ResizeLayoutAnimation;
  friend class SuspendAnimationsTask;
  friend class TabAnimation;

  TabStrip();
  void Init();

  // Retrieves the Tab at the specified index.
  Tab* GetTabAt(int index) const;

  // Gets the number of Tabs in the collection.
  int GetTabCount() const;

  // -- Tab Resize Layout -----------------------------------------------------

  // Returns the exact (unrounded) current width of each tab.
  void GetCurrentTabWidths(double* unselected_width,
                           double* selected_width) const;

  // Returns the exact (unrounded) desired width of each tab, based on the
  // desired strip width and number of tabs.  If
  // |width_of_tabs_for_mouse_close_| is nonnegative we use that value in
  // calculating the desired strip width; otherwise we use the current width.
  void GetDesiredTabWidths(int tab_count,
                           double* unselected_width,
                           double* selected_width) const;

  // Perform an animated resize-relayout of the TabStrip immediately.
  void ResizeLayoutTabs();

  // Returns whether or not the cursor is currently in the "tab strip zone"
  // which is defined as the region above the TabStrip and a bit below it.
  // Note: this method cannot be const because |ConvertPointToScreen| is not.
  //       #@*($&(#!!!
  bool IsCursorInTabStripZone();

  // Ensure that the message loop observer used for event spying is added and
  // removed appropriately so we can tell when to resize layout the tab strip.
  void AddMessageLoopObserver();
  void RemoveMessageLoopObserver();

  // Called to update the frame of the Loading animations.
  void LoadingAnimationCallback();

  // -- Link Drag & Drop ------------------------------------------------------

  // Returns the bounds to render the drop at, in screen coordinates. Sets
  // |is_beneath| to indicate whether the arrow is beneath the tab, or above
  // it.
  gfx::Rect GetDropBounds(int drop_index, bool drop_before, bool* is_beneath);

  // Updates the location of the drop based on the event.
  void UpdateDropIndex(const ChromeViews::DropTargetEvent& event);

  // Sets the location of the drop, repainting as necessary.
  void SetDropIndex(int index, bool drop_before);

  // Returns the drop effect for dropping a URL on the tab strip. This does
  // not query the data in anyway, it only looks at the source operations.
  int GetDropEffect(const ChromeViews::DropTargetEvent& event);

  // Returns the image to use for indicating a drop on a tab. If is_down is
  // true, this returns an arrow pointing down.
  static SkBitmap* GetDropArrowImage(bool is_down);

  // -- Animations ------------------------------------------------------------

  // Generates the ideal bounds of the TabStrip when all Tabs have finished
  // animating to their desired position/bounds. This is used by the standard
  // Layout method and other callers like the DraggedTabController that need
  // stable representations of Tab positions.
  void GenerateIdealBounds();

  // Lays out the New Tab button, assuming the right edge of the last Tab on
  // the TabStrip at |last_tab_right|.
  void LayoutNewTabButton(double last_tab_right, double unselected_width);

  // A generic Layout method for various classes of TabStrip animations,
  // including Insert, Remove and Resize Layout cases/
  void AnimationLayout(double unselected_width);

  // Starts various types of TabStrip animations.
  void StartResizeLayoutAnimation();
  void StartInsertTabAnimation(int index);
  void StartRemoveTabAnimation(int index, TabContents* contents);
  void StartMoveTabAnimation(int from_index, int to_index);

  // Returns true if detach or select changes in the model should be reflected
  // in the TabStrip. This returns false if we're closing all tabs in the
  // TabStrip and so we should prevent updating. This is not const because we
  // use this as a signal to cancel any active animations.
  bool CanUpdateDisplay();

  // Notifies the TabStrip that the specified TabAnimation has completed.
  // Optionally a full Layout will be performed, specified by |layout|.
  class TabAnimation;
  void FinishAnimation(TabAnimation* animation, bool layout);

  // Finds the index of the TabContents corresponding to |tab| in our
  // associated TabStripModel, or -1 if there is none (e.g. the specified |tab|
  // is being animated closed).
  int GetIndexOfTab(const Tab* tab) const;

  // Calculates the available width for tabs, assuming a Tab is to be closed.
  int GetAvailableWidthForTabs(Tab* last_tab) const;

  // -- Member Variables ------------------------------------------------------

  // Our model.
  TabStripModel* model_;

  // A factory that is used to construct a delayed callback to the
  // ResizeLayoutTabsNow method.
  ScopedRunnableMethodFactory<TabStrip> resize_layout_factory_;

  // True if the TabStrip has already been added as a MessageLoop observer.
  bool added_as_message_loop_observer_;

  // True if a resize layout animation should be run a short delay after the
  // mouse exits the TabStrip.
  // TODO(beng): (Cleanup) this would be better named "needs_resize_layout_".
  bool resize_layout_scheduled_;

  // The timer used to update frames for the Loading Animation.
  scoped_ptr<Timer> loading_animation_timer_;

  // The "New Tab" button.
  ChromeViews::Button* newtab_button_;
  gfx::Size newtab_button_size_;
  gfx::Size actual_newtab_button_size_;

  // The current widths of various types of tabs.  We save these so that, as
  // users close tabs while we're holding them at the same size, we can lay out
  // tabs exactly and eliminate the "pixel jitter" we'd get from just leaving
  // them all at their existing, rounded widths.
  double current_unselected_width_;
  double current_selected_width_;

  // If this value is nonnegative, it is used in GetDesiredTabWidths() to
  // calculate how much space in the tab strip to use for tabs.  Most of the
  // time this will be -1, but while we're handling closing a tab via the mouse,
  // we'll set this to the edge of the last tab before closing, so that if we
  // are closing the last tab and need to resize immediately, we'll resize only
  // back to this width, thus once again placing the last tab under the mouse
  // cursor.
  int available_width_for_tabs_;

  // Storage of strings needed for accessibility.
  std::wstring accessible_name_;

  // Used during a drop session of a url. Tracks the position of the drop as
  // well as a window used to highlight where the drop occurs.
  struct DropInfo {
    DropInfo(int index, bool drop_before, bool paint_down);
    ~DropInfo();

    // Index of the tab to drop on. If drop_before is true, the drop should
    // occur between the tab at drop_index - 1 and drop_index.
    // WARNING: if drop_before is true it is possible this will == tab_count,
    // which indicates the drop should create a new tab at the end of the tabs.
    int drop_index;
    bool drop_before;

    // Direction the arrow should point in. If true, the arrow is displayed
    // above the tab and points down. If false, the arrow is displayed beneath
    // the tab and points up.
    bool point_down;

    // Renders the drop indicator.
    ChromeViews::HWNDViewContainer* arrow_window;
    ChromeViews::ImageView* arrow_view;

   private:
    DISALLOW_EVIL_CONSTRUCTORS(DropInfo);
  };

  // Valid for the lifetime of a drag over us.
  scoped_ptr<DropInfo> drop_info_;

  // The controller for a drag initiated from a Tab. Valid for the lifetime of
  // the drag session.
  scoped_ptr<DraggedTabController> drag_controller_;

  // The Tabs we contain, and their last generated "good" bounds.
  struct TabData {
    Tab* tab;
    gfx::Rect ideal_bounds;
  };
  std::vector<TabData> tab_data_;

  // The currently running animation.
  scoped_ptr<TabAnimation> active_animation_;

  DISALLOW_EVIL_CONSTRUCTORS(TabStrip);
};

#endif  // CHROME_BROWSER_TABS_TAB_STRIP_H__
