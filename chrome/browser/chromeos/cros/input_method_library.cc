// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/input_method_library.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "third_party/icu/public/common/unicode/uloc.h"

#include <glib.h>
#include <signal.h>

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::InputMethodLibraryImpl);

namespace {

// Finds a property which has |new_prop.key| from |prop_list|, and replaces the
// property with |new_prop|. Returns true if such a property is found.
bool FindAndUpdateProperty(const chromeos::ImeProperty& new_prop,
                           chromeos::ImePropertyList* prop_list) {
  for (size_t i = 0; i < prop_list->size(); ++i) {
    chromeos::ImeProperty& prop = prop_list->at(i);
    if (prop.key == new_prop.key) {
      const int saved_id = prop.selection_item_id;
      // Update the list except the radio id. As written in
      // chromeos_input_method.h, |prop.selection_item_id| is dummy.
      prop = new_prop;
      prop.selection_item_id = saved_id;
      return true;
    }
  }
  return false;
}

// The default keyboard layout.
const char kDefaultKeyboardLayout[] = "us";

}  // namespace

namespace chromeos {

InputMethodLibraryImpl::InputMethodLibraryImpl()
    : input_method_status_connection_(NULL),
      previous_input_method_("", "", "", ""),
      current_input_method_("", "", "", ""),
      ime_running_(false),
      ime_connected_(false),
      defer_ime_startup_(false),
      active_input_method_(kHardwareKeyboardLayout),
      need_input_method_set_(false),
      ime_handle_(0),
      candidate_window_handle_(0) {
  scoped_ptr<InputMethodDescriptors> input_method_descriptors(
      CreateFallbackInputMethodDescriptors());
  current_input_method_ = input_method_descriptors->at(0);
}

InputMethodLibraryImpl::~InputMethodLibraryImpl() {
  StopInputMethodProcesses();
}

InputMethodLibraryImpl::Observer::~Observer() {
}

void InputMethodLibraryImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InputMethodLibraryImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

chromeos::InputMethodDescriptors*
InputMethodLibraryImpl::GetActiveInputMethods() {
  chromeos::InputMethodDescriptors* result = NULL;
  // The connection does not need to be alive, but it does need to be created.
  if (EnsureLoadedAndStarted()) {
    result = chromeos::GetActiveInputMethods(input_method_status_connection_);
  }
  if (!result || result->empty()) {
    result = CreateFallbackInputMethodDescriptors();
  }
  return result;
}

size_t InputMethodLibraryImpl::GetNumActiveInputMethods() {
  scoped_ptr<InputMethodDescriptors> input_methods(GetActiveInputMethods());
  return input_methods->size();
}

chromeos::InputMethodDescriptors*
InputMethodLibraryImpl::GetSupportedInputMethods() {
  chromeos::InputMethodDescriptors* result = NULL;
  // The connection does not need to be alive, but it does need to be created.
  if (EnsureLoadedAndStarted()) {
    result = chromeos::GetSupportedInputMethods(
        input_method_status_connection_);
  }
  if (!result || result->empty()) {
    result = CreateFallbackInputMethodDescriptors();
  }
  return result;
}

void InputMethodLibraryImpl::ChangeInputMethod(
    const std::string& input_method_id) {
  active_input_method_ = input_method_id;
  if (EnsureLoadedAndStarted()) {
    if (input_method_id != kHardwareKeyboardLayout) {
      StartInputMethodProcesses();
    }
    chromeos::ChangeInputMethod(
        input_method_status_connection_, input_method_id.c_str());
  }
}

void InputMethodLibraryImpl::SetImePropertyActivated(const std::string& key,
                                                     bool activated) {
  DCHECK(!key.empty());
  if (EnsureLoadedAndStarted()) {
    chromeos::SetImePropertyActivated(
        input_method_status_connection_, key.c_str(), activated);
  }
}

bool InputMethodLibraryImpl::InputMethodIsActivated(
    const std::string& input_method_id) {
  scoped_ptr<InputMethodDescriptors> active_input_method_descriptors(
      CrosLibrary::Get()->GetInputMethodLibrary()->GetActiveInputMethods());
  for (size_t i = 0; i < active_input_method_descriptors->size(); ++i) {
    if (active_input_method_descriptors->at(i).id == input_method_id) {
      return true;
    }
  }
  return false;
}

bool InputMethodLibraryImpl::GetImeConfig(
    const char* section, const char* config_name, ImeConfigValue* out_value) {
  bool success = false;
  if (EnsureLoadedAndStarted()) {
    success = chromeos::GetImeConfig(
        input_method_status_connection_, section, config_name, out_value);
  }
  return success;
}

bool InputMethodLibraryImpl::SetImeConfig(
    const char* section, const char* config_name, const ImeConfigValue& value) {
  MaybeUpdateImeState(section, config_name, value);

  const ConfigKeyType key = std::make_pair(section, config_name);
  current_config_values_[key] = value;
  if (ime_connected_) {
    pending_config_requests_[key] = value;
    FlushImeConfig();
  }
  return pending_config_requests_.empty();
}

void InputMethodLibraryImpl::MaybeUpdateImeState(
    const char* section, const char* config_name, const ImeConfigValue& value) {
  if (!strcmp(kGeneralSectionName, section) &&
      !strcmp(kPreloadEnginesConfigName, config_name)) {
    if (EnsureLoadedAndStarted()) {
      if (value.type == ImeConfigValue::kValueTypeStringList &&
          value.string_list_value.size() == 1 &&
          value.string_list_value[0] == kHardwareKeyboardLayout) {
        StopInputMethodProcesses();
      } else if (!defer_ime_startup_) {
        StartInputMethodProcesses();
      }
      chromeos::SetActiveInputMethods(input_method_status_connection_, value);
    }
  }
}

void InputMethodLibraryImpl::FlushImeConfig() {
  bool active_input_methods_are_changed = false;
  bool completed = false;
  if (EnsureLoadedAndStarted()) {
    InputMethodConfigRequests::iterator iter = pending_config_requests_.begin();
    while (iter != pending_config_requests_.end()) {
      const std::string& section = iter->first.first;
      const std::string& config_name = iter->first.second;
      const ImeConfigValue& value = iter->second;
      if (chromeos::SetImeConfig(input_method_status_connection_,
                                 section.c_str(), config_name.c_str(), value)) {
        // Check if it's a change in active input methods.
        if (config_name == kPreloadEnginesConfigName) {
          active_input_methods_are_changed = true;
        }
        // Successfully sent. Remove the command and proceed to the next one.
        pending_config_requests_.erase(iter++);
      } else {
        // If SetImeConfig() fails, subsequent calls will likely fail.
        break;
      }
    }
    if (pending_config_requests_.empty()) {
      // Calls to ChangeInputMethod() will fail if the input method has not yet
      // been added to preload_engines.  As such, the call is deferred until
      // after all config values have been sent to the IME process.
      if (need_input_method_set_) {
        if (chromeos::ChangeInputMethod(input_method_status_connection_,
                                        active_input_method_.c_str())) {
          need_input_method_set_ = false;
          completed = true;
          active_input_methods_are_changed = true;
        }
      } else {
        completed = true;
      }
    }
  }

  if (completed) {
    timer_.Stop();  // no-op if it's not running.
  } else {
    if (!timer_.IsRunning()) {
      static const int64 kTimerIntervalInMsec = 100;
      timer_.Start(base::TimeDelta::FromMilliseconds(kTimerIntervalInMsec),
                   this, &InputMethodLibraryImpl::FlushImeConfig);
    }
  }
  if (active_input_methods_are_changed) {
    FOR_EACH_OBSERVER(Observer, observers_, ActiveInputMethodsChanged(this));
  }
}

// static
void InputMethodLibraryImpl::InputMethodChangedHandler(
    void* object, const chromeos::InputMethodDescriptor& current_input_method) {
  InputMethodLibraryImpl* input_method_library =
      static_cast<InputMethodLibraryImpl*>(object);
  input_method_library->UpdateCurrentInputMethod(current_input_method);
}

// static
void InputMethodLibraryImpl::RegisterPropertiesHandler(
    void* object, const ImePropertyList& prop_list) {
  InputMethodLibraryImpl* input_method_library =
      static_cast<InputMethodLibraryImpl*>(object);
  input_method_library->RegisterProperties(prop_list);
}

// static
void InputMethodLibraryImpl::UpdatePropertyHandler(
    void* object, const ImePropertyList& prop_list) {
  InputMethodLibraryImpl* input_method_library =
      static_cast<InputMethodLibraryImpl*>(object);
  input_method_library->UpdateProperty(prop_list);
}

// static
void InputMethodLibraryImpl::ConnectionChangeHandler(void* object,
                                                     bool connected) {
  InputMethodLibraryImpl* input_method_library =
      static_cast<InputMethodLibraryImpl*>(object);
  input_method_library->ime_connected_ = connected;
  if (connected) {
    input_method_library->pending_config_requests_.clear();
    input_method_library->pending_config_requests_.insert(
        input_method_library->current_config_values_.begin(),
        input_method_library->current_config_values_.end());
    // When the IME process starts up, the hardware layout will be the current
    // method.  If this is not correct, we'll need to explicitly change it.
    if (input_method_library->active_input_method_ != kHardwareKeyboardLayout) {
      input_method_library->need_input_method_set_ = true;
    }
    input_method_library->FlushImeConfig();
  } else {
    // Stop attempting to resend config data, since it will continue to fail.
    input_method_library->timer_.Stop();  // no-op if it's not running.
  }
}

bool InputMethodLibraryImpl::EnsureStarted() {
  if (!input_method_status_connection_) {
    input_method_status_connection_ = chromeos::MonitorInputMethodStatus(
        this,
        &InputMethodChangedHandler,
        &RegisterPropertiesHandler,
        &UpdatePropertyHandler,
        &ConnectionChangeHandler);
  }
  return true;
}

bool InputMethodLibraryImpl::EnsureLoadedAndStarted() {
  return CrosLibrary::Get()->EnsureLoaded() &&
         EnsureStarted();
}

void InputMethodLibraryImpl::UpdateCurrentInputMethod(
    const chromeos::InputMethodDescriptor& new_input_method) {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    DLOG(INFO) << "UpdateCurrentInputMethod (Background thread)";
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        // NewRunnableMethod() copies |new_input_method| by value.
        NewRunnableMethod(
            this, &InputMethodLibraryImpl::UpdateCurrentInputMethod,
            new_input_method));
    return;
  }

  DLOG(INFO) << "UpdateCurrentInputMethod (UI thread)";
  // Change the keyboard layout to a preferred layout for the input method.
  CrosLibrary::Get()->GetKeyboardLibrary()->SetCurrentKeyboardLayoutByName(
      new_input_method.keyboard_layout);

  if (current_input_method_.id != new_input_method.id) {
    previous_input_method_ = current_input_method_;
    current_input_method_ = new_input_method;
  }
  FOR_EACH_OBSERVER(Observer, observers_, InputMethodChanged(this));
}

void InputMethodLibraryImpl::RegisterProperties(
    const ImePropertyList& prop_list) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &InputMethodLibraryImpl::RegisterProperties, prop_list));
    return;
  }

  // |prop_list| might be empty. This means "clear all properties."
  current_ime_properties_ = prop_list;
  FOR_EACH_OBSERVER(Observer, observers_, ImePropertiesChanged(this));
}

void InputMethodLibraryImpl::UpdateProperty(const ImePropertyList& prop_list) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &InputMethodLibraryImpl::UpdateProperty, prop_list));
    return;
  }

  for (size_t i = 0; i < prop_list.size(); ++i) {
    FindAndUpdateProperty(prop_list[i], &current_ime_properties_);
  }
  FOR_EACH_OBSERVER(Observer, observers_, ImePropertiesChanged(this));
}

void InputMethodLibraryImpl::StartInputMethodProcesses() {
  ime_running_ = true;
  MaybeLaunchIme();
}

void InputMethodLibraryImpl::MaybeLaunchIme() {
  if (!ime_running_) {
    return;
  }

  // TODO(zork): export "LD_PRELOAD=/usr/lib/libcrash.so"
  GSpawnFlags flags = G_SPAWN_DO_NOT_REAP_CHILD;
  if (ime_handle_ == 0) {
    GError *error = NULL;
    gchar **argv;
    gint argc;

    if (!g_shell_parse_argv(
        "/usr/bin/ibus-daemon --panel=disable --cache=none --restart",
        &argc, &argv, &error)) {
      LOG(ERROR) << "Could not parse command: " << error->message;
      g_error_free(error);
      return;
    }

    error = NULL;
    int handle;
    // TODO(zork): Send output to /var/log/ibus.log
    gboolean result = g_spawn_async(NULL, argv, NULL,
                                    flags, NULL, NULL,
                                    &handle, &error);
    g_strfreev(argv);
    if (!result) {
      LOG(ERROR) << "Could not launch ime: " << error->message;
      g_error_free(error);
      return;
    }
    ime_handle_ = handle;
    g_child_watch_add(ime_handle_, (GChildWatchFunc)OnImeShutdown, this);
  }

  if (candidate_window_handle_ == 0) {
    GError *error = NULL;
    gchar **argv;
    gint argc;

    if (!g_shell_parse_argv("/opt/google/chrome/candidate_window",
                            &argc, &argv, &error)) {
      LOG(ERROR) << "Could not parse command: " << error->message;
      g_error_free(error);
      return;
    }

    error = NULL;
    int handle;
    gboolean result = g_spawn_async(NULL, argv, NULL,
                                    flags, NULL, NULL,
                                    &handle, &error);
    g_strfreev(argv);
    if (!result) {
      LOG(ERROR) << "Could not launch ime candidate window" << error->message;
      g_error_free(error);
      return;
    }
    candidate_window_handle_ = handle;
    g_child_watch_add(candidate_window_handle_,
                      (GChildWatchFunc)OnImeShutdown, this);
  }
}

void InputMethodLibraryImpl::StopInputMethodProcesses() {
  ime_running_ = false;
  if (ime_handle_) {
    kill(ime_handle_, SIGTERM);
    ime_handle_ = 0;
  }
  if (candidate_window_handle_) {
    kill(candidate_window_handle_, SIGTERM);
    candidate_window_handle_ = 0;
  }
}

void InputMethodLibraryImpl::SetDeferImeStartup(bool defer) {
  defer_ime_startup_ = defer;
}

// static
void InputMethodLibraryImpl::OnImeShutdown(int pid, int status,
                                           InputMethodLibraryImpl* library) {
  g_spawn_close_pid(pid);
  if (library->ime_handle_ == pid) {
    library->ime_handle_ = 0;
  } else if (library->candidate_window_handle_ == pid) {
    library->candidate_window_handle_ = 0;
  }

  library->MaybeLaunchIme();
}

}  // namespace chromeos
