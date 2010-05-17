// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/tab_strip.h"

#include "app/animation_container.h"
#include "app/drag_drop_types.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "base/compiler_specific.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/tabs/tab.h"
#include "chrome/browser/views/tabs/tab_strip_controller.h"
#include "chrome/common/pref_names.h"
#include "gfx/canvas.h"
#include "gfx/path.h"
#include "gfx/size.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/image_view.h"
#include "views/widget/default_theme_provider.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#include "views/widget/widget_win.h"
#elif defined(OS_LINUX)
#include "views/widget/widget_gtk.h"
#endif

#undef min
#undef max

#if defined(COMPILER_GCC)
// Squash false positive signed overflow warning in GenerateStartAndEndWidths
// when doing 'start_tab_count < end_tab_count'.
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#endif

using views::DropTargetEvent;

// Duration of the first step in a new tab animation.
static const int kNewTabDurationMs = 50;

// Duration of the last step in the new tab animation.
static const int kNewTab3DurationMs = 100;

// Amount in pixels newly inserted tabs go past target bounds before animating
// to final position. This is used for ANIMATION_NEW_TAB_2.
static const int kNewTabOvershoot = 9;

// Amount in pixels the newly inserted tab is clipped against the previous
// tab while animating. This is used to make sure the user doesn't see the
// newly inserted tab behind other tabs and so that its shadow isn't visible
// until the user can actually see the tab.
static const int kNetTabSelectedOffset = -13;

static const int kNewTabButtonHOffset = -5;
static const int kNewTabButtonVOffset = 5;
static const int kResizeTabsTimeMs = 300;
static const int kSuspendAnimationsTimeMs = 200;
static const int kTabHOffset = -16;
static const int kTabStripAnimationVSlop = 40;

// Alpha value phantom tabs are rendered at.
static const int kPhantomTabAlpha = 105;

// Alpha value phantom tab icons are rendered at.
static const int kPhantomTabIconAlpha = 160;

// Size of the drop indicator.
static int drop_indicator_width;
static int drop_indicator_height;

static inline int Round(double x) {
  // Why oh why is this not in a standard header?
  return static_cast<int>(floor(x + 0.5));
}

namespace {

// Animation delegate used during new tab animation step 2 to vary the alpha of
// the tab.
class NewTabAlphaDelegate
    : public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  explicit NewTabAlphaDelegate(Tab* tab) : tab_(tab) {
  }

  virtual void AnimationProgressed(const Animation* animation) {
    if (tab_->render_unselected())
      tab_->set_alpha(animation->GetCurrentValue());
  }

 private:
  Tab* tab_;

  DISALLOW_COPY_AND_ASSIGN(NewTabAlphaDelegate);
};

// Animation delegate used when a dragged tab is released. When done sets the
// dragging state to false.
class ResetDraggingStateDelegate
    : public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  explicit ResetDraggingStateDelegate(BaseTab* tab) : tab_(tab) {
  }

  virtual void AnimationEnded(const Animation* animation) {
    tab_->set_dragging(false);
  }

  virtual void AnimationCanceled(const Animation* animation) {
    tab_->set_dragging(false);
  }

 private:
  BaseTab* tab_;

  DISALLOW_COPY_AND_ASSIGN(ResetDraggingStateDelegate);
};

///////////////////////////////////////////////////////////////////////////////
// NewTabButton
//
//  A subclass of button that hit-tests to the shape of the new tab button.

class NewTabButton : public views::ImageButton {
 public:
  explicit NewTabButton(views::ButtonListener* listener)
      : views::ImageButton(listener) {
  }
  virtual ~NewTabButton() {}

 protected:
  // Overridden from views::View:
  virtual bool HasHitTestMask() const {
    // When the button is sized to the top of the tab strip we want the user to
    // be able to click on complete bounds, and so don't return a custom hit
    // mask.
    return !browser_defaults::kSizeTabButtonToTopOfTabStrip;
  }
  virtual void GetHitTestMask(gfx::Path* path) const {
    DCHECK(path);

    SkScalar w = SkIntToScalar(width());

    // These values are defined by the shape of the new tab bitmap. Should that
    // bitmap ever change, these values will need to be updated. They're so
    // custom it's not really worth defining constants for.
    path->moveTo(0, 1);
    path->lineTo(w - 7, 1);
    path->lineTo(w - 4, 4);
    path->lineTo(w, 16);
    path->lineTo(w - 1, 17);
    path->lineTo(7, 17);
    path->lineTo(4, 13);
    path->lineTo(0, 1);
    path->close();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NewTabButton);
};

}  // namespace

// AnimationDelegate used when removing a tab. Does the necessary cleanup when
// done.
class TabStrip::RemoveTabDelegate
    : public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  RemoveTabDelegate(TabStrip* tab_strip, Tab* tab)
      : tabstrip_(tab_strip),
        tab_(tab) {
  }

  virtual void AnimationEnded(const Animation* animation) {
    CompleteRemove();
  }

  virtual void AnimationCanceled(const Animation* animation) {
    // We can be canceled for two interesting reasons:
    // . The tab we reference was dragged back into the tab strip. In this case
    //   we don't want to remove the tab (closing is false).
    // . The drag was completed before the animation completed
    //   (DestroyDraggedSourceTab). In this case we need to remove the tab
    //   (closing is true).
    if (tab_->closing())
      CompleteRemove();
  }

 private:
  void CompleteRemove() {
    if (!tab_->closing()) {
      // The tab was added back yet we weren't canceled. This shouldn't happen.
      NOTREACHED();
      return;
    }
    tabstrip_->RemoveAndDeleteTab(tab_);
    HighlightCloseButton();
  }

  // When the animation completes, we send the Container a message to simulate
  // a mouse moved event at the current mouse position. This tickles the Tab
  // the mouse is currently over to show the "hot" state of the close button.
  void HighlightCloseButton() {
    if (tabstrip_->available_width_for_tabs_ == -1 ||
        tabstrip_->IsDragSessionActive()) {
      // This function is not required (and indeed may crash!) for removes
      // spawned by non-mouse closes and drag-detaches.
      return;
    }

#if defined(OS_WIN)
    views::Widget* widget = tabstrip_->GetWidget();
    // This can be null during shutdown. See http://crbug.com/42737.
    if (!widget)
      return;
    // Force the close button (that slides under the mouse) to highlight by
    // saying the mouse just moved, but sending the same coordinates.
    DWORD pos = GetMessagePos();
    POINT cursor_point = {GET_X_LPARAM(pos), GET_Y_LPARAM(pos)};
    MapWindowPoints(NULL, widget->GetNativeView(), &cursor_point, 1);

    static_cast<views::WidgetWin*>(widget)->ResetLastMouseMoveFlag();
    // Return to message loop - otherwise we may disrupt some operation that's
    // in progress.
    SendMessage(widget->GetNativeView(), WM_MOUSEMOVE, 0,
                MAKELPARAM(cursor_point.x, cursor_point.y));
#else
    NOTIMPLEMENTED();
#endif
  }

  TabStrip* tabstrip_;
  Tab* tab_;

  DISALLOW_COPY_AND_ASSIGN(RemoveTabDelegate);
};

///////////////////////////////////////////////////////////////////////////////
// TabStrip, public:

// static
const int TabStrip::mini_to_non_mini_gap_ = 3;

// static
const int TabStrip::extra_gap_for_nano_ = 10;

TabStrip::TabStrip(TabStripController* controller)
    : BaseTabStrip(controller, BaseTabStrip::HORIZONTAL_TAB_STRIP),
      resize_layout_factory_(this),
      added_as_message_loop_observer_(false),
      needs_resize_layout_(false),
      current_unselected_width_(Tab::GetStandardSize().width()),
      current_selected_width_(Tab::GetStandardSize().width()),
      available_width_for_tabs_(-1),
      animation_container_(new AnimationContainer()),
      ALLOW_THIS_IN_INITIALIZER_LIST(bounds_animator_(this)),
      animation_type_(ANIMATION_DEFAULT),
      new_tab_button_enabled_(true),
      cancelling_animation_(false) {
  Init();
}

TabStrip::~TabStrip() {
  // The animations may reference the tabs. Shut down the animation before we
  // delete the tabs.
  StopAnimating(false);

  DestroyDragController();

  // Make sure we unhook ourselves as a message loop observer so that we don't
  // crash in the case where the user closes the window after closing a tab
  // but before moving the mouse.
  RemoveMessageLoopObserver();

  // The children (tabs) may callback to us from their destructor. Delete them
  // so that if they call back we aren't in a weird state.
  RemoveAllChildViews(true);
}

void TabStrip::InitTabStripButtons() {
  newtab_button_ = new NewTabButton(this);
  if (browser_defaults::kSizeTabButtonToTopOfTabStrip) {
    newtab_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                      views::ImageButton::ALIGN_BOTTOM);
  }
  LoadNewTabButtonImage();
  newtab_button_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_NEWTAB));
  AddChildView(newtab_button_);
}

gfx::Rect TabStrip::GetNewTabButtonBounds() {
  return newtab_button_->bounds();
}

////////////////////////////////////////////////////////////////////////////////
// TabStrip, BaseTabStrip implementation:

int TabStrip::GetPreferredHeight() {
  return GetPreferredSize().height();
}

void TabStrip::SetBackgroundOffset(const gfx::Point& offset) {
  for (int i = 0; i < tab_count(); ++i)
    GetTabAtTabDataIndex(i)->SetBackgroundOffset(offset);
}

bool TabStrip::IsPositionInWindowCaption(const gfx::Point& point) {
  views::View* v = GetViewForPoint(point);

  // If there is no control at this location, claim the hit was in the title
  // bar to get a move action.
  if (v == this)
    return true;

  // Check to see if the point is within the non-button parts of the new tab
  // button. The button has a non-rectangular shape, so if it's not in the
  // visual portions of the button we treat it as a click to the caption.
  gfx::Point point_in_newtab_coords(point);
  View::ConvertPointToView(this, newtab_button_, &point_in_newtab_coords);
  if (newtab_button_->bounds().Contains(point) &&
      !newtab_button_->HitTest(point_in_newtab_coords)) {
    return true;
  }

  // All other regions, including the new Tab button, should be considered part
  // of the containing Window's client area so that regular events can be
  // processed for them.
  return false;
}

void TabStrip::SetDraggedTabBounds(int tab_index, const gfx::Rect& tab_bounds) {
}

bool TabStrip::IsAnimating() const {
  return bounds_animator_.IsAnimating() || new_tab_timer_.IsRunning();
}

TabStrip* TabStrip::AsTabStrip() {
  return this;
}

void TabStrip::RemoveTabAt(int model_index, bool initiated_close) {
  if (initiated_close) {
    int model_count = GetModelCount();
    if (model_index != model_count && model_count > 0) {
      Tab* last_tab = GetTabAtModelIndex(model_count - 1);
      // Limit the width available to the TabStrip for laying out Tabs, so that
      // Tabs are not resized until a later time (when the mouse pointer leaves
      // the TabStrip).
      available_width_for_tabs_ = GetAvailableWidthForTabs(last_tab);
      needs_resize_layout_ = true;
      AddMessageLoopObserver();
    } else if (model_count) {
      Tab* last_tab = GetTabAtModelIndex(model_count);
      // Limit the width available to the TabStrip for laying out Tabs, so that
      // Tabs are not resized until a later time (when the mouse pointer leaves
      // the TabStrip).
      available_width_for_tabs_ = GetAvailableWidthForTabs(last_tab);
      needs_resize_layout_ = true;
      AddMessageLoopObserver();
    }
  }
  StartRemoveTabAnimation(model_index);
}

void TabStrip::SelectTabAt(int old_model_index, int new_model_index) {
  // We have "tiny tabs" if the tabs are so tiny that the unselected ones are
  // a different size to the selected ones.
  bool tiny_tabs = current_unselected_width_ != current_selected_width_;
  if (!IsAnimating() && (!needs_resize_layout_ || tiny_tabs)) {
    Layout();
  } else {
    SchedulePaint();
  }

  if (old_model_index >= 0) {
    GetTabAtTabDataIndex(ModelIndexToTabIndex(old_model_index))->
        StopMiniTabTitleAnimation();
  }
}

void TabStrip::TabTitleChangedNotLoading(int model_index) {
  Tab* tab = GetTabAtModelIndex(model_index);
  if (tab->data().mini && !tab->IsSelected())
    tab->StartMiniTabTitleAnimation();
}

void TabStrip::SetTabData(int model_index, const TabRendererData& data) {
  Tab* tab = GetTabAtModelIndex(model_index);
  bool mini_state_changed = tab->data().mini != data.mini;
  tab->SetData(data);
  tab->SchedulePaint();

  if (mini_state_changed) {
    if (GetWindow() && GetWindow()->IsVisible())
      StartMiniTabAnimation();
    else
      Layout();
  }
}

void TabStrip::StartHighlight(int model_index) {
  GetTabAtModelIndex(model_index)->StartPulse();
}

void TabStrip::StopAllHighlighting() {
  for (int i = 0; i < tab_count(); ++i)
    GetTabAtTabDataIndex(i)->StopPulse();
}

BaseTab* TabStrip::CreateTabForDragging() {
  Tab* tab = new Tab(NULL);
  // Make sure the dragged tab shares our theme provider. We need to explicitly
  // do this as during dragging there isn't a theme provider.
  tab->set_theme_provider(GetThemeProvider());
  return tab;
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, views::View overrides:

void TabStrip::PaintChildren(gfx::Canvas* canvas) {
  // Tabs are painted in reverse order, so they stack to the left.
  // Phantom tabs appear behind all other tabs and are rendered first. To make
  // them slightly transparent we render them to a different layer.
  if (HasPhantomTabs()) {
    SkRect bounds;
    bounds.set(0, 0, SkIntToScalar(width()), SkIntToScalar(height()));
    canvas->saveLayerAlpha(&bounds, kPhantomTabAlpha,
                           SkCanvas::kARGB_ClipLayer_SaveFlag);
    canvas->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
    for (int i = tab_count() - 1; i >= 0; --i) {
      Tab* tab = GetTabAtTabDataIndex(i);
      if (tab->data().phantom)
        tab->ProcessPaint(canvas);
    }
    canvas->restore();

    canvas->saveLayerAlpha(&bounds, kPhantomTabIconAlpha,
                           SkCanvas::kARGB_ClipLayer_SaveFlag);
    canvas->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
    for (int i = tab_count() - 1; i >= 0; --i) {
      Tab* tab = GetTabAtTabDataIndex(i);
      if (tab->data().phantom) {
        canvas->save();
        canvas->ClipRectInt(tab->MirroredX(), tab->y(), tab->width(),
                            tab->height());
        canvas->TranslateInt(tab->MirroredX(), tab->y());
        tab->PaintIcon(canvas);
        canvas->restore();
      }
    }
    canvas->restore();
  }

  Tab* selected_tab = NULL;

  Tab* dragging_tab = NULL;

  int model_count = GetModelCount();

  for (int i = tab_count() - 1; i >= 0; --i) {
    Tab* tab = GetTabAtTabDataIndex(i);
    // We must ask the _Tab's_ model, not ourselves, because in some situations
    // the model will be different to this object, e.g. when a Tab is being
    // removed after its TabContents has been destroyed.
    if (!tab->data().phantom) {
      if (tab->dragging()) {
        dragging_tab = tab;
      } else if (!tab->IsSelected()) {
        if (tab->render_unselected() && model_count > 1) {
          // See comment above kNetTabAnimationSelectedOffset as to why we do
          // this.
          Tab* last_tab = GetTabAtModelIndex(model_count - 2);
          canvas->save();
          int clip_x = last_tab->bounds().right() + kNetTabSelectedOffset;
          int clip_width = width() - clip_x;
          clip_x = MirroredXWithWidthInsideView(clip_x, clip_width);
          canvas->ClipRectInt(clip_x, 0, clip_width, height());
          tab->ProcessPaint(canvas);
          canvas->restore();
        } else {
          tab->ProcessPaint(canvas);
        }
      } else {
        selected_tab = tab;
      }
    }
  }

  if (GetWindow()->GetNonClientView()->UseNativeFrame()) {
    // Make sure unselected tabs are somewhat transparent.
    SkPaint paint;
    paint.setColor(SkColorSetARGB(200, 255, 255, 255));
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->FillRectInt(
        0, 0, width(),
        height() - 2,  // Visible region that overlaps the toolbar.
        paint);
  }

  if (animation_type_ == ANIMATION_NEW_TAB_3)
    newtab_button_->ProcessPaint(canvas);

  // Paint the selected tab last, so it overlaps all the others.
  if (selected_tab)
    selected_tab->ProcessPaint(canvas);

  // Paint the New Tab button.
  if (animation_type_ != ANIMATION_NEW_TAB_1 &&
      animation_type_ != ANIMATION_NEW_TAB_2 &&
      animation_type_ != ANIMATION_NEW_TAB_3) {
    newtab_button_->ProcessPaint(canvas);
  }

  // And the dragged tab.
  if (dragging_tab)
    dragging_tab->ProcessPaint(canvas);
}

// Overridden to support automation. See automation_proxy_uitest.cc.
views::View* TabStrip::GetViewByID(int view_id) const {
  if (tab_count() > 0) {
    if (view_id == VIEW_ID_TAB_LAST) {
      return GetTabAtTabDataIndex(tab_count() - 1);
    } else if ((view_id >= VIEW_ID_TAB_0) && (view_id < VIEW_ID_TAB_LAST)) {
      int index = view_id - VIEW_ID_TAB_0;
      if (index >= 0 && index < tab_count()) {
        return GetTabAtTabDataIndex(index);
      } else {
        return NULL;
      }
    }
  }

  return View::GetViewByID(view_id);
}

void TabStrip::Layout() {
  BaseTabStrip::Layout();

  if (new_tab_button_enabled_) {
    newtab_button_->SetBounds(newtab_button_bounds_);
    newtab_button_->SetVisible(true);
  } else {
    newtab_button_->SetVisible(false);
  }
}

gfx::Size TabStrip::GetPreferredSize() {
  return gfx::Size(0, Tab::GetMinimumUnselectedSize().height());
}

void TabStrip::OnDragEntered(const DropTargetEvent& event) {
  // Force animations to stop, otherwise it makes the index calculation tricky.
  StopAnimating(true);

  UpdateDropIndex(event);
}

int TabStrip::OnDragUpdated(const DropTargetEvent& event) {
  UpdateDropIndex(event);
  return GetDropEffect(event);
}

void TabStrip::OnDragExited() {
  SetDropIndex(-1, false);
}

int TabStrip::OnPerformDrop(const DropTargetEvent& event) {
  if (!drop_info_.get())
    return DragDropTypes::DRAG_NONE;

  const int drop_index = drop_info_->drop_index;
  const bool drop_before = drop_info_->drop_before;

  // Hide the drop indicator.
  SetDropIndex(-1, false);

  GURL url;
  std::wstring title;
  if (!event.GetData().GetURLAndTitle(&url, &title) || !url.is_valid())
    return DragDropTypes::DRAG_NONE;

  controller()->PerformDrop(drop_before, drop_index, url);

  return GetDropEffect(event);
}

bool TabStrip::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_PAGETABLIST;
  return true;
}

views::View* TabStrip::GetViewForPoint(const gfx::Point& point) {
  // Return any view that isn't a Tab or this TabStrip immediately. We don't
  // want to interfere.
  views::View* v = View::GetViewForPoint(point);
  if (v && v != this && v->GetClassName() != Tab::kViewClassName)
    return v;

  // The display order doesn't necessarily match the child list order, so we
  // walk the display list hit-testing Tabs. Since the selected tab always
  // renders on top of adjacent tabs, it needs to be hit-tested before any
  // left-adjacent Tab, so we look ahead for it as we walk.
  for (int i = 0; i < tab_count(); ++i) {
    Tab* next_tab = i < (tab_count() - 1) ? GetTabAtTabDataIndex(i + 1) : NULL;
    if (next_tab && next_tab->IsSelected() && IsPointInTab(next_tab, point))
      return next_tab;
    Tab* tab = GetTabAtTabDataIndex(i);
    if (IsPointInTab(tab, point))
      return tab;
  }

  // No need to do any floating view stuff, we don't use them in the TabStrip.
  return this;
}

void TabStrip::ThemeChanged() {
  LoadNewTabButtonImage();
}

void TabStrip::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  AnimationType last_type = animation_type_;

  ResetAnimationState(false);

  if (!cancelling_animation_ && last_type == ANIMATION_NEW_TAB_2)
    NewTabAnimation2Done();
}

BaseTab* TabStrip::CreateTab() {
  Tab* tab = new Tab(this);
  tab->set_animation_container(animation_container_.get());
  return tab;
}

void TabStrip::StartInsertTabAnimation(int model_index, bool foreground) {
  if (!IsDragSessionActive() && !attaching_dragged_tab() &&
      ShouldStartIntertTabAnimationAtEnd(model_index, foreground)) {
    StartInsertTabAnimationAtEnd();
  } else {
    StartInsertTabAnimationImpl(model_index);
  }
}

void TabStrip::StartMoveTabAnimation() {
  ResetAnimationState(true);

  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::StartedDraggingTab(BaseTab* tab) {
  tab->set_dragging(true);

  // Stop any animations on the tab.
  bounds_animator_.StopAnimatingView(tab);

  // Move the tab to its ideal bounds.
  GenerateIdealBounds();
  int tab_data_index = TabIndexOfTab(tab);
  DCHECK(tab_data_index != -1);
  tab->SetBounds(ideal_bounds(tab_data_index));
  SchedulePaint();
}

void TabStrip::StoppedDraggingTab(BaseTab* tab) {
  int tab_data_index = TabIndexOfTab(tab);
  if (tab_data_index == -1) {
    // The tab was removed before the drag completed. Don't do anything.
    return;
  }

  // Animate the view back to its correct position.
  ResetAnimationState(true);
  GenerateIdealBounds();
  AnimateToIdealBounds();
  bounds_animator_.AnimateViewTo(tab, ideal_bounds(TabIndexOfTab(tab)));

  // Install a delegate to reset the dragging state when done. We have to leave
  // dragging true for the tab otherwise it'll draw beneath the new tab button.
  bounds_animator_.SetAnimationDelegate(tab,
                                        new ResetDraggingStateDelegate(tab),
                                        true);
}

void TabStrip::ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) {
  if (is_add && child == this)
    InitTabStripButtons();
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, Tab::Delegate implementation:

bool TabStrip::IsTabSelected(const BaseTab* btr) const {
  const Tab* tab = static_cast<const Tab*>(btr);
  if (tab->closing() || tab->render_unselected())
    return false;

  return BaseTabStrip::IsTabSelected(btr);
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, views::BaseButton::ButtonListener implementation:

void TabStrip::ButtonPressed(views::Button* sender, const views::Event& event) {
  if (sender == newtab_button_)
    controller()->CreateNewTab();
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, MessageLoop::Observer implementation:

#if defined(OS_WIN)
void TabStrip::WillProcessMessage(const MSG& msg) {
}

void TabStrip::DidProcessMessage(const MSG& msg) {
  // We spy on three different Windows messages here to see if the mouse has
  // moved out of the bounds of the tabstrip, which we use as our cue to kick
  // of the resize animation. The messages are:
  //
  // WM_MOUSEMOVE:
  //   For when the mouse moves from the tabstrip over into the rest of the
  //   browser UI, i.e. within the bounds of the same windows HWND.
  // WM_MOUSELEAVE:
  //   For when the mouse moves very rapidly from a tab closed in the middle of
  //   the tabstrip (_not_ the end) out of the bounds of the browser's HWND and
  //   over some other HWND.
  // WM_NCMOUSELEAVE:
  //   For when the mouse moves very rapidly from the end of the tabstrip (when
  //   the last tab is closed and the mouse is left floating over the title
  //   bar). Because the empty area of the tabstrip at the end of the title bar
  //   is registered by the ChromeFrame as part of the "caption" area of the
  //   window (the frame's OnNCHitTest method returns HTCAPTION for this
  //   region), the frame's HWND receives a WM_MOUSEMOVE message immediately,
  //   because as far as it is concerned the mouse has _left_ the client area
  //   of the window (and is now over the non-client area). To be notified
  //   again when the mouse leaves the _non-client_ area, we use the
  //   WM_NCMOUSELEAVE message, which causes us to re-evaluate the cursor
  //   position and correctly resize the tabstrip.
  //
  switch (msg.message) {
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
      HandleGlobalMouseMoveEvent();
      break;
  }
}
#else
void TabStrip::WillProcessEvent(GdkEvent* event) {
}

void TabStrip::DidProcessEvent(GdkEvent* event) {
  switch (event->type) {
    case GDK_MOTION_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      HandleGlobalMouseMoveEvent();
      break;
    default:
      break;
  }
}
#endif

///////////////////////////////////////////////////////////////////////////////
// TabStrip, private:

void TabStrip::Init() {
  SetID(VIEW_ID_TAB_STRIP);
  newtab_button_bounds_.SetRect(0, 0, kNewTabButtonWidth, kNewTabButtonHeight);
  if (browser_defaults::kSizeTabButtonToTopOfTabStrip) {
    newtab_button_bounds_.set_height(
        kNewTabButtonHeight + kNewTabButtonVOffset);
  }
  if (drop_indicator_width == 0) {
    // Direction doesn't matter, both images are the same size.
    SkBitmap* drop_image = GetDropArrowImage(true);
    drop_indicator_width = drop_image->width();
    drop_indicator_height = drop_image->height();
  }
  bounds_animator_.set_observer(this);
}

void TabStrip::LoadNewTabButtonImage() {
  ThemeProvider* tp = GetThemeProvider();

  // If we don't have a theme provider yet, it means we do not have a
  // root view, and are therefore in a test.
  bool in_test = false;
  if (tp == NULL) {
    tp = new views::DefaultThemeProvider();
    in_test = true;
  }

  SkBitmap* bitmap = tp->GetBitmapNamed(IDR_NEWTAB_BUTTON);
  SkColor color = tp->GetColor(BrowserThemeProvider::COLOR_BUTTON_BACKGROUND);
  SkBitmap* background = tp->GetBitmapNamed(
      IDR_THEME_WINDOW_CONTROL_BACKGROUND);

  newtab_button_->SetImage(views::CustomButton::BS_NORMAL, bitmap);
  newtab_button_->SetImage(views::CustomButton::BS_PUSHED,
                           tp->GetBitmapNamed(IDR_NEWTAB_BUTTON_P));
  newtab_button_->SetImage(views::CustomButton::BS_HOT,
                           tp->GetBitmapNamed(IDR_NEWTAB_BUTTON_H));
  newtab_button_->SetBackground(color, background,
                                tp->GetBitmapNamed(IDR_NEWTAB_BUTTON_MASK));
  if (in_test)
    delete tp;
}

Tab* TabStrip::GetTabAtTabDataIndex(int tab_data_index) const {
  return static_cast<Tab*>(base_tab_at_tab_index(tab_data_index));
}

Tab* TabStrip::GetTabAtModelIndex(int model_index) const {
  return GetTabAtTabDataIndex(ModelIndexToTabIndex(model_index));
}

void TabStrip::GetCurrentTabWidths(double* unselected_width,
                                   double* selected_width) const {
  *unselected_width = current_unselected_width_;
  *selected_width = current_selected_width_;
}

void TabStrip::GetDesiredTabWidths(int tab_count,
                                   int mini_tab_count,
                                   int nano_tab_count,
                                   double* unselected_width,
                                   double* selected_width) const {
  DCHECK(tab_count >= 0 && mini_tab_count >= 0 && mini_tab_count <= tab_count);
  DCHECK(nano_tab_count >= 0 && nano_tab_count <= tab_count);
  const double min_unselected_width = Tab::GetMinimumUnselectedSize().width();
  const double min_selected_width = Tab::GetMinimumSelectedSize().width();

  *unselected_width = min_unselected_width;
  *selected_width = min_selected_width;

  if (tab_count == 0) {
    // Return immediately to avoid divide-by-zero below.
    return;
  }

  // Determine how much space we can actually allocate to tabs.
  int available_width;
  if (available_width_for_tabs_ < 0) {
    available_width = width();
    available_width -= (kNewTabButtonHOffset + newtab_button_bounds_.width());
  } else {
    // Interesting corner case: if |available_width_for_tabs_| > the result
    // of the calculation in the conditional arm above, the strip is in
    // overflow.  We can either use the specified width or the true available
    // width here; the first preserves the consistent "leave the last tab under
    // the user's mouse so they can close many tabs" behavior at the cost of
    // prolonging the glitchy appearance of the overflow state, while the second
    // gets us out of overflow as soon as possible but forces the user to move
    // their mouse for a few tabs' worth of closing.  We choose visual
    // imperfection over behavioral imperfection and select the first option.
    available_width = available_width_for_tabs_;
  }

  if (mini_tab_count > 0) {
    available_width -= mini_tab_count * (Tab::GetMiniWidth() + kTabHOffset);
    tab_count -= mini_tab_count;
    if (tab_count == 0) {
      *selected_width = *unselected_width = Tab::GetStandardSize().width();
      return;
    }
    // Account for gap between the last mini-tab and first non-mini-tab.
    available_width -= mini_to_non_mini_gap_;
    // And add some extra space if you have nano tabs in the mix.
    if (nano_tab_count > 0)
      available_width -= extra_gap_for_nano_;
  }

  // Calculate the desired tab widths by dividing the available space into equal
  // portions.  Don't let tabs get larger than the "standard width" or smaller
  // than the minimum width for each type, respectively.
  const int total_offset = kTabHOffset * (tab_count - 1);
  const double desired_tab_width = std::min((static_cast<double>(
      available_width - total_offset) / static_cast<double>(tab_count)),
      static_cast<double>(Tab::GetStandardSize().width()));
  *unselected_width = std::max(desired_tab_width, min_unselected_width);
  *selected_width = std::max(desired_tab_width, min_selected_width);

  // When there are multiple tabs, we'll have one selected and some unselected
  // tabs.  If the desired width was between the minimum sizes of these types,
  // try to shrink the tabs with the smaller minimum.  For example, if we have
  // a strip of width 10 with 4 tabs, the desired width per tab will be 2.5.  If
  // selected tabs have a minimum width of 4 and unselected tabs have a minimum
  // width of 1, the above code would set *unselected_width = 2.5,
  // *selected_width = 4, which results in a total width of 11.5.  Instead, we
  // want to set *unselected_width = 2, *selected_width = 4, for a total width
  // of 10.
  if (tab_count > 1) {
    if ((min_unselected_width < min_selected_width) &&
        (desired_tab_width < min_selected_width)) {
      // Unselected width = (total width - selected width) / (num_tabs - 1)
      *unselected_width = std::max(static_cast<double>(
          available_width - total_offset - min_selected_width) /
          static_cast<double>(tab_count - 1), min_unselected_width);
    } else if ((min_unselected_width > min_selected_width) &&
               (desired_tab_width < min_unselected_width)) {
      // Selected width = (total width - (unselected width * (num_tabs - 1)))
      *selected_width = std::max(available_width - total_offset -
          (min_unselected_width * (tab_count - 1)), min_selected_width);
    }
  }
}

void TabStrip::ResizeLayoutTabs() {
  // We've been called back after the TabStrip has been emptied out (probably
  // just prior to the window being destroyed). We need to do nothing here or
  // else GetTabAt below will crash.
  if (tab_count() == 0)
    return;

  resize_layout_factory_.RevokeAll();

  // It is critically important that this is unhooked here, otherwise we will
  // keep spying on messages forever.
  RemoveMessageLoopObserver();

  available_width_for_tabs_ = -1;
  int mini_tab_count = GetMiniTabCount();
  if (mini_tab_count == tab_count()) {
    // Only mini-tabs, we know the tab widths won't have changed (all
    // mini-tabs have the same width), so there is nothing to do.
    return;
  }
  Tab* first_tab  = GetTabAtTabDataIndex(mini_tab_count);
  double unselected, selected;
  GetDesiredTabWidths(tab_count(), mini_tab_count, GetNanoTabCount(),
                      &unselected, &selected);
  int w = Round(first_tab->IsSelected() ? selected : selected);

  // We only want to run the animation if we're not already at the desired
  // size.
  if (abs(first_tab->width() - w) > 1)
    StartResizeLayoutAnimation();
}

bool TabStrip::IsCursorInTabStripZone() const {
  gfx::Rect bounds = GetLocalBounds(true);
  gfx::Point tabstrip_topleft(bounds.origin());
  View::ConvertPointToScreen(this, &tabstrip_topleft);
  bounds.set_origin(tabstrip_topleft);
  bounds.set_height(bounds.height() + kTabStripAnimationVSlop);

#if defined(OS_WIN)
  DWORD pos = GetMessagePos();
  gfx::Point cursor_point(pos);
#elif defined(OS_LINUX)
  // TODO(sky): make sure this is right with multiple monitors.
  GdkScreen* screen = gdk_screen_get_default();
  GdkDisplay* display = gdk_screen_get_display(screen);
  gint x, y;
  gdk_display_get_pointer(display, NULL, &x, &y, NULL);
  gfx::Point cursor_point(x, y);
#endif

  return bounds.Contains(cursor_point.x(), cursor_point.y());
}

void TabStrip::AddMessageLoopObserver() {
  if (!added_as_message_loop_observer_) {
    MessageLoopForUI::current()->AddObserver(this);
    added_as_message_loop_observer_ = true;
  }
}

void TabStrip::RemoveMessageLoopObserver() {
  if (added_as_message_loop_observer_) {
    MessageLoopForUI::current()->RemoveObserver(this);
    added_as_message_loop_observer_ = false;
  }
}

gfx::Rect TabStrip::GetDropBounds(int drop_index,
                                  bool drop_before,
                                  bool* is_beneath) {
  DCHECK(drop_index != -1);
  int center_x;
  if (drop_index < tab_count()) {
    Tab* tab = GetTabAtTabDataIndex(drop_index);
    if (drop_before)
      center_x = tab->x() - (kTabHOffset / 2);
    else
      center_x = tab->x() + (tab->width() / 2);
  } else {
    Tab* last_tab = GetTabAtTabDataIndex(drop_index - 1);
    center_x = last_tab->x() + last_tab->width() + (kTabHOffset / 2);
  }

  // Mirror the center point if necessary.
  center_x = MirroredXCoordinateInsideView(center_x);

  // Determine the screen bounds.
  gfx::Point drop_loc(center_x - drop_indicator_width / 2,
                      -drop_indicator_height);
  ConvertPointToScreen(this, &drop_loc);
  gfx::Rect drop_bounds(drop_loc.x(), drop_loc.y(), drop_indicator_width,
                        drop_indicator_height);

  // If the rect doesn't fit on the monitor, push the arrow to the bottom.
#if defined(OS_WIN)
  gfx::Rect monitor_bounds = win_util::GetMonitorBoundsForRect(drop_bounds);
  *is_beneath = (monitor_bounds.IsEmpty() ||
                 !monitor_bounds.Contains(drop_bounds));
#else
  *is_beneath = false;
  NOTIMPLEMENTED();
#endif
  if (*is_beneath)
    drop_bounds.Offset(0, drop_bounds.height() + height());

  return drop_bounds;
}

void TabStrip::UpdateDropIndex(const DropTargetEvent& event) {
  // If the UI layout is right-to-left, we need to mirror the mouse
  // coordinates since we calculate the drop index based on the
  // original (and therefore non-mirrored) positions of the tabs.
  const int x = MirroredXCoordinateInsideView(event.x());
  // We don't allow replacing the urls of mini-tabs.
  for (int i = GetMiniTabCount(); i < tab_count(); ++i) {
    Tab* tab = GetTabAtTabDataIndex(i);
    const int tab_max_x = tab->x() + tab->width();
    const int hot_width = tab->width() / 3;
    if (x < tab_max_x) {
      if (x < tab->x() + hot_width)
        SetDropIndex(i, true);
      else if (x >= tab_max_x - hot_width)
        SetDropIndex(i + 1, true);
      else
        SetDropIndex(i, false);
      return;
    }
  }

  // The drop isn't over a tab, add it to the end.
  SetDropIndex(tab_count(), true);
}

void TabStrip::SetDropIndex(int tab_data_index, bool drop_before) {
  if (tab_data_index == -1) {
    if (drop_info_.get())
      drop_info_.reset(NULL);
    return;
  }

  if (drop_info_.get() && drop_info_->drop_index == tab_data_index &&
      drop_info_->drop_before == drop_before) {
    return;
  }

  bool is_beneath;
  gfx::Rect drop_bounds = GetDropBounds(tab_data_index, drop_before,
                                        &is_beneath);

  if (!drop_info_.get()) {
    drop_info_.reset(new DropInfo(tab_data_index, drop_before, !is_beneath));
  } else {
    drop_info_->drop_index = tab_data_index;
    drop_info_->drop_before = drop_before;
    if (is_beneath == drop_info_->point_down) {
      drop_info_->point_down = !is_beneath;
      drop_info_->arrow_view->SetImage(
          GetDropArrowImage(drop_info_->point_down));
    }
  }

  // Reposition the window. Need to show it too as the window is initially
  // hidden.

#if defined(OS_WIN)
  drop_info_->arrow_window->SetWindowPos(
      HWND_TOPMOST, drop_bounds.x(), drop_bounds.y(), drop_bounds.width(),
      drop_bounds.height(), SWP_NOACTIVATE | SWP_SHOWWINDOW);
#else
  drop_info_->arrow_window->SetBounds(drop_bounds);
  drop_info_->arrow_window->Show();
#endif
}

int TabStrip::GetDropEffect(const views::DropTargetEvent& event) {
  const int source_ops = event.GetSourceOperations();
  if (source_ops & DragDropTypes::DRAG_COPY)
    return DragDropTypes::DRAG_COPY;
  if (source_ops & DragDropTypes::DRAG_LINK)
    return DragDropTypes::DRAG_LINK;
  return DragDropTypes::DRAG_MOVE;
}

// static
SkBitmap* TabStrip::GetDropArrowImage(bool is_down) {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      is_down ? IDR_TAB_DROP_DOWN : IDR_TAB_DROP_UP);
}

// TabStrip::DropInfo ----------------------------------------------------------

TabStrip::DropInfo::DropInfo(int drop_index, bool drop_before, bool point_down)
    : drop_index(drop_index),
      drop_before(drop_before),
      point_down(point_down) {
  arrow_view = new views::ImageView;
  arrow_view->SetImage(GetDropArrowImage(point_down));

#if defined(OS_WIN)
  arrow_window = new views::WidgetWin;
  arrow_window->set_window_style(WS_POPUP);
  arrow_window->set_window_ex_style(WS_EX_TOPMOST | WS_EX_NOACTIVATE |
                                    WS_EX_LAYERED | WS_EX_TRANSPARENT);
  arrow_window->Init(
      NULL,
      gfx::Rect(0, 0, drop_indicator_width, drop_indicator_height));
#else
  arrow_window = new views::WidgetGtk(views::WidgetGtk::TYPE_POPUP);
  arrow_window->MakeTransparent();
  arrow_window->Init(
      NULL,
      gfx::Rect(0, 0, drop_indicator_width, drop_indicator_height));
#endif
  arrow_window->SetContentsView(arrow_view);
}

TabStrip::DropInfo::~DropInfo() {
  // Close eventually deletes the window, which deletes arrow_view too.
  arrow_window->Close();
}

///////////////////////////////////////////////////////////////////////////////

// Called from:
// - BasicLayout
// - Tab insertion/removal
// - Tab reorder
void TabStrip::GenerateIdealBounds() {
  int non_closing_tab_count = 0;
  int mini_tab_count = 0;
  int nano_tab_count = 0;
  for (int i = 0; i < tab_count(); ++i) {
    BaseTab* tab = base_tab_at_tab_index(i);
    if (!tab->closing()) {
      ++non_closing_tab_count;
      if (tab->data().mini)
        mini_tab_count++;
      if (tab->data().app)
        nano_tab_count++;
    }
  }

  double unselected, selected;
  GetDesiredTabWidths(non_closing_tab_count, mini_tab_count, nano_tab_count,
                      &unselected, &selected);

  current_unselected_width_ = unselected;
  current_selected_width_ = selected;

  // NOTE: This currently assumes a tab's height doesn't differ based on
  // selected state or the number of tabs in the strip!
  int tab_height = Tab::GetStandardSize().height();
  double tab_x = 0;
  bool last_was_mini = false;
  for (int i = 0; i < tab_count(); ++i) {
      Tab* tab = GetTabAtTabDataIndex(i);
    if (!tab->closing()) {
      double tab_width = unselected;
      if (tab->data().mini) {
        tab_width = Tab::GetMiniWidth();
      } else {
        if (last_was_mini) {
          // Give a bigger gap between mini and non-mini tabs.
          tab_x += mini_to_non_mini_gap_;
          if (nano_tab_count > 0)
            tab_x += extra_gap_for_nano_;
        }
        if (tab->IsSelected())
          tab_width = selected;
      }
      double end_of_tab = tab_x + tab_width;
      int rounded_tab_x = Round(tab_x);
      set_ideal_bounds(i,
          gfx::Rect(rounded_tab_x, 0, Round(end_of_tab) - rounded_tab_x,
                    tab_height));
      tab_x = end_of_tab + kTabHOffset;
      last_was_mini = tab->data().mini;
    }
  }

  // Update bounds of new tab button.
  int new_tab_x;
  int new_tab_y = browser_defaults::kSizeTabButtonToTopOfTabStrip ?
      0 : kNewTabButtonVOffset;
  if (abs(Round(unselected) - Tab::GetStandardSize().width()) > 1 &&
      available_width_for_tabs_ == -1) {
    // We're shrinking tabs, so we need to anchor the New Tab button to the
    // right edge of the TabStrip's bounds, rather than the right edge of the
    // right-most Tab, otherwise it'll bounce when animating.
    new_tab_x = width() - newtab_button_bounds_.width();
  } else {
    new_tab_x = Round(tab_x - kTabHOffset) + kNewTabButtonHOffset;
  }
  newtab_button_bounds_.set_origin(gfx::Point(new_tab_x, new_tab_y));
}

void TabStrip::NewTabAnimation1Done() {
  int tab_data_index = tab_count() - 1;
  Tab* tab = GetTabAtTabDataIndex(tab_data_index);

  gfx::Rect old_tab_bounds = tab->bounds();

  GenerateIdealBounds();

  gfx::Rect end_bounds = ideal_bounds(tab_data_index);
  end_bounds.Offset(kNewTabOvershoot, 0);
  set_ideal_bounds(tab_data_index, end_bounds);

  int x = old_tab_bounds.right() - end_bounds.width();
  int w = end_bounds.width();
  if (x < 0) {
    w += x;
    x = 0;
  }
  tab->SetBounds(x, old_tab_bounds.y(), w, end_bounds.height());

  AnimateToIdealBounds();

  animation_type_ = ANIMATION_NEW_TAB_2;
  tab->set_render_as_new_tab(false);
  tab->set_render_unselected(true);
  tab->set_alpha(0);

  // BoundsAnimator takes ownership of NewTabAlphaDelegate.
  bounds_animator_.SetAnimationDelegate(tab, new NewTabAlphaDelegate(tab),
                                        true);
}

void TabStrip::NewTabAnimation2Done() {
  animation_type_ = ANIMATION_NEW_TAB_3;

  GenerateIdealBounds();

  AnimateToIdealBounds();

  SlideAnimation* animation = new SlideAnimation(NULL);
  animation->SetSlideDuration(kNewTab3DurationMs);
  animation->SetTweenType(Tween::EASE_IN_OUT);

  // BoundsAnimator takes ownership of animation.
  bounds_animator_.SetAnimationForView(
      GetTabAtTabDataIndex(tab_count() - 1), animation);
}

void TabStrip::AnimateToIdealBounds() {
  for (int i = 0; i < tab_count(); ++i) {
    Tab* tab = GetTabAtTabDataIndex(i);
    if (!tab->closing() && !tab->dragging())
      bounds_animator_.AnimateViewTo(tab, ideal_bounds(i));
  }

  if (animation_type_ != ANIMATION_NEW_TAB_3) {
    bounds_animator_.AnimateViewTo(newtab_button_, newtab_button_bounds_);
  }
}

bool TabStrip::ShouldStartIntertTabAnimationAtEnd(int model_index,
                                                  bool foreground) {
  return foreground && (model_index + 1 == GetModelCount()) &&
      controller()->IsNewTabPage(model_index);
}

void TabStrip::StartResizeLayoutAnimation() {
  ResetAnimationState(true);
  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::StartInsertTabAnimationAtEnd() {
  ResetAnimationState(true);

  // The TabStrip can now use its entire width to lay out Tabs.
  available_width_for_tabs_ = -1;

  animation_type_ = ANIMATION_NEW_TAB_1;

  GenerateIdealBounds();

  int tab_data_index = ModelIndexToTabIndex(GetModelCount() - 1);
  Tab* tab = GetTabAtTabDataIndex(tab_data_index);
  tab->SizeToNewTabButtonImages();
  tab->SetBounds(newtab_button_->x() +
                     (newtab_button_->width() - tab->width()) / 2,
                 ideal_bounds(tab_data_index).y(),
                 tab->width(), tab->height());
  tab->set_render_as_new_tab(true);

  new_tab_timer_.Start(base::TimeDelta::FromMilliseconds(kNewTabDurationMs),
                       this, &TabStrip::NewTabAnimation1Done);
}

void TabStrip::StartInsertTabAnimationImpl(int model_index) {
  ResetAnimationState(true);

  // The TabStrip can now use its entire width to lay out Tabs.
  available_width_for_tabs_ = -1;

  GenerateIdealBounds();

  int tab_data_index = ModelIndexToTabIndex(model_index);
  BaseTab* tab = base_tab_at_tab_index(tab_data_index);
  if (model_index == 0) {
    tab->SetBounds(0, ideal_bounds(tab_data_index).y(), 0,
                   ideal_bounds(tab_data_index).height());
  } else {
    BaseTab* last_tab = base_tab_at_tab_index(tab_data_index - 1);
    tab->SetBounds(last_tab->bounds().right() + kTabHOffset,
                   ideal_bounds(tab_data_index).y(), 0,
                   ideal_bounds(tab_data_index).height());
  }

  AnimateToIdealBounds();
}

void TabStrip::StartRemoveTabAnimation(int model_index) {
  ResetAnimationState(true);

  // Mark the tab as closing.
  Tab* tab = GetTabAtModelIndex(model_index);
  tab->set_closing(true);

  // Start an animation for the tabs.
  GenerateIdealBounds();
  AnimateToIdealBounds();

  // Animate the tab being closed to 0x0.
  gfx::Rect tab_bounds = tab->bounds();
  tab_bounds.set_width(0);
  bounds_animator_.AnimateViewTo(tab, tab_bounds);

  // Register delegate to do cleanup when done, BoundsAnimator takes
  // ownership of RemoveTabDelegate.
  bounds_animator_.SetAnimationDelegate(tab, new RemoveTabDelegate(this, tab),
                                        true);
}

void TabStrip::StartMiniTabAnimation() {
  ResetAnimationState(true);

  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::StopAnimating(bool layout) {
  if (!IsAnimating())
    return;

  new_tab_timer_.Stop();

  if (bounds_animator_.IsAnimating()) {
    // Cancelling the animation triggers OnBoundsAnimatorDone, which invokes
    // ResetAnimationState.
    cancelling_animation_ = true;
    bounds_animator_.Cancel();
    cancelling_animation_ = false;
  } else {
    ResetAnimationState(false);
  }

  DCHECK(!IsAnimating());

  if (layout)
    Layout();
}

void TabStrip::ResetAnimationState(bool stop_new_tab_timer) {
  if (animation_type_ == ANIMATION_NEW_TAB_2)
    newtab_button_->SchedulePaint();

  if (stop_new_tab_timer)
    new_tab_timer_.Stop();

  animation_type_ = ANIMATION_DEFAULT;

  // Reset the animation state of each tab.
  for (int i = 0; i < tab_count(); ++i) {
    Tab* tab = GetTabAtTabDataIndex(i);
    tab->set_render_as_new_tab(false);
    tab->set_render_unselected(false);
    tab->set_alpha(1);
  }
}

int TabStrip::GetMiniTabCount() const {
  int mini_count = 0;
  for (int i = 0; i < tab_count(); ++i) {
    if (base_tab_at_tab_index(i)->data().mini)
      mini_count++;
    else
      return mini_count;
  }
  return mini_count;
}

int TabStrip::GetNanoTabCount() const {
  int nano_count = 0;
  for (int i = 0; i < tab_count(); ++i) {
    if (base_tab_at_tab_index(i)->data().app)
      nano_count++;
    else
      return nano_count;
  }
  return nano_count;
}

int TabStrip::GetAvailableWidthForTabs(Tab* last_tab) const {
  return last_tab->x() + last_tab->width();
}

bool TabStrip::IsPointInTab(Tab* tab,
                            const gfx::Point& point_in_tabstrip_coords) {
  gfx::Point point_in_tab_coords(point_in_tabstrip_coords);
  View::ConvertPointToView(this, tab, &point_in_tab_coords);
  return tab->HitTest(point_in_tab_coords);
}

void TabStrip::HandleGlobalMouseMoveEvent() {
  if (!IsCursorInTabStripZone()) {
    // Mouse moved outside the tab slop zone, start a timer to do a resize
    // layout after a short while...
    if (resize_layout_factory_.empty()) {
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
          resize_layout_factory_.NewRunnableMethod(
              &TabStrip::ResizeLayoutTabs),
          kResizeTabsTimeMs);
    }
  } else {
    // Mouse moved quickly out of the tab strip and then into it again, so
    // cancel the timer so that the strip doesn't move when the mouse moves
    // back over it.
    resize_layout_factory_.RevokeAll();
  }
}

bool TabStrip::HasPhantomTabs() const {
  for (int i = 0; i < tab_count(); ++i) {
    if (GetTabAtTabDataIndex(i)->data().phantom)
      return true;
  }
  return false;
}
