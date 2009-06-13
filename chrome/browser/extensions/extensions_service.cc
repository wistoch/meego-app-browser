// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_service.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/crypto/signature_verifier.h"
#include "base/file_util.h"
#include "base/gfx/png_encoder.h"
#include "base/scoped_handle.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/third_party/nss/blapi.h"
#include "base/third_party/nss/sha256.h"
#include "base/thread.h"
#include "base/values.h"
#include "net/base/file_stream.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/external_extension_provider.h"
#include "chrome/browser/extensions/external_pref_extension_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/utility_process_host.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/extensions/extension_unpacker.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/zip.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/base64.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#include "base/win_util.h"
#include "chrome/browser/extensions/external_registry_extension_provider_win.h"
#endif

// ExtensionsService.

const char ExtensionsService::kExtensionHeaderMagic[] = "Cr24";

const char* ExtensionsService::kInstallDirectoryName = "Extensions";
const char* ExtensionsService::kCurrentVersionFileName = "Current Version";
const char* ExtensionsServiceBackend::kTempExtensionName = "TEMP_INSTALL";

namespace {

// A preference that keeps track of extension settings. This is a dictionary
// object read from the Preferences file, keyed off of extension id's.
const wchar_t kExternalExtensionsPref[] = L"extensions.settings";

// A preference keeping track of how the extension was installed.
const wchar_t kLocation[] = L"location";
const wchar_t kState[] = L"state";

// A temporary subdirectory where we unpack extensions.
const char* kUnpackExtensionDir = "TEMP_UNPACK";

// Unpacking errors
const char* kBadMagicNumberError = "Bad magic number";
const char* kBadHeaderSizeError = "Excessively large key or signature";
const char* kBadVersionNumberError = "Bad version number";
const char* kInvalidExtensionHeaderError = "Invalid extension header";
const char* kInvalidPublicKeyError = "Invalid public key";
const char* kInvalidSignatureError = "Invalid signature";
const char* kSignatureVerificationFailed = "Signature verification failed";
const char* kSignatureVerificationInitFailed =
    "Signature verification initialization failed. This is most likely "
    "caused by a public key in the wrong format (should encode algorithm).";
}

// This class coordinates an extension unpack task which is run in a separate
// process.  Results are sent back to this class, which we route to the
// ExtensionServiceBackend.
class ExtensionsServiceBackend::UnpackerClient
    : public UtilityProcessHost::Client {
 public:
  UnpackerClient(ExtensionsServiceBackend* backend,
                 const FilePath& extension_path,
                 const std::string& public_key,
                 const std::string& expected_id,
                 bool from_external)
    : backend_(backend), extension_path_(extension_path),
      public_key_(public_key), expected_id_(expected_id),
      from_external_(from_external), got_response_(false) {
  }

  // Starts the unpack task.  We call back to the backend when the task is done,
  // or a problem occurs.
  void Start() {
    AddRef();  // balanced in OnUnpackExtensionReply()

    // TODO(mpcomplete): handle multiple installs
    FilePath temp_dir = backend_->install_directory_.AppendASCII(
        kUnpackExtensionDir);
    if (!file_util::CreateDirectory(temp_dir)) {
      backend_->ReportExtensionInstallError(extension_path_,
          "Failed to create temporary directory.");
      return;
    }

    temp_extension_path_ = temp_dir.Append(extension_path_.BaseName());
    if (!file_util::CopyFile(extension_path_, temp_extension_path_)) {
      backend_->ReportExtensionInstallError(extension_path_,
          "Failed to copy extension file to temporary directory.");
      return;
    }

    if (backend_->resource_dispatcher_host_) {
      ChromeThread::GetMessageLoop(ChromeThread::IO)->PostTask(FROM_HERE,
          NewRunnableMethod(this, &UnpackerClient::StartProcessOnIOThread,
                            backend_->resource_dispatcher_host_,
                            MessageLoop::current()));
    } else {
      // Cheesy... but if we don't have a ResourceDispatcherHost, assume we're
      // in a unit test and run the unpacker directly in-process.
      ExtensionUnpacker unpacker(temp_extension_path_);
      if (unpacker.Run()) {
        OnUnpackExtensionSucceededImpl(*unpacker.parsed_manifest(),
                                       unpacker.decoded_images());
      } else {
        OnUnpackExtensionFailed(unpacker.error_message());
      }
    }
  }

 private:
  // UtilityProcessHost::Client
  virtual void OnProcessCrashed() {
    // Don't report crashes if they happen after we got a response.
    if (got_response_)
      return;

    OnUnpackExtensionFailed("Chrome crashed while trying to install.");
  }

  virtual void OnUnpackExtensionSucceeded(const DictionaryValue& manifest) {
     ExtensionUnpacker::DecodedImages images;
     if (!ExtensionUnpacker::ReadImagesFromFile(temp_extension_path_,
                                                &images)) {
       OnUnpackExtensionFailed("Couldn't read image data from disk.");
     } else {
       OnUnpackExtensionSucceededImpl(manifest, images);
     }
  }

  void OnUnpackExtensionSucceededImpl(
      const DictionaryValue& manifest,
      const ExtensionUnpacker::DecodedImages& images) {
    // Add our public key into the parsed manifest. We want it to be saved so
    // that we can later refer to it (eg for generating ids, validating
    // signatures, etc).
    // The const_cast is hacky, but seems like the right thing here, rather than
    // making a full copy just to make this change.
    const_cast<DictionaryValue*>(&manifest)->SetString(
        Extension::kPublicKeyKey, public_key_);

    // The extension was unpacked to the temp dir inside our unpacking dir.
    FilePath extension_dir = temp_extension_path_.DirName().AppendASCII(
        ExtensionsServiceBackend::kTempExtensionName);
    backend_->OnExtensionUnpacked(extension_path_, extension_dir,
                                  expected_id_, from_external_,
                                  manifest, images);
    Cleanup();
  }

  virtual void OnUnpackExtensionFailed(const std::string& error_message) {
    backend_->ReportExtensionInstallError(extension_path_, error_message);
    Cleanup();
  }

  // Cleans up our temp directory.
  void Cleanup() {
    if (got_response_)
      return;

    got_response_ = true;
    file_util::Delete(temp_extension_path_.DirName(), true);
    Release();  // balanced in Run()
  }

  // Starts the utility process that unpacks our extension.
  void StartProcessOnIOThread(ResourceDispatcherHost* rdh,
                              MessageLoop* file_loop) {
    UtilityProcessHost* host = new UtilityProcessHost(rdh, this, file_loop);
    host->StartExtensionUnpacker(temp_extension_path_);
  }

  scoped_refptr<ExtensionsServiceBackend> backend_;

  // The path to the crx file that we're installing.
  FilePath extension_path_;

  // The public key of the extension we're installing.
  std::string public_key_;

  // The path to the copy of the crx file in the temporary directory where we're
  // unpacking it.
  FilePath temp_extension_path_;

  // The ID we expect this extension to have, if any.
  std::string expected_id_;

  // True if this is being installed from an external source.
  bool from_external_;

  // True if we got a response from the utility process and have cleaned up
  // already.
  bool got_response_;
};

ExtensionsService::ExtensionsService(Profile* profile,
                                     MessageLoop* frontend_loop,
                                     MessageLoop* backend_loop)
    : prefs_(profile->GetPrefs()),
      backend_loop_(backend_loop),
      install_directory_(profile->GetPath().AppendASCII(kInstallDirectoryName)),
      extensions_enabled_(
          CommandLine::ForCurrentProcess()->
          HasSwitch(switches::kEnableExtensions)),
      show_extensions_prompts_(true) {
  // We pass ownership of this object to the Backend.
  DictionaryValue* external_extensions = new DictionaryValue;
  prefs_->RegisterDictionaryPref(kExternalExtensionsPref);
  GetExternalExtensions(external_extensions, NULL);
  backend_ = new ExtensionsServiceBackend(
          install_directory_, g_browser_process->resource_dispatcher_host(),
          frontend_loop, external_extensions);
}

ExtensionsService::~ExtensionsService() {
  for (ExtensionList::iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    delete *iter;
  }
}

bool ExtensionsService::Init() {
  // Start up the extension event routers.
  ExtensionBrowserEventRouter::GetInstance()->Init();

  scoped_ptr<DictionaryValue> external_extensions(new DictionaryValue);
  GetExternalExtensions(external_extensions.get(), NULL);

  scoped_ptr< std::set<std::string> >
      killed_extensions(new std::set<std::string>);
  GetExternalExtensions(NULL, killed_extensions.get());

  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::CheckForExternalUpdates,
      *killed_extensions.get(),
      scoped_refptr<ExtensionsService>(this)));

  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::LoadExtensionsFromInstallDirectory,
      scoped_refptr<ExtensionsService>(this),
      external_extensions.release()));

  return true;
}

void ExtensionsService::InstallExtension(const FilePath& extension_path) {
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::InstallExtension,
      extension_path,
      scoped_refptr<ExtensionsService>(this)));
}

void ExtensionsService::UninstallExtension(const std::string& extension_id) {
  Extension* extension = NULL;

  ExtensionList::iterator iter;
  for (iter = extensions_.begin(); iter != extensions_.end(); ++iter) {
    if ((*iter)->id() == extension_id) {
      extension = *iter;
      break;
    }
  }

  // Callers should not send us nonexistant extensions.
  CHECK(extension);

  // Remove the extension from our list.
  extensions_.erase(iter);

  // Tell other services the extension is gone.
  NotificationService::current()->Notify(NotificationType::EXTENSION_UNLOADED,
                                         NotificationService::AllSources(),
                                         Details<Extension>(extension));

  // For external extensions, we save a preference reminding ourself not to try
  // and install the extension anymore.
  if (Extension::IsExternalLocation(extension->location())) {
    UpdateExtensionPref(ASCIIToWide(extension->id()), kState,
                        Value::CreateIntegerValue(Extension::KILLBIT), true);
  } else {
    UpdateExtensionPref(ASCIIToWide(extension->id()), kState,
                        Value::CreateIntegerValue(Extension::DISABLED), true);
  }

  // Tell the backend to start deleting installed extensions on the file thread.
  if (extension->location() == Extension::INTERNAL ||
      Extension::IsExternalLocation(extension->location())) {
    backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
        &ExtensionsServiceBackend::UninstallExtension, extension_id));
  }

  delete extension;
}

void ExtensionsService::LoadExtension(const FilePath& extension_path) {
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::LoadSingleExtension,
      extension_path, scoped_refptr<ExtensionsService>(this)));
}

void ExtensionsService::OnExtensionsLoaded(ExtensionList* new_extensions) {
  // Filter out any extensions we don't want to enable. Themes are always
  // enabled, but other extensions are only loaded if --enable-extensions is
  // present.
  ExtensionList enabled_extensions;
  for (ExtensionList::iterator iter = new_extensions->begin();
       iter != new_extensions->end(); ++iter) {
    if (extensions_enabled() || (*iter)->IsTheme()) {
      enabled_extensions.push_back(*iter);
    } else {
      // Extensions that get enabled get added to extensions_ and deleted later.
      // Anything skipped must be deleted now so we don't leak.
      delete *iter;
    }
  }

  for (ExtensionList::const_iterator iter = enabled_extensions.begin();
       iter != enabled_extensions.end(); ++iter) {
    std::wstring extension_id = ASCIIToWide((*iter)->id());
    DictionaryValue* pref = GetOrCreateExtensionPref(extension_id);
    int location;
    int state;

    // Ensure all loaded extensions have a preference set. This deals with a
    // legacy problem where some extensions were installed before we were
    // storing state in the preferences.
    // TODO(aa): We should remove this eventually.
    if (!pref->GetInteger(kLocation, &location) ||
        !pref->GetInteger(kState, &state)) {
      UpdateExtensionPref(extension_id,
          kLocation, Value::CreateIntegerValue(Extension::INTERNAL), false);
      UpdateExtensionPref(extension_id,
          kState, Value::CreateIntegerValue(Extension::ENABLED), false);
    } else {
      // Sanity check: The kill-bit should only ever be set on external
      // extensions.
      DCHECK(state != Extension::KILLBIT ||
             Extension::IsExternalLocation(
                static_cast<Extension::Location>(location)));
    }

    extensions_.push_back(*iter);
  }

  NotificationService::current()->Notify(
      NotificationType::EXTENSIONS_LOADED,
      NotificationService::AllSources(),
      Details<ExtensionList>(&enabled_extensions));

  delete new_extensions;
}

void ExtensionsService::OnExtensionInstalled(Extension* extension,
    Extension::InstallType install_type) {
  UpdateExtensionPref(ASCIIToWide(extension->id()), kState,
                      Value::CreateIntegerValue(Extension::ENABLED), false);
  UpdateExtensionPref(ASCIIToWide(extension->id()), kLocation,
                      Value::CreateIntegerValue(Extension::INTERNAL), true);

  // If the extension is a theme, tell the profile (and therefore ThemeProvider)
  // to apply it.
  if (extension->IsTheme()) {
    NotificationService::current()->Notify(
        NotificationType::THEME_INSTALLED,
        NotificationService::AllSources(),
        Details<Extension>(extension));
  } else {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_INSTALLED,
        NotificationService::AllSources(),
        Details<Extension>(extension));
  }
}

void ExtensionsService::OnExternalExtensionInstalled(
    const std::string& id, Extension::Location location) {
  DCHECK(Extension::IsExternalLocation(location));
  UpdateExtensionPref(ASCIIToWide(id), kState,
                      Value::CreateIntegerValue(Extension::ENABLED), false);
  UpdateExtensionPref(ASCIIToWide(id), kLocation,
                      Value::CreateIntegerValue(location), true);
}

void ExtensionsService::OnExtensionOverinstallAttempted(const std::string& id) {
  Extension* extension = GetExtensionByID(id);
  if (extension && extension->IsTheme()) {
    NotificationService::current()->Notify(
        NotificationType::THEME_INSTALLED,
        NotificationService::AllSources(),
        Details<Extension>(extension));
  }
}

Extension* ExtensionsService::GetExtensionByID(std::string id) {
  for (ExtensionList::const_iterator iter = extensions_.begin();
      iter != extensions_.end(); ++iter) {
    if ((*iter)->id() == id)
      return *iter;
  }
  return NULL;
}

void ExtensionsService::GetExternalExtensions(
    DictionaryValue* external_extensions,
    std::set<std::string>* killed_extensions) {
  const DictionaryValue* dict = prefs_->GetDictionary(kExternalExtensionsPref);
  if (!dict || dict->GetSize() == 0)
    return;

  for (DictionaryValue::key_iterator i = dict->begin_keys();
       i != dict->end_keys(); ++i) {
    std::wstring key_name = *i;
    if (!Extension::IdIsValid(WideToASCII(key_name))) {
      LOG(WARNING) << "Invalid external extension ID encountered: "
                   << WideToASCII(key_name);
      continue;
    }

    DictionaryValue* extension = NULL;
    if (!dict->GetDictionary(key_name, &extension)) {
      NOTREACHED();
      continue;
    }

    // Check to see if the extension has been killed.
    int state;
    if (extension->GetInteger(kState, &state) &&
        state == static_cast<int>(Extension::KILLBIT)) {
      if (killed_extensions) {
        StringToLowerASCII(&key_name);
        killed_extensions->insert(WideToASCII(key_name));
      }
    }
    // Return all extensions found.
    if (external_extensions) {
      DictionaryValue* result =
          static_cast<DictionaryValue*>(extension->DeepCopy());
      StringToLowerASCII(&key_name);
      external_extensions->Set(key_name, result);
    }
  }
}

DictionaryValue* ExtensionsService::GetOrCreateExtensionPref(
    const std::wstring& extension_id) {
  DictionaryValue* dict = prefs_->GetMutableDictionary(kExternalExtensionsPref);
  DictionaryValue* extension = NULL;
  if (!dict->GetDictionary(extension_id, &extension)) {
    // Extension pref does not exist, create it.
    extension = new DictionaryValue();
    dict->Set(extension_id, extension);
  }

  return extension;
}

void ExtensionsService::ClearProvidersForTesting() {
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::ClearProvidersForTesting));
}

void ExtensionsService::SetProviderForTesting(
    Extension::Location location, ExternalExtensionProvider* test_provider) {
  backend_loop_->PostTask(FROM_HERE, NewRunnableMethod(backend_.get(),
      &ExtensionsServiceBackend::SetProviderForTesting,
      location, test_provider));
}

bool ExtensionsService::UpdateExtensionPref(const std::wstring& extension_id,
                                            const std::wstring& key,
                                            Value* data_value,
                                            bool schedule_save) {
  DictionaryValue* extension = GetOrCreateExtensionPref(extension_id);
  if (!extension->Set(key, data_value)) {
    NOTREACHED() << L"Cannot modify key: '" << key.c_str()
                 << "' for extension: '" << extension_id.c_str() << "'";
    return false;
  }

  if (schedule_save)
    prefs_->ScheduleSavePersistentPrefs();
  return true;
}

// ExtensionsServicesBackend

ExtensionsServiceBackend::ExtensionsServiceBackend(
    const FilePath& install_directory, ResourceDispatcherHost* rdh,
    MessageLoop* frontend_loop, DictionaryValue* extension_prefs)
        : frontend_(NULL),
          install_directory_(install_directory),
          resource_dispatcher_host_(rdh),
          alert_on_error_(false),
          frontend_loop_(frontend_loop) {
  external_extension_providers_[Extension::EXTERNAL_PREF] =
      new ExternalPrefExtensionProvider(extension_prefs);
#if defined(OS_WIN)
  external_extension_providers_[Extension::EXTERNAL_REGISTRY] =
      new ExternalRegistryExtensionProvider();
#endif
}

ExtensionsServiceBackend::~ExtensionsServiceBackend() {
  STLDeleteContainerPairSecondPointers(external_extension_providers_.begin(),
                                       external_extension_providers_.end());
}

void ExtensionsServiceBackend::LoadExtensionsFromInstallDirectory(
    scoped_refptr<ExtensionsService> frontend,
    DictionaryValue* extension_prefs) {
  frontend_ = frontend;
  alert_on_error_ = false;
  scoped_ptr<DictionaryValue> external_extensions(extension_prefs);

#if defined(OS_WIN)
  // On POSIX, AbsolutePath() calls realpath() which returns NULL if
  // it does not exist.  Instead we absolute-ify after creation in
  // case that is needed.
  // TODO(port): does this really need to happen before
  // CreateDirectory() on Windows?
  if (!file_util::AbsolutePath(&install_directory_))
    NOTREACHED();
#endif

  scoped_ptr<ExtensionList> extensions(new ExtensionList);

  // Create the <Profile>/Extensions directory if it doesn't exist.
  if (!file_util::DirectoryExists(install_directory_)) {
    file_util::CreateDirectory(install_directory_);
    LOG(INFO) << "Created Extensions directory.  No extensions to install.";
    ReportExtensionsLoaded(extensions.release());
    return;
  }

#if !defined(OS_WIN)
  if (!file_util::AbsolutePath(&install_directory_))
    NOTREACHED();
#endif

  LOG(INFO) << "Loading installed extensions...";

  // Find all child directories in the install directory and load their
  // manifests. Post errors and results to the frontend.
  file_util::FileEnumerator enumerator(install_directory_,
                                       false,  // Not recursive.
                                       file_util::FileEnumerator::DIRECTORIES);
  FilePath extension_path;
  for (extension_path = enumerator.Next(); !extension_path.value().empty();
       extension_path = enumerator.Next()) {
    std::string extension_id = WideToASCII(
        extension_path.BaseName().ToWStringHack());

    // The utility process might be in the middle of unpacking an extension, so
    // ignore the temp unpacking directory.
    if (extension_id == kUnpackExtensionDir)
      continue;

    // Ignore directories that aren't valid IDs.
    if (!Extension::IdIsValid(extension_id)) {
      LOG(WARNING) << "Invalid extension ID encountered in extensions "
                      "directory: " << extension_id;
      continue;
    }

    // If there is no Current Version file, just delete the directory and move
    // on. This can legitimately happen when an uninstall does not complete, for
    // example, when a plugin is in use at uninstall time.
    FilePath current_version_path = extension_path.AppendASCII(
        ExtensionsService::kCurrentVersionFileName);
    if (!file_util::PathExists(current_version_path)) {
      LOG(INFO) << "Deleting incomplete install for directory "
                << WideToASCII(extension_path.ToWStringHack()) << ".";
      file_util::Delete(extension_path, true);  // Recursive.
      continue;
    }

    std::string current_version;
    if (!ReadCurrentVersion(extension_path, &current_version))
      continue;

    Extension::Location location = Extension::INVALID;
    int location_value;
    DictionaryValue* pref = NULL;
    external_extensions->GetDictionary(ASCIIToWide(extension_id), &pref);
    if (!pref ||
        !pref->GetInteger(kLocation, &location_value)) {
      // Check with the external providers.
      if (!LookupExternalExtension(extension_id, NULL, &location))
        location = Extension::INTERNAL;
    } else {
      location = static_cast<Extension::Location>(location_value);
    }

    FilePath version_path = extension_path.AppendASCII(current_version);
    if (Extension::IsExternalLocation(location) &&
        CheckExternalUninstall(external_extensions.get(),
                               version_path, extension_id)) {
      // TODO(erikkay): Possibly defer this operation to avoid slowing initial
      // load of extensions.
      UninstallExtension(extension_id);

      // No error needs to be reported.  The extension effectively doesn't
      // exist.
      continue;
    }

    Extension* extension = LoadExtension(version_path,
                                         location,
                                         true);  // require id
    if (extension)
      extensions->push_back(extension);
  }

  LOG(INFO) << "Done.";
  ReportExtensionsLoaded(extensions.release());
}

void ExtensionsServiceBackend::LoadSingleExtension(
    const FilePath& path_in, scoped_refptr<ExtensionsService> frontend) {
  frontend_ = frontend;

  // Explicit UI loads are always noisy.
  alert_on_error_ = true;

  FilePath extension_path = path_in;
  if (!file_util::AbsolutePath(&extension_path))
    NOTREACHED();

  LOG(INFO) << "Loading single extension from " <<
      WideToASCII(extension_path.BaseName().ToWStringHack());

  Extension* extension = LoadExtension(extension_path,
                                       Extension::LOAD,
                                       false);  // don't require ID
  if (extension) {
    ExtensionList* extensions = new ExtensionList;
    extensions->push_back(extension);
    ReportExtensionsLoaded(extensions);
  }
}

DictionaryValue* ExtensionsServiceBackend::ReadManifest(FilePath manifest_path,
                                                        std::string* error) {
  JSONFileValueSerializer serializer(manifest_path);
  scoped_ptr<Value> root(serializer.Deserialize(error));
  if (!root.get())
    return NULL;

  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    *error = Extension::kInvalidManifestError;
    return NULL;
  }

  return static_cast<DictionaryValue*>(root.release());
}

Extension* ExtensionsServiceBackend::LoadExtension(
    const FilePath& extension_path,
    Extension::Location location,
    bool require_id) {
  FilePath manifest_path =
      extension_path.AppendASCII(Extension::kManifestFilename);
  if (!file_util::PathExists(manifest_path)) {
    ReportExtensionLoadError(extension_path, Extension::kInvalidManifestError);
    return NULL;
  }

  std::string error;
  scoped_ptr<DictionaryValue> root(ReadManifest(manifest_path, &error));
  if (!root.get()) {
    ReportExtensionLoadError(extension_path, error);
    return NULL;
  }

  scoped_ptr<Extension> extension(new Extension(extension_path));
  if (!extension->InitFromValue(*root.get(), require_id, &error)) {
    ReportExtensionLoadError(extension_path, error);
    return NULL;
  }

  extension->set_location(location);

  // Theme resource validation.
  if (extension->IsTheme()) {
    DictionaryValue* images_value = extension->GetThemeImages();
    DictionaryValue::key_iterator iter = images_value->begin_keys();
    while (iter != images_value->end_keys()) {
      std::string val;
      if (images_value->GetString(*iter , &val)) {
        FilePath image_path = extension->path().AppendASCII(val);
        if (!file_util::PathExists(image_path)) {
          ReportExtensionLoadError(extension_path,
              StringPrintf("Could not load '%s' for theme.",
              WideToUTF8(image_path.ToWStringHack()).c_str()));
          return NULL;
        }
      }
      ++iter;
    }

    // Themes cannot contain other extension types.
    return extension.release();
  }

  // Validate that claimed script resources actually exist.
  for (size_t i = 0; i < extension->content_scripts().size(); ++i) {
    const UserScript& script = extension->content_scripts()[i];

    for (size_t j = 0; j < script.js_scripts().size(); j++) {
      const FilePath& path = script.js_scripts()[j].path();
      if (!file_util::PathExists(path)) {
        ReportExtensionLoadError(extension_path,
            StringPrintf("Could not load '%s' for content script.",
            WideToUTF8(path.ToWStringHack()).c_str()));
        return NULL;
      }
    }

    for (size_t j = 0; j < script.css_scripts().size(); j++) {
      const FilePath& path = script.css_scripts()[j].path();
      if (!file_util::PathExists(path)) {
        ReportExtensionLoadError(extension_path,
            StringPrintf("Could not load '%s' for content script.",
            WideToUTF8(path.ToWStringHack()).c_str()));
        return NULL;
      }
    }
  }

  for (size_t i = 0; i < extension->plugins().size(); ++i) {
    const Extension::PluginInfo& plugin = extension->plugins()[i];
    if (!file_util::PathExists(plugin.path)) {
      ReportExtensionLoadError(extension_path,
          StringPrintf("Could not load '%s' for plugin.",
          WideToUTF8(plugin.path.ToWStringHack()).c_str()));
      return NULL;
    }
  }

  // Validate icon location for page actions.
  const PageActionMap& page_actions = extension->page_actions();
  for (PageActionMap::const_iterator i(page_actions.begin());
       i != page_actions.end(); ++i) {
    PageAction* page_action = i->second;
    FilePath path = page_action->icon_path();
    if (!file_util::PathExists(path)) {
      ReportExtensionLoadError(extension_path,
          StringPrintf("Could not load icon '%s' for page action.",
          WideToUTF8(path.ToWStringHack()).c_str()));
      return NULL;
    }
  }

  return extension.release();
}

void ExtensionsServiceBackend::ReportExtensionLoadError(
    const FilePath& extension_path, const std::string &error) {
  // TODO(port): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToASCII(extension_path.ToWStringHack());
  std::string message = StringPrintf("Could not load extension from '%s'. %s",
                                     path_str.c_str(), error.c_str());
  ExtensionErrorReporter::GetInstance()->ReportError(message, alert_on_error_);
}

void ExtensionsServiceBackend::ReportExtensionsLoaded(
    ExtensionList* extensions) {
  frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_, &ExtensionsService::OnExtensionsLoaded, extensions));
}

bool ExtensionsServiceBackend::ReadCurrentVersion(const FilePath& dir,
                                                  std::string* version_string) {
  FilePath current_version =
      dir.AppendASCII(ExtensionsService::kCurrentVersionFileName);
  if (file_util::PathExists(current_version)) {
    if (file_util::ReadFileToString(current_version, version_string)) {
      TrimWhitespace(*version_string, TRIM_ALL, version_string);
      return true;
    }
  }
  return false;
}

Extension::InstallType ExtensionsServiceBackend::CompareToInstalledVersion(
    const std::string& id,
    const std::string& new_version_str,
    std::string *current_version_str) {
  CHECK(current_version_str);
  FilePath dir(install_directory_.AppendASCII(id.c_str()));
  if (!ReadCurrentVersion(dir, current_version_str))
    return Extension::NEW_INSTALL;

  scoped_ptr<Version> current_version(
    Version::GetVersionFromString(*current_version_str));
  scoped_ptr<Version> new_version(
    Version::GetVersionFromString(new_version_str));
  int comp = new_version->CompareTo(*current_version);
  if (comp > 0)
    return Extension::UPGRADE;
  else if (comp == 0)
    return Extension::REINSTALL;
  else
    return Extension::DOWNGRADE;
}

bool ExtensionsServiceBackend::NeedsReinstall(const std::string& id,
    const std::string& current_version) {
  // Verify that the directory actually exists.
  // TODO(erikkay): A further step would be to verify that the extension
  // has actually loaded successfully.
  FilePath dir(install_directory_.AppendASCII(id.c_str()));
  FilePath version_dir(dir.AppendASCII(current_version));
  return !file_util::PathExists(version_dir);
}

bool ExtensionsServiceBackend::InstallDirSafely(const FilePath& source_dir,
                                                const FilePath& dest_dir) {
  if (file_util::PathExists(dest_dir)) {
    // By the time we get here, it should be safe to assume that this directory
    // is not currently in use (it's not the current active version).
    if (!file_util::Delete(dest_dir, true)) {
      ReportExtensionInstallError(source_dir,
          "Can't delete existing version directory.");
      return false;
    }
  } else {
    FilePath parent = dest_dir.DirName();
    if (!file_util::DirectoryExists(parent)) {
      if (!file_util::CreateDirectory(parent)) {
        ReportExtensionInstallError(source_dir,
                                    "Couldn't create extension directory.");
        return false;
      }
    }
  }
  if (!file_util::Move(source_dir, dest_dir)) {
    ReportExtensionInstallError(source_dir,
                                "Couldn't move temporary directory.");
    return false;
  }

  return true;
}

bool ExtensionsServiceBackend::SetCurrentVersion(const FilePath& dest_dir,
                                                 std::string version) {
  // Write out the new CurrentVersion file.
  // <profile>/Extension/<name>/CurrentVersion
  FilePath current_version =
      dest_dir.AppendASCII(ExtensionsService::kCurrentVersionFileName);
  FilePath current_version_old =
    current_version.InsertBeforeExtension(FILE_PATH_LITERAL("_old"));
  if (file_util::PathExists(current_version_old)) {
    if (!file_util::Delete(current_version_old, false)) {
      ReportExtensionInstallError(dest_dir,
                                  "Couldn't remove CurrentVersion_old file.");
      return false;
    }
  }
  if (file_util::PathExists(current_version)) {
    if (!file_util::Move(current_version, current_version_old)) {
      ReportExtensionInstallError(dest_dir,
                                  "Couldn't move CurrentVersion file.");
      return false;
    }
  }
  net::FileStream stream;
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;
  if (stream.Open(current_version, flags) != 0)
    return false;
  if (stream.Write(version.c_str(), version.size(), NULL) < 0) {
    // Restore the old CurrentVersion.
    if (file_util::PathExists(current_version_old)) {
      if (!file_util::Move(current_version_old, current_version)) {
        LOG(WARNING) << "couldn't restore " << current_version_old.value() <<
            " to " << current_version.value();

        // TODO(erikkay): This is an ugly state to be in.  Try harder?
      }
    }
    ReportExtensionInstallError(dest_dir,
                                "Couldn't create CurrentVersion file.");
    return false;
  }
  return true;
}

void ExtensionsServiceBackend::InstallExtension(
    const FilePath& extension_path, scoped_refptr<ExtensionsService> frontend) {
  LOG(INFO) << "Installing extension " << extension_path.value();

  frontend_ = frontend;
  alert_on_error_ = true;

  bool from_external = false;
  InstallOrUpdateExtension(extension_path, std::string(), from_external);
}

void ExtensionsServiceBackend::InstallOrUpdateExtension(
    const FilePath& extension_path,
    const std::string& expected_id,
    bool from_external) {
  std::string actual_public_key;
  if (!ValidateSignature(extension_path, &actual_public_key))
    return;  // Failures reported within ValidateSignature().

  UnpackerClient* client = new UnpackerClient(
      this, extension_path, actual_public_key, expected_id, from_external);
  client->Start();
}

bool ExtensionsServiceBackend::ValidateSignature(const FilePath& extension_path,
                                                 std::string* key_out) {
  ScopedStdioHandle file(file_util::OpenFile(extension_path, "rb"));
  if (!file.get()) {
    ReportExtensionInstallError(extension_path, "Could not open file.");
    return NULL;
  }

  // Read and verify the header.
  ExtensionsService::ExtensionHeader header;
  size_t len;

  // TODO(erikkay): Yuck.  I'm not a big fan of this kind of code, but it
  // appears that we don't have any endian/alignment aware serialization
  // code in the code base.  So for now, this assumes that we're running
  // on a little endian machine with 4 byte alignment.
  len = fread(&header, 1, sizeof(ExtensionsService::ExtensionHeader),
      file.get());
  if (len < sizeof(ExtensionsService::ExtensionHeader)) {
    ReportExtensionInstallError(extension_path, kInvalidExtensionHeaderError);
    return false;
  }
  if (strncmp(ExtensionsService::kExtensionHeaderMagic, header.magic,
      sizeof(header.magic))) {
    ReportExtensionInstallError(extension_path, kBadMagicNumberError);
    return false;
  }
  if (header.version != ExtensionsService::kCurrentVersion) {
    ReportExtensionInstallError(extension_path, kBadVersionNumberError);
    return false;
  }
  if (header.key_size > ExtensionsService::kMaxPublicKeySize ||
      header.signature_size > ExtensionsService::kMaxSignatureSize) {
    ReportExtensionInstallError(extension_path, kBadHeaderSizeError);
    return false;
  }

  std::vector<uint8> key;
  key.resize(header.key_size);
  len = fread(&key.front(), sizeof(uint8), header.key_size, file.get());
  if (len < header.key_size) {
    ReportExtensionInstallError(extension_path, kInvalidPublicKeyError);
    return false;
  }

  std::vector<uint8> signature;
  signature.resize(header.signature_size);
  len = fread(&signature.front(), sizeof(uint8), header.signature_size,
      file.get());
  if (len < header.signature_size) {
    ReportExtensionInstallError(extension_path, kInvalidSignatureError);
    return false;
  }

  // Note: this structure is an ASN.1 which encodes the algorithm used
  // with its parameters. This is defined in PKCS #1 v2.1 (RFC 3447).
  // It is encoding: { OID sha1WithRSAEncryption      PARAMETERS NULL }
  // TODO(aa): This needs to be factored away someplace common.
  const uint8 signature_algorithm[15] = {
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00
  };

  base::SignatureVerifier verifier;
  if (!verifier.VerifyInit(signature_algorithm,
                           sizeof(signature_algorithm),
                           &signature.front(),
                           signature.size(),
                           &key.front(),
                           key.size())) {
    ReportExtensionInstallError(extension_path,
        kSignatureVerificationInitFailed);
    return false;
  }

  unsigned char buf[1 << 12];
  while ((len = fread(buf, 1, sizeof(buf), file.get())) > 0)
    verifier.VerifyUpdate(buf, len);

  if (!verifier.VerifyFinal()) {
    ReportExtensionInstallError(extension_path, kSignatureVerificationFailed);
    return false;
  }

  net::Base64Encode(std::string(reinterpret_cast<char*>(&key.front()),
      key.size()), key_out);
  return true;
}

void ExtensionsServiceBackend::OnExtensionUnpacked(
    const FilePath& extension_path,
    const FilePath& temp_extension_dir,
    const std::string expected_id,
    bool from_external,
    const DictionaryValue& manifest,
    const std::vector< Tuple2<SkBitmap, FilePath> >& images) {
  Extension extension;
  std::string error;
  if (!extension.InitFromValue(manifest,
                               true,  // require ID
                               &error)) {
    ReportExtensionInstallError(extension_path, "Invalid extension manifest.");
    return;
  }

  if (!frontend_->extensions_enabled() && !extension.IsTheme()) {
#if defined(OS_WIN)
    if (frontend_->show_extensions_prompts()) {
      win_util::MessageBox(GetActiveWindow(),
          L"Extensions are not enabled. Add --enable-extensions to the "
          L"command-line to enable extensions.\n\n"
          L"This is a temporary message and it will be removed when extensions "
          L"UI is finalized.",
          l10n_util::GetString(IDS_PRODUCT_NAME).c_str(), MB_OK);
    }
#endif
    ReportExtensionInstallError(extension_path,
        "Extensions are not enabled.");
    return;
  }

#if defined(OS_WIN)
  if (!extension.IsTheme() && !from_external &&
      frontend_->show_extensions_prompts() &&
      win_util::MessageBox(GetActiveWindow(),
          L"Are you sure you want to install this extension?\n\n"
          L"This is a temporary message and it will be removed when extensions "
          L"UI is finalized.",
          l10n_util::GetString(IDS_PRODUCT_NAME).c_str(),
          MB_OKCANCEL) != IDOK) {
    ReportExtensionInstallError(extension_path,
        "User did not allow extension to be installed.");
    return;
  }
#endif

  // If an expected id was provided, make sure it matches.
  if (!expected_id.empty() && expected_id != extension.id()) {
    std::string error_msg = "ID in new extension manifest (";
    error_msg += extension.id();
    error_msg += ") does not match expected ID (";
    error_msg += expected_id;
    error_msg += ")";
    ReportExtensionInstallError(extension_path, error_msg);
    return;
  }

  // <profile>/Extensions/<id>
  FilePath dest_dir = install_directory_.AppendASCII(extension.id());
  std::string version = extension.VersionString();
  std::string current_version;
  Extension::InstallType install_type =
      CompareToInstalledVersion(extension.id(), version, &current_version);

  // Do not allow downgrade.
  if (install_type == Extension::DOWNGRADE) {
    ReportExtensionInstallError(extension_path,
        "Error: Attempt to downgrade extension from more recent version.");
    return;
  }

  if (install_type == Extension::REINSTALL) {
    if (NeedsReinstall(extension.id(), current_version)) {
      // Treat corrupted existing installation as new install case.
      install_type = Extension::NEW_INSTALL;
    } else {
      // The client may use this as a signal (to switch themes, for instance).
      ReportExtensionOverinstallAttempted(extension.id());
      return;
    }
  }

  // Write our parsed manifest back to disk, to ensure it doesn't contain an
  // exploitable bug that can be used to compromise the browser.
  std::string manifest_json;
  JSONStringValueSerializer serializer(&manifest_json);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(manifest)) {
    ReportExtensionInstallError(extension_path,
                                "Error serializing manifest.json.");
    return;
  }

  FilePath manifest_path =
      temp_extension_dir.AppendASCII(Extension::kManifestFilename);
  if (!file_util::WriteFile(manifest_path,
                            manifest_json.data(), manifest_json.size())) {
    ReportExtensionInstallError(extension_path, "Error saving manifest.json.");
    return;
  }

  // Delete any images that may be used by the browser.  We're going to write
  // out our own versions of the parsed images, and we want to make sure the
  // originals are gone for good.
  std::set<FilePath> image_paths = extension.GetBrowserImages();
  if (image_paths.size() != images.size()) {
    ReportExtensionInstallError(extension_path,
        "Decoded images don't match what's in the manifest.");
    return;
  }

  for (std::set<FilePath>::iterator it = image_paths.begin();
       it != image_paths.end(); ++it) {
    if (!file_util::Delete(temp_extension_dir.Append(*it), false)) {
      ReportExtensionInstallError(extension_path,
                                  "Error removing old image file.");
      return;
    }
  }

  // Write our parsed images back to disk as well.
  for (size_t i = 0; i < images.size(); ++i) {
    const SkBitmap& image = images[i].a;
    FilePath path = temp_extension_dir.Append(images[i].b);

    std::vector<unsigned char> image_data;
    // TODO(mpcomplete): It's lame that we're encoding all images as PNG, even
    // though they may originally be .jpg, etc.  Figure something out.
    // http://code.google.com/p/chromium/issues/detail?id=12459
    if (!PNGEncoder::EncodeBGRASkBitmap(image, false, &image_data)) {
      ReportExtensionInstallError(extension_path,
                                  "Error re-encoding theme image.");
      return;
    }

    // Note: we're overwriting existing files that the utility process wrote,
    // so we can be sure the directory exists.
    const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
    if (!file_util::WriteFile(path, image_data_ptr, image_data.size())) {
      ReportExtensionInstallError(extension_path, "Error saving theme image.");
      return;
    }
  }

  // <profile>/Extensions/<dir_name>/<version>
  FilePath version_dir = dest_dir.AppendASCII(version);

  // If anything fails after this, we want to delete the extension dir.
  ScopedTempDir scoped_version_dir;
  scoped_version_dir.Set(version_dir);

  if (!InstallDirSafely(temp_extension_dir, version_dir))
    return;

  if (!SetCurrentVersion(dest_dir, version))
    return;

  Extension::Location location = Extension::INVALID;
  if (from_external)
    LookupExternalExtension(extension.id(), NULL, &location);
  else
    location = Extension::INTERNAL;

  // Load the extension immediately and then report installation success. We
  // don't load extensions for external installs because external installation
  // occurs before the normal startup so we just let startup pick them up. We
  // notify on installation of external extensions because we need to update
  // the preferences for these extensions to reflect that they've just been
  // installed.
  if (!from_external) {
    Extension* extension = LoadExtension(version_dir,
                                         location,
                                         true);  // require id
    CHECK(extension);

    frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        frontend_, &ExtensionsService::OnExtensionInstalled, extension,
        install_type));

    // Only one extension, but ReportExtensionsLoaded can handle multiple,
    // so we need to construct a list.
    scoped_ptr<ExtensionList> extensions(new ExtensionList);
    extensions->push_back(extension);
    LOG(INFO) << "Done.";
    // Hand off ownership of the loaded extensions to the frontend.
    ReportExtensionsLoaded(extensions.release());
  } else {
    frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        frontend_, &ExtensionsService::OnExternalExtensionInstalled,
        extension.id(), location));
  }

  scoped_version_dir.Take();
}

void ExtensionsServiceBackend::ReportExtensionInstallError(
    const FilePath& extension_path,  const std::string &error) {

  // TODO(erikkay): note that this isn't guaranteed to work properly on Linux.
  std::string path_str = WideToASCII(extension_path.ToWStringHack());
  std::string message =
      StringPrintf("Could not install extension from '%s'. %s",
                   path_str.c_str(), error.c_str());
  ExtensionErrorReporter::GetInstance()->ReportError(message, alert_on_error_);
}

void ExtensionsServiceBackend::ReportExtensionOverinstallAttempted(
    const std::string& id) {
  frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      frontend_, &ExtensionsService::OnExtensionOverinstallAttempted, id));
}

bool ExtensionsServiceBackend::ShouldSkipInstallingExtension(
    const std::set<std::string>& ids_to_ignore,
    const std::string& id) {
  if (ids_to_ignore.find(id) != ids_to_ignore.end()) {
    LOG(INFO) << "Skipping uninstalled external extension " << id;
    return true;
  }
  return false;
}

void ExtensionsServiceBackend::CheckVersionAndInstallExtension(
    const std::string& id, const Version* extension_version,
    const FilePath& extension_path, bool from_external) {
  if (ShouldInstall(id, extension_version))
    InstallOrUpdateExtension(FilePath(extension_path), id, from_external);
}

bool ExtensionsServiceBackend::LookupExternalExtension(
    const std::string& id, Version** version, Extension::Location* location) {
  scoped_ptr<Version> extension_version;
  for (ProviderMap::const_iterator i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    const ExternalExtensionProvider* provider = i->second;
    extension_version.reset(provider->RegisteredVersion(id, location));
    if (extension_version.get()) {
      if (version)
        *version = extension_version.release();
      return true;
    }
  }
  return false;
}

// Some extensions will autoupdate themselves externally from Chrome.  These
// are typically part of some larger client application package.  To support
// these, the extension will register its location in the the preferences file
// (and also, on Windows, in the registry) and this code will periodically
// check that location for a .crx file, which it will then install locally if
// a new version is available.
void ExtensionsServiceBackend::CheckForExternalUpdates(
    std::set<std::string> ids_to_ignore,
    scoped_refptr<ExtensionsService> frontend) {
  // Note that this installation is intentionally silent (since it didn't
  // go through the front-end).  Extensions that are registered in this
  // way are effectively considered 'pre-bundled', and so implicitly
  // trusted.  In general, if something has HKLM or filesystem access,
  // they could install an extension manually themselves anyway.
  alert_on_error_ = false;
  frontend_ = frontend;

  // Ask each external extension provider to give us a call back for each
  // extension they know about. See OnExternalExtensionFound.
  for (ProviderMap::const_iterator i = external_extension_providers_.begin();
       i != external_extension_providers_.end(); ++i) {
    ExternalExtensionProvider* provider = i->second;
    provider->VisitRegisteredExtension(this, ids_to_ignore);
  }
}

bool ExtensionsServiceBackend::CheckExternalUninstall(
    DictionaryValue* extension_prefs, const FilePath& version_path,
    const std::string& id) {
  // First check the preferences for the kill-bit.
  int location_value = Extension::INVALID;
  DictionaryValue* extension = NULL;
  if (!extension_prefs->GetDictionary(ASCIIToWide(id), &extension))
    return false;
  int state;
  if (extension->GetInteger(kLocation, &location_value) &&
      location_value == Extension::EXTERNAL_PREF) {
    return extension->GetInteger(kState, &state) &&
           state == Extension::KILLBIT;
  }

  Extension::Location location =
      static_cast<Extension::Location>(location_value);

  // Check if the providers know about this extension.
  ProviderMap::const_iterator i = external_extension_providers_.find(location);
  if (i != external_extension_providers_.end()) {
    scoped_ptr<Version> version;
    version.reset(i->second->RegisteredVersion(id, NULL));
    if (version.get())
      return false;  // Yup, known extension, don't uninstall.
  }

  return true;  // This is not a known extension, uninstall.
}

// Assumes that the extension isn't currently loaded or in use.
void ExtensionsServiceBackend::UninstallExtension(
    const std::string& extension_id) {
  // First, delete the Current Version file. If the directory delete fails, then
  // at least the extension won't be loaded again.
  FilePath extension_directory = install_directory_.AppendASCII(extension_id);

  if (!file_util::PathExists(extension_directory)) {
    LOG(WARNING) << "Asked to remove a non-existent extension " << extension_id;
    return;
  }

  FilePath current_version_file = extension_directory.AppendASCII(
      ExtensionsService::kCurrentVersionFileName);
  if (!file_util::PathExists(current_version_file)) {
    LOG(WARNING) << "Extension " << extension_id
                 << " does not have a Current Version file.";
  } else {
    if (!file_util::Delete(current_version_file, false)) {
      LOG(WARNING) << "Could not delete Current Version file for extension "
                   << extension_id;
      return;
    }
  }

  // OK, now try and delete the entire rest of the directory. One major place
  // this can fail is if the extension contains a plugin (stupid plugins). It's
  // not a big deal though, because we'll notice next time we startup that the
  // Current Version file is gone and finish the delete then.
  if (!file_util::Delete(extension_directory, true)) {
    LOG(WARNING) << "Could not delete directory for extension "
                 << extension_id;
  }
}

void ExtensionsServiceBackend::ClearProvidersForTesting() {
  STLDeleteContainerPairSecondPointers(external_extension_providers_.begin(),
                                       external_extension_providers_.end());
  external_extension_providers_.clear();
}

void ExtensionsServiceBackend::SetProviderForTesting(
    Extension::Location location,
    ExternalExtensionProvider* test_provider) {
  DCHECK(test_provider);
  external_extension_providers_[location] = test_provider;
}

void ExtensionsServiceBackend::OnExternalExtensionFound(
    const std::string& id, const Version* version, const FilePath& path) {
  bool from_external = true;
  CheckVersionAndInstallExtension(id, version, path, from_external);
}

bool ExtensionsServiceBackend::ShouldInstall(const std::string& id,
                                             const Version* version) {
  std::string current_version;
  Extension::InstallType install_type =
      CompareToInstalledVersion(id, version->GetString(), &current_version);

  if (install_type == Extension::DOWNGRADE)
    return false;

  return (install_type == Extension::UPGRADE ||
          install_type == Extension::NEW_INSTALL ||
          NeedsReinstall(id, current_version));
}
