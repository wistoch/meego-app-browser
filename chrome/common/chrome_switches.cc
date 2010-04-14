// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"

#include "base/base_switches.h"
#include "base/command_line.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? try looking in
// base/base_switches.cc instead.
// -----------------------------------------------------------------------------

// Activate (make foreground) myself on launch.  Helpful when Chrome
// is launched on the command line (e.g. by Selenium).  Only needed on Mac.
const char kActivateOnLaunch[] = "activate-on-launch";

// By default, file:// URIs cannot read other file:// URIs. This is an
// override for developers who need the old behavior for testing.
const char kAllowFileAccessFromFiles[] = "allow-file-access-from-files";

// Allows debugging of sandboxed processes (see zygote_main_linux.cc).
const char kAllowSandboxDebugging[]         = "allow-sandbox-debugging";

// Enable web inspector for all windows, even if they're part of the browser.
// Allows us to use our dev tools to debug browser windows itself.
const char kAlwaysEnableDevTools[]          = "always-enable-dev-tools";

// Specifies that the associated value should be launched in "application" mode.
const char kApp[]                           = "app";

// Specifies that the extension-app with the specified id should be launched
// according to its configuration.
const char kAppId[]                         = "app-id";

// Lacks meaning with out kApp. Causes the specified app to be launched in an
// panel window.
const char kAppLaunchAsPanel[]              = "app-launch-as-panel";

// Makes the app launcher popup when a new tab is created.
const char kAppLauncherForNewTab[]          = "app-launcher-new-tab";

// Authentication white list for servers
const char kAuthServerWhitelist[]           = "auth-server-whitelist";

// The value of this switch tells the app to listen for and broadcast
// automation-related messages on IPC channel with the given ID.
const char kAutomationClientChannelID[]     = "automation-channel";

// Enables the bookmark menu.
const char kBookmarkMenu[]                  = "bookmark-menu";

// Causes the browser process to throw an assertion on startup.
const char kBrowserAssertTest[]             = "assert-test";

// Causes the browser process to crash on startup.
const char kBrowserCrashTest[]              = "crash-test";

// Path to the exe to run for the renderer and plugin subprocesses.
const char kBrowserSubprocessPath[]         = "browser-subprocess-path";

// Run Chrome in Chrome Frame mode. This means that Chrome expects to be run
// as a dependent process of the Chrome Frame plugin.
const char kChromeFrame[]                   = "chrome-frame";

// The Country we should use.  This is normally obtained from the operating
// system during first run and cached in the preferences afterwards.  This is a
// string value, the 2 letter code from ISO 3166-1.
const char kCountry[]                       = "country";

// Enables support to debug printing subsystem.
const char kDebugPrint[]                    = "debug-print";

// Triggers a pletora of diagnostic modes.
const char kDiagnostics[]                   = "diagnostics";

// Disables the alternate window station for the renderer.
const char kDisableAltWinstation[]          = "disable-winsta";

// Disable the ApplicationCache.
const char kDisableApplicationCache[]       = "disable-application-cache";

// Replaces the audio IPC layer for <audio> and <video> with a mock audio
// device, useful when using remote desktop or machines without sound cards.
// This is temporary until we fix the underlying problem.
//
// TODO(scherkus): remove --disable-audio when we have a proper fallback
// mechanism.
const char kDisableAudio[]                  = "disable-audio";

// Disable limits on the number of backing stores. Can prevent blinking for
// users with many windows/tabs and lots of memory.
const char kDisableBackingStoreLimit[]      = "disable-backing-store-limit";

// Disable support for cached byte-ranges.
const char kDisableByteRangeSupport[]       = "disable-byte-range-support";

// Disables the custom JumpList on Windows 7.
const char kDisableCustomJumpList[]         = "disable-custom-jumplist";

// Disables HTML5 DB support.
const char kDisableDatabases[]              = "disable-databases";

// Disables desktop notifications (default enabled on windows).
const char kDisableDesktopNotifications[]   = "disable-desktop-notifications";

// Browser flag to disable the web inspector for all renderers.
const char kDisableDevTools[]               = "disable-dev-tools";

// Disable extensions.
const char kDisableExtensions[]             = "disable-extensions";

// Suppresses support for the Geolocation javascript API.
const char kDisableGeolocation[]            = "disable-geolocation";

// Suppresses hang monitor dialogs in renderer processes.
const char kDisableHangMonitor[]            = "disable-hang-monitor";

// Disable the internal Flash Player.
const char kDisableInternalFlash[]          = "disable-internal-flash";

// Don't resolve hostnames to IPv6 addresses. This can be used when debugging
// issues relating to IPv6, but shouldn't otherwise be needed. Be sure to
// file bugs if something isn't working properly in the presence of IPv6.
// This flag can be overidden by the "enable-ipv6" flag.
const char kDisableIPv6[]                   = "disable-ipv6";

// Don't execute JavaScript (browser JS like the new tab page still runs).
const char kDisableJavaScript[]             = "disable-javascript";

// Prevent Java from running.
const char kDisableJava[]                   = "disable-java";

// Disable LocalStorage.
const char kDisableLocalStorage[]           = "disable-local-storage";

// Force logging to be disabled.  Logging is enabled by default in debug
// builds.
const char kDisableLogging[]                = "disable-logging";

// Whether we should prevent the new tab page from showing the first run
// notification.
const char kDisableNewTabFirstRun[]         = "disable-new-tab-first-run";

// Prevent plugins from running.
const char kDisablePlugins[]                = "disable-plugins";

// Disable pop-up blocking.
const char kDisablePopupBlocking[]          = "disable-popup-blocking";

// Normally when the user attempts to navigate to a page that was the result of
// a post we prompt to make sure they want to. This switch may be used to
// disable that check. This switch is used during automated testing.
const char kDisablePromptOnRepost[]         = "disable-prompt-on-repost";

// Disable remote web font support. SVG font should always work whether
// this option is specified or not.
const char kDisableRemoteFonts[]            = "disable-remote-fonts";

// Disable session storage.
const char kDisableSessionStorage[]         = "disable-session-storage";

// Enable shared workers. Functionality not yet complete.
const char kDisableSharedWorkers[]          = "disable-shared-workers";

// Disable site-specific tailoring to compatibility issues in WebKit.
const char kDisableSiteSpecificQuirks[]     = "disable-site-specific-quirks";

// Disable syncing browser data to a Google Account.
const char kDisableSync[]                   = "disable-sync";

// Disable syncing of autofill.
const char kDisableSyncAutofill[]           = "disable-sync-autofill";

// Disable syncing of bookmarks.
const char kDisableSyncBookmarks[]          = "disable-sync-bookmarks";

// Disable syncing of preferences.
const char kDisableSyncPreferences[]        = "disable-sync-preferences";

// Disable syncing of themes.
const char kDisableSyncThemes[]             = "disable-sync-themes";

// Disable syncing of typed urls.
const char kDisableSyncTypedUrls[]          = "disable-sync-typed-urls";

// Disable the tabbed bookmark manager. This makes us use the native bookmark
// manager instead.
const char kDisableTabbedBookmarkManager[]  = "disable-tabbed-bookmark-manager";

// Enables the backend service for web resources, used in the new tab page for
// loading tips and recommendations from a JSON feed.
const char kDisableWebResources[]           = "disable-web-resources";

// Don't enforce the same-origin policy.  (Used by people testing their sites.)
const char kDisableWebSecurity[]            = "disable-web-security";

// Disable Web Sockets support.
const char kDisableWebSockets[]             = "disable-web-sockets";

// Use a specific disk cache location, rather than one derived from the
// UserDatadir.
const char kDiskCacheDir[]                  = "disk-cache-dir";

// Forces the maximum disk space to be used by the disk cache, in bytes.
const char kDiskCacheSize[]                 = "disk-cache-size";

const char kDnsLogDetails[]                 = "dns-log-details";

// Disables prefetching of DNS information.
const char kDnsPrefetchDisable[]            = "dns-prefetch-disable";

// Specifies if the dom_automation_controller_ needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when automating
// dom based tests.
const char kDomAutomationController[]       = "dom-automation";

// Dump any accumualted histograms to the log when browser terminates (requires
// logging to be enabled to really do anything).  Used by developers and test
// scripts.
const char kDumpHistogramsOnExit[]          = "dump-histograms-on-exit";

// Enables AeroPeek for each tab. (This switch only works on Windows 7).
const char kEnableAeroPeekTabs[]            = "enable-aero-peek-tabs";

// Enables the benchmarking extensions.
const char kEnableBenchmarking[]            = "enable-benchmarking";

// Enables extension APIs that are in development.
const char kEnableExperimentalExtensionApis[] =
    "enable-experimental-extension-apis";

// Enable experimental WebGL support.
const char kEnableExperimentalWebGL[]       = "enable-webgl";

// Enable experimental extension apps.
const char kEnableExtensionApps[]           = "enable-extension-apps";

// Enable experimental timeline API.
const char kEnableExtensionTimelineApi[]    = "enable-extension-timeline-api";

// Enable extension toolstrips (deprecated API - will be removed).
const char kEnableExtensionToolstrips[]     = "enable-extension-toolstrips";

// Enable the fastback page cache.
const char kEnableFastback[]                = "enable-fastback";

// By default, cookies are not allowed on file://. They are needed for
// testing, for example page cycler and layout tests.  See bug 1157243.
const char kEnableFileCookies[]             = "enable-file-cookies";

// Enable the Indexed Database API.
const char kEnableIndexedDatabase[]         = "enable-indexed-database";

// Enable IPv6 support, even if probes suggest that it may not be fully
// supported.  Some probes may require internet connections, and this flag will
// allow support independent of application testing.
// This flag overrides "disable-ipv6" which appears elswhere in this file.
const char kEnableIPv6[]                    = "enable-ipv6";

// Enable the GPU plugin and Pepper 3D rendering.
const char kEnableGPUPlugin[]               = "enable-gpu-plugin";

// Enable experimental GPU rendering for backing store and video.
const char kEnableGPURendering[]            = "enable-gpu-rendering";

// Force logging to be enabled.  Logging is disabled by default in release
// builds.
const char kEnableLogging[]                 = "enable-logging";

// On Windows, converts the page to the currently-installed monitor profile.
// This does NOT enable color management for images. The source is still
// assumed to be sRGB.
const char kEnableMonitorProfile[]          = "enable-monitor-profile";

// Runs the Native Client inside the renderer process and enables GPU plugin
// (internally adds kInternalNaCl and lEnableGpuPlugin to the command line).
const char kEnableNaCl[]                    = "enable-nacl";

// Enable Native Web Worker support.
const char kEnableNativeWebWorkers[]        = "enable-native-web-workers";

// Enable Privacy Blacklists.
const char kEnablePrivacyBlacklists[]       = "enable-privacy-blacklists";

// Turns on the accessibility in the renderer.  Off by default until
// http://b/issue?id=1432077 is fixed.
const char kEnableRendererAccessibility[]   = "enable-renderer-accessibility";

// Enables StatsTable, logging statistics to a global named shared memory table.
const char kEnableStatsTable[]              = "enable-stats-table";

// Enable syncing browser data to a Google Account.
const char kEnableSync[]                    = "enable-sync";

// Enable syncing browser autofill.
const char kEnableSyncAutofill[]            = "enable-sync-autofill";

// Enable syncing browser bookmarks.
const char kEnableSyncBookmarks[]           = "enable-sync-bookmarks";

// Enable syncing browser preferences.
const char kEnableSyncPreferences[]         = "enable-sync-preferences";

// Enable syncing browser themes.
const char kEnableSyncThemes[]              = "enable-sync-themes";

// Enable syncing browser typed urls.
const char kEnableSyncTypedUrls[]           = "enable-sync-typed-urls";

// Whether the multiple profiles feature based on the user-data-dir flag is
// enabled or not.
const char kEnableUserDataDirProfiles[]     = "enable-udd-profiles";

// Enables the option to show tabs as a vertical stack down the side of the
// browser window.
const char kEnableVerticalTabs[]            = "enable-vertical-tabs";

// Enables video layering where video is rendered as a separate layer outside
// of the backing store.
const char kEnableVideoLayering[]           = "enable-video-layering";

// Enables video logging where video elements log playback performance data to
// the debug log.
const char kEnableVideoLogging[]            = "enable-video-logging";

// Spawn threads to watch for excessive delays in specified message loops.
// User should set breakpoints on Alarm() to examine problematic thread.
// Usage:   -enable-watchdog=[ui][io]
// Order of the listed sub-arguments does not matter.
const char kEnableWatchdog[]                = "enable-watchdog";

// Disable WebKit's XSSAuditor.  The XSSAuditor mitigates reflective XSS.
const char kEnableXSSAuditor[]             = "enable-xss-auditor";

// Enables experimental features for Spellchecker. Right now, the first
// experimental feature is auto spell correct, which corrects words which are
// misppelled by typing the word with two consecutive letters swapped. The
// features that will be added next are:
// 1 - Allow multiple spellcheckers to work simultaneously.
// 2 - Allow automatic detection of spell check language.
// TODO(sidchat): Implement the above fetaures to work under this flag.
const char kExperimentalSpellcheckerFeatures[] =
    "experimental-spellchecker-features";

// Explicitly allow additional ports using a comma separated list of port
// numbers.
const char kExplicitlyAllowedPorts[]        = "explicitly-allowed-ports";

// Causes the process to run as an extension subprocess.
const char kExtensionProcess[]              = "extension";

// Frequency in seconds for Extensions auto-update.
const char kExtensionsUpdateFrequency[]     = "extensions-update-frequency";

// The file descriptor limit is set to the value of this switch, subject to the
// OS hard limits. Useful for testing that file descriptor exhaustion is handled
// gracefully.
const char kFileDescriptorLimit[]           = "file-descriptor-limit";

// Display the First Run experience when the browser is started, regardless of
// whether or not it's actually the first run.
const char kFirstRun[]                      = "first-run";

// Some field tests may rendomized in the browser, and the randomly selected
// outcome needs to be propogated to the renderer.  For instance, this is used
// to modify histograms recorded in the renderer, or to get the renderer to
// also set of its state (initialize, or not initialize components) to match the
// experiment(s).
// The argument is a string-ized list of experiment names, and the associated
// value that was randomly selected.  In the recent implementetaion, the
// persistent representation generated by field_trial.cc and later decoded, is a
// list of name and value pairs, separated by slashes. See field trial.cc for
// current details.
const char kForceFieldTestNameAndValue[]    = "force-fieldtest";

// Extra command line options for launching the GPU process (normally used
// for debugging). Use like renderer-cmd-prefix.
const char kGpuLauncher[]                   = "gpu-launcher";

// Makes this process a GPU sub-process.
const char kGpuProcess[]                    = "gpu-process";

// Causes the GPU process to display a dialog on launch.
const char kGpuStartupDialog[]              = "gpu-startup-dialog";

// Make Windows happy by allowing it to show "Enable access to this program"
// checkbox in Add/Remove Programs->Set Program Access and Defaults. This
// only shows an error box because the only way to hide Chrome is by
// uninstalling it.
const char kHideIcons[]                     = "hide-icons";

// The value of this switch specifies which page will be displayed
// in newly-opened tabs.  We need this for testing purposes so
// that the UI tests don't depend on what comes up for http://google.com.
const char kHomePage[]                      = "homepage";

// Comma separated list of rules that control how hostnames are resolved.
//
// For example:
//    "MAP * 127.0.0.1" --> Forces all hostnames to be resolved to 127.0.0.1
//    "MAP *.google.com proxy" --> Forces all google.com subdomains to be
//                                 resolved to "proxy".
//    "MAP test.com [::1]:77 --> Forces "test.com" to resolve to IPv6 loopback.
//                               Will also force the port of the resulting
//                               socket address to be 77.
//    "MAP * baz, EXCLUDE www.google.com" --> Remaps everything to "baz",
//                                            except for "www.google.com".
const char kHostResolverRules[]             = "host-resolver-rules";

// Perform importing from another browser. The value associated with this
// setting encodes the target browser and what items to import.
const char kImport[]                        = "import";

// Perform bookmark importing from an HTML file. The value associated with this
// setting encodes the file path. It may be used jointly with kImport.
const char kImportFromFile[]                = "import-from-file";

// Runs plugins inside the renderer process
const char kInProcessPlugins[]              = "in-process-plugins";

// Causes the browser to launch directly in incognito mode.
const char kIncognito[]                     = "incognito";

// Runs the Native Client inside the renderer process.
const char kInternalNaCl[]                  = "internal-nacl";

// Uses internal plugin for displaying PDFs.
const char kInternalPDF[]                  = "internal-pdf";

// Runs a trusted Pepper plugin inside the renderer process.
const char kInternalPepper[]                = "internal-pepper";

// Specifies the flags passed to JS engine
const char kJavaScriptFlags[]               = "js-flags";

// Load an extension from the specified directory.
const char kLoadExtension[]                 = "load-extension";

// Load an NPAPI plugin from the specified path.
const char kLoadPlugin[]                    = "load-plugin";

// Long lived extensions.
const char kLongLivedExtensions[]           = "long-lived-extensions";

// Will filter log messages to show only the messages that are prefixed
// with the specified value. See also kEnableLogging and kLoggingLevel.
const char kLogFilterPrefix[]               = "log-filter-prefix";

// Make plugin processes log their sent and received messages to LOG(INFO).
const char kLogPluginMessages[]             = "log-plugin-messages";

// Sets the minimum log level. Valid values are from 0 to 3:
// INFO = 0, WARNING = 1, LOG_ERROR = 2, LOG_FATAL = 3.
const char kLoggingLevel[]                  = "log-level";

// Make Chrome default browser
const char kMakeDefaultBrowser[]            = "make-default-browser";

// Forces the maximum disk space to be used by the media cache, in bytes.
const char kMediaCacheSize[]                = "media-cache-size";

// Enable dynamic loading of the Memory Profiler DLL, which will trace
// all memory allocations during the run.
const char kMemoryProfiling[]               = "memory-profile";

// Enable histograming of tasks served by MessageLoop. See about:histograms/Loop
// for results, which show frequency of messages on each thread, including APC
// count, object signalling count, etc.
const char kMessageLoopHistogrammer[]       = "message-loop-histogrammer";

// Enables the recording of metrics reports but disables reporting.  In
// contrast to kDisableMetrics, this executes all the code that a normal client
// would use for reporting, except the report is dropped rather than sent to
// the server. This is useful for finding issues in the metrics code during UI
// and performance tests.
const char kMetricsRecordingOnly[]          = "metrics-recording-only";

// Causes the process to run as a NativeClient broker
// (used for launching NaCl loader processes on 64-bit Windows).
const char kNaClBrokerProcess[]             = "nacl-broker";

// Causes the process to run as a NativeClient loader.
const char kNaClLoaderProcess[]             = "nacl-loader";

// Causes the Native Client process to display a dialog on launch.
const char kNaClStartupDialog[]             = "nacl-startup-dialog";

// Allows the new tab page resource to be loaded from a local HTML file. This
// should be a path to the HTML file that you want to use for the new tab page.
// It is used for manually testing new versions of the new tab page only,
// performance will be poor.
const char kNewTabPage[]                    = "new-tab-page";

// Disables the default browser check. Useful for UI/browser tests where we
// want to avoid having the default browser info-bar displayed.
const char kNoDefaultBrowserCheck[]         = "no-default-browser-check";

// Don't record/playback events when using record & playback.
const char kNoEvents[]                      = "no-events";

// Bypass the First Run experience when the browser is started, regardless of
// whether or not it's actually the first run. Overrides kFirstRun in case
// you're for some reason tempted to pass them both.
const char kNoFirstRun[]                    = "no-first-run";

// Support a separate switch that enables the v8 playback extension.
// The extension causes javascript calls to Date.now() and Math.random()
// to return consistent values, such that subsequent loads of the same
// page will result in consistent js-generated data and XHR requests.
// Pages may still be able to generate inconsistent data from plugins.
const char kNoJsRandomness[]                = "no-js-randomness";

// Don't send HTTP-Referer headers.
const char kNoReferrers[]                   = "no-referrers";

// Don't use a proxy server, always make direct connections. Overrides any
// other proxy server flags that are passed.
const char kNoProxyServer[]                 = "no-proxy-server";

// Runs the renderer outside the sandbox.
const char kNoSandbox[]                     = "no-sandbox";

// Number of entries to show in the omnibox popup.
const char kOmniBoxPopupCount[]             = "omnibox-popup-count";

// Launch URL in new browser window.
const char kOpenInNewWindow[]               = "new-window";

// Package an extension to a .crx installable file from a given directory.
const char kPackExtension[]                 = "pack-extension";

// Optional PEM private key is to use in signing packaged .crx.
const char kPackExtensionKey[]              = "pack-extension-key";

// Specifies the path to the user data folder for the parent profile.
const char kParentProfile[]                 = "parent-profile";

// Read previously recorded data from the cache. Only cached data is read.
// See kRecordMode.
const char kPlaybackMode[]                  = "playback-mode";

// Specifies the plugin data directory, which is where plugins (Gears
// specifically) will store its state.
const char kPluginDataDir[]                 = "plugin-data-dir";

// Specifies a command that should be used to launch the plugin process.  Useful
// for running the plugin process through purify or quantify.  Ex:
//   --plugin-launcher="path\to\purify /Run=yes"
const char kPluginLauncher[]                = "plugin-launcher";

// Tells the plugin process the path of the plugin to load
const char kPluginPath[]                    = "plugin-path";

// Causes the process to run as a plugin subprocess.
const char kPluginProcess[]                 = "plugin";

// Causes the plugin process to display a dialog on launch.
const char kPluginStartupDialog[]           = "plugin-startup-dialog";

// Prints the pages on the screen.
const char kPrint[] = "print";

// Runs a single process for each site (i.e., group of pages from the same
// registered domain) the user visits.  We default to using a renderer process
// for each site instance (i.e., group of pages from the same registered
// domain with script connections to each other).
const char kProcessPerSite[]                = "process-per-site";

// Runs each set of script-connected tabs (i.e., a BrowsingInstance) in its own
// renderer process.  We default to using a renderer process for each
// site instance (i.e., group of pages from the same registered domain with
// script connections to each other).
const char kProcessPerTab[]                 = "process-per-tab";

// Causes the process to run as a profile import subprocess.
const char kProfileImportProcess[]          = "profile-import";

// Force proxy auto-detection.
const char kProxyAutoDetect[]               = "proxy-auto-detect";

// Specify a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are
// also specified.
// This is a comma separated list of bypass rules. See:
// "net/proxy/proxy_bypass_rules.h" for the format of these rules.
const char kProxyBypassList[]               = "proxy-bypass-list";

// Use the pac script at the given URL
const char kProxyPacUrl[]                   = "proxy-pac-url";

// Use a specified proxy server, overrides system settings. This switch only
// affects HTTP and HTTPS requests.
const char kProxyServer[]                   = "proxy-server";

// Adds a "Purge memory" button to the Task Manager, which tries to dump as
// much memory as possible.  This is mostly useful for testing how well the
// MemoryPurger functionality works.
//
// NOTE: This is only implemented for Views.
const char kPurgeMemoryButton[]             = "purge-memory-button";

// Chrome supports a playback and record mode.  Record mode saves *everything*
// to the cache.  Playback mode reads data exclusively from the cache.  This
// allows us to record a session into the cache and then replay it at will.
// See also kPlaybackMode.
const char kRecordMode[]                    = "record-mode";

// Enable remote debug / automation shell on the specified port.
const char kRemoteShellPort[]               = "remote-shell-port";

// Causes the renderer process to throw an assertion on launch.
const char kRendererAssertTest[]            = "renderer-assert-test";

#if !defined(OFFICIAL_BUILD)
// Causes the renderer process to throw an assertion on launch.
const char kRendererCheckFalseTest[]        = "renderer-check-false-test";
#endif

// On POSIX only: the contents of this flag are prepended to the renderer
// command line. Useful values might be "valgrind" or "xterm -e gdb --args".
const char kRendererCmdPrefix[]             = "renderer-cmd-prefix";

// Causes the renderer process to crash on launch.
const char kRendererCrashTest[]             = "renderer-crash-test";

// Causes the process to run as renderer instead of as browser.
const char kRendererProcess[]               = "renderer";

// Causes the renderer process to display a dialog on launch.
const char kRendererStartupDialog[]         = "renderer-startup-dialog";

// Indicates the last session should be restored on startup. This overrides
// the preferences value and is primarily intended for testing. The value of
// this switch is the number of tabs to wait until loaded before
// 'load completed' is sent to the ui_test.
const char kRestoreLastSession[]            = "restore-last-session";

// Runs the plugin processes inside the sandbox.
const char kSafePlugins[]                   = "safe-plugins";

// Enable support for SDCH filtering (dictionary based expansion of content).
// Optional argument is *the* only domain name that will have SDCH suppport.
// Default is  "-enable-sdch" to advertise SDCH on all domains.
// Sample usage with argument: "-enable-sdch=.google.com"
// SDCH is currently only supported server-side for searches on google.com.
const char kSdchFilter[]                    = "enable-sdch";

// Enables the showing of an info-bar instructing user they can search directly
// from the omnibox.
const char kSearchInOmniboxHint[]           = "search-in-omnibox-hint";

// See kHideIcons.
const char kShowIcons[]                     = "show-icons";

// Renders a border around composited Render Layers to help debug and study
// layer compositing.
const char kShowCompositedLayerBorders[]    = "show-composited-layer-borders";

// Visibly render a border around paint rects in the web page to help debug
// and study painting behavior.
const char kShowPaintRects[]                = "show-paint-rects";

// Change the DCHECKS to dump memory and continue instead of displaying error
// dialog. This is valid only in Release mode when --enable-dcheck is
// specified.
const char kSilentDumpOnDCHECK[]            = "silent-dump-on-dcheck";

// Replaces the buffered data source for <audio> and <video> with a simplified
// resource loader that downloads the entire resource into memory.
//
// TODO(scherkus): remove --simple-data-source when our media resource loading
// is cleaned up and playback testing completed.
const char kSimpleDataSource[]              = "simple-data-source";

// Runs the renderer and plugins in the same process as the browser
const char kSingleProcess[]                 = "single-process";

// Start the browser maximized, regardless of any previous settings.
const char kStartMaximized[]                = "start-maximized";

// Override the default server used for profile sync.
const char kSyncServiceURL[]                = "sync-url";

// Override the default notification method for sync.
const char kSyncNotificationMethod[]        = "sync-notification-method";

// Use the SyncerThread implementation that matches up with the old pthread
// impl semantics, but using Chrome synchronization primitives.  The only
// difference between this and the default is that we now have no timeout on
// Stop().  Should only use if you experience problems with the default.
const char kSyncerThreadTimedStop[]         = "syncer-thread-timed-stop";

// Used to set the value of SessionRestore::num_tabs_to_load_. See
// session_restore.h for details.
const char kTabCountToLoadOnSessionRestore[]=
    "tab-count-to-load-on-session-restore";

// Pass the name of the current running automated test to Chrome.
const char kTestName[]                      = "test-name";

// Runs the security test for the sandbox.
const char kTestSandbox[]                   = "test-sandbox";

// Pass the type of the current test harness ("browser" or "ui")
const char kTestType[]                      = "test-type";

// The value of this switch tells the app to listen for and broadcast
// testing-related messages on IPC channel with the given ID.
const char kTestingChannelID[]              = "testing-channel";

// Enables using ThumbnailStore instead of ThumbnailDatabase for setting and
// getting thumbnails for the new tab page.
const char kThumbnailStore[]                = "thumbnail-store";

// Excludes these plugins from the plugin sandbox.
// This is a comma-separated list of plugin library names.
const char kTrustedPlugins[]                = "trusted-plugins";

// Experimental. Shows a dialog asking the user to try chrome. This flag
// is to be used only by the upgrade process.
const char kTryChromeAgain[]                = "try-chrome-again";

// Runs un-installation steps that were done by chrome first-run.
const char kUninstall[]                     = "uninstall";

// Use Spdy for the transport protocol instead of HTTP.
// This is a temporary testing flag.
const char kUseSpdy[]                       = "use-spdy";

// These two flags are used to force http and https requests to fixed ports.
const char kFixedHttpPort[]                 = "testing-fixed-http-port";
const char kFixedHttpsPort[]                = "testing-fixed-https-port";

// Ignore certificate related errors.
const char kIgnoreCertificateErrors[]       = "ignore-certificate-errors";

// Set the maximum SPDY sessions per domain.
const char kMaxSpdySessionsPerDomain[]      = "max-spdy-sessions-per-domain";

// Use the low fragmentation heap for the CRT.
const char kUseLowFragHeapCrt[]             = "use-lf-heap";

// A string used to override the default user agent with a custom one.
const char kUserAgent[]                     = "user-agent";

// Specifies the user data directory, which is where the browser will look
// for all of its state.
const char kUserDataDir[]                   = "user-data-dir";

// directory to locate user scripts in as an over-ride of the default
const char kUserScriptsDir[]                = "user-scripts-dir";

// On POSIX only: the contents of this flag are prepended to the utility
// process command line. Useful values might be "valgrind" or "xterm -e gdb
// --args".
const char kUtilityCmdPrefix[]              = "utility-cmd-prefix";

// Causes the process to run as a utility subprocess.
const char kUtilityProcess[]                = "utility";

// The utility process is sandboxed, with access to one directory. This flag
// specifies the directory that can be accessed.
const char kUtilityProcessAllowedDir[]      = "utility-allowed-dir";

// Will add kWaitForDebugger to every child processes. If a value is passed, it
// will be used as a filter to determine if the child process should have the
// kWaitForDebugger flag passed on or not.
const char kWaitForDebuggerChildren[]       = "wait-for-debugger-children";

// Causes the worker process allocation to use as many processes as cores.
const char kWebWorkerProcessPerCore[]       = "web-worker-process-per-core";

// Causes workers to run together in one process, depending on their domains.
// Note this is duplicated in webworkerclient_impl.cc
const char kWebWorkerShareProcesses[]       = "web-worker-share-processes";

// Use WinHTTP to fetch and evaluate PAC scripts. Otherwise the default is
// to use Chromium's network stack to fetch, and V8 to evaluate.
const char kWinHttpProxyResolver[]          = "winhttp-proxy-resolver";

// Causes the process to run as a worker subprocess.
const char kWorkerProcess[]                 = "worker";

// The prefix used when starting the zygote process. (i.e. 'gdb --args')
const char kZygoteCmdPrefix[]               = "zygote-cmd-prefix";

// Causes the process to run as a renderer zygote.
const char kZygoteProcess[]                 = "zygote";

#if defined(OS_CHROMEOS)
// The name of the pipe over which the Chrome OS login manager will send
// single-sign-on cookies.
const char kCookiePipe[]                    = "cookie-pipe";

// Enable the redirection of viewable document requests to the Google
// Document Viewer.
const char kEnableGView[]                   = "enable-gview";

// Should we show the image based login?
const char kEnableLoginImages[]             = "enable-login-images";

// Enable Chrome-as-a-login-manager behavior.
const char kLoginManager[]                  = "login-manager";
// Enable Chrome to do ClientLogin on its own in the login-manager context.
const char kInChromeAuth[]                  = "in-chrome-auth";
// Allows to override the first login screen. The value should be the name
// of the first login screen to show (see
// chrome/browser/chromeos/login/login_wizard_view.cc for actual names).
// Ignored if kLoginManager is not specified.
// TODO(avayvod): Remove when the switch is no longer needed for testing.
const char kLoginScreen[]          = "login-screen";
// Allows control over the initial login screen size. Pass width,height.
const char kLoginScreenSize[]               = "login-screen-size";

// Attempts to load libcros and validate it, then exits. A nonzero return code
// means the library could not be loaded correctly.
const char kTestLoadLibcros[]               = "test-load-libcros";

// TODO(davemoore) Delete this once chromeos has started using
// login-profile as its arg.
const char kProfile[]                       = "profile";

// Specifies the profile to use once a chromeos user is logged in.
const char kLoginProfile[]                  = "login-profile";

// Specifies the user which is already logged in.
const char kLoginUser[]                     = "login-user";

// Use the frame layout used in chromeos.
const char kChromeosFrame[]                 = "chromeos-frame";
#endif

#if defined(OS_WIN)
// Use NSS instead of the system SSL library for SSL.
// This is a temporary testing flag.
const char kUseNSSForSSL[]                  = "use-nss-for-ssl";
#endif

#if defined(OS_POSIX)
// Bypass the error dialog when the profile lock couldn't be attained.
// A flag, generated internally by Chrome for renderer and other helper process
// command lines on Linux and Mac.  It tells the helper process to enable crash
// dumping and reporting, because helpers cannot access the profile or other
// files needed to make this decision.
// If passed to the browser, it'll be passed on to all the helper processes
// as well, thereby force-enabling the crash reporter.
const char kEnableCrashReporter[]           = "enable-crash-reporter";

// This switch is used during automated testing.
const char kNoProcessSingletonDialog[]      = "no-process-singleton-dialog";
#endif

#if defined(OS_MACOSX)
// Cause the OS X sandbox write to syslog every time an access to a resource
// is denied by the sandbox.
const char kEnableSandboxLogging[]          = "enable-sandbox-logging";

// Temporary flag to allow Flash to negotiate the Core Animation drawing model.
// This will eventually become the default, and the flag can be removed.
const char kEnableFlashCoreAnimation[]      = "enable-flash-core-animation";
#else
// Enable Kiosk mode.
const char kKioskMode[]                     = "kiosk";
#endif

#ifndef NDEBUG
// Debug only switch to specify which gears plugin dll to load.
const char kGearsPluginPathOverride[]       = "gears-plugin-path";

// Makes sure any sync login attempt will fail with an error.  (Only
// used for testing.)
const char kInvalidateSyncLogin[]           = "invalidate-sync-login";

// Makes sure any sync xmpp login attempt will fail with an error.  (Only
// used for testing.)
const char kInvalidateSyncXmppLogin[]       = "invalidate-sync-xmpp-login";

// Debug only switch to specify which websocket live experiment host to be used.
// If host is specified, it also makes initial delay shorter (5 min to 5 sec)
// to make it faster to test websocket live experiment code.
const char kWebSocketLiveExperimentHost[]   = "websocket-live-experiment-host";
#endif

// USE_SECCOMP_SANDBOX controls whether the seccomp sandbox is opt-in or -out.
// TODO(evan): unify all of these once we turn the seccomp sandbox always
// on.  Also remove the #include of command_line.h above.
#if defined(USE_SECCOMP_SANDBOX)
// Disable the seccomp sandbox (Linux only)
const char kDisableSeccompSandbox[]         = "disable-seccomp-sandbox";
#else
// Enable the seccomp sandbox (Linux only)
const char kEnableSeccompSandbox[]          = "enable-seccomp-sandbox";
#endif

bool SeccompSandboxEnabled() {
#if defined(USE_SECCOMP_SANDBOX)
  return !CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableSeccompSandbox);
#else
  return CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableSeccompSandbox);
#endif
}

// -----------------------------------------------------------------------------
// DO NOT ADD YOUR CRAP TO THE BOTTOM OF THIS FILE.
//
// You were going to just dump your switches here, weren't you? Instead,
// please put them in alphabetical order above, or in order inside the
// appropriate ifdef at the bottom. The order should match the header.
// -----------------------------------------------------------------------------

}  // namespace switches
