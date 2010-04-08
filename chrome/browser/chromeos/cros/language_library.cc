// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/language_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "third_party/cros/chromeos_keyboard.h"
#include "third_party/icu/public/common/unicode/uloc.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
template <>
struct RunnableMethodTraits<chromeos::LanguageLibraryImpl> {
  void RetainCallee(chromeos::LanguageLibraryImpl* obj) {}
  void ReleaseCallee(chromeos::LanguageLibraryImpl* obj) {}
};

namespace {

// Finds a property which has |new_prop.key| from |prop_list|, and replaces the
// property with |new_prop|. Returns true if such a property is found.
bool FindAndUpdateProperty(const chromeos::ImeProperty& new_prop,
                           chromeos::ImePropertyList* prop_list) {
  for (size_t i = 0; i < prop_list->size(); ++i) {
    chromeos::ImeProperty& prop = prop_list->at(i);
    if (prop.key == new_prop.key) {
      const int saved_id = prop.selection_item_id;
      // Update the list except the radio id. As written in chromeos_language.h,
      // |prop.selection_item_id| is dummy.
      prop = new_prop;
      prop.selection_item_id = saved_id;
      return true;
    }
  }
  return false;
}

}  // namespace

namespace chromeos {

std::string LanguageLibrary::NormalizeLanguageCode(
    const std::string& language_code) {
  // We only handle two-letter codes here.
  // Some ibus engines return locale codes like "zh_CN" as language codes,
  // and we don't want to rewrite this to "zho".
  if (language_code.size() != 2) {
    return language_code;
  }
  const char* three_letter_code = uloc_getISO3Language(
      language_code.c_str());
  if (three_letter_code && strlen(three_letter_code) > 0) {
    return three_letter_code;
  }
  return language_code;
}

bool LanguageLibrary::IsKeyboardLayout(
    const std::string& input_method_id) {
  const bool case_insensitive = false;
  return StartsWithASCII(input_method_id, "xkb:", case_insensitive);
}

LanguageLibraryImpl::LanguageLibraryImpl()
    : language_status_connection_(NULL),
      current_input_method_("", "", "") {
}

LanguageLibraryImpl::~LanguageLibraryImpl() {
  if (language_status_connection_) {
    chromeos::DisconnectLanguageStatus(language_status_connection_);
  }
}

LanguageLibraryImpl::Observer::~Observer() {
}

void LanguageLibraryImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LanguageLibraryImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

namespace {

// Removes "US" input method. Older (v18 and earlier) cros might add the dummy
// input method to |input_methods|, but we don't need it anymore.
// TODO(yusukes): Remove this function when we finish migrating to v20.
void RemoveDummyInputMethod(chromeos::InputMethodDescriptors* input_methods) {
  chromeos::InputMethodDescriptors::iterator iter;
  for (iter = input_methods->begin(); iter != input_methods->end(); ++iter) {
    if ((iter->id == "USA") && (input_methods->size() > 1)) {
      input_methods->erase(iter);
      return;
    }
  }
}

}  // namespace

chromeos::InputMethodDescriptors* LanguageLibraryImpl::GetActiveInputMethods() {
  chromeos::InputMethodDescriptors* result = NULL;
  if (EnsureLoadedAndStarted()) {
    result = chromeos::GetActiveInputMethods(language_status_connection_);
  }
  if (!result) {
    result = CreateFallbackInputMethodDescriptors();
  }
  // TODO(yusukes): Remove this hack.
  RemoveDummyInputMethod(result);
  return result;
}

chromeos::InputMethodDescriptors*
LanguageLibraryImpl::GetSupportedInputMethods() {
  chromeos::InputMethodDescriptors* result = NULL;
  if (EnsureLoadedAndStarted()) {
    result = chromeos::GetSupportedInputMethods(language_status_connection_);
  }
  if (!result) {
    result = CreateFallbackInputMethodDescriptors();
  }
  // TODO(yusukes): Remove this hack.
  RemoveDummyInputMethod(result);
  return result;
}

void LanguageLibraryImpl::ChangeInputMethod(
    const std::string& input_method_id) {
  if (EnsureLoadedAndStarted()) {
    chromeos::ChangeInputMethod(
        language_status_connection_, input_method_id.c_str());
  }
}

void LanguageLibraryImpl::SetImePropertyActivated(const std::string& key,
                                                  bool activated) {
  DCHECK(!key.empty());
  if (EnsureLoadedAndStarted()) {
    chromeos::SetImePropertyActivated(
        language_status_connection_, key.c_str(), activated);
  }
}

bool LanguageLibraryImpl::SetInputMethodActivated(
    const std::string& input_method_id, bool activated) {
  bool success = false;
  if (EnsureLoadedAndStarted()) {
    success = chromeos::SetInputMethodActivated(language_status_connection_,
                                                input_method_id.c_str(),
                                                activated);
  }
  return success;
}

bool LanguageLibraryImpl::InputMethodIsActivated(
    const std::string& input_method_id) {
  scoped_ptr<InputMethodDescriptors> active_input_method_descriptors(
      CrosLibrary::Get()->GetLanguageLibrary()->GetActiveInputMethods());
  for (size_t i = 0; i < active_input_method_descriptors->size(); ++i) {
    if (active_input_method_descriptors->at(i).id == input_method_id) {
      return true;
    }
  }
  return false;
}

bool LanguageLibraryImpl::GetImeConfig(
    const char* section, const char* config_name, ImeConfigValue* out_value) {
  bool success = false;
  if (EnsureLoadedAndStarted()) {
    success = chromeos::GetImeConfig(
        language_status_connection_, section, config_name, out_value);
  }
  return success;
}

bool LanguageLibraryImpl::SetImeConfig(
    const char* section, const char* config_name, const ImeConfigValue& value) {
  bool success = false;
  if (EnsureLoadedAndStarted()) {
    success = chromeos::SetImeConfig(
        language_status_connection_, section, config_name, value);
  }
  return success;
}

// static
void LanguageLibraryImpl::InputMethodChangedHandler(
    void* object, const chromeos::InputMethodDescriptor& current_input_method) {
  LanguageLibraryImpl* language_library =
      static_cast<LanguageLibraryImpl*>(object);
  language_library->UpdateCurrentInputMethod(current_input_method);
}

// static
void LanguageLibraryImpl::RegisterPropertiesHandler(
    void* object, const ImePropertyList& prop_list) {
  LanguageLibraryImpl* language_library =
      static_cast<LanguageLibraryImpl*>(object);
  language_library->RegisterProperties(prop_list);
}

// static
void LanguageLibraryImpl::UpdatePropertyHandler(
    void* object, const ImePropertyList& prop_list) {
  LanguageLibraryImpl* language_library =
      static_cast<LanguageLibraryImpl*>(object);
  language_library->UpdateProperty(prop_list);
}

bool LanguageLibraryImpl::EnsureStarted() {
  if (language_status_connection_) {
    if (chromeos::LanguageStatusConnectionIsAlive(
            language_status_connection_)) {
      return true;
    }
    DLOG(WARNING) << "IBus/XKB connection is closed. Trying to reconnect...";
    chromeos::DisconnectLanguageStatus(language_status_connection_);
  }
  chromeos::LanguageStatusMonitorFunctions monitor_functions;
  monitor_functions.current_language = &InputMethodChangedHandler;
  monitor_functions.register_ime_properties = &RegisterPropertiesHandler;
  monitor_functions.update_ime_property = &UpdatePropertyHandler;
  language_status_connection_
      = chromeos::MonitorLanguageStatus(monitor_functions, this);
  return language_status_connection_ != NULL;
}

bool LanguageLibraryImpl::EnsureLoadedAndStarted() {
  return CrosLibrary::Get()->EnsureLoaded() &&
         EnsureStarted();
}

void LanguageLibraryImpl::UpdateCurrentInputMethod(
    const chromeos::InputMethodDescriptor& current_input_method) {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    DLOG(INFO) << "UpdateCurrentInputMethod (Background thread)";
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        // NewRunnableMethod() copies |current_input_method| by value.
        NewRunnableMethod(
            this, &LanguageLibraryImpl::UpdateCurrentInputMethod,
            current_input_method));
    return;
  }

  DLOG(INFO) << "UpdateCurrentInputMethod (UI thread)";
  const char kDefaultLayout[] = "us";
  if (IsKeyboardLayout(current_input_method.id)) {
    // If the new input method is a keyboard layout, switch the keyboard.
    std::vector<std::string> portions;
    SplitString(current_input_method.id, ':', &portions);
    const std::string keyboard_layout =
        (portions.size() > 1 && !portions[1].empty() ?
         portions[1] : kDefaultLayout);
    chromeos::SetCurrentKeyboardLayoutByName(keyboard_layout);
  } else {
    // If the new input method is an IME, change the keyboard back to the
    // default layout (US).  TODO(satorux): What if the user is using a non-US
    // keyboard, such as a Japanese keyboard? We need to rework this.
    chromeos::SetCurrentKeyboardLayoutByName(kDefaultLayout);
  }

  current_input_method_ = current_input_method;
  FOR_EACH_OBSERVER(Observer, observers_, InputMethodChanged(this));
}

void LanguageLibraryImpl::RegisterProperties(const ImePropertyList& prop_list) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &LanguageLibraryImpl::RegisterProperties, prop_list));
    return;
  }

  // |prop_list| might be empty. This means "clear all properties."
  current_ime_properties_ = prop_list;
  FOR_EACH_OBSERVER(Observer, observers_, ImePropertiesChanged(this));
}

void LanguageLibraryImpl::UpdateProperty(const ImePropertyList& prop_list) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(
            this, &LanguageLibraryImpl::UpdateProperty, prop_list));
    return;
  }

  for (size_t i = 0; i < prop_list.size(); ++i) {
    FindAndUpdateProperty(prop_list[i], &current_ime_properties_);
  }
  FOR_EACH_OBSERVER(Observer, observers_, ImePropertiesChanged(this));
}

}  // namespace chromeos
