// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.

#ifndef CHROME_COMMON_URL_CONSTANTS_H_
#define CHROME_COMMON_URL_CONSTANTS_H_

namespace chrome {

// Canonical schemes you can use as input to GURL.SchemeIs().
extern const char kAboutScheme[];
extern const char kChromeInternalScheme[];
extern const char kChromeUIScheme[];  // The scheme used for DOMUIs.
extern const char kDataScheme[];
extern const char kExtensionScheme[];
extern const char kFileScheme[];
extern const char kFtpScheme[];
extern const char kGearsScheme[];
extern const char kHttpScheme[];
extern const char kHttpsScheme[];
extern const char kJavaScriptScheme[];
extern const char kMailToScheme[];
extern const char kPrintScheme[];
extern const char kUserScriptScheme[];
extern const char kViewSourceScheme[];

// Used to separate a standard scheme and the hostname: "://".
extern const char kStandardSchemeSeparator[];

// Null terminated list of schemes that are savable.
extern const char* kSavableSchemes[];

// About URLs (including schemes).
extern const char kAboutBlankURL[];
extern const char kAboutBrowserCrash[];
extern const char kAboutCacheURL[];
extern const char kAboutNetInternalsURL[];
extern const char kAboutCrashURL[];
extern const char kAboutCreditsURL[];
extern const char kAboutHangURL[];
extern const char kAboutMemoryURL[];
extern const char kAboutPluginsURL[];
extern const char kAboutShorthangURL[];
extern const char kAboutTermsURL[];

// chrome: URLs (including schemes). Should be kept in sync with the
// components below.
extern const char kChromeUIBookmarksURL[];
extern const char kChromeUIDevToolsURL[];
extern const char kChromeUIDownloadsURL[];
extern const char kChromeUIExtensionsURL[];
extern const char kChromeUIHistoryURL[];
extern const char kChromeUIPluginsURL[];
extern const char kChromeUIFileBrowseURL[];
extern const char kChromeUIMediaplayerURL[];
extern const char kChromeUIIPCURL[];
extern const char kChromeUINetworkURL[];
extern const char kChromeUINewTabURL[];

// chrome components of URLs. Should be kept in sync with the full URLs
// above.
extern const char kChromeUIBookmarksHost[];
extern const char kChromeUIDevToolsHost[];
extern const char kChromeUIDialogHost[];
extern const char kChromeUIDownloadsHost[];
extern const char kChromeUIExtensionsHost[];
extern const char kChromeUIFavIconPath[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUIPluginsHost[];
extern const char kChromeUIFileBrowseHost[];
extern const char kChromeUIMediaplayerHost[];
extern const char kChromeUIInspectorHost[];
extern const char kChromeUINetInternalsHost[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUIThumbnailPath[];
extern const char kChromeUIThemePath[];

// Sync related URL components.
extern const char kSyncResourcesHost[];
extern const char kSyncGaiaLoginPath[];
extern const char kSyncMergeAndSyncPath[];
extern const char kSyncThrobberPath[];
extern const char kSyncSetupFlowPath[];
extern const char kSyncSetupDonePath[];

// Network related URLs.
extern const char kNetworkViewCacheURL[];
extern const char kNetworkViewInternalsURL[];

// Call near the beginning of startup to register Chrome's internal URLs that
// should be parsed as "standard" with the googleurl library.
void RegisterChromeSchemes();

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
