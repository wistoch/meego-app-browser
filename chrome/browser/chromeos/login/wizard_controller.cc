// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include <gdk/gdk.h>
#include <signal.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/logging.h"  // For NOTREACHED.
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/account_screen.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_screen.h"
#include "chrome/browser/chromeos/login/network_screen.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/update_screen.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "views/accelerator.h"
#include "views/painter.h"
#include "views/screen.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"

namespace {

const int kWizardScreenWidth = 700;
const int kWizardScreenHeight = 416;

// RootView of the Widget WizardController creates. Contains the contents of the
// WizardController.
class ContentView : public views::View {
 public:
  ContentView(bool paint_background, int window_x, int window_y, int screen_w,
              int screen_h)
      : window_x_(window_x),
        window_y_(window_y),
        screen_w_(screen_w),
        screen_h_(screen_h),
        accel_account_screen_(views::Accelerator(base::VKEY_A,
                                                 false, true, true)),
        accel_login_screen_(views::Accelerator(base::VKEY_L,
                                               false, true, true)),
        accel_network_screen_(views::Accelerator(base::VKEY_N,
                                                 false, true, true)),
        accel_update_screen_(views::Accelerator(base::VKEY_U,
                                                false, true, true)) {
    if (paint_background) {
      painter_.reset(chromeos::CreateWizardPainter(
                         &chromeos::BorderDefinition::kWizardBorder));
    }

    AddAccelerator(accel_account_screen_);
    AddAccelerator(accel_login_screen_);
    AddAccelerator(accel_network_screen_);
    AddAccelerator(accel_update_screen_);
  }

  ~ContentView() {
    NotificationService::current()->Notify(
        NotificationType::WIZARD_CONTENT_VIEW_DESTROYED,
        NotificationService::AllSources(),
        NotificationService::NoDetails());
  }

  bool AcceleratorPressed(const views::Accelerator& accel) {
    WizardController* controller = WizardController::default_controller();
    if (!controller)
      return false;

    if (accel == accel_account_screen_) {
      controller->ShowAccountScreen();
    } else if (accel == accel_login_screen_) {
      controller->ShowLoginScreen();
    } else if (accel == accel_network_screen_) {
      controller->ShowNetworkScreen();
    } else if (accel == accel_update_screen_) {
      controller->ShowUpdateScreen();
    } else {
      return false;
    }

    return true;
  }

  void PaintBackground(gfx::Canvas* canvas) {
    if (painter_.get()) {
      // TODO(sky): nuke this once new login manager is in place. This needs to
      // exist because with no window manager transparency isn't really
      // supported.
      canvas->TranslateInt(-window_x_, -window_y_);
      painter_->Paint(screen_w_, screen_h_, canvas);
    }
  }

  virtual void Layout() {
    for (int i = 0; i < GetChildViewCount(); ++i) {
      views::View* cur = GetChildViewAt(i);
      if (cur->IsVisible())
        cur->SetBounds(0, 0, width(), height());
    }
  }

 private:
  scoped_ptr<views::Painter> painter_;

  const int window_x_;
  const int window_y_;
  const int screen_w_;
  const int screen_h_;

  views::Accelerator accel_account_screen_;
  views::Accelerator accel_login_screen_;
  views::Accelerator accel_network_screen_;
  views::Accelerator accel_update_screen_;

  DISALLOW_COPY_AND_ASSIGN(ContentView);
};


// Returns bounds of the screen to use for login wizard.
// The rect is centered within the default monitor and sized accordingly if
// |size| is not empty. Otherwise the whole monitor is occupied.
gfx::Rect CalculateScreenBounds(const gfx::Size& size) {
  gfx::Rect bounds(views::Screen::GetMonitorWorkAreaNearestWindow(NULL));
  if (!size.IsEmpty()) {
    int horizontal_diff = bounds.width() - size.width();
    int vertical_diff = bounds.height() - size.height();
    bounds.Inset(horizontal_diff / 2, vertical_diff / 2);
  }

  return bounds;
}

}  // namespace

const char WizardController::kNetworkScreenName[] = "network";
const char WizardController::kLoginScreenName[] = "login";
const char WizardController::kAccountScreenName[] = "account";
const char WizardController::kUpdateScreenName[] = "update";

// Passing this parameter as a "first screen" initiates full OOBE flow.
const char WizardController::kOutOfBoxScreenName[] = "oobe";

// Special test value that commands not to create any window yet.
const char WizardController::kTestNoScreenName[] = "test:nowindow";

// Initialize default controller.
// static
WizardController* WizardController::default_controller_ = NULL;

///////////////////////////////////////////////////////////////////////////////
// WizardController, public:

WizardController::WizardController()
    : widget_(NULL),
      background_widget_(NULL),
      background_view_(NULL),
      contents_(NULL),
      current_screen_(NULL),
      is_out_of_box_(false),
      observer_(NULL) {
  DCHECK(default_controller_ == NULL);
  default_controller_ = this;
}

WizardController::~WizardController() {
  // Close ends up deleting the widget.
  if (background_widget_)
    background_widget_->Close();

  if (widget_)
    widget_->Close();

  default_controller_ = NULL;
}

void WizardController::Init(const std::string& first_screen_name,
                            const gfx::Rect& screen_bounds,
                            bool paint_background) {
  DCHECK(!contents_);

  int offset_x = (screen_bounds.width() - kWizardScreenWidth) / 2;
  int offset_y = (screen_bounds.height() - kWizardScreenHeight) / 2;
  int window_x = screen_bounds.x() + offset_x;
  int window_y = screen_bounds.y() + offset_y;

  contents_ = new ContentView(paint_background,
                              offset_x, offset_y,
                              screen_bounds.width(), screen_bounds.height());

  views::WidgetGtk* window =
      new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
  widget_ = window;
  if (!paint_background)
    window->MakeTransparent();
  window->Init(NULL, gfx::Rect(window_x, window_y, kWizardScreenWidth,
                               kWizardScreenHeight));
  chromeos::WmIpc::instance()->SetWindowType(
      window->GetNativeView(),
      chromeos::WM_IPC_WINDOW_LOGIN_GUEST,
      NULL);
  window->SetContentsView(contents_);

  if (chromeos::UserManager::Get()->GetUsers().empty() ||
      first_screen_name == kOutOfBoxScreenName) {
    is_out_of_box_ = true;
  }

  ShowFirstScreen(first_screen_name);

  // This keeps the window from flashing at startup.
  GdkWindow* gdk_window = window->GetNativeView()->window;
  gdk_window_set_back_pixmap(gdk_window, NULL, false);
}

void WizardController::Show() {
  DCHECK(widget_);
  widget_->Show();
}

void WizardController::ShowBackground(const gfx::Rect& bounds) {
  DCHECK(!background_widget_);
  background_widget_ =
      chromeos::BackgroundView::CreateWindowContainingView(bounds,
                                                           &background_view_);
  background_widget_->Show();
}

void WizardController::OwnBackground(
    views::Widget* background_widget,
    chromeos::BackgroundView* background_view) {
  DCHECK(!background_widget_);
  background_widget_ = background_widget;
  background_view_ = background_view;
}

chromeos::NetworkScreen* WizardController::GetNetworkScreen() {
  if (!network_screen_.get())
    network_screen_.reset(new chromeos::NetworkScreen(this, is_out_of_box_));
  return network_screen_.get();
}

chromeos::LoginScreen* WizardController::GetLoginScreen() {
  if (!login_screen_.get())
    login_screen_.reset(new chromeos::LoginScreen(this));
  return login_screen_.get();
}

chromeos::AccountScreen* WizardController::GetAccountScreen() {
  if (!account_screen_.get())
    account_screen_.reset(new chromeos::AccountScreen(this));
  return account_screen_.get();
}

chromeos::UpdateScreen* WizardController::GetUpdateScreen() {
  if (!update_screen_.get())
    update_screen_.reset(new chromeos::UpdateScreen(this));
  return update_screen_.get();
}

void WizardController::ShowNetworkScreen() {
  SetStatusAreaVisible(false);
  SetCurrentScreen(GetNetworkScreen());
}

void WizardController::ShowLoginScreen() {
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetLoginScreen());
}

void WizardController::ShowAccountScreen() {
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetAccountScreen());
}

void WizardController::ShowUpdateScreen() {
  SetStatusAreaVisible(true);
  SetCurrentScreen(GetUpdateScreen());
}

void WizardController::SetStatusAreaVisible(bool visible) {
  // When ExistingUserController passes background ownership
  // to WizardController it happens after screen is shown.
  if (background_view_) {
    background_view_->SetStatusAreaVisible(visible);
  }
}

void WizardController::SetCustomization(
    const chromeos::StartupCustomizationDocument* customization) {
  customization_.reset(customization);
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, ExitHandlers:
void WizardController::OnLoginSignInSelected() {
  // We're on the stack, so don't try and delete us now.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WizardController::OnLoginCreateAccount() {
  ShowAccountScreen();
}

void WizardController::OnNetworkConnected() {
  if (is_out_of_box_) {
    ShowUpdateScreen();
    GetUpdateScreen()->StartUpdate();
  } else {
    ShowLoginScreen();
  }
}

void WizardController::OnNetworkOffline() {
  // TODO(dpolukhin): if(is_out_of_box_) we cannot work offline and
  // should report some error message here and stay on the same screen.
  ShowLoginScreen();
}

void WizardController::OnAccountCreateBack() {
  ShowLoginScreen();
}

void WizardController::OnAccountCreated() {
  ShowLoginScreen();
  chromeos::LoginScreen* login = GetLoginScreen();
  if (!username_.empty()) {
    login->view()->SetUsername(username_);
    if (!password_.empty()) {
      login->view()->SetPassword(password_);
      // TODO(dpolukhin): clear password memory for real. Now it is not
      // a problem becuase we can't extract password from the form.
      password_.clear();
      login->view()->Login();
    }
  }
}

void WizardController::OnConnectionFailed() {
  // TODO(dpolukhin): show error message before going back to network screen.
  is_out_of_box_ = false;
  ShowNetworkScreen();
}

void WizardController::OnUpdateCompleted() {
  ShowLoginScreen();
}

void WizardController::OnUpdateNetworkError() {
  // If network connection got interrupted while downloading the update,
  // return to network selection screen.
  // TODO(nkostylev): Show message to the user explaining update error.
  ShowNetworkScreen();
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, private:

void WizardController::SetCurrentScreen(WizardScreen* new_current) {
  if (current_screen_ == new_current)
    return;
  if (current_screen_)
    current_screen_->Hide();
  current_screen_ = new_current;
  if (current_screen_) {
    current_screen_->Show();
    contents_->Layout();
  }
  contents_->SchedulePaint();
}

void WizardController::OnSetUserNamePassword(const std::string& username,
                                             const std::string& password) {
  username_ = username;
  password_ = password;
}

void WizardController::ShowFirstScreen(const std::string& first_screen_name) {
  if (first_screen_name == kNetworkScreenName) {
    ShowNetworkScreen();
  } else if (first_screen_name == kLoginScreenName) {
    ShowLoginScreen();
  } else if (first_screen_name == kAccountScreenName) {
    ShowAccountScreen();
  } else if (first_screen_name == kUpdateScreenName) {
    ShowUpdateScreen();
    GetUpdateScreen()->StartUpdate();
  } else if (first_screen_name != kTestNoScreenName) {
    if (is_out_of_box_) {
      ShowNetworkScreen();
    } else {
      ShowLoginScreen();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, chromeos::ScreenObserver overrides:
void WizardController::OnExit(ExitCodes exit_code) {
  switch (exit_code) {
    case LOGIN_SIGN_IN_SELECTED:
      OnLoginSignInSelected();
      break;
    case LOGIN_CREATE_ACCOUNT:
      OnLoginCreateAccount();
      break;
    case NETWORK_CONNECTED:
      OnNetworkConnected();
      break;
    case NETWORK_OFFLINE:
      OnNetworkOffline();
      break;
    case ACCOUNT_CREATE_BACK:
      OnAccountCreateBack();
      break;
    case ACCOUNT_CREATED:
      OnAccountCreated();
      break;
    case CONNECTION_FAILED:
      OnConnectionFailed();
      break;
    case UPDATE_INSTALLED:
    case UPDATE_NOUPDATE:
      OnUpdateCompleted();
      break;
    case UPDATE_NETWORK_ERROR:
    case UPDATE_OTHER_ERROR:
      OnUpdateNetworkError();
      break;
    default:
      NOTREACHED();
  }
}

///////////////////////////////////////////////////////////////////////////////
// WizardController, WizardScreen overrides:
views::View* WizardController::GetWizardView() {
  return contents_;
}

chromeos::ScreenObserver* WizardController::GetObserver(WizardScreen* screen) {
  return observer_ ? observer_ : this;
}

namespace browser {

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
void ShowLoginWizard(const std::string& first_screen_name,
                     const gfx::Size& size) {
  LOG(INFO) << "showing login" << first_screen_name;

  // Tell the window manager that the user isn't logged in.
  chromeos::WmIpc::instance()->SetLoggedInProperty(false);

  gfx::Rect screen_bounds(CalculateScreenBounds(size));

  if (first_screen_name.empty() &&
      chromeos::CrosLibrary::Get()->EnsureLoaded() &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLoginImages)) {
    std::vector<chromeos::UserManager::User> users =
        chromeos::UserManager::Get()->GetUsers();
    if (!users.empty()) {
      // ExistingUserController deletes itself.
      (new chromeos::ExistingUserController(users, screen_bounds))->Init();
      return;
    }
  }

  // Load partner customization startup manifest if needed.
  scoped_ptr<chromeos::StartupCustomizationDocument> customization;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kStartupManifest)) {
    customization.reset(new chromeos::StartupCustomizationDocument());
    FilePath manifest_path =
        CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kStartupManifest);
    bool manifest_loaded = customization->LoadManifestFromFile(manifest_path);
    DCHECK(manifest_loaded) << manifest_path.value();
  }

  // Create and show the wizard.
  WizardController* controller = new WizardController();
  controller->SetCustomization(customization.release());
  controller->ShowBackground(screen_bounds);
  controller->Init(first_screen_name, screen_bounds, true);
  controller->Show();
  if (chromeos::CrosLibrary::Get()->EnsureLoaded())
    chromeos::CrosLibrary::Get()->GetLoginLibrary()->EmitLoginPromptReady();
}

}  // namespace browser
