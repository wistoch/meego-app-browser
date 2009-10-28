// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlobj.h>

#include "chrome_frame/html_utils.h"
#include "chrome_frame/utils.h"

#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/scoped_comptr_win.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "grit/chrome_frame_resources.h"
#include "chrome_frame/resource.h"

// Note that these values are all lower case and are compared to
// lower-case-transformed values.
const wchar_t kMetaTag[] = L"meta";
const wchar_t kHttpEquivAttribName[] = L"http-equiv";
const wchar_t kContentAttribName[] = L"content";
const wchar_t kXUACompatValue[] = L"x-ua-compatible";
const wchar_t kBodyTag[] = L"body";
const wchar_t kChromeContentPrefix[] = L"chrome=";
const wchar_t kChromeProtocolPrefix[] = L"cf:";

static const wchar_t kChromeFrameConfigKey[] =
    L"Software\\Google\\ChromeFrame";
static const wchar_t kChromeFrameOptinUrlsKey[] = L"OptinUrls";

// Used to isolate chrome frame builds from google chrome release channels.
const wchar_t kChromeFrameOmahaSuffix[] = L"-cf";
const wchar_t kDevChannelName[] = L"-dev";

const wchar_t kChromeAttachExternalTabPrefix[] = L"attach_external_tab";

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

std::wstring GetResourceString(int resource_id) {
  std::wstring resource_string;
  HMODULE this_module = reinterpret_cast<HMODULE>(&__ImageBase);
  const ATLSTRINGRESOURCEIMAGE* image = AtlGetStringResourceImage(
      this_module, resource_id);
  if (image) {
    resource_string.assign(image->achString, image->nLength);
  } else {
    NOTREACHED() << "Unable to find resource id " << resource_id;
  }
  return resource_string;
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
    wide_server_version = GetResourceString(IDS_VERSIONUNKNOWN);
  } else {
    wide_server_version = ASCIIToWide(server_version);
  }
  std::wstring title = GetResourceString(IDS_VERSIONMISMATCH_HEADER);
  std::wstring message;
  SStringPrintf(&message, GetResourceString(IDS_VERSIONMISMATCH).c_str(),
                wide_server_version.c_str(), version_string.c_str());

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

bool IsOptInUrl(const wchar_t* url) {
  RegKey config_key;
  if (!config_key.Open(HKEY_CURRENT_USER, kChromeFrameConfigKey, KEY_READ))
    return false;

  RegistryValueIterator optin_urls_list(config_key.Handle(),
                                        kChromeFrameOptinUrlsKey);
  while (optin_urls_list.Valid()) {
    if (MatchPattern(url, optin_urls_list.Name()))
      return true;
    ++optin_urls_list;
  }

  return false;
}

HRESULT GetUrlFromMoniker(IMoniker* moniker, IBindCtx* bind_context,
                          std::wstring* url) {
  if (!moniker || !url) {
    NOTREACHED();
    return E_INVALIDARG;
  }

  ScopedComPtr<IBindCtx> temp_bind_context;
  if (!bind_context) {
    CreateBindCtx(0, temp_bind_context.Receive());
    bind_context = temp_bind_context;
  }

  CComHeapPtr<WCHAR> display_name;
  HRESULT hr = moniker->GetDisplayName(bind_context, NULL, &display_name);
  if (display_name)
    *url = display_name;

  return hr;
}

bool IsValidUrlScheme(const std::wstring& url, bool is_privileged) {
  if (url.empty())
    return false;

  GURL crack_url(url);

  if (crack_url.SchemeIs("http") || crack_url.SchemeIs("https") ||
      crack_url.SchemeIs("about") || crack_url.SchemeIs("view-source"))
    return true;

  if (is_privileged && crack_url.SchemeIs("chrome-extension"))
    return true;

  if (StartsWith(url, kChromeAttachExternalTabPrefix, false))
    return true;

  return false;
}
