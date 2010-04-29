// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/toolbar_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/bookmark_menu_button.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "gfx/canvas.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/button_dropdown.h"
#include "views/focus/view_storage.h"
#include "views/widget/tooltip_manager.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"

static const int kControlHorizOffset = 4;
static const int kControlVertOffset = 6;
static const int kControlIndent = 3;
static const int kStatusBubbleWidth = 480;

// Separation between the location bar and the menus.
static const int kMenuButtonOffset = 3;

// Padding to the right of the location bar
static const int kPaddingRight = 2;

static const int kPopupTopSpacingNonGlass = 3;
static const int kPopupBottomSpacingNonGlass = 2;
static const int kPopupBottomSpacingGlass = 1;

static SkBitmap* kPopupBackgroundEdge = NULL;

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, public:

ToolbarView::ToolbarView(Browser* browser)
    : model_(browser->toolbar_model()),
      back_(NULL),
      forward_(NULL),
      home_(NULL),
      reload_(NULL),
      location_bar_(NULL),
      go_(NULL),
      browser_actions_(NULL),
      page_menu_(NULL),
      app_menu_(NULL),
      bookmark_menu_(NULL),
      profile_(NULL),
      browser_(browser),
      profiles_menu_contents_(NULL),
      last_focused_view_storage_id_(-1),
      menu_bar_emulation_mode_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      destroyed_flag_(NULL) {
  SetID(VIEW_ID_TOOLBAR);
  browser_->command_updater()->AddCommandObserver(IDC_BACK, this);
  browser_->command_updater()->AddCommandObserver(IDC_FORWARD, this);
  browser_->command_updater()->AddCommandObserver(IDC_HOME, this);
  browser_->command_updater()->AddCommandObserver(IDC_RELOAD, this);

  if (browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP))
    display_mode_ = DISPLAYMODE_NORMAL;
  else
    display_mode_ = DISPLAYMODE_LOCATION;

  if (!kPopupBackgroundEdge) {
    kPopupBackgroundEdge = ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_LOCATIONBG_POPUPMODE_EDGE);
  }
}

ToolbarView::~ToolbarView() {
  if (destroyed_flag_)
    *destroyed_flag_ = true;
  if (menu_bar_emulation_mode_) {
    focus_manager_->UnregisterAccelerators(this);
    focus_manager_->RemoveFocusChangeListener(this);
  }
}

void ToolbarView::Init(Profile* profile) {
  back_menu_model_.reset(new BackForwardMenuModel(
      browser_, BackForwardMenuModel::BACKWARD_MENU));
  forward_menu_model_.reset(new BackForwardMenuModel(
      browser_, BackForwardMenuModel::FORWARD_MENU));

  // Create all the individual Views in the Toolbar.
  CreateLeftSideControls();
  CreateCenterStack(profile);
  CreateRightSideControls(profile);

  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);

  SetProfile(profile);
  if (!app_menu_model_.get()) {
    SetAppMenuModel(new AppMenuModel(this, browser_));
  }

  focus_manager_ = GetFocusManager();
}

void ToolbarView::SetProfile(Profile* profile) {
  if (profile == profile_)
    return;

  profile_ = profile;
  location_bar_->SetProfile(profile);
}

void ToolbarView::Update(TabContents* tab, bool should_restore_state) {
  if (location_bar_)
    location_bar_->Update(should_restore_state ? tab : NULL);

  if (browser_actions_)
    browser_actions_->RefreshBrowserActionViews();
}

void ToolbarView::SetAppMenuModel(AppMenuModel* model) {
  app_menu_model_.reset(model);
  app_menu_menu_.reset(new views::Menu2(app_menu_model_.get()));
}

void ToolbarView::EnterMenuBarEmulationMode(int last_focused_view_storage_id,
                                            views::MenuButton* menu_to_focus) {
  last_focused_view_storage_id_ = last_focused_view_storage_id;
  if (!menu_to_focus)
    menu_to_focus = page_menu_;

  // If we're already in the menu bar emulation mode, just set the focus.
  if (menu_bar_emulation_mode_) {
    menu_to_focus->RequestFocus();
    return;
  }

  // Make the menus focusable and set focus to the initial menu.
  menu_bar_emulation_mode_ = true;
  page_menu_->SetFocusable(true);
  app_menu_->SetFocusable(true);
  menu_to_focus->RequestFocus();

  // Listen so we know when focus has moved to something other than one
  // of these menus.
  focus_manager_->AddFocusChangeListener(this);

  // Add accelerators so that the usual keys used to interact with a
  // menu bar work as expected.
  views::Accelerator return_key(base::VKEY_RETURN, false, false, false);
  focus_manager_->RegisterAccelerator(return_key, this);
  views::Accelerator space(base::VKEY_SPACE, false, false, false);
  focus_manager_->RegisterAccelerator(space, this);
  views::Accelerator escape(base::VKEY_ESCAPE, false, false, false);
  focus_manager_->RegisterAccelerator(escape, this);
  views::Accelerator down(base::VKEY_DOWN, false, false, false);
  focus_manager_->RegisterAccelerator(down, this);
  views::Accelerator up(base::VKEY_UP, false, false, false);
  focus_manager_->RegisterAccelerator(up, this);
  views::Accelerator left(base::VKEY_LEFT, false, false, false);
  focus_manager_->RegisterAccelerator(left, this);
  views::Accelerator right(base::VKEY_RIGHT, false, false, false);
  focus_manager_->RegisterAccelerator(right, this);
}

void ToolbarView::AddMenuListener(views::MenuListener* listener) {
  menu_listeners_.push_back(listener);
}

void ToolbarView::RemoveMenuListener(views::MenuListener* listener) {
  for (std::vector<views::MenuListener*>::iterator iter =
      menu_listeners_.begin();
    iter != menu_listeners_.end();
    ++iter) {
      if (*iter == listener) {
        menu_listeners_.erase(iter);
        return;
      }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, FocusChangeListener overrides:

void ToolbarView::FocusWillChange(views::View* focused_before,
                                  views::View* focused_now) {
  // If the focus is switching to something outside the menu bar,
  // take it out of the focus traversal.
  if (focused_now != NULL &&
      focused_now != page_menu_ &&
      focused_now != app_menu_) {
    // Post ExitMenuBarEmulationMode to the queue rather than running it
    // right away, because otherwise we'll remove ourselves from the
    // list of listeners while FocusManager is in the middle of iterating
    // over that list.
    MessageLoop::current()->PostTask(
        FROM_HERE, method_factory_.NewRunnableMethod(
            &ToolbarView::ExitMenuBarEmulationMode));
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, AccessibleToolbarView overrides:

bool ToolbarView::IsAccessibleViewTraversable(views::View* view) {
  return view != location_bar_;
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, Menu::BaseControllerDelegate overrides:

bool ToolbarView::GetAcceleratorInfo(int id, menus::Accelerator* accel) {
  return GetWidget()->GetAccelerator(id, accel);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::MenuDelegate implementation:

void ToolbarView::RunMenu(views::View* source, const gfx::Point& pt) {
  switch (source->GetID()) {
    case VIEW_ID_PAGE_MENU:
      RunPageMenu(pt);
      break;
    case VIEW_ID_APP_MENU:
      RunAppMenu(pt);
      break;
    default:
      NOTREACHED() << "Invalid source menu.";
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, LocationBarView::Delegate implementation:

TabContents* ToolbarView::GetTabContents() {
  return browser_->GetSelectedTabContents();
}

void ToolbarView::OnInputInProgress(bool in_progress) {
  // The edit should make sure we're only notified when something changes.
  DCHECK(model_->input_in_progress() != in_progress);

  model_->set_input_in_progress(in_progress);
  location_bar_->Update(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, CommandUpdater::CommandObserver implementation:

void ToolbarView::EnabledStateChangedForCommand(int id, bool enabled) {
  views::Button* button = NULL;
  switch (id) {
    case IDC_BACK:
      button = back_;
      break;
    case IDC_FORWARD:
      button = forward_;
      break;
    case IDC_HOME:
      button = home_;
      break;
    case IDC_RELOAD:
      button = reload_;
      break;
  }
  if (button)
    button->SetEnabled(enabled);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::Button::ButtonListener implementation:

void ToolbarView::ButtonPressed(views::Button* sender,
                                const views::Event& event) {
  int id = sender->tag();
  switch (id) {
    case IDC_BACK:
    case IDC_FORWARD:
    case IDC_RELOAD:
      // Forcibly reset the location bar, since otherwise it won't discard any
      // ongoing user edits, since it doesn't realize this is a user-initiated
      // action.
      location_bar_->Revert();
      // Shift-clicking or Ctrl-clicking the reload button means we should
      // ignore any cached content.
      // TODO(avayvod): eliminate duplication of this logic in
      // CompactLocationBarView.
      if (id == IDC_RELOAD && (event.IsShiftDown() || event.IsControlDown()))
        id = IDC_RELOAD_IGNORING_CACHE;
      break;
  }
  browser_->ExecuteCommandWithDisposition(
      id, event_utils::DispositionFromEventFlags(sender->mouse_event_flags()));
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, NotificationObserver implementation:

void ToolbarView::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    std::wstring* pref_name = Details<std::wstring>(details).ptr();
    if (*pref_name == prefs::kShowHomeButton) {
      Layout();
      SchedulePaint();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, menus::SimpleMenuModel::Delegate implementation:

bool ToolbarView::IsCommandIdChecked(int command_id) const {
  if (command_id == IDC_SHOW_BOOKMARK_BAR)
    return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
  return false;
}

bool ToolbarView::IsCommandIdEnabled(int command_id) const {
  return browser_->command_updater()->IsCommandEnabled(command_id);
}

bool ToolbarView::GetAcceleratorForCommandId(int command_id,
    menus::Accelerator* accelerator) {
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere so we need to check for them explicitly here.
  // TODO(cpu) Bug 1109102. Query WebKit land for the actual bindings.
  switch (command_id) {
    case IDC_CUT:
      *accelerator = views::Accelerator(base::VKEY_X, false, true, false);
      return true;
    case IDC_COPY:
      *accelerator = views::Accelerator(base::VKEY_C, false, true, false);
      return true;
    case IDC_PASTE:
      *accelerator = views::Accelerator(base::VKEY_V, false, true, false);
      return true;
  }
  // Else, we retrieve the accelerator information from the frame.
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

void ToolbarView::ExecuteCommand(int command_id) {
  browser_->ExecuteCommand(command_id);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::View overrides:

bool ToolbarView::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  // The only accelerators we handle here are if the menus are focused.
  views::View* focused_view = GetFocusManager()->GetFocusedView();
  if (focused_view != page_menu_ && focused_view != app_menu_) {
    ExitMenuBarEmulationMode();
    return false;
  }

  // Safe to cast, given the check above.
  views::MenuButton* menu = static_cast<views::MenuButton*>(focused_view);
  switch (accelerator.GetKeyCode()) {
    case base::VKEY_ESCAPE:
      RestoreLastFocusedView();
      return true;
    case base::VKEY_LEFT:
    case base::VKEY_RIGHT:
      if (menu == app_menu_)
        page_menu_->RequestFocus();
      else
        app_menu_->RequestFocus();
      return true;
    case base::VKEY_UP:
    case base::VKEY_DOWN:
    case base::VKEY_RETURN:
    case base::VKEY_SPACE:
      // Hide the tooltip before activating a menu button.
      if (GetWidget()->GetTooltipManager())
        GetWidget()->GetTooltipManager()->HideKeyboardTooltip();

      ActivateMenuButton(menu);
      return true;
    default:
      return false;
  }
}

gfx::Size ToolbarView::GetPreferredSize() {
  if (IsDisplayModeNormal()) {
    int min_width = kControlIndent + back_->GetPreferredSize().width() +
        forward_->GetPreferredSize().width() + kControlHorizOffset +
        (show_home_button_.GetValue() ?
            (home_->GetPreferredSize().width() + kControlHorizOffset) : 0) +
        reload_->GetPreferredSize().width() +
        browser_actions_->GetPreferredSize().width() +
        go_->GetPreferredSize().width() +
        kMenuButtonOffset +
        (bookmark_menu_ ? bookmark_menu_->GetPreferredSize().width() : 0) +
        page_menu_->GetPreferredSize().width() +
        app_menu_->GetPreferredSize().width() + kPaddingRight;

    static SkBitmap normal_background;
    if (normal_background.isNull()) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      normal_background = *rb.GetBitmapNamed(IDR_CONTENT_TOP_CENTER);
    }

    return gfx::Size(min_width, normal_background.height());
  }

  int vertical_spacing = PopupTopSpacing() +
      (GetWindow()->GetNonClientView()->UseNativeFrame() ?
          kPopupBottomSpacingGlass : kPopupBottomSpacingNonGlass);
  return gfx::Size(0, location_bar_->GetPreferredSize().height() +
      vertical_spacing);
}

void ToolbarView::Layout() {
  // If we have not been initialized yet just do nothing.
  if (back_ == NULL)
    return;

  if (!IsDisplayModeNormal()) {
    int edge_width = (browser_->window() && browser_->window()->IsMaximized()) ?
        0 : kPopupBackgroundEdge->width();  // See Paint().
    location_bar_->SetBounds(edge_width, PopupTopSpacing(),
        width() - (edge_width * 2), location_bar_->GetPreferredSize().height());
    return;
  }

  int child_y = std::min(kControlVertOffset, height());
  // We assume all child elements are the same height.
  int child_height =
      std::min(go_->GetPreferredSize().height(), height() - child_y);

  // If the window is maximized, we extend the back button to the left so that
  // clicking on the left-most pixel will activate the back button.
  // TODO(abarth):  If the window becomes maximized but is not resized,
  //                then Layout() might not be called and the back button
  //                will be slightly the wrong size.  We should force a
  //                Layout() in this case.
  //                http://crbug.com/5540
  int back_width = back_->GetPreferredSize().width();
  if (browser_->window() && browser_->window()->IsMaximized())
    back_->SetBounds(0, child_y, back_width + kControlIndent, child_height);
  else
    back_->SetBounds(kControlIndent, child_y, back_width, child_height);

  forward_->SetBounds(back_->x() + back_->width(), child_y,
                      forward_->GetPreferredSize().width(), child_height);

  if (show_home_button_.GetValue()) {
    home_->SetVisible(true);
    home_->SetBounds(forward_->x() + forward_->width() + kControlHorizOffset,
                     child_y, home_->GetPreferredSize().width(), child_height);
  } else {
    home_->SetVisible(false);
    home_->SetBounds(forward_->x() + forward_->width(), child_y, 0,
                     child_height);
  }

  reload_->SetBounds(home_->x() + home_->width() + kControlHorizOffset, child_y,
                     reload_->GetPreferredSize().width(), child_height);

  int go_button_width = go_->GetPreferredSize().width();
  int browser_actions_width = browser_actions_->GetPreferredSize().width();
  int page_menu_width = page_menu_->GetPreferredSize().width();
  int app_menu_width = app_menu_->GetPreferredSize().width();
  int bookmark_menu_width = bookmark_menu_ ?
      bookmark_menu_->GetPreferredSize().width() : 0;
  int location_x = reload_->x() + reload_->width();
  int available_width = width() - kPaddingRight - bookmark_menu_width -
      app_menu_width - page_menu_width - browser_actions_width -
      kMenuButtonOffset - go_button_width - location_x;

  location_bar_->SetBounds(location_x, child_y, std::max(available_width, 0),
                           child_height);

  go_->SetBounds(location_bar_->x() + location_bar_->width(), child_y,
                 go_button_width, child_height);

  int next_menu_x = go_->x() + go_->width() + kMenuButtonOffset;

  browser_actions_->SetBounds(next_menu_x, 0, browser_actions_width, height());

  // The browser actions need to do a layout explicitly, because when an
  // extension is loaded/unloaded/changed, BrowserActionContainer removes and
  // re-adds everything, regardless of whether it has a page action. For a
  // page action, browser action bounds do not change, as a result of which
  // SetBounds does not do a layout at all.
  // TODO(sidchat): Rework the above bahavior so that explicit layout is not
  //                required.
  browser_actions_->Layout();

  next_menu_x += browser_actions_width;

  if (bookmark_menu_) {
    bookmark_menu_->SetBounds(next_menu_x, child_y, bookmark_menu_width,
                              child_height);
    next_menu_x += bookmark_menu_width;
  }

  page_menu_->SetBounds(next_menu_x, child_y, page_menu_width, child_height);
  next_menu_x += page_menu_width;

  app_menu_->SetBounds(next_menu_x, child_y, app_menu_width, child_height);
}

void ToolbarView::Paint(gfx::Canvas* canvas) {
  View::Paint(canvas);

  if (IsDisplayModeNormal())
    return;

  // In maximized mode, we don't draw the endcaps on the location bar, because
  // when they're flush against the edge of the screen they just look glitchy.
  if (!browser_->window() || !browser_->window()->IsMaximized()) {
    int top_spacing = PopupTopSpacing();
    canvas->DrawBitmapInt(*kPopupBackgroundEdge, 0, top_spacing);
    canvas->DrawBitmapInt(*kPopupBackgroundEdge,
                          width() - kPopupBackgroundEdge->width(), top_spacing);
  }

  // For glass, we need to draw a black line below the location bar to separate
  // it from the content area.  For non-glass, the NonClientView draws the
  // toolbar background below the location bar for us.
  if (GetWindow()->GetNonClientView()->UseNativeFrame())
    canvas->FillRectInt(SK_ColorBLACK, 0, height() - 1, width(), 1);
}

void ToolbarView::ThemeChanged() {
  LoadLeftSideControlsImages();
  LoadCenterStackImages();
  LoadRightSideControlsImages();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, private:

int ToolbarView::PopupTopSpacing() const {
  return GetWindow()->GetNonClientView()->UseNativeFrame() ?
      0 : kPopupTopSpacingNonGlass;
}

void ToolbarView::CreateLeftSideControls() {
  back_ = new views::ButtonDropDown(this, back_menu_model_.get());
  back_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                  views::Event::EF_MIDDLE_BUTTON_DOWN);
  back_->set_tag(IDC_BACK);
  back_->SetImageAlignment(views::ImageButton::ALIGN_RIGHT,
                           views::ImageButton::ALIGN_TOP);
  back_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_BACK));
  back_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_BACK));
  back_->SetID(VIEW_ID_BACK_BUTTON);

  forward_ = new views::ButtonDropDown(this, forward_menu_model_.get());
  forward_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                        views::Event::EF_MIDDLE_BUTTON_DOWN);
  forward_->set_tag(IDC_FORWARD);
  forward_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_FORWARD));
  forward_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_FORWARD));
  forward_->SetID(VIEW_ID_FORWARD_BUTTON);

  home_ = new views::ImageButton(this);
  home_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                  views::Event::EF_MIDDLE_BUTTON_DOWN);
  home_->set_tag(IDC_HOME);
  home_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_HOME));
  home_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_HOME));
  home_->SetID(VIEW_ID_HOME_BUTTON);

  LoadLeftSideControlsImages();

  AddChildView(back_);
  AddChildView(forward_);
  AddChildView(home_);
}

void ToolbarView::CreateCenterStack(Profile *profile) {
  reload_ = new views::ImageButton(this);
  reload_->set_tag(IDC_RELOAD);
  reload_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_RELOAD));
  reload_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_RELOAD));
  reload_->SetID(VIEW_ID_RELOAD_BUTTON);

  location_bar_ = new LocationBarView(profile, browser_->command_updater(),
      model_, this, (display_mode_ == DISPLAYMODE_LOCATION) ?
          LocationBarView::POPUP : LocationBarView::NORMAL);

  // The Go button.
  go_ = new GoButton(location_bar_, browser_);
  go_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_GO));
  go_->SetID(VIEW_ID_GO_BUTTON);

  LoadCenterStackImages();

  AddChildView(reload_);
  location_bar_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_LOCATION));
  AddChildView(location_bar_);
  location_bar_->Init();
  AddChildView(go_);
}

void ToolbarView::CreateRightSideControls(Profile* profile) {
  browser_actions_ = new BrowserActionsContainer(browser_, this,
                                                 true);  // should_save_size

  page_menu_ = new views::MenuButton(NULL, std::wstring(), this, false);
  page_menu_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_PAGE));
  page_menu_->SetTooltipText(l10n_util::GetString(IDS_PAGEMENU_TOOLTIP));
  page_menu_->SetID(VIEW_ID_PAGE_MENU);

  app_menu_ = new views::MenuButton(NULL, std::wstring(), this, false);
  app_menu_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_APP));
  app_menu_->SetTooltipText(l10n_util::GetStringF(IDS_APPMENU_TOOLTIP,
      l10n_util::GetString(IDS_PRODUCT_NAME)));
  app_menu_->SetID(VIEW_ID_APP_MENU);

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kBookmarkMenu)) {
    bookmark_menu_ = new BookmarkMenuButton(browser_);
    AddChildView(bookmark_menu_);
  } else {
    bookmark_menu_ = NULL;
  }

  LoadRightSideControlsImages();

  AddChildView(browser_actions_);
  AddChildView(page_menu_);
  AddChildView(app_menu_);
}

void ToolbarView::LoadLeftSideControlsImages() {
  ThemeProvider* tp = GetThemeProvider();

  SkColor color = tp->GetColor(BrowserThemeProvider::COLOR_BUTTON_BACKGROUND);
  SkBitmap* background = tp->GetBitmapNamed(IDR_THEME_BUTTON_BACKGROUND);

  back_->SetImage(views::CustomButton::BS_NORMAL, tp->GetBitmapNamed(IDR_BACK));
  back_->SetImage(views::CustomButton::BS_HOT, tp->GetBitmapNamed(IDR_BACK_H));
  back_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_BACK_P));
  back_->SetImage(views::CustomButton::BS_DISABLED,
      tp->GetBitmapNamed(IDR_BACK_D));
  back_->SetBackground(color, background,
      tp->GetBitmapNamed(IDR_BACK_MASK));

  forward_->SetImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_FORWARD));
  forward_->SetImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_FORWARD_H));
  forward_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_FORWARD_P));
  forward_->SetImage(views::CustomButton::BS_DISABLED,
      tp->GetBitmapNamed(IDR_FORWARD_D));
  forward_->SetBackground(color, background,
      tp->GetBitmapNamed(IDR_FORWARD_MASK));

  home_->SetImage(views::CustomButton::BS_NORMAL, tp->GetBitmapNamed(IDR_HOME));
  home_->SetImage(views::CustomButton::BS_HOT, tp->GetBitmapNamed(IDR_HOME_H));
  home_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_HOME_P));
  home_->SetBackground(color, background,
      tp->GetBitmapNamed(IDR_BUTTON_MASK));
}

void ToolbarView::LoadCenterStackImages() {
  ThemeProvider* tp = GetThemeProvider();

  SkColor color = tp->GetColor(BrowserThemeProvider::COLOR_BUTTON_BACKGROUND);
  SkBitmap* background = tp->GetBitmapNamed(IDR_THEME_BUTTON_BACKGROUND);

  reload_->SetImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_RELOAD));
  reload_->SetImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_RELOAD_H));
  reload_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_RELOAD_P));
  reload_->SetBackground(color, background,
      tp->GetBitmapNamed(IDR_RELOAD_MASK));

  go_->SetImage(views::CustomButton::BS_NORMAL, tp->GetBitmapNamed(IDR_GO));
  go_->SetImage(views::CustomButton::BS_HOT, tp->GetBitmapNamed(IDR_GO_H));
  go_->SetImage(views::CustomButton::BS_PUSHED, tp->GetBitmapNamed(IDR_GO_P));
  go_->SetToggledImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_STOP));
  go_->SetToggledImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_STOP_H));
  go_->SetToggledImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_STOP_P));
  go_->SetBackground(color, background,
      tp->GetBitmapNamed(IDR_GO_MASK));
}

void ToolbarView::LoadRightSideControlsImages() {
  ThemeProvider* tp = GetThemeProvider();

  // We use different menu button images if the locale is right-to-left.
  if (UILayoutIsRightToLeft())
    page_menu_->SetIcon(*tp->GetBitmapNamed(IDR_MENU_PAGE_RTL));
  else
    page_menu_->SetIcon(*tp->GetBitmapNamed(IDR_MENU_PAGE));
  if (UILayoutIsRightToLeft())
    app_menu_->SetIcon(*tp->GetBitmapNamed(IDR_MENU_CHROME_RTL));
  else
    app_menu_->SetIcon(*tp->GetBitmapNamed(IDR_MENU_CHROME));

  if (bookmark_menu_ != NULL)
    bookmark_menu_->SetIcon(*tp->GetBitmapNamed(IDR_MENU_BOOKMARK));
}

void ToolbarView::RunPageMenu(const gfx::Point& pt) {
  bool destroyed_flag = false;
  destroyed_flag_ = &destroyed_flag;

  page_menu_model_.reset(new PageMenuModel(this, browser_));
  page_menu_menu_.reset(new views::Menu2(page_menu_model_.get()));
  for (unsigned int i = 0; i < menu_listeners_.size(); i++) {
    page_menu_menu_->AddMenuListener(menu_listeners_[i]);
  }
  page_menu_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);

  if (destroyed_flag)
    return;

  for (unsigned int i = 0; i < menu_listeners_.size(); i++) {
    page_menu_menu_->RemoveMenuListener(menu_listeners_[i]);
  }
  SwitchToOtherMenuIfNeeded(page_menu_menu_.get(), app_menu_);
}

void ToolbarView::RunAppMenu(const gfx::Point& pt) {
  bool destroyed_flag = false;
  destroyed_flag_ = &destroyed_flag;

  if (app_menu_model_->BuildProfileSubMenu())
    app_menu_menu_->Rebuild();
  for (unsigned int i = 0; i < menu_listeners_.size(); i++) {
    app_menu_menu_->AddMenuListener(menu_listeners_[i]);
  }
  app_menu_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);

  if (destroyed_flag)
    return;

  for (unsigned int i = 0; i < menu_listeners_.size(); i++) {
    app_menu_menu_->RemoveMenuListener(menu_listeners_[i]);
  }
  SwitchToOtherMenuIfNeeded(app_menu_menu_.get(), page_menu_);
}

void ToolbarView::SwitchToOtherMenuIfNeeded(
    views::Menu2* previous_menu, views::MenuButton* next_menu_button) {
  // If the user tried to move to the right or left, switch from the
  // app menu to the page menu. Switching to the next menu is delayed
  // until the next event loop so that the call stack that initiated
  // activating the first menu can return. (If we didn't do this, the
  // call stack would grow each time the user switches menus, and
  // the actions taken after the user finally exits a menu would cause
  // flicker.)
  views::MenuWrapper::MenuAction action = previous_menu->GetMenuAction();
  if (action == views::MenuWrapper::MENU_ACTION_NEXT ||
      action == views::MenuWrapper::MENU_ACTION_PREVIOUS) {
    MessageLoop::current()->PostTask(
        FROM_HERE, method_factory_.NewRunnableMethod(
            &ToolbarView::ActivateMenuButton,
            next_menu_button));
  }
}

void ToolbarView::ActivateMenuButton(views::MenuButton* menu_button) {
#if defined(OS_LINUX)
  // Under GTK, opening a pop-up menu causes the main window to lose focus.
  // Focus is automatically returned when the menu closes.
  //
  // Make sure that the menu button being activated has focus, so that
  // when the user escapes from the menu without selecting anything, focus
  // will be returned here.
  if (!menu_button->HasFocus()) {
    menu_button->RequestFocus();
    GetFocusManager()->StoreFocusedView();
  }
#endif

#if defined(OS_WIN)
  // On Windows, we have to explicitly clear the focus before opening
  // the pop-up menu, then set the focus again when it closes.
  GetFocusManager()->ClearFocus();
#endif

  // Tell the menu button to activate, opening its pop-up menu.
  menu_button->Activate();

#if defined(OS_WIN)
  EnterMenuBarEmulationMode(last_focused_view_storage_id_, menu_button);
#endif
}

void ToolbarView::ExitMenuBarEmulationMode() {
  if (page_menu_->HasFocus() || app_menu_->HasFocus())
    RestoreLastFocusedView();

  focus_manager_->UnregisterAccelerators(this);
  focus_manager_->RemoveFocusChangeListener(this);
  page_menu_->SetFocusable(false);
  app_menu_->SetFocusable(false);
  menu_bar_emulation_mode_ = false;
}

void ToolbarView::RestoreLastFocusedView() {
  views::ViewStorage* view_storage = views::ViewStorage::GetSharedInstance();
  views::View* last_focused_view =
      view_storage->RetrieveView(last_focused_view_storage_id_);
  if (last_focused_view) {
    last_focused_view->RequestFocus();
  } else {
    // Focus the location bar
    views::View* view = GetAncestorWithClassName(BrowserView::kViewClassName);
    if (view) {
      BrowserView* browser_view = static_cast<BrowserView*>(view);
      browser_view->SetFocusToLocationBar(false);
    }
  }
}
