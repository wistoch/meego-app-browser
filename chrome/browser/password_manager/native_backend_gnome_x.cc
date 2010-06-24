// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/native_backend_gnome_x.h"

#include <map>
#include <string>

#include <dbus/dbus-glib.h>
#include <dlfcn.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"

using webkit_glue::PasswordForm;

namespace {

/* Many of the gnome_keyring_* functions use variable arguments, which makes
 * them difficult if not impossible to wrap in C. Therefore, we want the
 * actual uses below to either call the functions directly (if we are linking
 * against libgnome-keyring), or call them via appropriately-typed function
 * pointers (if we are dynamically loading libgnome-keyring).
 *
 * Thus, instead of making a wrapper class with two implementations, we use
 * the preprocessor to rename the calls below in the dynamic load case, and
 * provide a function to initialize a set of function pointers that have the
 * alternate names. We also make sure the types are correct, since otherwise
 * dynamic loading like this would leave us vulnerable to signature changes. */

#if defined(DLOPEN_GNOME_KEYRING)

// Call a given parameter with the name of each func we use from
// gnome keyring.
#define GNOME_KEYRING_FOR_EACH_FUNC(F)          \
  F(is_available)                               \
  F(store_password_sync)                        \
  F(delete_password_sync)                       \
  F(find_itemsv_sync)                           \
  F(result_to_message)                          \
  F(found_list_free)                            \
  F(list_item_ids_sync)                         \
  F(item_get_attributes_sync)                   \
  F(attribute_list_free)                        \
  F(item_get_info_sync)                         \
  F(item_info_get_secret)                       \
  F(item_info_free)

#define GNOME_KEYRING_DECLARE_TYPE(name) \
  typeof(&gnome_keyring_##name) wrap_gnome_keyring_##name;
GNOME_KEYRING_FOR_EACH_FUNC(GNOME_KEYRING_DECLARE_TYPE)
#undef GNOME_KEYRING_DECLARE_TYPE

// Make it easy to initialize the function pointers above with a loop below.
#define GNOME_KEYRING_FUNCTION(name) \
  {"gnome_keyring_"#name, reinterpret_cast<void**>(&wrap_gnome_keyring_##name)},
const struct {
  const char* name;
  void** pointer;
} gnome_keyring_functions[] = {
  GNOME_KEYRING_FOR_EACH_FUNC(GNOME_KEYRING_FUNCTION)
  {NULL, NULL}
};
#undef GNOME_KEYRING_FUNCTION

#undef GNOME_KEYRING_FOR_EACH_FUNC

// Allow application code below to use the normal function names, but actually
// end up using the function pointers above instead.
#define gnome_keyring_is_available \
    wrap_gnome_keyring_is_available
#define gnome_keyring_store_password_sync \
    wrap_gnome_keyring_store_password_sync
#define gnome_keyring_delete_password_sync \
    wrap_gnome_keyring_delete_password_sync
#define gnome_keyring_find_itemsv_sync \
    wrap_gnome_keyring_find_itemsv_sync
#define gnome_keyring_result_to_message \
    wrap_gnome_keyring_result_to_message
#define gnome_keyring_found_list_free \
    wrap_gnome_keyring_found_list_free
#define gnome_keyring_list_item_ids_sync \
    wrap_gnome_keyring_list_item_ids_sync
#define gnome_keyring_item_get_attributes_sync \
  wrap_gnome_keyring_item_get_attributes_sync
#define gnome_keyring_attribute_list_free \
  wrap_gnome_keyring_attribute_list_free
#define gnome_keyring_item_get_info_sync \
  wrap_gnome_keyring_item_get_info_sync
#define gnome_keyring_item_info_get_secret \
  wrap_gnome_keyring_item_info_get_secret
#define gnome_keyring_item_info_free \
  wrap_gnome_keyring_item_info_free

/* Load the library and initialize the function pointers. */
bool LoadGnomeKeyring() {
  void* handle = dlopen("libgnome-keyring.so.0", RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    // We wanted to use GNOME Keyring, but we couldn't load it. Warn, because
    // either the user asked for this, or we autodetected it incorrectly. (Or
    // the system has broken libraries, which is also good to warn about.)
    LOG(WARNING) << "Could not load libgnome-keyring.so.0: " << dlerror();
    return false;
  }
  for (size_t i = 0; gnome_keyring_functions[i].name; ++i) {
    dlerror();
    *gnome_keyring_functions[i].pointer =
        dlsym(handle, gnome_keyring_functions[i].name);
    const char* error = dlerror();
    if (error) {
      LOG(ERROR) << "Unable to load symbol " <<
          gnome_keyring_functions[i].name << ": " << error;
      dlclose(handle);
      return false;
    }
  }
  // We leak the library handle. That's OK: this function is called only once.
  return true;
}

// Older versions of GNOME Keyring have bugs that prevent them from
// working correctly with the find_itemsv API. (In particular, the
// non-pageable memory allocator is rather busted.) There is no
// official way to check the version, nor could we figure out any
// reasonable unofficial way to do it.  So we work around it by using
// a much slower API.
#define GNOME_KEYRING_WORKAROUND_MEMORY_CORRUPTION

#else  // !defined(DLOPEN_GNOME_KEYRING)

bool LoadGnomeKeyring() {
  // We don't need to do anything here. When linking directly, we
  // assume that whoever is compiling this code has checked that the
  // version is OK.
  return true;
}

#endif  // !defined(DLOPEN_GNOME_KEYRING)

#define GNOME_KEYRING_APPLICATION_CHROME "chrome"

// Convert the attributes of a given keyring entry into a new
// PasswordForm.  Note: does *not* get the actual password, as that is
// not a key attribute!  Returns NULL if the attributes are for the
// wrong application.
PasswordForm* FormFromAttributes(GnomeKeyringAttributeList* attrs) {
  // Read the string and int attributes into the appropriate map.
  std::map<std::string, std::string> string_attr_map;
  std::map<std::string, uint32_t> uint_attr_map;
  for (guint i = 0; i < attrs->len; ++i) {
    GnomeKeyringAttribute attr = gnome_keyring_attribute_list_index(attrs, i);
    if (attr.type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) {
      if (std::string(attr.name) == "application" &&
          std::string(attr.value.string) != GNOME_KEYRING_APPLICATION_CHROME) {
        // This is not a password we care about.
        return NULL;
      }
      string_attr_map[attr.name] = attr.value.string;
    } else if (attr.type == GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32) {
      uint_attr_map[attr.name] = attr.value.integer;
    }
  }

  PasswordForm* form = new PasswordForm();
  form->origin = GURL(string_attr_map["origin_url"]);
  form->action = GURL(string_attr_map["action_url"]);
  form->username_element = UTF8ToUTF16(string_attr_map["username_element"]);
  form->username_value = UTF8ToUTF16(string_attr_map["username_value"]);
  form->password_element = UTF8ToUTF16(string_attr_map["password_element"]);
  form->submit_element = UTF8ToUTF16(string_attr_map["submit_element"]);
  form->signon_realm = string_attr_map["signon_realm"];
  form->ssl_valid = uint_attr_map["ssl_valid"];
  form->preferred = uint_attr_map["preferred"];
  int64 date_created = 0;
  bool date_ok = StringToInt64(string_attr_map["date_created"],
                               &date_created);
  DCHECK(date_ok);
  DCHECK_NE(date_created, 0);
  form->date_created = base::Time::FromTimeT(date_created);
  form->blacklisted_by_user = uint_attr_map["blacklisted_by_user"];
  form->scheme = static_cast<PasswordForm::Scheme>(uint_attr_map["scheme"]);

  return form;
}

}  // namespace

// Schema is analagous to the fields in PasswordForm.
const GnomeKeyringPasswordSchema NativeBackendGnome::kGnomeSchema = {
  GNOME_KEYRING_ITEM_GENERIC_SECRET, {
    { "origin_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "action_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "username_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "username_value", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "password_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "submit_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "signon_realm", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "ssl_valid", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32 },
    { "preferred", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32 },
    { "date_created", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "blacklisted_by_user", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32 },
    { "scheme", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32 },
    // This field is always "chrome" so that we can search for it.
    { "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { NULL }
  }
};

NativeBackendGnome::NativeBackendGnome() {
}

NativeBackendGnome::~NativeBackendGnome() {
}

bool NativeBackendGnome::Init() {
  return LoadGnomeKeyring() && gnome_keyring_is_available();
}

bool NativeBackendGnome::AddLogin(const PasswordForm& form) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  GnomeKeyringResult result = gnome_keyring_store_password_sync(
      &kGnomeSchema,
      NULL,  // Default keyring.
      form.origin.spec().c_str(),  // Display name.
      UTF16ToUTF8(form.password_value).c_str(),
      "origin_url", form.origin.spec().c_str(),
      "action_url", form.action.spec().c_str(),
      "username_element", UTF16ToUTF8(form.username_element).c_str(),
      "username_value", UTF16ToUTF8(form.username_value).c_str(),
      "password_element", UTF16ToUTF8(form.password_element).c_str(),
      "submit_element", UTF16ToUTF8(form.submit_element).c_str(),
      "signon_realm", form.signon_realm.c_str(),
      "ssl_valid", form.ssl_valid,
      "preferred", form.preferred,
      "date_created", Int64ToString(form.date_created.ToTimeT()).c_str(),
      "blacklisted_by_user", form.blacklisted_by_user,
      "scheme", form.scheme,
      "application", GNOME_KEYRING_APPLICATION_CHROME,
      NULL);

  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring save failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  return true;
}

bool NativeBackendGnome::UpdateLogin(const PasswordForm& form) {
  // Based on LoginDatabase::UpdateLogin(), we search for forms to update by
  // origin_url, username_element, username_value, password_element, and
  // signon_realm. We then compare the result to the updated form. If they
  // differ in any of the action, password_value, ssl_valid, or preferred
  // fields, then we add a new login with those fields updated and only delete
  // the original on success.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  GList* found = NULL;
  // Search gnome keyring for matching passwords.
  GnomeKeyringResult result = gnome_keyring_find_itemsv_sync(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      &found,
      "origin_url", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      form.origin.spec().c_str(),
      "username_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      UTF16ToUTF8(form.username_element).c_str(),
      "username_value", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      UTF16ToUTF8(form.username_value).c_str(),
      "password_element", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      UTF16ToUTF8(form.password_element).c_str(),
      "signon_realm", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      form.signon_realm.c_str(),
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  bool ok = true;
  PasswordFormList forms;
  ConvertFormList(found, &forms);
  for (size_t i = 0; i < forms.size(); ++i) {
    if (forms[i]->action != form.action ||
        forms[i]->password_value != form.password_value ||
        forms[i]->ssl_valid != form.ssl_valid ||
        forms[i]->preferred != form.preferred) {
      PasswordForm updated = *forms[i];
      updated.action = form.action;
      updated.password_value = form.password_value;
      updated.ssl_valid = form.ssl_valid;
      updated.preferred = form.preferred;
      if (AddLogin(updated))
        RemoveLogin(*forms[i]);
      else
        ok = false;
    }
    delete forms[i];
  }
  return ok;
}

bool NativeBackendGnome::RemoveLogin(const PasswordForm& form) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  // We find forms using the same fields as LoginDatabase::RemoveLogin().
  GnomeKeyringResult result = gnome_keyring_delete_password_sync(
      &kGnomeSchema,
      "origin_url", form.origin.spec().c_str(),
      "action_url", form.action.spec().c_str(),
      "username_element", UTF16ToUTF8(form.username_element).c_str(),
      "username_value", UTF16ToUTF8(form.username_value).c_str(),
      "password_element", UTF16ToUTF8(form.password_element).c_str(),
      "submit_element", UTF16ToUTF8(form.submit_element).c_str(),
      "signon_realm", form.signon_realm.c_str(),
      NULL);
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring delete failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  return true;
}

bool NativeBackendGnome::RemoveLoginsCreatedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  bool ok = true;
  // We could walk the list and delete items as we find them, but it is much
  // easier to build the list and use RemoveLogin() to delete them.
  PasswordFormList forms;
  if (!GetAllLogins(&forms))
    return false;

  for (size_t i = 0; i < forms.size(); ++i) {
    if (delete_begin <= forms[i]->date_created &&
        (delete_end.is_null() || forms[i]->date_created < delete_end)) {
      if (!RemoveLogin(*forms[i]))
        ok = false;
    }
    delete forms[i];
  }
  return ok;
}

bool NativeBackendGnome::GetLogins(const PasswordForm& form,
                                   PasswordFormList* forms) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  GList* found = NULL;
  // Search gnome keyring for matching passwords.
  GnomeKeyringResult result = gnome_keyring_find_itemsv_sync(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      &found,
      "signon_realm", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      form.signon_realm.c_str(),
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  ConvertFormList(found, forms);
  return true;
}

bool NativeBackendGnome::GetLoginsCreatedBetween(const base::Time& get_begin,
                                                 const base::Time& get_end,
                                                 PasswordFormList* forms) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  // We could walk the list and add items as we find them, but it is much
  // easier to build the list and then filter the results.
  PasswordFormList all_forms;
  if (!GetAllLogins(&all_forms))
    return false;

  forms->reserve(forms->size() + all_forms.size());
  for (size_t i = 0; i < all_forms.size(); ++i) {
    if (get_begin <= all_forms[i]->date_created &&
        (get_end.is_null() || all_forms[i]->date_created < get_end)) {
      forms->push_back(all_forms[i]);
    } else {
      delete all_forms[i];
    }
  }

  return true;
}

bool NativeBackendGnome::GetAutofillableLogins(PasswordFormList* forms) {
  return GetLoginsList(forms, true);
}

bool NativeBackendGnome::GetBlacklistLogins(PasswordFormList* forms) {
  return GetLoginsList(forms, false);
}

bool NativeBackendGnome::GetLoginsList(PasswordFormList* forms,
                                       bool autofillable) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));

  uint32_t blacklisted_by_user = !autofillable;

#if !defined(GNOME_KEYRING_WORKAROUND_MEMORY_CORRUPTION)
  GList* found = NULL;
  // Search gnome keyring for matching passwords.
  GnomeKeyringResult result = gnome_keyring_find_itemsv_sync(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      &found,
      "blacklisted_by_user", GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32,
      blacklisted_by_user,
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  ConvertFormList(found, forms);
  return true;
#else
  PasswordFormList all_forms;
  if (!GetAllLogins(&all_forms))
    return false;
  // Now manually filter the result for the values we care about.
  for (size_t i = 0; i < all_forms.size(); ++i) {
    if (all_forms[i]->blacklisted_by_user == blacklisted_by_user)
      forms->push_back(all_forms[i]);
    else
      delete all_forms[i];
  }
  return true;
#endif
}

bool NativeBackendGnome::GetAllLogins(PasswordFormList* forms) {
  // Older versions of GNOME Keyring have bugs that prevent them from
  // working correctly with the find_itemsv API. (In particular, the
  // non-pageable memory allocator is rather busted.) There is no
  // official way to check the version, nor could we figure out any
  // reasonable unofficial way to do it.  So we work around it by
  // using a much slower API.

#if !defined(GNOME_KEYRING_WORKAROUND_MEMORY_CORRUPTION)
  GList* found = NULL;
  // We need to search for something, otherwise we get no results - so
  // we search for the fixed application string.
  GnomeKeyringResult result = gnome_keyring_find_itemsv_sync(
      GNOME_KEYRING_ITEM_GENERIC_SECRET,
      &found,
      "application", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING,
      GNOME_KEYRING_APPLICATION_CHROME,
      NULL);
  if (result == GNOME_KEYRING_RESULT_NO_MATCH)
    return true;
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring find failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }
  ConvertFormList(found, forms);
  return true;
#else
  GList* ids = NULL;
  GnomeKeyringResult result = gnome_keyring_list_item_ids_sync(NULL, &ids);
  if (result != GNOME_KEYRING_RESULT_OK) {
    LOG(ERROR) << "Keyring itemid list failed: "
               << gnome_keyring_result_to_message(result);
    return false;
  }

  for (GList* i = ids; i; i = i->next) {
    int id = GPOINTER_TO_INT(i->data);
    GnomeKeyringAttributeList* attrs = NULL;
    result = gnome_keyring_item_get_attributes_sync(NULL, id, &attrs);
    if (result != GNOME_KEYRING_RESULT_OK) {
      LOG(ERROR) << "Keyring get item attributes failed:"
                 << gnome_keyring_result_to_message(result);
      gnome_keyring_attribute_list_free(attrs);
      break;
    }

    PasswordForm* form = FormFromAttributes(attrs);
    gnome_keyring_attribute_list_free(attrs);

    if (form) {
      GnomeKeyringItemInfo* info = NULL;
      result = gnome_keyring_item_get_info_sync(NULL, id, &info);
      if (result != GNOME_KEYRING_RESULT_OK) {
        delete form;
        break;
      }
      form->password_value = UTF8ToUTF16(
          gnome_keyring_item_info_get_secret(info));

      gnome_keyring_item_info_free(info);
      forms->push_back(form);
    }
  }
  g_list_free(ids);

  return result == GNOME_KEYRING_RESULT_OK;
#endif
}

void NativeBackendGnome::ConvertFormList(GList* found,
                                         PasswordFormList* forms) {
  GList* element = g_list_first(found);
  while (element != NULL) {
    GnomeKeyringFound* data = static_cast<GnomeKeyringFound*>(element->data);
    GnomeKeyringAttributeList* attrs = data->attributes;

    PasswordForm* form = FormFromAttributes(attrs);
    form->password_value = UTF8ToUTF16(data->secret);
    forms->push_back(form);

    element = g_list_next(element);
  }
  gnome_keyring_found_list_free(found);
}
