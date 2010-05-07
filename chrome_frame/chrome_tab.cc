// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chrome_tab.cc : Implementation of DLL Exports.

// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

#include <atlsecurity.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/win_util.h"
#include "chrome/common/chrome_constants.h"
#include "grit/chrome_frame_resources.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/chrome_active_document.h"
#include "chrome_frame/chrome_frame_activex.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/chrome_frame_reporting.h"
#include "chrome_frame/chrome_launcher.h"
#include "chrome_frame/chrome_protocol.h"
#include "chrome_frame/module_utils.h"
#include "chrome_frame/resource.h"
#include "chrome_frame/utils.h"
#include "googleurl/src/url_util.h"

namespace {
// This function has the side effect of initializing an unprotected
// vector pointer inside GoogleUrl. If this is called during DLL loading,
// it has the effect of avoiding an initializiation race on that pointer.
// TODO(siggi): fix GoogleUrl.
void InitGoogleUrl() {
  static const char kDummyUrl[] = "http://www.google.com";

  url_util::IsStandard(kDummyUrl,
                       url_parse::MakeRange(0, arraysize(kDummyUrl)));
}
}

static const wchar_t kBhoRegistryPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"
    L"\\Browser Helper Objects";

const wchar_t kInternetSettings[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";

const wchar_t kProtocolHandlers[] =
    L"Software\\Classes\\Protocols\\Handler";

const wchar_t kBhoNoLoadExplorerValue[] = L"NoExplorer";

// {0562BFC3-2550-45b4-BD8E-A310583D3A6F}
static const GUID kChromeFrameProvider =
    { 0x562bfc3, 0x2550, 0x45b4,
        { 0xbd, 0x8e, 0xa3, 0x10, 0x58, 0x3d, 0x3a, 0x6f } };

// Object entries go here instead of with each object, so that we can move
// the objects to a lib. Also reduces magic.
OBJECT_ENTRY_AUTO(CLSID_ChromeFrameBHO, Bho)
OBJECT_ENTRY_AUTO(__uuidof(ChromeActiveDocument), ChromeActiveDocument)
OBJECT_ENTRY_AUTO(__uuidof(ChromeFrame), ChromeFrameActivex)
OBJECT_ENTRY_AUTO(__uuidof(ChromeProtocol), ChromeProtocol)


// See comments in DllGetClassObject.
LPFNGETCLASSOBJECT g_dll_get_class_object_redir_ptr = NULL;

class ChromeTabModule
    : public AtlPerUserModule<CAtlDllModuleT<ChromeTabModule> > {
 public:
  typedef AtlPerUserModule<CAtlDllModuleT<ChromeTabModule> > ParentClass;

  DECLARE_LIBID(LIBID_ChromeTabLib)
  DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CHROMETAB,
                                    "{FD9B1B31-F4D8-436A-8F4F-D3C2E36733D3}")

  // Override to add our SYSTIME binary value to registry scripts.
  // See chrome_frame_activex.rgs for usage.
  virtual HRESULT AddCommonRGSReplacements(IRegistrarBase* registrar) throw() {
    HRESULT hr = ParentClass::AddCommonRGSReplacements(registrar);

    if (SUCCEEDED(hr)) {
      SYSTEMTIME local_time;
      ::GetSystemTime(&local_time);
      std::string hex(HexEncode(&local_time, sizeof(local_time)));
      base::StringPiece sp_hex(hex);
      hr = registrar->AddReplacement(L"SYSTIME",
                                     base::SysNativeMBToWide(sp_hex).c_str());
      DCHECK(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr)) {
      FilePath app_path =
          chrome_launcher::GetChromeExecutablePath().DirName();
      hr = registrar->AddReplacement(L"CHROME_APPPATH",
                                     app_path.value().c_str());
      DCHECK(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr)) {
      hr = registrar->AddReplacement(L"CHROME_APPNAME",
                                     chrome::kBrowserProcessExecutableName);
      DCHECK(SUCCEEDED(hr));

      // Fill in VERSION from the VERSIONINFO stored in the DLL's resources.
      scoped_ptr<FileVersionInfo> module_version_info(
          FileVersionInfo::CreateFileVersionInfoForCurrentModule());
      DCHECK(module_version_info != NULL);
      std::wstring file_version(module_version_info->file_version());
      hr = registrar->AddReplacement(L"VERSION", file_version.c_str());
      DCHECK(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr)) {
      // Add the directory of chrome_launcher.exe.  This will be the same
      // as the directory for the current DLL.
      std::wstring module_dir;
      FilePath module_path;
      if (PathService::Get(base::FILE_MODULE, &module_path)) {
        module_dir = module_path.DirName().ToWStringHack();
      } else {
        NOTREACHED();
      }
      hr = registrar->AddReplacement(L"CHROME_LAUNCHER_APPPATH",
                                     module_dir.c_str());
      DCHECK(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr)) {
      // Add the filename of chrome_launcher.exe
      hr = registrar->AddReplacement(L"CHROME_LAUNCHER_APPNAME",
                                     chrome_launcher::kLauncherExeBaseName);
      DCHECK(SUCCEEDED(hr));
    }

    return hr;
  }
};

ChromeTabModule _AtlModule;

base::AtExitManager* g_exit_manager = NULL;
bool RegisterSecuredMimeHandler(bool enable);  // forward

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,
                               DWORD reason,
                               LPVOID reserved) {
  UNREFERENCED_PARAMETER(instance);
  if (reason == DLL_PROCESS_ATTACH) {
#ifndef NDEBUG
    // Silence traces from the ATL registrar to reduce the log noise.
    ATL::CTrace::s_trace.ChangeCategory(atlTraceRegistrar, 0,
                                        ATLTRACESTATUS_DISABLED);
#endif
    InitGoogleUrl();

    g_exit_manager = new base::AtExitManager();
    CommandLine::Init(0, NULL);
    InitializeCrashReporting();
    logging::InitLogging(NULL, logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                        logging::LOCK_LOG_FILE, logging::DELETE_OLD_LOG_FILE);

    if (!DllRedirector::RegisterAsFirstCFModule()) {
      // We are not the first ones in, get the module who registered first.
      HMODULE original_module = DllRedirector::GetFirstCFModule();
      DCHECK(original_module != NULL)
          << "Could not get first CF module handle.";
      HMODULE this_module = reinterpret_cast<HMODULE>(&__ImageBase);
      if (original_module != this_module) {
        // Someone else was here first, try and get a pointer to their
        // DllGetClassObject export:
        g_dll_get_class_object_redir_ptr =
            DllRedirector::GetDllGetClassObjectPtr(original_module);
        DCHECK(g_dll_get_class_object_redir_ptr != NULL)
            << "Found CF module with no DllGetClassObject export.";
      }
    }

    // Enable ETW logging.
    logging::LogEventProvider::Initialize(kChromeFrameProvider);
  } else if (reason == DLL_PROCESS_DETACH) {
    DllRedirector::UnregisterAsFirstCFModule();
    g_patch_helper.UnpatchIfNeeded();
    delete g_exit_manager;
    g_exit_manager = NULL;
    ShutdownCrashReporting();
  }
  return _AtlModule.DllMain(reason, reserved);
}

#ifdef _MANAGED
#pragma managed(pop)
#endif

const wchar_t kPostPlatformUAKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\"
    L"User Agent\\Post Platform";
const wchar_t kClockUserAgent[] = L"chromeframe";

// To delete the clock user agent, set value to NULL.
// TODO(tommi): Remove this method when it's no longer used.
HRESULT SetClockUserAgent(const wchar_t* value) {
  HRESULT hr;
  RegKey ua_key;
  if (ua_key.Create(HKEY_LOCAL_MACHINE, kPostPlatformUAKey, KEY_WRITE)) {
    if (value) {
      ua_key.WriteValue(kClockUserAgent, value);
    } else {
      ua_key.DeleteValue(kClockUserAgent);
    }
    hr = S_OK;
  } else {
    DLOG(ERROR) << __FUNCTION__ << ": " << kPostPlatformUAKey;
    hr = E_UNEXPECTED;
  }

  return hr;
}

HRESULT RefreshElevationPolicy() {
  const wchar_t kIEFrameDll[] = L"ieframe.dll";
  const char kIERefreshPolicy[] = "IERefreshElevationPolicy";
  HRESULT hr = E_NOTIMPL;
  HMODULE ieframe_module = LoadLibrary(kIEFrameDll);
  if (ieframe_module) {
    typedef HRESULT (__stdcall *IERefreshPolicy)();
    IERefreshPolicy ie_refresh_policy = reinterpret_cast<IERefreshPolicy>(
        GetProcAddress(ieframe_module, kIERefreshPolicy));

    if (ie_refresh_policy) {
      hr = ie_refresh_policy();
    } else {
      hr = HRESULT_FROM_WIN32(GetLastError());
    }

    FreeLibrary(ieframe_module);
  } else {
    hr = HRESULT_FROM_WIN32(GetLastError());
  }

  return hr;
}

HRESULT RegisterChromeTabBHO() {
  RegKey ie_bho_key;
  if (!ie_bho_key.Create(HKEY_LOCAL_MACHINE, kBhoRegistryPath,
                       KEY_CREATE_SUB_KEY)) {
    DLOG(WARNING) << "Failed to open registry key "
                  << kBhoRegistryPath
                  << " for write";
    return E_FAIL;
  }

  wchar_t bho_class_id_as_string[MAX_PATH] = {0};
  StringFromGUID2(CLSID_ChromeFrameBHO, bho_class_id_as_string,
                  arraysize(bho_class_id_as_string));

  if (!ie_bho_key.CreateKey(bho_class_id_as_string, KEY_READ | KEY_WRITE)) {
    DLOG(WARNING) << "Failed to create bho registry key under "
                  << kBhoRegistryPath
                  << " for write";
    return E_FAIL;
  }

  ie_bho_key.WriteValue(kBhoNoLoadExplorerValue, 1);
  DLOG(INFO) << "Registered ChromeTab BHO";

  // We now add the chromeframe user agent at runtime.
  RefreshElevationPolicy();
  return S_OK;
}

HRESULT UnregisterChromeTabBHO() {
  // TODO(tommi): remove this in future versions.
  SetClockUserAgent(NULL);

  RegKey ie_bho_key;
  if (!ie_bho_key.Open(HKEY_LOCAL_MACHINE, kBhoRegistryPath,
                       KEY_READ | KEY_WRITE)) {
    DLOG(WARNING) << "Failed to open registry key "
                  << kBhoRegistryPath
                  << " for write.";
    return E_FAIL;
  }

  wchar_t bho_class_id_as_string[MAX_PATH] = {0};
  StringFromGUID2(CLSID_ChromeFrameBHO, bho_class_id_as_string,
                  arraysize(bho_class_id_as_string));

  if (!ie_bho_key.DeleteKey(bho_class_id_as_string)) {
    DLOG(WARNING) << "Failed to delete bho registry key "
                  << bho_class_id_as_string
                  << " under "
                  << kBhoRegistryPath;
    return E_FAIL;
  }

  DLOG(INFO) << "Unregistered ChromeTab BHO";
  return S_OK;
}

HRESULT CleanupCFProtocol() {
  RegKey protocol_handlers_key;
  if (protocol_handlers_key.Open(HKEY_LOCAL_MACHINE, kProtocolHandlers,
                                 KEY_READ | KEY_WRITE)) {
    RegKey cf_protocol_key;
    if (cf_protocol_key.Open(protocol_handlers_key.Handle(), L"cf",
                             KEY_QUERY_VALUE)) {
      std::wstring protocol_clsid_string;
      if (cf_protocol_key.ReadValue(L"CLSID", &protocol_clsid_string)) {
        CLSID protocol_clsid = {0};
        IIDFromString(protocol_clsid_string.c_str(), &protocol_clsid);
        if (IsEqualGUID(protocol_clsid, CLSID_ChromeProtocol))
          protocol_handlers_key.DeleteKey(L"cf");
      }
    }
  }

  return S_OK;
}

// Used to determine whether the DLL can be unloaded by OLE
STDAPI DllCanUnloadNow() {
  return _AtlModule.DllCanUnloadNow();
}

// Returns a class factory to create an object of the requested type
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
  // If we found another module present when we were loaded, then delegate to
  // that:
  if (g_dll_get_class_object_redir_ptr) {
    return g_dll_get_class_object_redir_ptr(rclsid, riid, ppv);
  } else {
    g_patch_helper.InitializeAndPatchProtocolsIfNeeded();
    return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
  }
}

// DllRegisterServer - Adds entries to the system registry
STDAPI DllRegisterServer() {
  // registers object, typelib and all interfaces in typelib
  HRESULT hr = _AtlModule.DllRegisterServer(TRUE);

  if (SUCCEEDED(hr)) {
    // Best effort attempt to register the BHO. At this point we silently
    // ignore any errors during registration. There are some traces emitted
    // to the debug log.
    RegisterChromeTabBHO();
    if (!RegisterSecuredMimeHandler(true))
      hr = E_FAIL;
  }

  if (UtilIsPersistentNPAPIMarkerSet()) {
    hr = _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEFRAME_NPAPI, TRUE);
  }

  return hr;
}

// DllUnregisterServer - Removes entries from the system registry
STDAPI DllUnregisterServer() {
  HRESULT hr = _AtlModule.DllUnregisterServer(TRUE);

  if (SUCCEEDED(hr)) {
    // Best effort attempt to unregister the BHO. At this point we silently
    // ignore any errors during unregistration. There are some traces emitted
    // to the debug log.
    UnregisterChromeTabBHO();
    if (!RegisterSecuredMimeHandler(false))
      hr = E_FAIL;
  }

  if (UtilIsNPAPIPluginRegistered()) {
    hr = _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEFRAME_NPAPI, FALSE);
  }

  // TODO(joshia): Remove after 2 refresh releases
  CleanupCFProtocol();
  return hr;
}

// Registers the NPAPI plugin and sets the persistent marker that tells us
// to re-register it through updates.
STDAPI RegisterNPAPIPlugin() {
  HRESULT hr = _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEFRAME_NPAPI,
                                                      TRUE);
  if (SUCCEEDED(hr)) {
    if (!UtilChangePersistentNPAPIMarker(true)) {
      hr = E_FAIL;
    }
  }
  return hr;
}

// Unregisters the NPAPI plugin and clears the persistent marker that tells us
// to re-register it through updates.
STDAPI UnregisterNPAPIPlugin() {
  HRESULT hr = _AtlModule.UpdateRegistryFromResourceS(IDR_CHROMEFRAME_NPAPI,
                                                      FALSE);
  if (SUCCEEDED(hr)) {
    if (!UtilChangePersistentNPAPIMarker(false)) {
      hr = E_FAIL;
    }
  }
  return hr;
}

class SecurityDescBackup {
 public:
  explicit SecurityDescBackup(const std::wstring& backup_key)
      : backup_key_name_(backup_key) {}
  ~SecurityDescBackup() {}

  // Save given security descriptor to the backup key.
  bool SaveSecurity(const CSecurityDesc& sd) {
    CString str;
    if (!sd.ToString(&str))
      return false;

    RegKey backup_key(HKEY_LOCAL_MACHINE, backup_key_name_.c_str(),
                      KEY_READ | KEY_WRITE);
    if (backup_key.Valid()) {
      return backup_key.WriteValue(NULL, str.GetString());
    }

    return false;
  }

  // Restore security descriptor from backup key to given key name.
  bool RestoreSecurity(const wchar_t* key_name) {
    std::wstring sddl;
    if (!ReadBackupKey(&sddl))
      return false;

    // Create security descriptor from string.
    CSecurityDesc sd;
    if (!sd.FromString(sddl.c_str()))
      return false;

    bool result = true;
    // Restore DACL and Owner of the key from saved security descriptor.
    CDacl dacl;
    CSid owner;
    sd.GetDacl(&dacl);
    sd.GetOwner(&owner);

    DWORD error = ::SetNamedSecurityInfo(const_cast<wchar_t*>(key_name),
        SE_REGISTRY_KEY, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        const_cast<SID*>(owner.GetPSID()), NULL,
        const_cast<ACL*>(dacl.GetPACL()), NULL);

    DeleteBackupKey();
    return (error == ERROR_SUCCESS);
  }

 private:
  // Read SDDL string from backup key
  bool ReadBackupKey(std::wstring* sddl) {
    RegKey backup_key(HKEY_LOCAL_MACHINE, backup_key_name_.c_str(), KEY_READ);
    if (!backup_key.Valid())
      return false;

    DWORD len = 0;
    DWORD reg_type = REG_NONE;
    if (!backup_key.ReadValue(NULL, NULL, &len, &reg_type))
      return false;

    if (reg_type != REG_SZ)
      return false;

    size_t wchar_count = 1 + len / sizeof(wchar_t);
    if (!backup_key.ReadValue(NULL, WriteInto(sddl, wchar_count), &len,
                              &reg_type)) {
      return false;
    }

    return true;
  }

  void DeleteBackupKey() {
    ::RegDeleteKey(HKEY_LOCAL_MACHINE, backup_key_name_.c_str());
  }

  std::wstring backup_key_name_;
};

struct TokenWithPrivileges {
  TokenWithPrivileges() {
    token_.GetEffectiveToken(TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY);
    token_.GetUser(&user_);
  }

  ~TokenWithPrivileges() {
    token_.EnableDisablePrivileges(take_ownership_);
    token_.EnableDisablePrivileges(restore_);
  }

  bool EnablePrivileges() {
    if (take_ownership_.GetCount() == 0)
      if (!token_.EnablePrivilege(L"SeTakeOwnershipPrivilege",
                                  &take_ownership_))
        return false;

    if (restore_.GetCount() == 0)
      if (!token_.EnablePrivilege(L"SeRestorePrivilege", &restore_))
        return false;

    return true;
  }

  const CSid& GetUser() const {
    return user_;
  }

 private:
  CAccessToken token_;
  CTokenPrivileges take_ownership_;
  CTokenPrivileges restore_;
  CSid user_;
};

static bool SetOrDeleteMimeHandlerKey(bool set) {
  std::wstring key_name = kInternetSettings;
  key_name.append(L"\\Secure Mime Handlers");
  RegKey key(HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_READ | KEY_WRITE);
  if (!key.Valid())
    return false;

  bool result;
  if (set) {
    result = key.WriteValue(L"ChromeTab.ChromeActiveDocument", 1);
    result = key.WriteValue(L"ChromeTab.ChromeActiveDocument.1", 1) && result;
  } else {
    result = key.DeleteValue(L"ChromeTab.ChromeActiveDocument");
    result = key.DeleteValue(L"ChromeTab.ChromeActiveDocument.1") && result;
  }

  return result;
}

bool RegisterSecuredMimeHandler(bool enable) {
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    return SetOrDeleteMimeHandlerKey(enable);
  }

  std::wstring mime_key = kInternetSettings;
  mime_key.append(L"\\Secure Mime Handlers");
  std::wstring backup_key = kInternetSettings;
  backup_key.append(L"\\__backup_SMH__");
  std::wstring object_name = L"MACHINE\\";
  object_name.append(mime_key);

  TokenWithPrivileges token_;
  if (!token_.EnablePrivileges())
    return false;

  // If there is a backup key - something bad happened; try to restore
  // security on "Secure Mime Handlers" from the backup.
  SecurityDescBackup backup(backup_key);
  backup.RestoreSecurity(object_name.c_str());

  // Read old security descriptor of the Mime key first.
  CSecurityDesc sd;
  if (!AtlGetSecurityDescriptor(object_name.c_str(), SE_REGISTRY_KEY, &sd)) {
    return false;
  }

  backup.SaveSecurity(sd);
  bool result = false;
  // set new owner
  if (AtlSetOwnerSid(object_name.c_str(), SE_REGISTRY_KEY, token_.GetUser())) {
    // set new dacl
    CDacl new_dacl;
    sd.GetDacl(&new_dacl);
    new_dacl.AddAllowedAce(token_.GetUser(), GENERIC_WRITE | GENERIC_READ);
    if (AtlSetDacl(object_name.c_str(), SE_REGISTRY_KEY, new_dacl)) {
      result = SetOrDeleteMimeHandlerKey(enable);
    }
  }

  backup.RestoreSecurity(object_name.c_str());
  return result;
}
