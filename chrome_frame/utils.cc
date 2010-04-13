// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <htiframe.h>
#include <mshtml.h>
#include <shlobj.h>
#include <wininet.h>

#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/scoped_bstr_win.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_variant_win.h"
#include "base/string_util.h"
#include "base/thread_local.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/chrome_frame_distribution.h"
#include "chrome_frame/extra_system_apis.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/simple_resource_loader.h"
#include "chrome_frame/utils.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"

#include "grit/chromium_strings.h"

// Note that these values are all lower case and are compared to
// lower-case-transformed values.
const wchar_t kMetaTag[] = L"meta";
const wchar_t kHttpEquivAttribName[] = L"http-equiv";
const wchar_t kContentAttribName[] = L"content";
const wchar_t kXUACompatValue[] = L"x-ua-compatible";
const wchar_t kBodyTag[] = L"body";
const wchar_t kChromeContentPrefix[] = L"chrome=";
const wchar_t kChromeProtocolPrefix[] = L"gcf:";
const wchar_t kChromeMimeType[] = L"application/chromepage";
const wchar_t kPatchProtocols[] = L"PatchProtocols";

static const wchar_t kChromeFrameConfigKey[] =
    L"Software\\Google\\ChromeFrame";
static const wchar_t kChromeFrameOptinUrlsKey[] = L"OptinUrls";
static const wchar_t kEnableGCFProtocol[] = L"EnableGCFProtocol";

static const wchar_t kChromeFrameNPAPIKey[] =
    L"Software\\MozillaPlugins\\@google.com/ChromeFrame,version=1.0";
static const wchar_t kChromeFramePersistNPAPIReg[] = L"PersistNPAPIReg";

// Used to isolate chrome frame builds from google chrome release channels.
const wchar_t kChromeFrameOmahaSuffix[] = L"-cf";
const wchar_t kDevChannelName[] = L"-dev";

const wchar_t kChromeAttachExternalTabPrefix[] = L"attach_external_tab";

// Indicates that we are running in a test environment, where execptions, etc
// are handled by the chrome test crash server.
const wchar_t kChromeFrameHeadlessMode[] = L"ChromeFrameHeadlessMode";

// Indicates that we are running in an environment that wishes to avoid
// DLL pinning, such as the perf tests.
const wchar_t kChromeFrameUnpinnedMode[] = L"kChromeFrameUnpinnedMode";

// {1AF32B6C-A3BA-48B9-B24E-8AA9C41F6ECD}
static const IID IID_IWebBrowserPriv2IE7 = { 0x1AF32B6C, 0xA3BA, 0x48B9,
    { 0xB2, 0x4E, 0x8A, 0xA9, 0xC4, 0x1F, 0x6E, 0xCD } };

// {3ED72303-6FFC-4214-BA90-FAF1862DEC8A}
static const IID IID_IWebBrowserPriv2IE8 = { 0x3ED72303, 0x6FFC, 0x4214,
    { 0xBA, 0x90, 0xFA, 0xF1, 0x86, 0x2D, 0xEC, 0x8A } };

// {486F6159-9F3F-4827-82D4-283CEF397733}
static const IID IID_IWebBrowserPriv2IE8XP = { 0x486F6159, 0x9F3F, 0x4827,
    { 0x82, 0xD4, 0x28, 0x3C, 0xEF, 0x39, 0x77, 0x33 } };

// {38339692-0BC9-46CB-8E5C-4677A5C83DD5}
static const IID IID_IWebBrowserPriv2IE8XPBeta = { 0x38339692, 0x0BC9, 0x46CB,
    { 0x8E, 0x5C, 0x46, 0x77, 0xA5, 0xC8, 0x3D, 0xD5 } };

namespace {

// A flag used to signal when an active browser instance on the current thread
// is loading a Chrome Frame document.  There's no reference stored with the
// pointer so it should not be dereferenced and used for comparison against a
// living instance only.
base::LazyInstance<base::ThreadLocalPointer<IBrowserService> >
    g_tls_browser_for_cf_navigation(base::LINKER_INITIALIZED);

}  // end anonymous namespace

HRESULT UtilRegisterTypeLib(HINSTANCE tlb_instance,
                            LPCOLESTR index,
                            bool for_current_user_only) {
  CComBSTR path;
  CComPtr<ITypeLib> type_lib;
  HRESULT hr = AtlLoadTypeLib(tlb_instance, index, &path, &type_lib);
  if (SUCCEEDED(hr)) {
    hr = UtilRegisterTypeLib(type_lib, path, NULL, for_current_user_only);
  }
  return hr;
}

HRESULT UtilUnRegisterTypeLib(HINSTANCE tlb_instance,
                              LPCOLESTR index,
                              bool for_current_user_only) {
  CComBSTR path;
  CComPtr<ITypeLib> type_lib;
  HRESULT hr = AtlLoadTypeLib(tlb_instance, index, &path, &type_lib);
  if (SUCCEEDED(hr)) {
    hr = UtilUnRegisterTypeLib(type_lib, for_current_user_only);
  }
  return hr;
}

HRESULT UtilRegisterTypeLib(LPCWSTR typelib_path,
                            bool for_current_user_only) {
  if (NULL == typelib_path) {
    return E_INVALIDARG;
  }
  CComBSTR path;
  CComPtr<ITypeLib> type_lib;
  HRESULT hr = ::LoadTypeLib(typelib_path, &type_lib);
  if (SUCCEEDED(hr)) {
    hr = UtilRegisterTypeLib(type_lib,
                             typelib_path,
                             NULL,
                             for_current_user_only);
  }
  return hr;
}

HRESULT UtilUnRegisterTypeLib(LPCWSTR typelib_path,
                              bool for_current_user_only) {
  CComPtr<ITypeLib> type_lib;
  HRESULT hr = ::LoadTypeLib(typelib_path, &type_lib);
  if (SUCCEEDED(hr)) {
    hr = UtilUnRegisterTypeLib(type_lib, for_current_user_only);
  }
  return hr;
}

HRESULT UtilRegisterTypeLib(ITypeLib* typelib,
                            LPCWSTR typelib_path,
                            LPCWSTR help_dir,
                            bool for_current_user_only) {
  typedef HRESULT(WINAPI *RegisterTypeLibPrototype)(ITypeLib FAR* type_lib,
                                                    OLECHAR FAR* full_path,
                                                    OLECHAR FAR* help_dir);
  LPCSTR function_name =
    for_current_user_only ? "RegisterTypeLibForUser" : "RegisterTypeLib";
  RegisterTypeLibPrototype reg_tlb =
      reinterpret_cast<RegisterTypeLibPrototype>(
          GetProcAddress(GetModuleHandle(_T("oleaut32.dll")),
                                         function_name));
  if (NULL == reg_tlb) {
    return E_FAIL;
  }
  return reg_tlb(typelib,
                 const_cast<OLECHAR*>(typelib_path),
                 const_cast<OLECHAR*>(help_dir));
}

HRESULT UtilUnRegisterTypeLib(ITypeLib* typelib,
                              bool for_current_user_only) {
  if (NULL == typelib) {
    return E_INVALIDARG;
  }
  typedef HRESULT(WINAPI *UnRegisterTypeLibPrototype)(
      REFGUID libID,
      unsigned short wVerMajor,  // NOLINT
      unsigned short wVerMinor,  // NOLINT
      LCID lcid,
      SYSKIND syskind);
  LPCSTR function_name =
    for_current_user_only ? "UnRegisterTypeLibForUser" : "UnRegisterTypeLib";

  UnRegisterTypeLibPrototype unreg_tlb =
      reinterpret_cast<UnRegisterTypeLibPrototype>(
          GetProcAddress(GetModuleHandle(_T("oleaut32.dll")),
                                         function_name));
  if (NULL == unreg_tlb) {
    return E_FAIL;
  }
  TLIBATTR* tla = NULL;
  HRESULT hr = typelib->GetLibAttr(&tla);
  if (SUCCEEDED(hr)) {
    hr = unreg_tlb(tla->guid,
                   tla->wMajorVerNum,
                   tla->wMinorVerNum,
                   tla->lcid,
                   tla->syskind);
    typelib->ReleaseTLibAttr(tla);
  }
  return hr;
}

bool UtilIsNPAPIPluginRegistered() {
  std::wstring npapi_key_name(kChromeFrameNPAPIKey);
  RegKey npapi_key(HKEY_LOCAL_MACHINE, npapi_key_name.c_str(), KEY_QUERY_VALUE);
  return npapi_key.Valid();
}

bool UtilChangePersistentNPAPIMarker(bool set) {
  BrowserDistribution* cf_dist = BrowserDistribution::GetDistribution();
  std::wstring cf_state_key_path(cf_dist->GetStateKey());

  RegKey cf_state_key(HKEY_LOCAL_MACHINE, cf_state_key_path.c_str(),
                      KEY_READ | KEY_WRITE);

  bool success = false;
  if (cf_state_key.Valid()) {
    if (set) {
      success = cf_state_key.WriteValue(kChromeFramePersistNPAPIReg, 1);
    } else {
      // Unfortunately, DeleteValue returns true only if the value
      // previously existed, so we do a separate existence check to
      // validate success.
      cf_state_key.DeleteValue(kChromeFramePersistNPAPIReg);
      success = !cf_state_key.ValueExists(kChromeFramePersistNPAPIReg);
    }
  }
  return success;
}

bool UtilIsPersistentNPAPIMarkerSet() {
  BrowserDistribution* cf_dist = BrowserDistribution::GetDistribution();
  std::wstring cf_state_key_path(cf_dist->GetStateKey());

  RegKey cf_state_key(HKEY_LOCAL_MACHINE, cf_state_key_path.c_str(),
                      KEY_QUERY_VALUE);

  bool success = false;
  if (cf_state_key.Valid()) {
    DWORD val = 0;
    if (cf_state_key.ReadValueDW(kChromeFramePersistNPAPIReg, &val)) {
      success = (val != 0);
    }
  }
  return success;
}


HRESULT UtilGetXUACompatContentValue(const std::wstring& html_string,
                                     std::wstring* content_value) {
  if (!content_value) {
    return E_POINTER;
  }

  // Fail fast if the string X-UA-Compatible isn't in html_string
  if (StringToLowerASCII(html_string).find(kXUACompatValue) ==
      std::wstring::npos) {
    return E_FAIL;
  }

  HTMLScanner scanner(html_string.c_str());

  // Build the list of meta tags that occur before the body tag is hit.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(kMetaTag, &tag_list, kBodyTag);

  // Search the list of meta tags for one with an http-equiv="X-UA-Compatible"
  // attribute.
  HTMLScanner::StringRange attribute;
  std::string search_attribute_ascii(WideToASCII(kXUACompatValue));
  HTMLScanner::StringRangeList::const_iterator tag_list_iter(tag_list.begin());
  for (; tag_list_iter != tag_list.end(); tag_list_iter++) {
    if (!tag_list_iter->GetTagAttribute(kHttpEquivAttribName, &attribute)) {
      continue;
    }

    // We found an http-equiv meta tag, check its value using the ascii
    // case-insensitive comparison method.
    if (!attribute.LowerCaseEqualsASCII(search_attribute_ascii.c_str())) {
      continue;
    }

    // We found our X-UA-Compatible meta tag so look for and extract
    // the value of the content attribute.
    if (!tag_list_iter->GetTagAttribute(kContentAttribName, &attribute)) {
      continue;
    }

    // Found the content string, copy and return.
    content_value->assign(attribute.Copy());
    return S_OK;
  }

  return E_FAIL;
}

void DisplayVersionMismatchWarning(HWND parent,
                                   const std::string& server_version) {
  // Obtain the current module version.
  FileVersionInfo* file_version_info =
      FileVersionInfo::CreateFileVersionInfoForCurrentModule();
  DCHECK(file_version_info);
  std::wstring version_string(file_version_info->file_version());
  std::wstring wide_server_version;
  if (server_version.empty()) {
    wide_server_version = SimpleResourceLoader::Get(IDS_VERSIONUNKNOWN);
  } else {
    wide_server_version = ASCIIToWide(server_version);
  }
  std::wstring title = SimpleResourceLoader::Get(IDS_VERSIONMISMATCH_HEADER);
  std::wstring message;
  SStringPrintf(&message,
                SimpleResourceLoader::Get(IDS_VERSIONMISMATCH).c_str(),
                wide_server_version.c_str(),
                version_string.c_str());

  ::MessageBox(parent, message.c_str(), title.c_str(), MB_OK);
}

std::string CreateJavascript(const std::string& function_name,
                             const std::string args) {
  std::string script_string = "javascript:";
  script_string += function_name + "(";
  if (!args.empty()) {
    script_string += "'";
    script_string += args;
    script_string += "'";
  }
  script_string += ")";
  return script_string;
}

AddRefModule::AddRefModule() {
  // TODO(tommi): Override the module's Lock/Unlock methods to call
  //  npapi::SetValue(NPPVpluginKeepLibraryInMemory) and keep the dll loaded
  //  while the module's refcount is > 0.  Only do this when we're being
  //  used as an NPAPI module.
  _pAtlModule->Lock();
}


AddRefModule::~AddRefModule() {
  _pAtlModule->Unlock();
}

namespace {
const char kIEImageName[] = "iexplore.exe";
const char kFirefoxImageName[] = "firefox.exe";
const char kOperaImageName[] = "opera.exe";
}  // namespace

std::wstring GetHostProcessName(bool include_extension) {
  FilePath exe;
  if (PathService::Get(base::FILE_EXE, &exe))
    exe = exe.BaseName();
  if (!include_extension) {
    exe = exe.RemoveExtension();
  }
  return exe.ToWStringHack();
}

BrowserType GetBrowserType() {
  static BrowserType browser_type = BROWSER_INVALID;

  if (browser_type == BROWSER_INVALID) {
    std::wstring exe(GetHostProcessName(true));
    if (!exe.empty()) {
      std::wstring::const_iterator begin = exe.begin();
      std::wstring::const_iterator end = exe.end();
      if (LowerCaseEqualsASCII(begin, end, kIEImageName)) {
        browser_type = BROWSER_IE;
      } else if (LowerCaseEqualsASCII(begin, end, kFirefoxImageName)) {
        browser_type = BROWSER_FIREFOX;
      } else if (LowerCaseEqualsASCII(begin, end, kOperaImageName)) {
        browser_type = BROWSER_OPERA;
      } else {
        browser_type = BROWSER_UNKNOWN;
      }
    } else {
      NOTREACHED();
    }
  }

  return browser_type;
}

IEVersion GetIEVersion() {
  static IEVersion ie_version = IE_INVALID;

  if (ie_version == IE_INVALID) {
    wchar_t exe_path[MAX_PATH];
    HMODULE mod = GetModuleHandle(NULL);
    GetModuleFileName(mod, exe_path, arraysize(exe_path) - 1);
    std::wstring exe_name(file_util::GetFilenameFromPath(exe_path));
    if (!LowerCaseEqualsASCII(exe_name, kIEImageName)) {
      ie_version = NON_IE;
    } else {
      uint32 high = 0;
      uint32 low  = 0;
      if (GetModuleVersion(mod, &high, &low)) {
        switch (HIWORD(high)) {
          case 6:
            ie_version = IE_6;
            break;
          case 7:
            ie_version = IE_7;
            break;
          default:
            ie_version = HIWORD(high) >= 8 ? IE_8 : IE_UNSUPPORTED;
            break;
        }
      } else {
        NOTREACHED() << "Can't get IE version";
      }
    }
  }

  return ie_version;
}

FilePath GetIETemporaryFilesFolder() {
  LPITEMIDLIST tif_pidl = NULL;
  HRESULT hr = SHGetFolderLocation(NULL, CSIDL_INTERNET_CACHE, NULL,
                                   SHGFP_TYPE_CURRENT, &tif_pidl);
  if (SUCCEEDED(hr) && tif_pidl) {
    ScopedComPtr<IShellFolder> parent_folder;
    LPITEMIDLIST relative_pidl = NULL;
    hr = SHBindToParent(tif_pidl, IID_IShellFolder,
                        reinterpret_cast<void**>(parent_folder.Receive()),
                        const_cast<LPCITEMIDLIST*>(&relative_pidl));
    if (SUCCEEDED(hr) && relative_pidl) {
      STRRET path = {0};
      hr = parent_folder->GetDisplayNameOf(relative_pidl,
                                           SHGDN_NORMAL | SHGDN_FORPARSING,
                                           &path);
      DCHECK(SUCCEEDED(hr));
      ScopedBstr temp_internet_files_bstr;
      StrRetToBSTR(&path, relative_pidl, temp_internet_files_bstr.Receive());
      FilePath temp_internet_files(static_cast<BSTR>(temp_internet_files_bstr));
      ILFree(tif_pidl);
      return temp_internet_files;
    } else {
      NOTREACHED() << "SHBindToParent failed with Error:" << hr;
      ILFree(tif_pidl);
    }
  } else {
    NOTREACHED() << "SHGetFolderLocation for internet cache failed. Error:"
                 << hr;
  }
  // As a last ditch effort we use the SHGetFolderPath function to retrieve the
  // path. This function has a limitation of MAX_PATH.
  wchar_t path[MAX_PATH + 1] = {0};
  hr = SHGetFolderPath(NULL, CSIDL_INTERNET_CACHE, NULL, SHGFP_TYPE_CURRENT,
                       path);
  if (SUCCEEDED(hr)) {
    return FilePath(path);
  } else {
    NOTREACHED() << "SHGetFolderPath for internet cache failed. Error:"
                 << hr;
  }
  return FilePath();
}

bool IsIEInPrivate() {
  typedef BOOL (WINAPI* IEIsInPrivateBrowsingPtr)();
  bool incognito_mode = false;
  HMODULE h = GetModuleHandle(L"ieframe.dll");
  if (h) {
    IEIsInPrivateBrowsingPtr IsInPrivate =
        reinterpret_cast<IEIsInPrivateBrowsingPtr>(GetProcAddress(h,
        "IEIsInPrivateBrowsing"));
    if (IsInPrivate) {
      incognito_mode = !!IsInPrivate();
    }
  }

  return incognito_mode;
}

HRESULT DoFileDownloadInIE(const wchar_t* url) {
  DCHECK(url);

  HMODULE mod = ::GetModuleHandleA("ieframe.dll");
  if (!mod)
    mod = ::GetModuleHandleA("shdocvw.dll");

  if (!mod) {
    NOTREACHED();
    return E_UNEXPECTED;
  }

  typedef HRESULT (WINAPI* DoFileDownloadFn)(const wchar_t*);
  DoFileDownloadFn fn = reinterpret_cast<DoFileDownloadFn>(
      ::GetProcAddress(mod, "DoFileDownload"));
  DCHECK(fn);
  return fn ? fn(url) : E_UNEXPECTED;
}

bool GetModuleVersion(HMODULE module, uint32* high, uint32* low) {
  DCHECK(module != NULL)
      << "Please use GetModuleHandle(NULL) to get the process name";
  DCHECK(high);

  bool ok = false;

  HRSRC res = FindResource(module,
      reinterpret_cast<const wchar_t*>(VS_VERSION_INFO), RT_VERSION);
  if (res) {
    HGLOBAL res_data = LoadResource(module, res);
    DWORD version_resource_size = SizeofResource(module, res);
    const void* readonly_resource_data = LockResource(res_data);
    if (readonly_resource_data && version_resource_size) {
      // Copy data as VerQueryValue tries to modify the data. This causes
      // exceptions and heap corruption errors if debugger is attached.
      scoped_array<char> data(new char[version_resource_size]);
      memcpy(data.get(), readonly_resource_data, version_resource_size);
      if (data.get()) {
        VS_FIXEDFILEINFO* ver_info = NULL;
        UINT info_size = 0;
        if (VerQueryValue(data.get(), L"\\",
                          reinterpret_cast<void**>(&ver_info), &info_size)) {
          *high = ver_info->dwFileVersionMS;
          if (low != NULL)
            *low = ver_info->dwFileVersionLS;
          ok = true;
        }

        UnlockResource(res_data);
      }
      FreeResource(res_data);
    }
  }

  return ok;
}

namespace {

const int kMaxSubmenuDepth = 10;

// Copies original_menu and returns the copy. The caller is responsible for
// closing the returned HMENU. This does not currently copy over bitmaps
// (e.g. hbmpChecked, hbmpUnchecked or hbmpItem), so checkmarks, radio buttons,
// and custom icons won't work.
// It also copies over submenus up to a maximum depth of kMaxSubMenuDepth.
//
// TODO(robertshield): Add support for the bitmap fields if need be.
HMENU UtilCloneContextMenuImpl(HMENU original_menu, int depth) {
  DCHECK(IsMenu(original_menu));

  if (depth >= kMaxSubmenuDepth)
    return NULL;

  HMENU new_menu = CreatePopupMenu();
  int item_count = GetMenuItemCount(original_menu);
  if (item_count <= 0) {
    NOTREACHED();
  } else {
    for (int i = 0; i < item_count; i++) {
      MENUITEMINFO item_info = { 0 };
      item_info.cbSize = sizeof(MENUITEMINFO);
      item_info.fMask = MIIM_ID | MIIM_STRING | MIIM_FTYPE |
                        MIIM_STATE | MIIM_DATA | MIIM_SUBMENU |
                        MIIM_CHECKMARKS | MIIM_BITMAP;

      // Call GetMenuItemInfo a first time to obtain the buffer size for
      // the label.
      if (GetMenuItemInfo(original_menu, i, TRUE, &item_info)) {
        item_info.cch++;  // Increment this as per MSDN
        std::vector<wchar_t> buffer(item_info.cch, 0);
        item_info.dwTypeData = &buffer[0];

        // Call GetMenuItemInfo a second time with dwTypeData set to a buffer
        // of a correct size to get the label.
        GetMenuItemInfo(original_menu, i, TRUE, &item_info);

        // Clone any submenus. Within reason.
        if (item_info.hSubMenu) {
          HMENU new_submenu = UtilCloneContextMenuImpl(item_info.hSubMenu,
                                                       depth + 1);
          item_info.hSubMenu = new_submenu;
        }

        // Now insert the item into the new menu.
        InsertMenuItem(new_menu, i, TRUE, &item_info);
      }
    }
  }
  return new_menu;
}

}  // namespace

HMENU UtilCloneContextMenu(HMENU original_menu) {
  return UtilCloneContextMenuImpl(original_menu, 0);
}

std::string ResolveURL(const std::string& document,
                       const std::string& relative) {
  if (document.empty()) {
    return GURL(relative).spec();
  } else {
    return GURL(document).Resolve(relative).spec();
  }
}

bool HaveSameOrigin(const std::string& url1, const std::string& url2) {
  GURL a(url1), b(url2);
  bool ret;
  if (a.is_valid() != b.is_valid()) {
    // Either (but not both) url is invalid, so they can't match.
    ret = false;
  } else if (!a.is_valid()) {
    // Both URLs are invalid (see first check).  Just check if the opaque
    // strings match exactly.
    ret = url1.compare(url2) == 0;
  } else if (a.GetOrigin() != b.GetOrigin()) {
    // The origins don't match.
    ret = false;
  } else {
    // we have a match.
    ret = true;
  }

  return ret;
}

int GetConfigInt(int default_value, const wchar_t* value_name) {
  int ret = default_value;
  RegKey config_key;
  if (config_key.Open(HKEY_CURRENT_USER, kChromeFrameConfigKey,
                      KEY_QUERY_VALUE)) {
    int value = FALSE;
    if (config_key.ReadValueDW(value_name, reinterpret_cast<DWORD*>(&value))) {
      ret = value;
    }
  }

  return ret;
}

bool GetConfigBool(bool default_value, const wchar_t* value_name) {
  DWORD value = GetConfigInt(default_value, value_name);
  return (value != FALSE);
}

bool SetConfigInt(const wchar_t* value_name, int value) {
  RegKey config_key;
  if (config_key.Create(HKEY_CURRENT_USER, kChromeFrameConfigKey,
                        KEY_SET_VALUE)) {
    if (config_key.WriteValue(value_name, value)) {
      return true;
    }
  }

  return false;
}

bool SetConfigBool(const wchar_t* value_name, bool value) {
  return SetConfigInt(value_name, value);
}

bool DeleteConfigValue(const wchar_t* value_name) {
  RegKey config_key;
  if (config_key.Open(HKEY_CURRENT_USER, kChromeFrameConfigKey,
                      KEY_WRITE)) {
    return config_key.DeleteValue(value_name);
  }
  return false;
}

bool IsOptInUrl(const wchar_t* url) {
  RegKey config_key;
  if (!config_key.Open(HKEY_CURRENT_USER, kChromeFrameConfigKey, KEY_READ))
    return false;

  RegistryValueIterator optin_urls_list(config_key.Handle(),
                                        kChromeFrameOptinUrlsKey);
  while (optin_urls_list.Valid()) {
    if (MatchPatternWide(url, optin_urls_list.Name()))
      return true;
    ++optin_urls_list;
  }

  return false;
}

HRESULT NavigateBrowserToMoniker(IUnknown* browser, IMoniker* moniker,
                                 const wchar_t* headers, IBindCtx* bind_ctx,
                                 const wchar_t* fragment) {
  DCHECK(browser);
  DCHECK(moniker);
  DCHECK(bind_ctx);

  ScopedComPtr<IWebBrowser2> web_browser2;
  HRESULT hr = DoQueryService(SID_SWebBrowserApp, browser,
                              web_browser2.Receive());
  DCHECK(web_browser2);
  DLOG_IF(WARNING, FAILED(hr)) << StringPrintf(L"SWebBrowserApp 0x%08X", hr);
  if (FAILED(hr))
    return hr;

  // Create a new bind context that's not associated with our callback.
  // Calling RevokeBindStatusCallback doesn't disassociate the callback with
  // the bind context in IE7.  The returned bind context has the same
  // implementation of GetRunningObjectTable as the bind context we held which
  // basically delegates to ole32's GetRunningObjectTable.  The object table
  // is then used to determine if the moniker is already running and via
  // that mechanism is associated with the same internet request as has already
  // been issued.

  // TODO(tommi): See if we can get HlinkSimpleNavigateToMoniker to work
  // instead.  Looks like we'll need to support IHTMLDocument2 (get_URL in
  // particular), access to IWebBrowser2 etc.
  // HlinkSimpleNavigateToMoniker(moniker, url, NULL, host, bind_context,
  //                              NULL, 0, 0);

  ScopedComPtr<IUriContainer> uri_container;
  hr = uri_container.QueryFrom(moniker);

  ScopedVariant headers_var;
  if (headers && headers[0])
    headers_var.Set(headers);

  if (uri_container) {
    // IE7 and IE8.
    const IID* interface_ids[] = {
      &IID_IWebBrowserPriv2IE7,
      &IID_IWebBrowserPriv2IE8,
      &IID_IWebBrowserPriv2IE8XP,
      &IID_IWebBrowserPriv2IE8XPBeta,
    };

    ScopedComPtr<IWebBrowserPriv2Common, NULL> browser_priv2;
    for (int i = 0; i < arraysize(interface_ids) && browser_priv2 == NULL;
         ++i) {
      hr = web_browser2.QueryInterface(*interface_ids[i],
          reinterpret_cast<void**>(browser_priv2.Receive()));
    }

    DCHECK(browser_priv2);

    if (browser_priv2) {
      ScopedComPtr<IUri> uri_obj;
      uri_container->GetIUri(uri_obj.Receive());
      DCHECK(uri_obj);

      hr = browser_priv2->NavigateWithBindCtx2(uri_obj, NULL, NULL, NULL,
                                               headers_var.AsInput(), bind_ctx,
                                               const_cast<wchar_t*>(fragment));
      DLOG_IF(WARNING, FAILED(hr))
          << StringPrintf(L"NavigateWithBindCtx2 0x%08X", hr);
    }
  } else {
    // IE6
    LPOLESTR url = NULL;
    if (SUCCEEDED(hr = moniker->GetDisplayName(bind_ctx, NULL, &url))) {
      DLOG(INFO) << __FUNCTION__ << " " << url;
      ScopedComPtr<IWebBrowserPriv> browser_priv;
      if (SUCCEEDED(hr = browser_priv.QueryFrom(web_browser2))) {
        GURL target_url(url);
        // On IE6 if the original URL has a fragment then the navigation
        // attempt is ignored. To workaround this we strip the fragment from
        // the url and initiate the navigation. When the active document loads
        // we retrieve the original url with the fragment from the Navigation
        // manager and use it.
        if (target_url.has_ref()) {
          url_parse::Component comp;
          GURL::Replacements replacements;
          replacements.SetRef("", comp);

          target_url = target_url.ReplaceComponents(replacements);
          fragment = NULL;
        }

        ScopedVariant var_url(UTF8ToWide(target_url.spec()).c_str());
        hr = browser_priv->NavigateWithBindCtx(var_url.AsInput(), NULL, NULL,
                                               NULL, headers_var.AsInput(),
                                               bind_ctx,
                                               const_cast<wchar_t*>(fragment));
        DLOG_IF(WARNING, FAILED(hr))
            << StringPrintf(L"NavigateWithBindCtx 0x%08X", hr);
      } else {
        NOTREACHED();
      }
      ::CoTaskMemFree(url);
    } else {
      DLOG(ERROR) << StringPrintf("GetDisplayName: 0x%08X", hr);
    }
  }

  return hr;
}

void MarkBrowserOnThreadForCFNavigation(IBrowserService* browser) {
  DCHECK(browser != NULL);
  DCHECK(g_tls_browser_for_cf_navigation.Pointer()->Get() == NULL ||
         g_tls_browser_for_cf_navigation.Pointer()->Get() == browser);
  g_tls_browser_for_cf_navigation.Pointer()->Set(browser);
}

bool CheckForCFNavigation(IBrowserService* browser, bool clear_flag) {
  DCHECK(browser);
  bool ret = (g_tls_browser_for_cf_navigation.Pointer()->Get() == browser);
  if (ret && clear_flag)
    g_tls_browser_for_cf_navigation.Pointer()->Set(NULL);
  return ret;
}

bool IsValidUrlScheme(const std::wstring& url, bool is_privileged) {
  if (url.empty())
    return false;

  GURL crack_url(url);

  if (crack_url.SchemeIs(chrome::kHttpScheme) ||
      crack_url.SchemeIs(chrome::kHttpsScheme) ||
      crack_url.SchemeIs(chrome::kAboutScheme))
    return true;

  // Additional checking for view-source. Allow only http and https
  // URLs in view source.
  if (crack_url.SchemeIs(chrome::kViewSourceScheme)) {
    GURL sub_url(crack_url.path());
    if (sub_url.SchemeIs(chrome::kHttpScheme) ||
        sub_url.SchemeIs(chrome::kHttpsScheme))
      return true;
    else
      return false;
  }

  if (is_privileged &&
      (crack_url.SchemeIs(chrome::kDataScheme) ||
       crack_url.SchemeIs(chrome::kExtensionScheme)))
    return true;

  if (StartsWith(url, kChromeAttachExternalTabPrefix, false))
    return true;

  return false;
}

std::string GetRawHttpHeaders(IWinInetHttpInfo* info) {
  DCHECK(info);

  std::string buffer;

  DWORD size = 0;
  DWORD flags = 0;
  DWORD reserved = 0;
  HRESULT hr = info->QueryInfo(HTTP_QUERY_RAW_HEADERS_CRLF, NULL, &size,
                               &flags, &reserved);
  if (!size) {
    DLOG(WARNING) << "Failed to query HTTP headers size. Error: " << hr;
  } else {
    buffer.resize(size + 1);
    hr = info->QueryInfo(HTTP_QUERY_RAW_HEADERS_CRLF, &buffer[0],
                         &size, &flags, &reserved);
    if (FAILED(hr)) {
      DLOG(WARNING) << "Failed to query HTTP headers. Error: " << hr;
    }
  }

  return buffer;
}

bool IsSubFrameRequest(IUnknown* service_provider) {
  DCHECK(service_provider);

  // We need to be able to get at an IWebBrowser2 if we are to decide whether
  // this request originates from a non-top-level frame.
  ScopedComPtr<IWebBrowser2> web_browser;
  HRESULT hr = DoQueryService(IID_ITargetFrame2, service_provider,
                              web_browser.Receive());

  bool is_sub_frame_request = false;
  if (web_browser) {
    // Now check to see if we are in a sub-frame.
    ScopedComPtr<IHTMLWindow2> current_frame, parent_frame;
    hr = DoQueryService(IID_IHTMLWindow2, service_provider,
                        current_frame.Receive());
    if (current_frame) {
      // Only the top level window will return self when get_parent is called.
      current_frame->get_parent(parent_frame.Receive());
      if (parent_frame != current_frame) {
        DLOG(INFO) << "Sub frame detected";
        is_sub_frame_request = true;
      }
    }
  } else {
    DLOG(INFO) << "IsSubFrameRequest - no IWebBrowser2";
    is_sub_frame_request = true;
  }

  return is_sub_frame_request;
}

bool IsHeadlessMode() {
  bool headless = GetConfigBool(false, kChromeFrameHeadlessMode);
  return headless;
}

bool IsUnpinnedMode() {
  bool unpinned = GetConfigBool(false, kChromeFrameUnpinnedMode);
  return unpinned;
}

std::wstring GetActualUrlFromMoniker(IMoniker* moniker,
                                     IBindCtx* bind_context,
                                     const std::wstring& bho_url) {
  CComHeapPtr<WCHAR> display_name;
  moniker->GetDisplayName(bind_context, NULL, &display_name);
  std::wstring moniker_url = display_name;

  GURL parsed_url(WideToUTF8(bho_url));
  if (!parsed_url.has_ref())
    return moniker_url;

  if (StartsWith(bho_url, moniker_url, false) &&
      bho_url[moniker_url.length()] == L'#')
    return bho_url;

  return moniker_url;
}

bool IsTopLevelWindow(HWND window) {
  long style = GetWindowLong(window, GWL_STYLE);  // NOLINT
  if (!(style & WS_CHILD))
    return true;

  HWND parent = GetParent(window);
  return !parent || (parent == GetDesktopWindow());
}

HRESULT RewindStream(IStream* stream) {
  HRESULT hr = E_POINTER;
  if (stream) {
    LARGE_INTEGER zero = {0};
    ULARGE_INTEGER new_pos = {0};
    hr = stream->Seek(zero, STREAM_SEEK_SET, &new_pos);
  }

  return hr;
}

std::wstring GuidToString(const GUID& guid) {
  std::wstring ret;
  ::StringFromGUID2(guid, WriteInto(&ret, 39), 39);
  return ret;
}

int32 MapCookieStateToCookieAction(InternetCookieState cookie_state) {
  int32 cookie_action = COOKIEACTION_NONE;

  switch (cookie_state) {
    case COOKIE_STATE_UNKNOWN:
      cookie_action = COOKIEACTION_NONE;
      break;
    case COOKIE_STATE_ACCEPT:
      cookie_action = COOKIEACTION_ACCEPT;
      break;
    case COOKIE_STATE_LEASH:
      cookie_action = COOKIEACTION_LEASH;
      break;
    case COOKIE_STATE_DOWNGRADE:
      cookie_action = COOKIEACTION_DOWNGRADE;
      break;
    case COOKIE_STATE_REJECT:
      cookie_action = COOKIEACTION_REJECT;
      break;
    default:
      cookie_action = COOKIEACTION_REJECT;
      break;
  }
  return cookie_action;
}

GURL GetUrlWithoutFragment(const wchar_t* url) {
  GURL parsed_url(url);

  if (parsed_url.has_ref()) {
    url_parse::Component comp;
    GURL::Replacements replacements;
    replacements.SetRef("", comp);

    parsed_url = parsed_url.ReplaceComponents(replacements);
  }
  return parsed_url;
}

bool CompareUrlsWithoutFragment(const wchar_t* url1, const wchar_t* url2) {
  GURL parsed_url1 = GetUrlWithoutFragment(url1);
  GURL parsed_url2 = GetUrlWithoutFragment(url2);
  return parsed_url1 == parsed_url2;
}

