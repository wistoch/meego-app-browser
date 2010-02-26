// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome.

#ifndef CHROME_COMMON_CHROME_SWITCHES_H_
#define CHROME_COMMON_CHROME_SWITCHES_H_

#include "build/build_config.h"
#include "base/base_switches.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch yo are looking for? Try looking in
// base/base_switches.cc instead.
// -----------------------------------------------------------------------------

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kActivateOnLaunch[];
extern const char kAllowFileAccessFromFiles[];
extern const char kAllowSandboxDebugging[];
extern const char kAlwaysEnableDevTools[];
extern const char kApp[];
extern const char kAppId[];
extern const char kAppLaunchAsPanel[];
extern const char kAutomationClientChannelID[];
extern const char kBookmarkMenu[];
extern const char kBrowserAssertTest[];
extern const char kBrowserCrashTest[];
extern const char kBrowserSubprocessPath[];
extern const char kChromeFrame[];
extern const char kCountry[];
extern const char kDebugPrint[];
extern const char kDiagnostics[];
extern const char kDisableAltWinstation[];
extern const char kDisableApplicationCache[];
extern const char kDisableAudio[];
extern const char kDisableByteRangeSupport[];
extern const char kDisableCustomJumpList[];
extern const char kDisableDatabases[];
extern const char kDisableDesktopNotifications[];
extern const char kDisableDevTools[];
extern const char kDisableExtensions[];
extern const char kDisableHangMonitor[];
extern const char kDisableIPv6[];
extern const char kDisableJavaScript[];
extern const char kDisableJava[];
extern const char kDisableLocalStorage[];
extern const char kDisableLogging[];
extern const char kDisableMetrics[];
extern const char kDisableNewTabFirstRun[];
extern const char kDisablePlugins[];
extern const char kDisablePopupBlocking[];
extern const char kDisablePromptOnRepost[];
extern const char kDisableRemoteFonts[];
extern const char kDisableSessionStorage[];
extern const char kDisableSharedWorkers[];
extern const char kDisableSiteSpecificQuirks[];
extern const char kDisableSync[];
extern const char kDisableSyncBookmarks[];
extern const char kDisableSyncPreferences[];
extern const char kDisableWebResources[];
extern const char kDisableWebSecurity[];
extern const char kDisableWebSockets[];
extern const char kDisableXSSAuditor[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];
extern const char kDnsLogDetails[];
extern const char kDnsPrefetchDisable[];
extern const char kDomAutomationController[];
extern const char kDumpHistogramsOnExit[];
extern const char kEnableBenchmarking[];
extern const char kEnableExperimentalExtensionApis[];
extern const char kEnableMetricsExtensionApi[];
extern const char kEnableExperimentalWebGL[];
extern const char kEnableExtensionApps[];
extern const char kEnableExtensionTimelineApi[];
extern const char kEnableExtensionToolstrips[];
extern const char kEnableFastback[];
extern const char kEnableFileCookies[];
extern const char kEnableGeolocation[];
extern const char kEnableGPUPlugin[];
extern const char kEnableIndexedDatabase[];
extern const char kEnableIPv6[];
extern const char kEnableLogging[];
extern const char kEnableMonitorProfile[];
extern const char kEnableNativeWebWorkers[];
#if defined(OS_MACOSX)
// TODO(dspringer): remove this when NaCl x86-32 security issues are fixed
extern const char kEnableNaClOnMac[];
#endif
extern const char kEnablePrivacyBlacklists[];
extern const char kEnableRendererAccessibility[];
extern const char kEnableSeccompSandbox[];
extern const char kEnableStatsTable[];
extern const char kEnableSync[];
extern const char kEnableSyncBookmarks[];
extern const char kEnableSyncPreferences[];
extern const char kEnableUserDataDirProfiles[];
extern const char kEnableVerticalTabs[];
extern const char kEnableWatchdog[];
extern const char kExperimentalSpellcheckerFeatures[];
extern const char kExplicitlyAllowedPorts[];
extern const char kExtensionProcess[];
extern const char kExtensionsUpdateFrequency[];
extern const char kFileDescriptorLimit[];
extern const char kFirstRun[];
extern const char kForceFieldTestNameAndValue[];
extern const char kGpuLauncher[];
extern const char kGpuProcess[];
extern const char kHideIcons[];
extern const char kHomePage[];
extern const char kHostResolverRules[];
extern const char kImport[];
extern const char kInProcessPlugins[];
extern const char kIncognito[];
extern const char kInternalNaCl[];
extern const char kInternalPepper[];
extern const char kJavaScriptFlags[];
extern const char kLoadExtension[];
extern const char kLoadPlugin[];
extern const char kLongLivedExtensions[];
extern const char kLogFilterPrefix[];
extern const char kLogPluginMessages[];
extern const char kLoggingLevel[];
extern const char kMakeDefaultBrowser[];
extern const char kMediaCacheSize[];
extern const char kMemoryProfiling[];
extern const char kMessageLoopHistogrammer[];
extern const char kMetricsRecordingOnly[];
extern const char kNaClBrokerProcess[];
extern const char kNaClLoaderProcess[];
extern const char kNaClStartupDialog[];
extern const char kNewTabPage[];
extern const char kNoDefaultBrowserCheck[];
extern const char kNoEvents[];
extern const char kNoFirstRun[];
extern const char kNoJsRandomness[];
extern const char kNoProxyServer[];
extern const char kNoReferrers[];
extern const char kNoSandbox[];
extern const char kOmniBoxPopupCount[];
extern const char kOpenInNewWindow[];
extern const char kPackExtension[];
extern const char kPackExtensionKey[];
extern const char kParentProfile[];
extern const char kPlaybackMode[];
extern const char kPluginDataDir[];
extern const char kPluginLauncher[];
extern const char kPluginPath[];
extern const char kPluginProcess[];
extern const char kPluginStartupDialog[];
extern const char kPrint[];
extern const char kProcessPerSite[];
extern const char kProcessPerTab[];
extern const char kProfileImportProcess[];
extern const char kProxyAutoDetect[];
extern const char kProxyBypassList[];
extern const char kProxyPacUrl[];
extern const char kProxyServer[];
extern const char kPurgeMemoryButton[];
extern const char kRecordMode[];
extern const char kRemoteShellPort[];
extern const char kRendererAssertTest[];
extern const char kRendererCmdPrefix[];
extern const char kRendererCrashTest[];
extern const char kRendererProcess[];
extern const char kRendererStartupDialog[];
extern const char kRestoreLastSession[];
extern const char kSafePlugins[];
extern const char kSdchFilter[];
extern const char kSearchInOmniboxHint[];
extern const char kShowCompositedLayerBorders[];
extern const char kShowIcons[];
extern const char kShowPaintRects[];
extern const char kSilentDumpOnDCHECK[];
extern const char kSimpleDataSource[];
extern const char kSingleProcess[];
extern const char kStartMaximized[];
extern const char kSyncServiceURL[];
extern const char kSyncNotificationMethod[];
extern const char kSyncerThreadTimedStop[];
extern const char kTabCountToLoadOnSessionRestore[];
extern const char kTestName[];
extern const char kTestSandbox[];
extern const char kTestingChannelID[];
extern const char kThumbnailStore[];
extern const char kTrustedPlugins[];
extern const char kTryChromeAgain[];
extern const char kUninstall[];
extern const char kUseSpdy[];
extern const char kFixedHttpPort[];
extern const char kFixedHttpsPort[];
extern const char kUseLowFragHeapCrt[];
extern const char kUserAgent[];
extern const char kUserDataDir[];
extern const char kUserScriptsDir[];
extern const char kUtilityCmdPrefix[];
extern const char kUtilityProcess[];
extern const char kUtilityProcessAllowedDir[];
extern const char kWaitForDebuggerChildren[];
extern const char kWebWorkerProcessPerCore[];
extern const char kWebWorkerShareProcesses[];
extern const char kWinHttpProxyResolver[];
extern const char kWorkerProcess[];
extern const char kWorkerStartupDialog[];
extern const char kZygoteCmdPrefix[];
extern const char kZygoteProcess[];

#if defined(OS_CHROMEOS)
extern const char kCookiePipe[];
extern const char kEnableGView[];
extern const char kLoginManager[];
extern const char kInChromeAuth[];
// TODO(avayvod): Remove this flag when it's unnecessary for testing
// purposes.
extern const char kLoginScreen[];
extern const char kLoginScreenSize[];
extern const char kTestLoadLibcros[];
extern const char kProfile[];
extern const char kLoginProfile[];
extern const char kChromeosFrame[];
#endif

#if defined(OS_WIN)
extern const char kUseNSSForSSL[];
#endif

#if defined(OS_LINUX)
extern const char kAutoSSLClientAuth[];
#endif

#if defined(OS_POSIX)
extern const char kEnableCrashReporter[];
extern const char kNoProcessSingletonDialog[];
#endif

#if defined(OS_MACOSX)
extern const char kEnableSandboxLogging[];
#else
extern const char kKioskMode[];
#endif

#ifndef NDEBUG
extern const char kGearsPluginPathOverride[];
extern const char kInvalidateSyncLogin[];
#endif

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CHROME_COMMON_CHROME_SWITCHES_H_
