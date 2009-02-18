// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_message_filter.h"

#include "base/clipboard.h"
#include "base/gfx/native_widget_types.h"
#include "base/histogram.h"
#include "base/process_util.h"
#include "base/thread.h"
#include "chrome/browser/chrome_plugin_browsing_context.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "net/base/cookie_monster.h"
#include "net/base/mime_util.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin.h"

#if defined(OS_WIN)
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/common/clipboard_service.h"
#elif defined(OS_MACOSX) || defined(OS_LINUX)
// TODO(port) remove this.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

namespace {

// Context menus are somewhat complicated. We need to intercept them here on
// the I/O thread to add any spelling suggestions to them. After that's done,
// we need to forward the modified message to the UI thread and the normal
// message forwarding isn't set up for sending modified messages.
//
// Therefore, this class dispatches the IPC message to the RenderProcessHost
// with the given ID (if possible) to emulate the normal dispatch.
class ContextMenuMessageDispatcher : public Task {
 public:
  ContextMenuMessageDispatcher(
      int render_process_host_id,
      const ViewHostMsg_ContextMenu& context_menu_message)
      : render_process_host_id_(render_process_host_id),
        context_menu_message_(context_menu_message) {
  }

  void Run() {
    RenderProcessHost* host =
        RenderProcessHost::FromID(render_process_host_id_);
    if (host)
      host->OnMessageReceived(context_menu_message_);
  }

 private:
  int render_process_host_id_;
  const ViewHostMsg_ContextMenu context_menu_message_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuMessageDispatcher);
};

// Completes a clipboard write initiated by the renderer. The write must be
// performed on the UI thread because the clipboard service from the IO thread
// cannot create windows so it cannot be the "owner" of the clipboard's
// contents.
class WriteClipboardTask : public Task {
 public:
  explicit WriteClipboardTask(Clipboard::ObjectMap* objects)
      : objects_(objects) {}
  ~WriteClipboardTask() {}

  void Run() {
    g_browser_process->clipboard_service()->WriteObjects(*objects_.get());
  }

 private:
  scoped_ptr<Clipboard::ObjectMap> objects_;
};


}  // namespace

ResourceMessageFilter::ResourceMessageFilter(
    ResourceDispatcherHost* resource_dispatcher_host,
    AudioRendererHost* audio_renderer_host,
    PluginService* plugin_service,
    printing::PrintJobManager* print_job_manager,
    int render_process_host_id,
    Profile* profile,
    RenderWidgetHelper* render_widget_helper,
    SpellChecker* spellchecker)
    : channel_(NULL),
      resource_dispatcher_host_(resource_dispatcher_host),
      plugin_service_(plugin_service),
      print_job_manager_(print_job_manager),
      render_process_host_id_(render_process_host_id),
      spellchecker_(spellchecker),
      ALLOW_THIS_IN_INITIALIZER_LIST(resolve_proxy_msg_helper_(this, NULL)),
      render_handle_(NULL),
      request_context_(profile->GetRequestContext()),
      profile_(profile),
      render_widget_helper_(render_widget_helper),
      audio_renderer_host_(audio_renderer_host) {

  DCHECK(request_context_.get());
  DCHECK(request_context_->cookie_store());
}

ResourceMessageFilter::~ResourceMessageFilter() {
  if (render_handle_)
    base::CloseProcessHandle(render_handle_);

  // This function should be called on the IO thread.
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));
  NotificationService::current()->RemoveObserver(
      this,
      NotificationType::SPELLCHECKER_REINITIALIZED,
      Source<Profile>(static_cast<Profile*>(profile_)));
}

// Called on the IPC thread:
void ResourceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;

  // Add the observers to intercept 
  NotificationService::current()->AddObserver(
      this,
      NotificationType::SPELLCHECKER_REINITIALIZED,
      Source<Profile>(static_cast<Profile*>(profile_)));
}

// Called on the IPC thread:
void ResourceMessageFilter::OnChannelConnected(int32 peer_pid) {
  DCHECK(!render_handle_);
  render_handle_ = base::OpenProcessHandle(peer_pid);
  DCHECK(render_handle_);
}

// Called on the IPC thread:
void ResourceMessageFilter::OnChannelClosing() {
  channel_ = NULL;

  // Unhook us from all pending network requests so they don't get sent to a
  // deleted object.
  resource_dispatcher_host_->CancelRequestsForProcess(render_process_host_id_);
}

// Called on the IPC thread:
bool ResourceMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ResourceMessageFilter, message, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWindow, OnMsgCreateWindow)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnMsgCreateWidget)
    // TODO(brettw): we should get the view ID for this so the resource
    // dispatcher can prioritize things based on the visible view.
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestResource, OnRequestResource)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CancelRequest, OnCancelRequest)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClosePage_ACK, OnClosePageACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DataReceived_ACK, OnDataReceivedACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UploadProgress_ACK, OnUploadProgressACK)

    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SyncLoad, OnSyncLoad)

    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCookie, OnSetCookie)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetCookies, OnGetCookies)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetDataDir, OnGetDataDir)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PluginMessage, OnPluginMessage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PluginSyncMessage, OnPluginSyncMessage)
#if defined(OS_WIN)  // This hack is Windows-specific.
    IPC_MESSAGE_HANDLER(ViewHostMsg_LoadFont, OnLoadFont)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetScreenInfo, OnGetScreenInfo)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetPlugins, OnGetPlugins)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetPluginPath, OnGetPluginPath)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DownloadUrl, OnDownloadUrl)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_ContextMenu,
        OnReceiveContextMenuMsg(message))
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPlugin,
                                    OnOpenChannelToPlugin)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SpellCheck, OnSpellCheck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DnsPrefetch, OnDnsPrefetch)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_PaintRect,
        render_widget_helper_->DidReceivePaintMsg(message))
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteObjectsAsync,
                        OnClipboardWriteObjects)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteObjectsSync,
                        OnClipboardWriteObjects)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardIsFormatAvailable,
                        OnClipboardIsFormatAvailable)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardReadText, OnClipboardReadText)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardReadAsciiText,
                        OnClipboardReadAsciiText)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardReadHTML,
                        OnClipboardReadHTML)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetWindowRect, OnGetWindowRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetRootWindowRect, OnGetRootWindowRect)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetMimeTypeFromExtension,
                        OnGetMimeTypeFromExtension)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetMimeTypeFromFile,
                        OnGetMimeTypeFromFile)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetPreferredExtensionForMimeType,
                        OnGetPreferredExtensionForMimeType)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetCPBrowsingContext,
                        OnGetCPBrowsingContext)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DuplicateSection, OnDuplicateSection)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ResourceTypeStats, OnResourceTypeStats)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ResolveProxy, OnResolveProxy)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetDefaultPrintSettings,
                                    OnGetDefaultPrintSettings)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ScriptedPrint,
                                    OnScriptedPrint)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateAudioStream, OnCreateAudioStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartAudioStream, OnStartAudioStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CloseAudioStream, OnCloseAudioStream)
    IPC_MESSAGE_HANDLER(ViewHostMsg_NotifyAudioPacketReady,
                        OnNotifyAudioPacketReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetAudioVolume, OnGetAudioVolume)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetAudioVolume, OnSetAudioVolume)
    IPC_MESSAGE_UNHANDLED(
        handled = false)
  IPC_END_MESSAGE_MAP_EX()

  if (!msg_is_ok) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(message.type(),
                                                         render_handle_);
  }

  return handled;
}

void ResourceMessageFilter::OnReceiveContextMenuMsg(const IPC::Message& msg) {
  void* iter = NULL;
  ContextMenuParams params;
  if (!IPC::ParamTraits<ContextMenuParams>::Read(&msg, &iter, &params))
    return;

  // Fill in the dictionary suggestions if required.
  if (!params.misspelled_word.empty() &&
      spellchecker_ != NULL && params.spellcheck_enabled) {
    int misspell_location, misspell_length;
    bool is_misspelled = !spellchecker_->SpellCheckWord(
        params.misspelled_word.c_str(),
        static_cast<int>(params.misspelled_word.length()),
        &misspell_location, &misspell_length,
        &params.dictionary_suggestions);

    // If not misspelled, make the misspelled_word param empty.
    if (!is_misspelled)
      params.misspelled_word.clear();
  }

  // Create a new ViewHostMsg_ContextMenu message.
  const ViewHostMsg_ContextMenu context_menu_message(msg.routing_id(), params);
  render_widget_helper_->ui_loop()->PostTask(FROM_HERE,
      new ContextMenuMessageDispatcher(render_process_host_id_,
                                       context_menu_message));
}

// Called on the IPC thread:
bool ResourceMessageFilter::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

void ResourceMessageFilter::OnMsgCreateWindow(
    int opener_id, bool user_gesture, int* route_id,
    ModalDialogEvent* modal_dialog_event) {
  render_widget_helper_->CreateNewWindow(opener_id,
                                         user_gesture,
                                         render_handle_,
                                         route_id,
                                         modal_dialog_event);
}

void ResourceMessageFilter::OnMsgCreateWidget(int opener_id,
                                              bool activatable,
                                              int* route_id) {
  render_widget_helper_->CreateNewWidget(opener_id, activatable, route_id);
}

void ResourceMessageFilter::OnRequestResource(
    const IPC::Message& message,
    int request_id,
    const ViewHostMsg_Resource_Request& request) {
  resource_dispatcher_host_->BeginRequest(this,
                                          render_handle_,
                                          render_process_host_id_,
                                          message.routing_id(),
                                          request_id,
                                          request,
                                          request_context_,
                                          NULL);
}

void ResourceMessageFilter::OnDataReceivedACK(int request_id) {
  resource_dispatcher_host_->OnDataReceivedACK(render_process_host_id_,
                                               request_id);
}

void ResourceMessageFilter::OnUploadProgressACK(int request_id) {
  resource_dispatcher_host_->OnUploadProgressACK(render_process_host_id_,
                                                 request_id);
}

void ResourceMessageFilter::OnCancelRequest(int request_id) {
  resource_dispatcher_host_->CancelRequest(render_process_host_id_, request_id,
                                           true);
}

void ResourceMessageFilter::OnClosePageACK(int new_render_process_host_id,
                                           int new_request_id) {
  resource_dispatcher_host_->OnClosePageACK(new_render_process_host_id,
                                            new_request_id);
}

void ResourceMessageFilter::OnSyncLoad(
    int request_id,
    const ViewHostMsg_Resource_Request& request,
    IPC::Message* sync_result) {
  resource_dispatcher_host_->BeginRequest(this,
                                          render_handle_,
                                          render_process_host_id_,
                                          sync_result->routing_id(),
                                          request_id,
                                          request,
                                          request_context_,
                                          sync_result);
}

void ResourceMessageFilter::OnSetCookie(const GURL& url,
                                        const GURL& policy_url,
                                        const std::string& cookie) {
  if (request_context_->cookie_policy()->CanSetCookie(url, policy_url))
    request_context_->cookie_store()->SetCookie(url, cookie);
}

void ResourceMessageFilter::OnGetCookies(const GURL& url,
                                         const GURL& policy_url,
                                         std::string* cookies) {
  if (request_context_->cookie_policy()->CanGetCookies(url, policy_url))
    *cookies = request_context_->cookie_store()->GetCookies(url);
}

void ResourceMessageFilter::OnGetDataDir(std::wstring* data_dir) {
  *data_dir = plugin_service_->GetChromePluginDataDir().ToWStringHack();
}

void ResourceMessageFilter::OnPluginMessage(const FilePath& plugin_path,
                                            const std::vector<uint8>& data) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  ChromePluginLib *chrome_plugin = ChromePluginLib::Find(plugin_path);
  if (chrome_plugin) {
    void *data_ptr = const_cast<void*>(reinterpret_cast<const void*>(&data[0]));
    uint32 data_len = static_cast<uint32>(data.size());
    chrome_plugin->functions().on_message(data_ptr, data_len);
  }
}

void ResourceMessageFilter::OnPluginSyncMessage(const FilePath& plugin_path,
                                                const std::vector<uint8>& data,
                                                std::vector<uint8> *retval) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  ChromePluginLib *chrome_plugin = ChromePluginLib::Find(plugin_path);
  if (chrome_plugin) {
    void *data_ptr = const_cast<void*>(reinterpret_cast<const void*>(&data[0]));
    uint32 data_len = static_cast<uint32>(data.size());
    void *retval_buffer = 0;
    uint32 retval_size = 0;
    chrome_plugin->functions().on_sync_message(data_ptr, data_len,
                                               &retval_buffer, &retval_size);
    if (retval_buffer) {
      retval->resize(retval_size);
      memcpy(&(retval->at(0)), retval_buffer, retval_size);
      CPB_Free(retval_buffer);
    }
  }
}

#if defined(OS_WIN)  // This hack is Windows-specific. 
void ResourceMessageFilter::OnLoadFont(LOGFONT font) {
  // If renderer is running in a sandbox, GetTextMetrics
  // can sometimes fail. If a font has not been loaded
  // previously, GetTextMetrics will try to load the font
  // from the font file. However, the sandboxed renderer does
  // not have permissions to access any font files and
  // the call fails. So we make the browser pre-load the
  // font for us by using a dummy call to GetTextMetrics of
  // the same font.

  // Maintain a circular queue for the fonts and DCs to be cached.
  // font_index maintains next available location in the queue.
  static const int kFontCacheSize = 32;
  static HFONT fonts[kFontCacheSize] = {0};
  static HDC hdcs[kFontCacheSize] = {0};
  static size_t font_index = 0;

  UMA_HISTOGRAM_COUNTS_100(L"Memory.CachedFontAndDC",
      fonts[kFontCacheSize-1] ? kFontCacheSize : static_cast<int>(font_index));

  HDC hdc = GetDC(NULL);
  HFONT font_handle = CreateFontIndirect(&font);
  DCHECK(NULL != font_handle);

  HGDIOBJ old_font = SelectObject(hdc, font_handle);
  DCHECK(NULL != old_font);

  TEXTMETRIC tm;
  BOOL ret = GetTextMetrics(hdc, &tm);
  DCHECK(ret);

  if (fonts[font_index] || hdcs[font_index]) {
    // We already have too many fonts, we will delete one and take it's place.
    DeleteObject(fonts[font_index]);
    ReleaseDC(NULL, hdcs[font_index]);
  }

  fonts[font_index] = font_handle;
  hdcs[font_index] = hdc;
  font_index = (font_index + 1) % kFontCacheSize;
}
#endif

void ResourceMessageFilter::OnGetScreenInfo(
    gfx::NativeViewId window, webkit_glue::ScreenInfo* results) {
  *results = webkit_glue::GetScreenInfoHelper(gfx::NativeViewFromId(window));
}

void ResourceMessageFilter::OnGetPlugins(bool refresh,
                                         std::vector<WebPluginInfo>* plugins) {
  plugin_service_->GetPlugins(refresh, plugins);
}

void ResourceMessageFilter::OnGetPluginPath(const GURL& url,
                                            const std::string& mime_type,
                                            const std::string& clsid,
                                            FilePath* filename,
                                            std::string* url_mime_type) {
  *filename = plugin_service_->GetPluginPath(url, mime_type, clsid,
                                             url_mime_type);
}

void ResourceMessageFilter::OnOpenChannelToPlugin(const GURL& url,
                                                  const std::string& mime_type,
                                                  const std::string& clsid,
                                                  const std::wstring& locale,
                                                  IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToPlugin(this, url, mime_type, clsid,
                                       locale, reply_msg);
}

void ResourceMessageFilter::OnDownloadUrl(const IPC::Message& message,
                                          const GURL& url,
                                          const GURL& referrer) {
  resource_dispatcher_host_->BeginDownload(url,
                                           referrer,
                                           render_process_host_id_,
                                           message.routing_id(),
                                           request_context_);
}

void ResourceMessageFilter::OnClipboardWriteObjects(
    const Clipboard::ObjectMap& objects) {
  // We cannot write directly from the IO thread, and cannot service the IPC
  // on the UI thread. We'll copy the relevant data and get a handle to any
  // shared memory so it doesn't go away when we resume the renderer, and post
  // a task to perform the write on the UI thread.
  Clipboard::ObjectMap* long_living_objects = new Clipboard::ObjectMap(objects); 

  // We pass the render_handle_ to assist the clipboard with using shared
  // memory objects. render_handle_ is a handle to the process that would
  // own any shared memory that might be in the object list.
#if defined(OS_WIN)
  Clipboard::DuplicateRemoteHandles(render_handle_, long_living_objects);
#else
  NOTIMPLEMENTED();  // TODO(port) implement this.
#endif

  render_widget_helper_->ui_loop()->PostTask(FROM_HERE,
      new WriteClipboardTask(long_living_objects));
}

void ResourceMessageFilter::OnClipboardIsFormatAvailable(unsigned int format,
                                                         bool* result) {
#if defined(OS_WIN)
  DCHECK(result);
  *result = GetClipboardService()->IsFormatAvailable(format);
#else
  NOTIMPLEMENTED();  // TODO(port) this function should take a
                     // Clipboard::FormatType instead of an int.
#endif
}

void ResourceMessageFilter::OnClipboardReadText(std::wstring* result) {
  GetClipboardService()->ReadText(result);
}

void ResourceMessageFilter::OnClipboardReadAsciiText(std::string* result) {
  GetClipboardService()->ReadAsciiText(result);
}

void ResourceMessageFilter::OnClipboardReadHTML(std::wstring* markup,
                                                GURL* src_url) {
  std::string src_url_str;
  GetClipboardService()->ReadHTML(markup, &src_url_str);
  *src_url = GURL(src_url_str);
}

#if defined(OS_WIN)

void ResourceMessageFilter::OnGetWindowRect(gfx::NativeViewId window_id,
                                            gfx::Rect* rect) {
  HWND window = gfx::NativeViewFromId(window_id);
  RECT window_rect = {0};
  GetWindowRect(window, &window_rect);
  *rect = window_rect;
}

void ResourceMessageFilter::OnGetRootWindowRect(gfx::NativeViewId window_id,
                                                gfx::Rect* rect) {
  HWND window = gfx::NativeViewFromId(window_id);
  RECT window_rect = {0};
  HWND root_window = ::GetAncestor(window, GA_ROOT);
  GetWindowRect(root_window, &window_rect);
  *rect = window_rect;
}

#endif  // OS_WIN

void ResourceMessageFilter::OnGetMimeTypeFromExtension(
    const FilePath::StringType& ext, std::string* mime_type) {
  net::GetMimeTypeFromExtension(ext, mime_type);
}

void ResourceMessageFilter::OnGetMimeTypeFromFile(
    const FilePath& file_path, std::string* mime_type) {
  net::GetMimeTypeFromFile(file_path, mime_type);
}

void ResourceMessageFilter::OnGetPreferredExtensionForMimeType(
    const std::string& mime_type, FilePath::StringType* ext) {
  net::GetPreferredExtensionForMimeType(mime_type, ext);
}

void ResourceMessageFilter::OnGetCPBrowsingContext(uint32* context) {
  // Always allocate a new context when a plugin requests one, since it needs to
  // be unique for that plugin instance.
  *context =
      CPBrowsingContextManager::Instance()->Allocate(request_context_.get());
}

void ResourceMessageFilter::OnDuplicateSection(
    base::SharedMemoryHandle renderer_handle,
    base::SharedMemoryHandle* browser_handle) {
  // Duplicate the handle in this process right now so the memory is kept alive
  // (even if it is not mapped)
  base::SharedMemory shared_buf(renderer_handle, true, render_handle_);
  shared_buf.GiveToProcess(base::GetCurrentProcessHandle(), browser_handle);
}

void ResourceMessageFilter::OnResourceTypeStats(
    const CacheManager::ResourceTypeStats& stats) {
  HISTOGRAM_COUNTS(L"WebCoreCache.ImagesSizeKB",
                   static_cast<int>(stats.images.size / 1024));
  HISTOGRAM_COUNTS(L"WebCoreCache.CSSStylesheetsSizeKB",
                   static_cast<int>(stats.css_stylesheets.size / 1024));
  HISTOGRAM_COUNTS(L"WebCoreCache.ScriptsSizeKB",
                   static_cast<int>(stats.scripts.size / 1024));
  HISTOGRAM_COUNTS(L"WebCoreCache.XSLStylesheetsSizeKB",
                   static_cast<int>(stats.xsl_stylesheets.size / 1024));
  HISTOGRAM_COUNTS(L"WebCoreCache.FontsSizeKB",
                   static_cast<int>(stats.fonts.size / 1024));
}

void ResourceMessageFilter::OnResolveProxy(const GURL& url,
                                           IPC::Message* reply_msg) { 
  resolve_proxy_msg_helper_.Start(url, reply_msg);
}

void ResourceMessageFilter::OnResolveProxyCompleted(
    IPC::Message* reply_msg,
    int result,
    const std::string& proxy_list) {
  ViewHostMsg_ResolveProxy::WriteReplyParams(reply_msg, result, proxy_list);
  Send(reply_msg);
}

void ResourceMessageFilter::OnGetDefaultPrintSettings(IPC::Message* reply_msg) {
  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager_->PopPrinterQuery(0, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &ResourceMessageFilter::OnGetDefaultPrintSettingsReply,
      printer_query,
      reply_msg);
  // Loads default settings. This is asynchronous, only the IPC message sender
  // will hang until the settings are retrieved.
  printer_query->GetSettings(printing::PrinterQuery::DEFAULTS,
                             NULL,
                             0,
                             task);
}

void ResourceMessageFilter::OnGetDefaultPrintSettingsReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  ViewMsg_Print_Params params;
  if (printer_query->last_status() != printing::PrintingContext::OK) {
    memset(&params, 0, sizeof(params));
  } else {
    printer_query->settings().RenderParams(&params);
    params.document_cookie = printer_query->cookie();
  }
  ViewHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  // If user hasn't cancelled.
  if (printer_query->cookie() && printer_query->settings().dpi()) {
    print_job_manager_->QueuePrinterQuery(printer_query.get());
  } else {
    printer_query->StopWorker();
  }
}

#if defined(OS_WIN)

void ResourceMessageFilter::OnScriptedPrint(gfx::NativeViewId host_window_id,
                                            int cookie,
                                            int expected_pages_count,
                                            IPC::Message* reply_msg) {
  HWND host_window = gfx::NativeViewFromId(host_window_id);

  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager_->PopPrinterQuery(cookie, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &ResourceMessageFilter::OnScriptedPrintReply,
      printer_query,
      reply_msg);
  // Shows the Print... dialog box. This is asynchronous, only the IPC message
  // sender will hang until the Print dialog is dismissed.
  if (!host_window || !IsWindow(host_window)) {
    // TODO(maruel):  bug 1214347 Get the right browser window instead.
    host_window = GetDesktopWindow();
  } else {
    host_window = GetAncestor(host_window, GA_ROOTOWNER);
  }
  DCHECK(host_window);
  printer_query->GetSettings(printing::PrinterQuery::ASK_USER,
                             host_window,
                             expected_pages_count,
                             task);
}

void ResourceMessageFilter::OnScriptedPrintReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  ViewMsg_PrintPages_Params params;
  if (printer_query->last_status() != printing::PrintingContext::OK ||
      !printer_query->settings().dpi()) {
    memset(&params, 0, sizeof(params));
  } else {
    printer_query->settings().RenderParams(&params.params);
    params.params.document_cookie = printer_query->cookie();
    params.pages =
        printing::PageRange::GetPages(printer_query->settings().ranges);
  }
  ViewHostMsg_ScriptedPrint::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  if (params.params.dpi && params.params.document_cookie) {
    print_job_manager_->QueuePrinterQuery(printer_query.get());
  } else {
    printer_query->StopWorker();
  }
}

#endif  // OS_WIN

// static
ClipboardService* ResourceMessageFilter::GetClipboardService() {
  // We have a static instance of the clipboard service for use by all message
  // filters.  This instance lives for the life of the browser processes.
  static ClipboardService* clipboard_service = new ClipboardService();

  return clipboard_service;
}

// Notes about SpellCheck.
//
// Spellchecking generally uses a fair amount of RAM.  For this reason, we load
// the spellcheck dictionaries into the browser process, and all renderers ask
// the browsers to do SpellChecking.
//
// This filter should not try to initialize the spellchecker. It is up to the 
// profile to initialize it when required, and send it here. If |spellchecker_|
// is made NULL, it corresponds to spellchecker turned off - i.e., all
// spellings are correct.
//
// Note: This is called in the IO thread.
void ResourceMessageFilter::OnSpellCheck(const std::wstring& word,
                                         IPC::Message* reply_msg) {
  int misspell_location = 0;
  int misspell_length = 0;

  if (spellchecker_ != NULL) {
    spellchecker_->SpellCheckWord(word.c_str(),
                                  static_cast<int>(word.length()),
                                  &misspell_location, &misspell_length, NULL);
  }

  ViewHostMsg_SpellCheck::WriteReplyParams(reply_msg, misspell_location,
                                             misspell_length);
  Send(reply_msg);
  return;
}

void ResourceMessageFilter::Observe(NotificationType type, 
                                    const NotificationSource &source,
                                    const NotificationDetails &details) {
  if (type == NotificationType::SPELLCHECKER_REINITIALIZED) {
    spellchecker_ = Details<SpellcheckerReinitializedDetails>
        (details).ptr()->spellchecker;
  }
}

void ResourceMessageFilter::OnDnsPrefetch(
    const std::vector<std::string>& hostnames) {
  chrome_browser_net::DnsPrefetchList(hostnames);
}

void ResourceMessageFilter::OnCreateAudioStream(
   const IPC::Message& msg, int stream_id,
   const ViewHostMsg_Audio_CreateStream& params) {
  // TODO(hclam): call to AudioRendererHost::CreateStream and send a message to
  // renderer to notify the result.
}

void ResourceMessageFilter::OnNotifyAudioPacketReady(
    const IPC::Message& msg, int stream_id) {
  // TODO(hclam): delegate to AudioRendererHost and handle this message.
}

void ResourceMessageFilter::OnStartAudioStream(
    const IPC::Message& msg, int stream_id) {
  // TODO(hclam): delegate to AudioRendererHost and handle this message.
}


void ResourceMessageFilter::OnCloseAudioStream(
    const IPC::Message& msg, int stream_id) {
  // TODO(hclam): delegate to AudioRendererHost and handle this message.
}

void ResourceMessageFilter::OnGetAudioVolume(
    const IPC::Message& msg, int stream_id) {
  // TODO(hclam): delegate to AudioRendererHost and handle this message. Send
  // a message about the volume.
}

void ResourceMessageFilter::OnSetAudioVolume(
    const IPC::Message& msg, int stream_id,
    double left_channel, double right_channel) {
  // TODO(hclam): delegate to AudioRendererHost and handle this message.
}
