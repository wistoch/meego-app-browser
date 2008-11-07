// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell.h"

#include "base/command_line.h"
#include "base/debug_on_start.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/gfx/bitmap_platform_device.h"
#include "base/gfx/size.h"
#include "base/icu_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stats_table.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "googleurl/src/url_util.h"
#include "net/base/mime_util.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkit_resources.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/weburlrequest.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/webwidget.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"

#include "webkit_strings.h"

#include "SkBitmap.h"

// Default timeout for page load when running non-interactive file
// tests, in ms.
const int kDefaultFileTestTimeoutMillisecs = 10 * 1000;

// Content area size for newly created windows.
const int kTestWindowWidth = 800;
const int kTestWindowHeight = 600;

// Initialize static member variable
WindowList* TestShell::window_list_;
WebPreferences* TestShell::web_prefs_ = NULL;
bool TestShell::interactive_ = true;
int TestShell::file_test_timeout_ms_ = kDefaultFileTestTimeoutMillisecs;

// URLRequestTestShellFileJob is used to serve the inspector
class URLRequestTestShellFileJob : public URLRequestFileJob {
 public:
  virtual ~URLRequestTestShellFileJob() { }

  static URLRequestJob* InspectorFactory(URLRequest* request, 
                                         const std::string& scheme) {
    std::wstring path;
    PathService::Get(base::DIR_EXE, &path);
    file_util::AppendToPath(&path, L"Resources");
    file_util::AppendToPath(&path, L"Inspector");
    file_util::AppendToPath(&path, UTF8ToWide(request->url().path()));
    return new URLRequestTestShellFileJob(request, path);
  }

 private:
  URLRequestTestShellFileJob(URLRequest* request, const std::wstring& path)
      : URLRequestFileJob(request) { 
    this->file_path_ = path;  // set URLRequestFileJob::file_path_
  }

  DISALLOW_COPY_AND_ASSIGN(URLRequestTestShellFileJob);
};

TestShell::TestShell() 
    : m_mainWnd(NULL),
      m_editWnd(NULL),
      m_webViewHost(NULL),
      m_popupHost(NULL),
      m_focusedWidgetHost(NULL),
#if defined(OS_WIN)
      default_edit_wnd_proc_(0),
#endif
      test_is_preparing_(false),
      test_is_pending_(false),
      is_modal_(false),
      dump_stats_table_on_exit_(false) {
    delegate_ = new TestWebViewDelegate(this);
    layout_test_controller_.reset(new LayoutTestController(this));
    event_sending_controller_.reset(new EventSendingController(this));
    text_input_controller_.reset(new TextInputController(this));
    navigation_controller_.reset(new TestNavigationController(this));

    URLRequestFilter* filter = URLRequestFilter::GetInstance();
    filter->AddHostnameHandler("test-shell-resource", "inspector", 
                               &URLRequestTestShellFileJob::InspectorFactory);
    url_util::AddStandardScheme("test-shell-resource");
}

TestShell::~TestShell() {
    // Call GC twice to clean up garbage.
    CallJSGC();
    CallJSGC();

    PlatformCleanUp();

    StatsTable *table = StatsTable::current();
    if (dump_stats_table_on_exit_) {
      // Dump the stats table.
      printf("<stats>\n");
      if (table != NULL) {
          int counter_max = table->GetMaxCounters();
          for (int index=0; index < counter_max; index++) {
              std::wstring name(table->GetRowName(index));
              if (name.length() > 0) {
                  int value = table->GetRowValue(index);
                  printf("%s:\t%d\n", WideToUTF8(name).c_str(), value);
              }
          }
      }
      printf("</stats>\n");
    }
}

void TestShell::ShutdownTestShell() {
#if defined(OS_WIN)
    OleUninitialize();
#endif
    SimpleResourceLoaderBridge::Shutdown();
    delete window_list_;
    delete TestShell::web_prefs_;
}

// All fatal log messages (e.g. DCHECK failures) imply unit test failures
static void UnitTestAssertHandler(const std::string& str) {
    FAIL() << str;
}

// static
void TestShell::InitLogging(bool suppress_error_dialogs,
                            bool running_layout_tests) {
    if (suppress_error_dialogs)
        logging::SetLogAssertHandler(UnitTestAssertHandler);

#if defined(OS_WIN)
    if (!IsDebuggerPresent()) {
        UINT new_flags = SEM_FAILCRITICALERRORS |
                         SEM_NOGPFAULTERRORBOX |
                         SEM_NOOPENFILEERRORBOX;
        // Preserve existing error mode, as discussed at
        // http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx
        UINT existing_flags = SetErrorMode(new_flags);
        SetErrorMode(existing_flags | new_flags);
    }
#endif

    // Only log to a file if we're running layout tests. This prevents debugging
    // output from disrupting whether or not we pass.
    logging::LoggingDestination destination = 
        logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG;
    if (running_layout_tests)
      destination = logging::LOG_ONLY_TO_FILE;

    // We might have multiple test_shell processes going at once
    FilePath log_filename;
    PathService::Get(base::DIR_EXE, &log_filename);
    log_filename.Append(FILE_PATH_LITERAL("test_shell.log"));
    logging::InitLogging(log_filename.value().c_str(),
                         destination,
                         logging::LOCK_LOG_FILE,
                         logging::DELETE_OLD_LOG_FILE);

    // we want process and thread IDs because we may have multiple processes
    logging::SetLogItems(true, true, false, true);
}

// static
void TestShell::CleanupLogging() {
    logging::CloseLogFile();
}

// static
void TestShell::SetAllowScriptsToCloseWindows() {
  if (web_prefs_)
    web_prefs_->allow_scripts_to_close_windows = true;
}

// static
void TestShell::ResetWebPreferences() {
    DCHECK(web_prefs_);

    // Match the settings used by Mac DumpRenderTree.
    if (web_prefs_) {
        *web_prefs_ = WebPreferences();
        web_prefs_->standard_font_family = L"Times";
        web_prefs_->fixed_font_family = L"Courier";
        web_prefs_->serif_font_family = L"Times";
        web_prefs_->sans_serif_font_family = L"Helvetica";
        // These two fonts are picked from the intersection of
        // Win XP font list and Vista font list :
        //   http://www.microsoft.com/typography/fonts/winxp.htm 
        //   http://blogs.msdn.com/michkap/archive/2006/04/04/567881.aspx
        // Some of them are installed only with CJK and complex script
        // support enabled on Windows XP and are out of consideration here. 
        // (although we enabled both on our buildbots.)
        // They (especially Impact for fantasy) are not typical cursive
        // and fantasy fonts, but it should not matter for layout tests
        // as long as they're available.
        web_prefs_->cursive_font_family = L"Comic Sans MS";
        web_prefs_->fantasy_font_family = L"Impact";
        web_prefs_->default_encoding = L"ISO-8859-1";
        web_prefs_->default_font_size = 16;
        web_prefs_->default_fixed_font_size = 13;
        web_prefs_->minimum_font_size = 1;
        web_prefs_->minimum_logical_font_size = 9;
        web_prefs_->javascript_can_open_windows_automatically = true;
        web_prefs_->dom_paste_enabled = true;
        web_prefs_->developer_extras_enabled = interactive_;
        web_prefs_->shrinks_standalone_images_to_fit = false;
        web_prefs_->uses_universal_detector = false;
        web_prefs_->text_areas_are_resizable = false;
        web_prefs_->java_enabled = true;
        web_prefs_->allow_scripts_to_close_windows = false;
    }
}

// static
bool TestShell::RemoveWindowFromList(gfx::WindowHandle window) {
  WindowList::iterator entry =
      std::find(TestShell::windowList()->begin(),
                TestShell::windowList()->end(),
                window);
  if (entry != TestShell::windowList()->end()) {
    TestShell::windowList()->erase(entry);
    return true;
  }

  return false;
}

void TestShell::Show(WebView* webview, WindowOpenDisposition disposition) {
  delegate_->Show(webview, disposition);
}

void TestShell::BindJSObjectsToWindow(WebFrame* frame) {
    // Only bind the test classes if we're running tests.
    if (!interactive_) {
        layout_test_controller_->BindToJavascript(frame, 
                                                  L"layoutTestController");
        event_sending_controller_->BindToJavascript(frame,
                                                    L"eventSender");
        text_input_controller_->BindToJavascript(frame,
                                                 L"textInputController");
    }
}


void TestShell::CallJSGC() {
    WebFrame* frame = webView()->GetMainFrame();
    frame->CallJSGC();
}


WebView* TestShell::CreateWebView(WebView* webview) {
    // If we're running layout tests, only open a new window if the test has
    // called layoutTestController.setCanOpenWindows()
    if (!interactive_ && !layout_test_controller_->CanOpenWindows())
        return NULL;

    TestShell* new_win;
    if (!CreateNewWindow(std::wstring(), &new_win))
        return NULL;

    return new_win->webView();
}

void TestShell::SizeToDefault() {
   SizeTo(kTestWindowWidth, kTestWindowHeight);
}

void TestShell::LoadURL(const wchar_t* url) {
    LoadURLForFrame(url, NULL);
}

bool TestShell::Navigate(const TestNavigationEntry& entry, bool reload) {
    WebRequestCachePolicy cache_policy;
    if (reload) {
      cache_policy = WebRequestReloadIgnoringCacheData;
    } else if (entry.GetPageID() != -1) {
      cache_policy = WebRequestReturnCacheDataElseLoad;
    } else {
      cache_policy = WebRequestUseProtocolCachePolicy;
    }

    scoped_ptr<WebRequest> request(WebRequest::Create(entry.GetURL()));
    request->SetCachePolicy(cache_policy);
    // If we are reloading, then WebKit will use the state of the current page.
    // Otherwise, we give it the state to navigate to.
    if (!reload)
      request->SetHistoryState(entry.GetContentState());
      
    request->SetExtraData(
        new TestShellExtraRequestData(entry.GetPageID()));

    // Get the right target frame for the entry.
    WebFrame* frame = webView()->GetMainFrame();
    if (!entry.GetTargetFrame().empty())
        frame = webView()->GetFrameWithName(entry.GetTargetFrame());
    // TODO(mpcomplete): should we clear the target frame, or should
    // back/forward navigations maintain the target frame?

    frame->LoadRequest(request.get());
    // Restore focus to the main frame prior to loading new request.
    // This makes sure that we don't have a focused iframe. Otherwise, that
    // iframe would keep focus when the SetFocus called immediately after
    // LoadRequest, thus making some tests fail (see http://b/issue?id=845337
    // for more details).
    webView()->SetFocusedFrame(frame);
    SetFocus(webViewHost(), true);

    return true;
}

void TestShell::GoBackOrForward(int offset) {
    navigation_controller_->GoToOffset(offset);
}

std::wstring TestShell::GetDocumentText() {
  return webkit_glue::DumpDocumentText(webView()->GetMainFrame());
}

void TestShell::Reload() {
    navigation_controller_->Reload();
}

void TestShell::SetFocus(WebWidgetHost* host, bool enable) {
  if (interactive_) {
    InteractiveSetFocus(host, enable);
  } else {
    if (enable) {
      if (m_focusedWidgetHost != host) {
        if (m_focusedWidgetHost)
            m_focusedWidgetHost->webwidget()->SetFocus(false);
        host->webwidget()->SetFocus(enable);
        m_focusedWidgetHost = host;
      }
    } else {
      if (m_focusedWidgetHost == host) {
        host->webwidget()->SetFocus(enable);
        m_focusedWidgetHost = NULL;
      }
    }
  }
}

//-----------------------------------------------------------------------------

namespace webkit_glue {

void PrefetchDns(const std::string& hostname) {}

void PrecacheUrl(const char16* url, int url_length) {}

void AppendToLog(const char* file, int line, const char* msg) {
  logging::LogMessage(file, line).stream() << msg;
}

bool GetMimeTypeFromExtension(const std::wstring &ext, std::string *mime_type) {
  return net::GetMimeTypeFromExtension(ext, mime_type);
}

bool GetMimeTypeFromFile(const std::wstring &file_path,
                         std::string *mime_type) {
  return net::GetMimeTypeFromFile(file_path, mime_type);
}

bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      std::wstring* ext) {
  return net::GetPreferredExtensionForMimeType(mime_type, ext);
}

std::string GetDataResource(int resource_id) {
  if (resource_id == IDR_BROKENIMAGE) {
    // Use webkit's broken image icon (16x16)
    static std::string broken_image_data;
    if (broken_image_data.empty()) {
      std::wstring path;
      PathService::Get(base::DIR_SOURCE_ROOT, &path);
      file_util::AppendToPath(&path, L"webkit");
      file_util::AppendToPath(&path, L"tools");
      file_util::AppendToPath(&path, L"test_shell");
      file_util::AppendToPath(&path, L"resources");
      file_util::AppendToPath(&path, L"missingImage.gif");
      bool success = file_util::ReadFileToString(path, &broken_image_data);
      if (!success) {
        LOG(FATAL) << "Failed reading: " << path;
      }
    }
    return broken_image_data;
  } else if (resource_id == IDR_FEED_PREVIEW) {
    // It is necessary to return a feed preview template that contains
    // a {{URL}} substring where the feed URL should go; see the code 
    // that computes feed previews in feed_preview.cc:MakeFeedPreview. 
    // This fixes issue #932714.    
    return std::string("Feed preview for {{URL}}");
  } else {
    return std::string();
  }
}

SkBitmap* GetBitmapResource(int resource_id) {
  return NULL;
}

bool GetApplicationDirectory(std::wstring *path) {
  return PathService::Get(base::DIR_EXE, path);
}

GURL GetInspectorURL() {
  return GURL("test-shell-resource://inspector/inspector.html");
}

std::string GetUIResourceProtocol() {
  return "test-shell-resource";
}

bool GetExeDirectory(std::wstring *path) {
  return PathService::Get(base::DIR_EXE, path);
}

bool SpellCheckWord(const wchar_t* word, int word_len,
                    int* misspelling_start, int* misspelling_len) {
  // Report all words being correctly spelled.
  *misspelling_start = 0;
  *misspelling_len = 0;
  return true;
}

bool IsPluginRunningInRendererProcess() {
  return true;
}

bool GetPluginFinderURL(std::string* plugin_finder_url) {
  return false;
}

bool IsDefaultPluginEnabled() {
  return false;
}

std::wstring GetWebKitLocale() {
  return L"en-US";
}

}  // namespace webkit_glue
