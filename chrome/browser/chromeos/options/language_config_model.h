// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_MODEL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "chrome/browser/language_combobox_model.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "third_party/cros/chromeos_input_method.h"

namespace chromeos {

// The combobox model is used for adding languages in the language config
// view.
class AddLanguageComboboxModel : public LanguageComboboxModel {
 public:
  AddLanguageComboboxModel(Profile* profile,
                           const std::vector<std::string>& locale_codes);
  // LanguageComboboxModel overrides.
  virtual int GetItemCount();
  virtual std::wstring GetItemAt(int index);

  // Converts the given index (index of the items in the combobox) to the
  // index of the internal language list. The returned index can be used
  // for GetLocaleFromIndex() and GetLanguageNameAt().
  int GetLanguageIndex(int index) const;

  // Marks the given language code to be ignored. Ignored languages won't
  // be shown in the combobox. It would be simpler if we could remove and
  // add language codes from the model, but ComboboxModel does not allow
  // items to be added/removed. Thus we use |ignore_set_| instead.
  void SetIgnored(const std::string& language_code, bool ignored);

 private:
  std::set<std::string> ignore_set_;
  DISALLOW_COPY_AND_ASSIGN(AddLanguageComboboxModel);
};

// The model of LanguageConfigView.
class LanguageConfigModel : public NotificationObserver {
 public:
  LanguageConfigModel(PrefService* pref_service);

  // Initializes the model.
  void Init();

  // Counts the number of active input methods for the given language code.
  size_t CountNumActiveInputMethods(const std::string& language_code);

  // Returns true if the language code is in the preferred language list.
  bool HasLanguageCode(const std::string& language_code) const;

  // Adds the given language to the preferred language list, and returns
  // the index of the row where the language is added.
  size_t AddLanguageCode(const std::string& language_code);

  // Removes the language at the given row.
  void RemoveLanguageAt(size_t row);

  // Updates Chrome's input method preferences.
  void UpdateInputMethodPreferences(
      const std::vector<std::string>& new_input_method_ids);

  // Deactivates the input methods for the given language code.
  void DeactivateInputMethodsFor(const std::string& language_code);

  // Activates or deactivates an IME whose ID is |input_method_id|.
  void SetInputMethodActivated(const std::string& input_method_id,
                               bool activated);

  // Returns true if an IME of |input_method_id| is activated.
  bool InputMethodIsActivated(const std::string& input_method_id);

  // Gets the list of active IME IDs like "pinyin" and "m17n:ar:kbd".
  void GetActiveInputMethodIds(
      std::vector<std::string>* out_input_method_ids);

  // Converts an input method ID to a language code of the IME. Returns "Eng"
  // when |input_method_id| is unknown.
  // Example: "hangul" => "ko"
  std::string GetLanguageCodeFromInputMethodId(
      const std::string& input_method_id) const;

  // Converts an input method ID to a display name of the IME. Returns
  // "USA" (US keyboard) when |input_method_id| is unknown.
  // Examples: "pinyin" => "Pinyin"
  //           "m17n:ar:kbd" => "kbd (m17n)"
  std::string GetInputMethodDisplayNameFromId(
      const std::string& input_method_id) const;

  // Gets the list of input method ids associated with the given language
  // code.  The original contents of |input_method_ids| will be lost.
  void GetInputMethodIdsFromLanguageCode(
      const std::string& language_code,
      std::vector<std::string>* input_method_ids) const;

  // Callback for |preload_engines_| pref updates. Initializes the preferred
  // language codes based on the updated pref value.
  void NotifyPrefChanged();

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  const std::string& preferred_language_code_at(size_t at) const {
    return preferred_language_codes_[at];
  }

  size_t num_preferred_language_codes() const {
    return preferred_language_codes_.size();
  }

  const std::string& supported_input_method_id_at(size_t at) const {
    return supported_input_method_ids_[at];
  }

  size_t num_supported_input_method_ids() const {
    return supported_input_method_ids_.size();
  }

  const std::vector<std::string>& supported_language_codes() const {
    return supported_language_codes_;
  }

 private:
  // Initializes id_to_{code,display_name}_map_ maps,
  // as well as supported_{language_codes,input_method_ids}_ vectors.
  void InitInputMethodIdMapsAndVectors();

  // Adds the given language code and input method pair to the internal maps.
  void AddInputMethodToMaps(const std::string& language_code,
                            const InputMethodDescriptor& input_method);

  PrefService* pref_service_;
  // The codes of the preferred languages.
  std::vector<std::string> preferred_language_codes_;
  StringPrefMember preload_engines_;
  std::map<std::string, std::string> id_to_language_code_map_;
  std::map<std::string, std::string> id_to_display_name_map_;
  // List of supported language codes like "en" and "ja".
  std::vector<std::string> supported_language_codes_;
  // List of supported IME IDs like "pinyin" and "m17n:ar:kbd".
  std::vector<std::string> supported_input_method_ids_;

  DISALLOW_COPY_AND_ASSIGN(LanguageConfigModel);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_MODEL_H_
