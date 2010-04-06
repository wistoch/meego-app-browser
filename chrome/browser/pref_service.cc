// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pref_service.h"

#include <algorithm>
#include <string>

#include "app/l10n_util.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// A helper function for RegisterLocalized*Pref that creates a Value* based on
// the string value in the locale dll.  Because we control the values in a
// locale dll, this should always return a Value of the appropriate type.
Value* CreateLocaleDefaultValue(Value::ValueType type, int message_id) {
  std::wstring resource_string = l10n_util::GetString(message_id);
  DCHECK(!resource_string.empty());
  switch (type) {
    case Value::TYPE_BOOLEAN: {
      if (L"true" == resource_string)
        return Value::CreateBooleanValue(true);
      if (L"false" == resource_string)
        return Value::CreateBooleanValue(false);
      break;
    }

    case Value::TYPE_INTEGER: {
      return Value::CreateIntegerValue(
          StringToInt(WideToUTF16Hack(resource_string)));
      break;
    }

    case Value::TYPE_REAL: {
      return Value::CreateRealValue(
          StringToDouble(WideToUTF16Hack(resource_string)));
      break;
    }

    case Value::TYPE_STRING: {
      return Value::CreateStringValue(resource_string);
      break;
    }

    default: {
      NOTREACHED() <<
          "list and dictionary types can not have default locale values";
    }
  }
  NOTREACHED();
  return Value::CreateNullValue();
}

// Forwards a notification after a PostMessage so that we can wait for the
// MessageLoop to run.
void NotifyReadError(PrefService* pref, int message_id) {
  Source<PrefService> source(pref);
  NotificationService::current()->Notify(NotificationType::PROFILE_ERROR,
                                         source, Details<int>(&message_id));
}

}  // namespace

PrefService::PrefService(const FilePath& pref_filename)
    : persistent_(new DictionaryValue),
      writer_(pref_filename),
      read_only_(false) {
  InitFromDisk();
}

PrefService::~PrefService() {
  DCHECK(CalledOnValidThread());

  // Verify that there are no pref observers when we shut down.
  for (PrefObserverMap::iterator it = pref_observers_.begin();
       it != pref_observers_.end(); ++it) {
    NotificationObserverList::Iterator obs_iterator(*(it->second));
    if (obs_iterator.GetNext()) {
      LOG(WARNING) << "pref observer found at shutdown " << it->first;
    }
  }

  STLDeleteContainerPointers(prefs_.begin(), prefs_.end());
  prefs_.clear();
  STLDeleteContainerPairSecondPointers(pref_observers_.begin(),
                                       pref_observers_.end());
  pref_observers_.clear();

  if (writer_.HasPendingWrite() && !read_only_)
    writer_.DoScheduledWrite();
}

void PrefService::InitFromDisk() {
  PrefReadError error = LoadPersistentPrefs();
  if (error == PREF_READ_ERROR_NONE)
    return;

  // Failing to load prefs on startup is a bad thing(TM). See bug 38352 for
  // an example problem that this can cause.
  // Do some diagnosis and try to avoid losing data.
  int message_id = 0;
  if (error <= PREF_READ_ERROR_JSON_TYPE) {
    // JSON errors indicate file corruption of some sort.
    // It's possible the user hand-edited the file, so don't clobber it yet.
    // Give them a chance to recover the file.
    // TODO(erikkay) maybe we should just move it aside and continue.
    read_only_ = true;
    message_id = IDS_PREFERENCES_CORRUPT_ERROR;
  } if (error == PREF_READ_ERROR_NO_FILE) {
    // If the file just doesn't exist, maybe this is first run.  In any case
    // there's no harm in writing out default prefs in this case.
  } else {
    // If the file exists but is simply unreadable, put the file into a state
    // where we don't try to save changes.  Otherwise, we could clobber the
    // existing prefs.
    read_only_ = true;
    message_id = IDS_PREFERENCES_UNREADABLE_ERROR;
  }

  if (message_id) {
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(&NotifyReadError, this, message_id));
  }
  UMA_HISTOGRAM_ENUMERATION("PrefService.ReadError", error, 20);
}

bool PrefService::ReloadPersistentPrefs() {
  return (LoadPersistentPrefs() == PREF_READ_ERROR_NONE);
}

PrefService::PrefReadError PrefService::LoadPersistentPrefs() {
  DCHECK(CalledOnValidThread());
  JSONFileValueSerializer serializer(writer_.path());

  int error_code = 0;
  std::string error_msg;
  scoped_ptr<Value> root(serializer.Deserialize(&error_code, &error_msg));
  if (!root.get()) {
    PLOG(ERROR) << "Error reading Preferences: " << error_msg << " " <<
        writer_.path().value();
    PrefReadError pref_error;
    switch (error_code) {
      case JSONFileValueSerializer::JSON_ACCESS_DENIED:
        pref_error = PREF_READ_ERROR_ACCESS_DENIED;
        break;
      case JSONFileValueSerializer::JSON_CANNOT_READ_FILE:
        pref_error = PREF_READ_ERROR_FILE_OTHER;
        break;
      case JSONFileValueSerializer::JSON_FILE_LOCKED:
        pref_error = PREF_READ_ERROR_FILE_LOCKED;
        break;
      case JSONFileValueSerializer::JSON_NO_SUCH_FILE:
        pref_error = PREF_READ_ERROR_NO_FILE;
        break;
      default:
        pref_error = PREF_READ_ERROR_JSON_PARSE;
        break;
    }
    return pref_error;
  }

  // Preferences should always have a dictionary root.
  if (!root->IsType(Value::TYPE_DICTIONARY))
    return PREF_READ_ERROR_JSON_TYPE;

  persistent_.reset(static_cast<DictionaryValue*>(root.release()));
  for (PreferenceSet::iterator it = prefs_.begin();
       it != prefs_.end(); ++it) {
    (*it)->root_pref_ = persistent_.get();
  }

  return PREF_READ_ERROR_NONE;
}

bool PrefService::SavePersistentPrefs() {
  DCHECK(CalledOnValidThread());

  std::string data;
  if (!SerializeData(&data))
    return false;

  // Lie about our ability to save.
  if (read_only_)
    return true;

  writer_.WriteNow(data);
  return true;
}

void PrefService::ScheduleSavePersistentPrefs() {
  DCHECK(CalledOnValidThread());

  if (read_only_)
    return;

  writer_.ScheduleWrite(this);
}

void PrefService::RegisterBooleanPref(const wchar_t* path,
                                      bool default_value) {
  Preference* pref = new Preference(persistent_.get(), path,
      Value::CreateBooleanValue(default_value));
  RegisterPreference(pref);
}

void PrefService::RegisterIntegerPref(const wchar_t* path,
                                      int default_value) {
  Preference* pref = new Preference(persistent_.get(), path,
      Value::CreateIntegerValue(default_value));
  RegisterPreference(pref);
}

void PrefService::RegisterRealPref(const wchar_t* path,
                                   double default_value) {
  Preference* pref = new Preference(persistent_.get(), path,
      Value::CreateRealValue(default_value));
  RegisterPreference(pref);
}

void PrefService::RegisterStringPref(const wchar_t* path,
                                     const std::wstring& default_value) {
  Preference* pref = new Preference(persistent_.get(), path,
      Value::CreateStringValue(default_value));
  RegisterPreference(pref);
}

void PrefService::RegisterFilePathPref(const wchar_t* path,
                                       const FilePath& default_value) {
  Preference* pref = new Preference(persistent_.get(), path,
      Value::CreateStringValue(default_value.value()));
  RegisterPreference(pref);
}

void PrefService::RegisterListPref(const wchar_t* path) {
  Preference* pref = new Preference(persistent_.get(), path,
      new ListValue);
  RegisterPreference(pref);
}

void PrefService::RegisterDictionaryPref(const wchar_t* path) {
  Preference* pref = new Preference(persistent_.get(), path,
      new DictionaryValue());
  RegisterPreference(pref);
}

void PrefService::RegisterLocalizedBooleanPref(const wchar_t* path,
                                               int locale_default_message_id) {
  Preference* pref = new Preference(persistent_.get(), path,
      CreateLocaleDefaultValue(Value::TYPE_BOOLEAN, locale_default_message_id));
  RegisterPreference(pref);
}

void PrefService::RegisterLocalizedIntegerPref(const wchar_t* path,
                                               int locale_default_message_id) {
  Preference* pref = new Preference(persistent_.get(), path,
      CreateLocaleDefaultValue(Value::TYPE_INTEGER, locale_default_message_id));
  RegisterPreference(pref);
}

void PrefService::RegisterLocalizedRealPref(const wchar_t* path,
                                            int locale_default_message_id) {
  Preference* pref = new Preference(persistent_.get(), path,
      CreateLocaleDefaultValue(Value::TYPE_REAL, locale_default_message_id));
  RegisterPreference(pref);
}

void PrefService::RegisterLocalizedStringPref(const wchar_t* path,
                                              int locale_default_message_id) {
  Preference* pref = new Preference(persistent_.get(), path,
      CreateLocaleDefaultValue(Value::TYPE_STRING, locale_default_message_id));
  RegisterPreference(pref);
}

bool PrefService::GetBoolean(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  bool result = false;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsBoolean(&result);
  DCHECK(rv);
  return result;
}

int PrefService::GetInteger(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  int result = 0;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsInteger(&result);
  DCHECK(rv);
  return result;
}

double PrefService::GetReal(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  double result = 0.0;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsReal(&result);
  DCHECK(rv);
  return result;
}

std::wstring PrefService::GetString(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  std::wstring result;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsString(&result);
  DCHECK(rv);
  return result;
}

FilePath PrefService::GetFilePath(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  FilePath::StringType result;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return FilePath(result);
  }
  bool rv = pref->GetValue()->GetAsString(&result);
  DCHECK(rv);
#if defined(OS_POSIX)
  // We store filepaths as UTF8, so convert it back to the system type.
  result = base::SysWideToNativeMB(UTF8ToWide(result));
#endif
  return FilePath(result);
}

bool PrefService::HasPrefPath(const wchar_t* path) const {
  Value* value = NULL;
  return persistent_->Get(path, &value);
}

const PrefService::Preference* PrefService::FindPreference(
    const wchar_t* pref_name) const {
  DCHECK(CalledOnValidThread());
  Preference p(NULL, pref_name, NULL);
  PreferenceSet::const_iterator it = prefs_.find(&p);
  return it == prefs_.end() ? NULL : *it;
}

const DictionaryValue* PrefService::GetDictionary(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return NULL;
  }
  const Value* value = pref->GetValue();
  if (value->GetType() == Value::TYPE_NULL)
    return NULL;
  return static_cast<const DictionaryValue*>(value);
}

const ListValue* PrefService::GetList(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return NULL;
  }
  const Value* value = pref->GetValue();
  if (value->GetType() == Value::TYPE_NULL)
    return NULL;
  return static_cast<const ListValue*>(value);
}

void PrefService::AddPrefObserver(const wchar_t* path,
                                  NotificationObserver* obs) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to add an observer for an unregistered pref: "
        << path;
    return;
  }

  // Get the pref observer list associated with the path.
  NotificationObserverList* observer_list = NULL;
  PrefObserverMap::iterator observer_iterator = pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end()) {
    observer_list = new NotificationObserverList;
    pref_observers_[path] = observer_list;
  } else {
    observer_list = observer_iterator->second;
  }

  // Verify that this observer doesn't already exist.
  NotificationObserverList::Iterator it(*observer_list);
  NotificationObserver* existing_obs;
  while ((existing_obs = it.GetNext()) != NULL) {
    DCHECK(existing_obs != obs) << path << " observer already registered";
    if (existing_obs == obs)
      return;
  }

  // Ok, safe to add the pref observer.
  observer_list->AddObserver(obs);
}

void PrefService::RemovePrefObserver(const wchar_t* path,
                                     NotificationObserver* obs) {
  DCHECK(CalledOnValidThread());

  PrefObserverMap::iterator observer_iterator = pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end()) {
    return;
  }

  NotificationObserverList* observer_list = observer_iterator->second;
  observer_list->RemoveObserver(obs);
}

void PrefService::RegisterPreference(Preference* pref) {
  DCHECK(CalledOnValidThread());

  if (FindPreference(pref->name().c_str())) {
    NOTREACHED() << "Tried to register duplicate pref " << pref->name();
    delete pref;
    return;
  }
  prefs_.insert(pref);
}

void PrefService::ClearPref(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to clear an unregistered pref: " << path;
    return;
  }

  Value* value;
  bool has_old_value = persistent_->Get(path, &value);
  persistent_->Remove(path, NULL);

  if (has_old_value)
    FireObservers(path);
}

void PrefService::Set(const wchar_t* path, const Value& value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }

  // Allow dictionary and list types to be set to null.
  if (value.GetType() == Value::TYPE_NULL &&
      (pref->type() == Value::TYPE_DICTIONARY ||
       pref->type() == Value::TYPE_LIST)) {
    scoped_ptr<Value> old_value(GetPrefCopy(path));
    if (!old_value->Equals(&value)) {
      persistent_->Remove(path, NULL);
      FireObservers(path);
    }
    return;
  }

  if (pref->type() != value.GetType()) {
    NOTREACHED() << "Wrong type for Set: " << path;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  persistent_->Set(path, value.DeepCopy());

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetBoolean(const wchar_t* path, bool value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->type() != Value::TYPE_BOOLEAN) {
    NOTREACHED() << "Wrong type for SetBoolean: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  persistent_->SetBoolean(path, value);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetInteger(const wchar_t* path, int value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->type() != Value::TYPE_INTEGER) {
    NOTREACHED() << "Wrong type for SetInteger: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  persistent_->SetInteger(path, value);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetReal(const wchar_t* path, double value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->type() != Value::TYPE_REAL) {
    NOTREACHED() << "Wrong type for SetReal: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  persistent_->SetReal(path, value);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetString(const wchar_t* path, const std::wstring& value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->type() != Value::TYPE_STRING) {
    NOTREACHED() << "Wrong type for SetString: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  persistent_->SetString(path, value);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetFilePath(const wchar_t* path, const FilePath& value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->type() != Value::TYPE_STRING) {
    NOTREACHED() << "Wrong type for SetFilePath: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
#if defined(OS_POSIX)
  // Value::SetString only knows about UTF8 strings, so convert the path from
  // the system native value to UTF8.
  std::string path_utf8 = WideToUTF8(base::SysNativeMBToWide(value.value()));
  persistent_->SetString(path, path_utf8);
#else
  persistent_->SetString(path, value.value());
#endif

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetInt64(const wchar_t* path, int64 value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->type() != Value::TYPE_STRING) {
    NOTREACHED() << "Wrong type for SetInt64: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  persistent_->SetString(path, Int64ToWString(value));

  FireObserversIfChanged(path, old_value.get());
}

int64 PrefService::GetInt64(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return 0;
  }
  std::wstring result(L"0");
  bool rv = pref->GetValue()->GetAsString(&result);
  DCHECK(rv);
  return StringToInt64(WideToUTF16Hack(result));
}

void PrefService::RegisterInt64Pref(const wchar_t* path, int64 default_value) {
  Preference* pref = new Preference(persistent_.get(), path,
      Value::CreateStringValue(Int64ToWString(default_value)));
  RegisterPreference(pref);
}

DictionaryValue* PrefService::GetMutableDictionary(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return NULL;
  }
  if (pref->type() != Value::TYPE_DICTIONARY) {
    NOTREACHED() << "Wrong type for GetMutableDictionary: " << path;
    return NULL;
  }

  DictionaryValue* dict = NULL;
  if (!persistent_->GetDictionary(path, &dict)) {
    dict = new DictionaryValue;
    persistent_->Set(path, dict);
  }
  return dict;
}

ListValue* PrefService::GetMutableList(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return NULL;
  }
  if (pref->type() != Value::TYPE_LIST) {
    NOTREACHED() << "Wrong type for GetMutableList: " << path;
    return NULL;
  }

  ListValue* list = NULL;
  if (!persistent_->GetList(path, &list)) {
    list = new ListValue;
    persistent_->Set(path, list);
  }
  return list;
}

Value* PrefService::GetPrefCopy(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  DCHECK(pref);
  return pref->GetValue()->DeepCopy();
}

void PrefService::FireObserversIfChanged(const wchar_t* path,
                                         const Value* old_value) {
  Value* new_value = NULL;
  persistent_->Get(path, &new_value);
  if (!old_value->Equals(new_value))
    FireObservers(path);
}

void PrefService::FireObservers(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  // Convert path to a std::wstring because the Details constructor requires a
  // class.
  std::wstring path_str(path);
  PrefObserverMap::iterator observer_iterator = pref_observers_.find(path_str);
  if (observer_iterator == pref_observers_.end())
    return;

  NotificationObserverList::Iterator it(*(observer_iterator->second));
  NotificationObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    observer->Observe(NotificationType::PREF_CHANGED,
                      Source<PrefService>(this),
                      Details<std::wstring>(&path_str));
  }
}

bool PrefService::SerializeData(std::string* output) {
  // TODO(tc): Do we want to prune webkit preferences that match the default
  // value?
  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  scoped_ptr<DictionaryValue> copy(persistent_->DeepCopyWithoutEmptyChildren());
  return serializer.Serialize(*(copy.get()));
}

///////////////////////////////////////////////////////////////////////////////
// PrefService::Preference

PrefService::Preference::Preference(DictionaryValue* root_pref,
                                    const wchar_t* name,
                                    Value* default_value)
      : type_(Value::TYPE_NULL),
        name_(name),
        default_value_(default_value),
        root_pref_(root_pref) {
  DCHECK(name);

  if (default_value) {
    type_ = default_value->GetType();
    DCHECK(type_ != Value::TYPE_NULL && type_ != Value::TYPE_BINARY) <<
        "invalid preference type: " << type_;
  }

  // We set the default value of lists and dictionaries to be null so it's
  // easier for callers to check for empty list/dict prefs.
  if (Value::TYPE_LIST == type_ || Value::TYPE_DICTIONARY == type_)
    default_value_.reset(Value::CreateNullValue());
}

const Value* PrefService::Preference::GetValue() const {
  DCHECK(NULL != root_pref_) <<
      "Must register pref before getting its value";

  Value* temp_value = NULL;
  if (root_pref_->Get(name_.c_str(), &temp_value) &&
      temp_value->GetType() == type_) {
    return temp_value;
  }

  // Pref not found, just return the app default.
  return default_value_.get();
}

bool PrefService::Preference::IsDefaultValue() const {
  DCHECK(default_value_.get());
  return default_value_->Equals(GetValue());
}
