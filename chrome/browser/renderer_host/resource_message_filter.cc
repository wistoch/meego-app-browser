// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_message_filter.h"

#include "app/clipboard/clipboard.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/process_util.h"
#include "base/thread.h"
#include "chrome/browser/appcache/appcache_dispatcher_host.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/chrome_plugin_browsing_context.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/download/download_file.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/geolocation/geolocation_dispatcher_host.h"
#include "chrome/browser/gpu_process_host.h"
#include "chrome/browser/host_zoom_map.h"
#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"
#include "chrome/browser/metrics/histogram_synchronizer.h"
#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_ui.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/database_dispatcher_host.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/browser/task_manager.h"
#include "chrome/browser/worker_host/message_port_dispatcher.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/worker_messages.h"
#include "gfx/native_widget_types.h"
#include "net/base/cookie_monster.h"
#include "net/base/keygen_handler.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNotificationPresenter.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugin.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebCache;

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
      int render_process_id,
      const ViewHostMsg_ContextMenu& context_menu_message)
      : render_process_id_(render_process_id),
        context_menu_message_(context_menu_message) {
  }

  void Run() {
    RenderProcessHost* host =
        RenderProcessHost::FromID(render_process_id_);
    if (host)
      host->OnMessageReceived(context_menu_message_);
  }

 private:
  int render_process_id_;
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
    g_browser_process->clipboard()->WriteObjects(*objects_.get());
  }

 private:
  scoped_ptr<Clipboard::ObjectMap> objects_;
};

void RenderParamsFromPrintSettings(const printing::PrintSettings& settings,
                                   ViewMsg_Print_Params* params) {
  DCHECK(params);
#if defined(OS_WIN) || defined(OS_MACOSX)
  params->printable_size.SetSize(
      settings.page_setup_pixels().content_area().width(),
      settings.page_setup_pixels().content_area().height());
  params->dpi = settings.dpi();
  // Currently hardcoded at 1.25. See PrintSettings' constructor.
  params->min_shrink = settings.min_shrink;
  // Currently hardcoded at 2.0. See PrintSettings' constructor.
  params->max_shrink = settings.max_shrink;
  // Currently hardcoded at 72dpi. See PrintSettings' constructor.
  params->desired_dpi = settings.desired_dpi;
  // Always use an invalid cookie.
  params->document_cookie = 0;
  params->selection_only = settings.selection_only;
#else
  NOTIMPLEMENTED();
#endif
}

Blacklist::Match* GetPrivacyBlacklistMatchForURL(
    const GURL& url, ChromeURLRequestContext* context) {
  const Blacklist* blacklist = context->GetPrivacyBlacklist();
  // TODO(phajdan.jr): DCHECK(blacklist_manager) when blacklists are stable.
  if (!blacklist)
    return NULL;
  return blacklist->FindMatch(url);
}

class SetCookieCompletion : public net::CompletionCallback {
 public:
  SetCookieCompletion(int render_process_id,
                      int render_view_id,
                      const GURL& url,
                      const std::string& cookie_line,
                      ChromeURLRequestContext* context)
      : render_process_id_(render_process_id),
        render_view_id_(render_view_id),
        url_(url),
        cookie_line_(cookie_line),
        context_(context) {
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    int result = params.a;
    if (result == net::OK ||
        result == net::OK_FOR_SESSION_ONLY) {
      net::CookieOptions options;
      if (result == net::OK_FOR_SESSION_ONLY)
        options.set_force_session();
      context_->cookie_store()->SetCookieWithOptions(url_, cookie_line_,
                                                     options);
    } else {
      if (!context_->IsExternal()) {
        CallRenderViewHostResourceDelegate(
            render_process_id_, render_view_id_,
            &RenderViewHostDelegate::Resource::OnContentBlocked,
            CONTENT_SETTINGS_TYPE_COOKIES);
      }
    }
    delete this;
  }

 private:
  int render_process_id_;
  int render_view_id_;
  GURL url_;
  std::string cookie_line_;
  scoped_refptr<ChromeURLRequestContext> context_;
};

class GetCookiesCompletion : public net::CompletionCallback {
 public:
  GetCookiesCompletion(const GURL& url, IPC::Message* reply_msg,
                       ResourceMessageFilter* filter,
                       URLRequestContext* context)
      : url_(url),
        reply_msg_(reply_msg),
        filter_(filter),
        context_(context) {
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    int result = params.a;
    std::string cookies;
    if (result == net::OK)
      cookies = context_->cookie_store()->GetCookies(url_);
    ViewHostMsg_GetCookies::WriteReplyParams(reply_msg_, cookies);
    filter_->Send(reply_msg_);
    delete this;
  }

 private:
  GURL url_;
  IPC::Message* reply_msg_;
  scoped_refptr<ResourceMessageFilter> filter_;
  scoped_refptr<URLRequestContext> context_;
};

class GetRawCookiesCompletion : public net::CompletionCallback {
 public:
  GetRawCookiesCompletion(const GURL& url, IPC::Message* reply_msg,
                          ResourceMessageFilter* filter,
                          URLRequestContext* context)
      : url_(url),
        reply_msg_(reply_msg),
        filter_(filter),
        context_(context) {
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    // Ignore the policy result.  We only waited on the policy result so that
    // any pending 'set-cookie' requests could be flushed.  The intent of
    // querying the raw cookies is to reveal the contents of the cookie DB, so
    // it important that we don't read the cookie db ahead of pending writes.

    net::CookieMonster* cookie_monster =
        context_->cookie_store()->GetCookieMonster();
    net::CookieMonster::CookieList cookie_list =
        cookie_monster->GetAllCookiesForURL(url_);

    std::vector<webkit_glue::WebCookie> cookies;
    for (size_t i = 0; i < cookie_list.size(); ++i) {
      // TODO(darin): url.host() is not necessarily the domain of the cookie.
      // We need a different API on CookieMonster to provide the domain info.
      // See http://crbug.com/34315.
      cookies.push_back(
          webkit_glue::WebCookie(cookie_list[i].first, cookie_list[i].second));
    }

    ViewHostMsg_GetRawCookies::WriteReplyParams(reply_msg_, cookies);
    filter_->Send(reply_msg_);
    delete this;
  }

 private:
  GURL url_;
  IPC::Message* reply_msg_;
  scoped_refptr<ResourceMessageFilter> filter_;
  scoped_refptr<URLRequestContext> context_;
};

void WriteFileSize(IPC::Message* reply_msg,
                   const file_util::FileInfo& file_info) {
  ViewHostMsg_GetFileSize::WriteReplyParams(reply_msg, file_info.size);
}

void WriteFileModificationTime(IPC::Message* reply_msg,
                               const file_util::FileInfo& file_info) {
  ViewHostMsg_GetFileModificationTime::WriteReplyParams(
      reply_msg, file_info.last_modified);
}

}  // namespace

ResourceMessageFilter::ResourceMessageFilter(
    ResourceDispatcherHost* resource_dispatcher_host,
    int child_id,
    AudioRendererHost* audio_renderer_host,
    PluginService* plugin_service,
    printing::PrintJobManager* print_job_manager,
    Profile* profile,
    RenderWidgetHelper* render_widget_helper,
    URLRequestContextGetter* request_context)
    : Receiver(RENDER_PROCESS, child_id),
      channel_(NULL),
      resource_dispatcher_host_(resource_dispatcher_host),
      plugin_service_(plugin_service),
      print_job_manager_(print_job_manager),
      profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(resolve_proxy_msg_helper_(this, NULL)),
      request_context_(request_context),
      media_request_context_(profile->GetRequestContextForMedia()),
      extensions_request_context_(profile->GetRequestContextForExtensions()),
      extensions_message_service_(profile->GetExtensionMessageService()),
      render_widget_helper_(render_widget_helper),
      audio_renderer_host_(audio_renderer_host),
      appcache_dispatcher_host_(
          new AppCacheDispatcherHost(profile->GetRequestContext())),
      ALLOW_THIS_IN_INITIALIZER_LIST(dom_storage_dispatcher_host_(
          new DOMStorageDispatcherHost(this, profile->GetWebKitContext(),
              resource_dispatcher_host->webkit_thread()))),
      ALLOW_THIS_IN_INITIALIZER_LIST(db_dispatcher_host_(
          new DatabaseDispatcherHost(profile->GetDatabaseTracker(), this,
              profile->GetHostContentSettingsMap()))),
      notification_prefs_(
          profile->GetDesktopNotificationService()->prefs_cache()),
      host_zoom_map_(profile->GetHostZoomMap()),
      off_the_record_(profile->IsOffTheRecord()),
      next_route_id_callback_(NewCallbackWithReturnValue(
          render_widget_helper, &RenderWidgetHelper::GetNextRoutingID)),
      ALLOW_THIS_IN_INITIALIZER_LIST(geolocation_dispatcher_host_(
          new GeolocationDispatcherHost(
              this->id(), new GeolocationPermissionContext(profile)))) {
  DCHECK(request_context_);
  DCHECK(media_request_context_);
  DCHECK(audio_renderer_host_.get());
  DCHECK(appcache_dispatcher_host_.get());
  DCHECK(dom_storage_dispatcher_host_.get());

  render_widget_helper_->Init(id(), resource_dispatcher_host_);
}

ResourceMessageFilter::~ResourceMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // Tell the DOM Storage dispatcher host to stop sending messages via us.
  dom_storage_dispatcher_host_->Shutdown();

  // Shut down the database dispatcher host.
  db_dispatcher_host_->Shutdown();

  // Let interested observers know we are being deleted.
  NotificationService::current()->Notify(
      NotificationType::RESOURCE_MESSAGE_FILTER_SHUTDOWN,
      Source<ResourceMessageFilter>(this),
      NotificationService::NoDetails());

  if (handle())
    base::CloseProcessHandle(handle());
}

// Called on the IPC thread:
void ResourceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;

  // Add the observers to intercept.
  registrar_.Add(this, NotificationType::BLACKLIST_NONVISUAL_RESOURCE_BLOCKED,
                 NotificationService::AllSources());
}

// Called on the IPC thread:
void ResourceMessageFilter::OnChannelConnected(int32 peer_pid) {
  DCHECK(!handle()) << " " << handle();
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  base::ProcessHandle peer_handle;
  if (!base::OpenProcessHandle(peer_pid, &peer_handle)) {
    NOTREACHED();
  }
  set_handle(peer_handle);

  // Hook AudioRendererHost to this object after channel is connected so it can
  // this object for sending messages.
  audio_renderer_host_->IPCChannelConnected(id(), handle(), this);

  WorkerService::GetInstance()->Initialize(resource_dispatcher_host_);
  appcache_dispatcher_host_->Initialize(this, id(), handle());
  dom_storage_dispatcher_host_->Init(id(), handle());
  db_dispatcher_host_->Init(handle());
}

void ResourceMessageFilter::OnChannelError() {
  NotificationService::current()->Notify(
      NotificationType::RESOURCE_MESSAGE_FILTER_SHUTDOWN,
      Source<ResourceMessageFilter>(this),
      NotificationService::NoDetails());
}

// Called on the IPC thread:
void ResourceMessageFilter::OnChannelClosing() {
  channel_ = NULL;

  // Unhook us from all pending network requests so they don't get sent to a
  // deleted object.
  resource_dispatcher_host_->CancelRequestsForProcess(id());

  // Unhook AudioRendererHost.
  audio_renderer_host_->IPCChannelClosing();
}

// Called on the IPC thread:
bool ResourceMessageFilter::OnMessageReceived(const IPC::Message& msg) {
  MessagePortDispatcher* mp_dispatcher = MessagePortDispatcher::GetInstance();
  bool msg_is_ok = true;
  bool handled =
      resource_dispatcher_host_->OnMessageReceived(msg, this, &msg_is_ok) ||
      appcache_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok) ||
      dom_storage_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok) ||
      audio_renderer_host_->OnMessageReceived(msg, &msg_is_ok) ||
      db_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok) ||
      mp_dispatcher->OnMessageReceived(
          msg, this, next_route_id_callback(), &msg_is_ok) ||
      geolocation_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok);

  if (!handled) {
    DCHECK(msg_is_ok);  // It should have been marked handled if it wasn't OK.
    handled = true;
    IPC_BEGIN_MESSAGE_MAP_EX(ResourceMessageFilter, msg, msg_is_ok)
      // On Linux we need to dispatch these messages to the UI2 thread
      // because we cannot make X calls from the IO thread.  Mac
      // doesn't have windowed plug-ins so we handle the messages in
      // the UI thread.  On Windows, we intercept the messages and
      // handle them directly.
#if !defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetScreenInfo,
                                      OnGetScreenInfo)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetWindowRect,
                                      OnGetWindowRect)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetRootWindowRect,
                                      OnGetRootWindowRect)
#endif

      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWindow, OnMsgCreateWindow)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnMsgCreateWidget)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SetCookie, OnSetCookie)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetCookies, OnGetCookies)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetRawCookies, OnGetRawCookies)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DeleteCookie, OnDeleteCookie)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetCookiesEnabled, OnGetCookiesEnabled)
#if defined(OS_WIN)  // This hack is Windows-specific.
      IPC_MESSAGE_HANDLER(ViewHostMsg_PreCacheFont, OnPreCacheFont)
#endif
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetPlugins, OnGetPlugins)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetPluginPath, OnGetPluginPath)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DownloadUrl, OnDownloadUrl)
      IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_ContextMenu,
                                  OnReceiveContextMenuMsg(msg))
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPlugin,
                                      OnOpenChannelToPlugin)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_LaunchNaCl, OnLaunchNaCl)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWorker, OnCreateWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_LookupSharedWorker, OnLookupSharedWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentDetached, OnDocumentDetached)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CancelCreateDedicatedWorker,
                          OnCancelCreateDedicatedWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToWorker,
                          OnForwardToWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SpellChecker_PlatformCheckSpelling,
                          OnPlatformCheckSpelling)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SpellChecker_PlatformFillSuggestionList,
                          OnPlatformFillSuggestionList)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetDocumentTag,
                                      OnGetDocumentTag)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentWithTagClosed,
                          OnDocumentWithTagClosed)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ShowSpellingPanel, OnShowSpellingPanel)
      IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateSpellingPanelWithMisspelledWord,
                          OnUpdateSpellingPanelWithMisspelledWord)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DnsPrefetch, OnDnsPrefetch)
      IPC_MESSAGE_HANDLER(ViewHostMsg_RendererHistograms,
                          OnRendererHistograms)
      IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_UpdateRect,
          render_widget_helper_->DidReceiveUpdateMsg(msg))
      IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteObjectsAsync,
                          OnClipboardWriteObjectsAsync)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteObjectsSync,
                          OnClipboardWriteObjectsSync)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardIsFormatAvailable,
                                      OnClipboardIsFormatAvailable)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardReadText,
                                      OnClipboardReadText)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardReadAsciiText,
                                      OnClipboardReadAsciiText)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardReadHTML,
                                      OnClipboardReadHTML)
#if defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardFindPboardWriteStringAsync,
                          OnClipboardFindPboardWriteString)
#endif
      IPC_MESSAGE_HANDLER(ViewHostMsg_CheckNotificationPermission,
                          OnCheckNotificationPermission)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetMimeTypeFromExtension,
                          OnGetMimeTypeFromExtension)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetMimeTypeFromFile,
                          OnGetMimeTypeFromFile)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetPreferredExtensionForMimeType,
                          OnGetPreferredExtensionForMimeType)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetCPBrowsingContext,
                          OnGetCPBrowsingContext)
#if defined(OS_WIN)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DuplicateSection, OnDuplicateSection)
#endif
#if defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER(ViewHostMsg_AllocatePDFTransport,
                          OnAllocateSharedMemoryBuffer)
#endif
#if defined(OS_POSIX)
      IPC_MESSAGE_HANDLER(ViewHostMsg_AllocateSharedMemoryBuffer,
                          OnAllocateSharedMemoryBuffer)
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_AllocateTempFileForPrinting,
                                      OnAllocateTempFileForPrinting)
      IPC_MESSAGE_HANDLER(ViewHostMsg_TempFileForPrintingWritten,
                          OnTempFileForPrintingWritten)
#endif
      IPC_MESSAGE_HANDLER(ViewHostMsg_ResourceTypeStats, OnResourceTypeStats)
      IPC_MESSAGE_HANDLER(ViewHostMsg_V8HeapStats, OnV8HeapStats)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DidZoomURL, OnDidZoomURL)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ResolveProxy, OnResolveProxy)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetDefaultPrintSettings,
                                      OnGetDefaultPrintSettings)
#if defined(OS_WIN) || defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ScriptedPrint,
                                      OnScriptedPrint)
#endif
#if defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER(ViewHostMsg_AllocTransportDIB,
                          OnAllocTransportDIB)
      IPC_MESSAGE_HANDLER(ViewHostMsg_FreeTransportDIB,
                          OnFreeTransportDIB)
#endif
      IPC_MESSAGE_HANDLER(ViewHostMsg_OpenChannelToExtension,
                          OnOpenChannelToExtension)
      IPC_MESSAGE_HANDLER(ViewHostMsg_OpenChannelToTab, OnOpenChannelToTab)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CloseCurrentConnections,
                          OnCloseCurrentConnections)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SetCacheMode, OnSetCacheMode)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetFileSize, OnGetFileSize)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetFileModificationTime,
                                      OnGetFileModificationTime)
      IPC_MESSAGE_HANDLER(ViewHostMsg_Keygen, OnKeygen)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetExtensionMessageBundle,
                                      OnGetExtensionMessageBundle)
#if defined(USE_TCMALLOC)
      IPC_MESSAGE_HANDLER(ViewHostMsg_RendererTcmalloc, OnRendererTcmalloc)
#endif
      IPC_MESSAGE_HANDLER(ViewHostMsg_EstablishGpuChannel,
                          OnEstablishGpuChannel)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SynchronizeGpu,
                                      OnSynchronizeGpu)
      IPC_MESSAGE_UNHANDLED(
          handled = false)
    IPC_END_MESSAGE_MAP_EX()
  }

  if (!msg_is_ok)
    BrowserRenderProcessHost::BadMessageTerminateProcess(msg.type(), handle());

  return handled;
}

void ResourceMessageFilter::OnDestruct() {
  ChromeThread::DeleteOnIOThread::Destruct(this);
}

void ResourceMessageFilter::OnReceiveContextMenuMsg(const IPC::Message& msg) {
  void* iter = NULL;
  ContextMenuParams params;
  if (!IPC::ParamTraits<ContextMenuParams>::Read(&msg, &iter, &params))
    return;

  // Create a new ViewHostMsg_ContextMenu message.
  const ViewHostMsg_ContextMenu context_menu_message(msg.routing_id(), params);
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      new ContextMenuMessageDispatcher(id(), context_menu_message));
}

// Called on the IPC thread:
bool ResourceMessageFilter::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

URLRequestContext* ResourceMessageFilter::GetRequestContext(
    uint32 request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  URLRequestContextGetter* request_context = request_context_;
  // If the request has resource type of ResourceType::MEDIA, we use a request
  // context specific to media for handling it because these resources have
  // specific needs for caching.
  if (request_data.resource_type == ResourceType::MEDIA)
    request_context = media_request_context_;
  return request_context->GetURLRequestContext();
}

void ResourceMessageFilter::OnMsgCreateWindow(
    int opener_id, bool user_gesture,
    WindowContainerType window_container_type,
    int64 session_storage_namespace_id, int* route_id,
    int64* cloned_session_storage_namespace_id) {
  *cloned_session_storage_namespace_id = dom_storage_dispatcher_host_->
      CloneSessionStorage(session_storage_namespace_id);
  render_widget_helper_->CreateNewWindow(opener_id,
                                         user_gesture,
                                         window_container_type,
                                         handle(),
                                         route_id);
}

void ResourceMessageFilter::OnMsgCreateWidget(int opener_id,
                                              WebKit::WebPopupType popup_type,
                                              int* route_id) {
  render_widget_helper_->CreateNewWidget(opener_id, popup_type, route_id);
}

void ResourceMessageFilter::OnSetCookie(const IPC::Message& message,
                                        const GURL& url,
                                        const GURL& first_party_for_cookies,
                                        const std::string& cookie) {
  ChromeURLRequestContext* context = GetRequestContextForURL(url);

  scoped_ptr<Blacklist::Match> match(
      GetPrivacyBlacklistMatchForURL(url, context));
  if (match.get() && (match->attributes() & Blacklist::kBlockCookies))
    return;

  SetCookieCompletion* callback =
      new SetCookieCompletion(id(), message.routing_id(), url, cookie, context);

  int policy = net::OK;
  if (context->cookie_policy()) {
    policy = context->cookie_policy()->CanSetCookie(
        url, first_party_for_cookies, cookie, callback);
    if (policy == net::ERR_IO_PENDING)
      return;
  }
  callback->Run(policy);
}

void ResourceMessageFilter::OnGetCookies(const GURL& url,
                                         const GURL& first_party_for_cookies,
                                         IPC::Message* reply_msg) {
  URLRequestContext* context = GetRequestContextForURL(url);

  GetCookiesCompletion* callback =
      new GetCookiesCompletion(url, reply_msg, this, context);

  int policy = net::OK;
  if (context->cookie_policy()) {
    policy = context->cookie_policy()->CanGetCookies(
        url, first_party_for_cookies, callback);
    if (policy == net::ERR_IO_PENDING) {
      Send(new ViewMsg_SignalCookiePromptEvent());
      return;
    }
  }
  callback->Run(policy);
}

void ResourceMessageFilter::OnGetRawCookies(
    const GURL& url,
    const GURL& first_party_for_cookies,
    IPC::Message* reply_msg) {

  ChromeURLRequestContext* context = GetRequestContextForURL(url);

  // Only return raw cookies to trusted renderers or if this request is
  // not targeted to an an external host like ChromeFrame.
  // TODO(ananta) We need to support retreiving raw cookies from external
  // hosts.
  if (!ChildProcessSecurityPolicy::GetInstance()->CanReadRawCookies(id()) ||
      context->IsExternal()) {
    ViewHostMsg_GetRawCookies::WriteReplyParams(
        reply_msg,
        std::vector<webkit_glue::WebCookie>());
    Send(reply_msg);
    return;
  }

  GetRawCookiesCompletion* callback =
      new GetRawCookiesCompletion(url, reply_msg, this, context);

  // We check policy here to avoid sending back cookies that would not normally
  // be applied to outbound requests for the given URL.  Since this cookie info
  // is visible in the developer tools, it is helpful to make it match reality.
  int policy = net::OK;
  if (context->cookie_policy()) {
    policy = context->cookie_policy()->CanGetCookies(
       url, first_party_for_cookies, callback);
    if (policy == net::ERR_IO_PENDING) {
      Send(new ViewMsg_SignalCookiePromptEvent());
      return;
    }
  }
  callback->Run(policy);
}

void ResourceMessageFilter::OnDeleteCookie(const GURL& url,
                                           const std::string& cookie_name) {
  URLRequestContext* context = GetRequestContextForURL(url);
  context->cookie_store()->DeleteCookie(url, cookie_name);
}

void ResourceMessageFilter::OnGetCookiesEnabled(
    const GURL& url,
    const GURL& first_party_for_cookies,
    bool* enabled) {
  *enabled = GetRequestContextForURL(url)->AreCookiesEnabled();
}

#if defined(OS_WIN)  // This hack is Windows-specific.
void ResourceMessageFilter::OnPreCacheFont(LOGFONT font) {
  // If the renderer is running in a sandbox, GetTextMetrics()
  // can sometimes fail. If a font has not been loaded
  // previously, GetTextMetrics() will try to load the font
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

  UMA_HISTOGRAM_COUNTS_100("Memory.CachedFontAndDC",
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

void ResourceMessageFilter::OnGetPlugins(bool refresh,
                                         IPC::Message* reply_msg) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::OnGetPluginsOnFileThread, refresh,
          reply_msg));
}

void ResourceMessageFilter::OnGetPluginsOnFileThread(
    bool refresh, IPC::Message* reply_msg) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  std::vector<WebPluginInfo> plugins;
  NPAPI::PluginList::Singleton()->GetEnabledPlugins(refresh, &plugins);
  ViewHostMsg_GetPlugins::WriteReplyParams(reply_msg, plugins);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ResourceMessageFilter::Send, reply_msg));
}

void ResourceMessageFilter::OnGetPluginPath(const GURL& url,
                                            const GURL& policy_url,
                                            const std::string& mime_type,
                                            FilePath* filename,
                                            std::string* url_mime_type) {
  *filename = plugin_service_->GetPluginPath(
      url, policy_url, mime_type, url_mime_type);
}

void ResourceMessageFilter::OnOpenChannelToPlugin(const GURL& url,
                                                  const std::string& mime_type,
                                                  const std::wstring& locale,
                                                  IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToPlugin(
      this, url, mime_type, locale, reply_msg);
}

void ResourceMessageFilter::OnLaunchNaCl(
    const std::wstring& url, int channel_descriptor, IPC::Message* reply_msg) {
  NaClProcessHost* host = new NaClProcessHost(resource_dispatcher_host_, url);
  host->Launch(this, channel_descriptor, reply_msg);
}

void ResourceMessageFilter::OnCreateWorker(
    const ViewHostMsg_CreateWorker_Params& params, int* route_id) {
  *route_id = params.route_id != MSG_ROUTING_NONE ?
      params.route_id : render_widget_helper_->GetNextRoutingID();
  WorkerService::GetInstance()->CreateWorker(
      params.url, params.is_shared, off_the_record(), params.name,
      params.document_id, id(), params.render_view_route_id, this, *route_id,
      db_dispatcher_host_->database_tracker(),
      GetRequestContextForURL(params.url)->host_content_settings_map());
}

void ResourceMessageFilter::OnLookupSharedWorker(
    const ViewHostMsg_CreateWorker_Params& params, bool* exists, int* route_id,
    bool* url_mismatch) {
  *route_id = render_widget_helper_->GetNextRoutingID();
  *exists = WorkerService::GetInstance()->LookupSharedWorker(
      params.url, params.name, off_the_record(), params.document_id, id(),
      params.render_view_route_id, this, *route_id, url_mismatch);
}

void ResourceMessageFilter::OnDocumentDetached(unsigned long long document_id) {
  // Notify the WorkerService that the passed document was detached so any
  // associated shared workers can be shut down.
  WorkerService::GetInstance()->DocumentDetached(this, document_id);
}

void ResourceMessageFilter::OnCancelCreateDedicatedWorker(int route_id) {
  WorkerService::GetInstance()->CancelCreateDedicatedWorker(this, route_id);
}

void ResourceMessageFilter::OnForwardToWorker(const IPC::Message& message) {
  WorkerService::GetInstance()->ForwardMessage(message, this);
}

void ResourceMessageFilter::OnDownloadUrl(const IPC::Message& message,
                                          const GURL& url,
                                          const GURL& referrer) {
  URLRequestContext* context = request_context_->GetURLRequestContext();
  resource_dispatcher_host_->BeginDownload(url,
                                           referrer,
                                           DownloadSaveInfo(),
                                           id(),
                                           message.routing_id(),
                                           context);
}

void ResourceMessageFilter::OnClipboardWriteObjectsSync(
    const Clipboard::ObjectMap& objects,
    base::SharedMemoryHandle bitmap_handle) {
  DCHECK(base::SharedMemory::IsHandleValid(bitmap_handle))
      << "Bad bitmap handle";
  // We cannot write directly from the IO thread, and cannot service the IPC
  // on the UI thread. We'll copy the relevant data and get a handle to any
  // shared memory so it doesn't go away when we resume the renderer, and post
  // a task to perform the write on the UI thread.
  Clipboard::ObjectMap* long_living_objects = new Clipboard::ObjectMap(objects);

  // Splice the shared memory handle into the clipboard data.
  Clipboard::ReplaceSharedMemHandle(long_living_objects, bitmap_handle,
                                    handle());

  ChromeThread::PostTask(
      ChromeThread::UI,
      FROM_HERE,
      new WriteClipboardTask(long_living_objects));
}

void ResourceMessageFilter::OnClipboardWriteObjectsAsync(
    const Clipboard::ObjectMap& objects) {
  // We cannot write directly from the IO thread, and cannot service the IPC
  // on the UI thread. We'll copy the relevant data and post a task to preform
  // the write on the UI thread.
  Clipboard::ObjectMap* long_living_objects = new Clipboard::ObjectMap(objects);

  ChromeThread::PostTask(
      ChromeThread::UI,
      FROM_HERE,
      new WriteClipboardTask(long_living_objects));
}

#if !defined(USE_X11)
// On non-X11 platforms, clipboard actions can be performed on the IO thread.
// On X11, since the clipboard is linked with GTK, we either have to do this
// with GTK on the UI thread, or with Xlib on the BACKGROUND_X11 thread. In an
// ideal world, we would do the latter. However, for now we're going to
// terminate these calls on the UI thread. This risks deadlock in the case of
// plugins, but it's better than crashing which is what doing on the IO thread
// gives us.
//
// See resource_message_filter_gtk.cc for the Linux implementation of these
// functions.

void ResourceMessageFilter::OnClipboardIsFormatAvailable(
    Clipboard::FormatType format, Clipboard::Buffer buffer,
    IPC::Message* reply) {
  const bool result = GetClipboard()->IsFormatAvailable(format, buffer);
  ViewHostMsg_ClipboardIsFormatAvailable::WriteReplyParams(reply, result);
  Send(reply);
}

void ResourceMessageFilter::OnClipboardReadText(Clipboard::Buffer buffer,
                                                IPC::Message* reply) {
  string16 result;
  GetClipboard()->ReadText(buffer, &result);
  ViewHostMsg_ClipboardReadText::WriteReplyParams(reply, result);
  Send(reply);
}

void ResourceMessageFilter::OnClipboardReadAsciiText(Clipboard::Buffer buffer,
                                                     IPC::Message* reply) {
  std::string result;
  GetClipboard()->ReadAsciiText(buffer, &result);
  ViewHostMsg_ClipboardReadAsciiText::WriteReplyParams(reply, result);
  Send(reply);
}

void ResourceMessageFilter::OnClipboardReadHTML(Clipboard::Buffer buffer,
                                                IPC::Message* reply) {
  std::string src_url_str;
  string16 markup;
  GetClipboard()->ReadHTML(buffer, &markup, &src_url_str);
  const GURL src_url = GURL(src_url_str);

  ViewHostMsg_ClipboardReadHTML::WriteReplyParams(reply, markup, src_url);
  Send(reply);
}

#endif

void ResourceMessageFilter::OnCheckNotificationPermission(
    const GURL& source_url, int* result) {
  *result = WebKit::WebNotificationPresenter::PermissionNotAllowed;

  ChromeURLRequestContext* context = GetRequestContextForURL(source_url);
  if (context->CheckURLAccessToExtensionPermission(source_url,
      Extension::kNotificationPermission)) {
    *result = WebKit::WebNotificationPresenter::PermissionAllowed;
    return;
  }

  // Fall back to the regular notification preferences, which works on an
  // origin basis.
  *result = notification_prefs_->HasPermission(source_url.GetOrigin());
}

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
  *context = CPBrowsingContextManager::Instance()->Allocate(
      request_context_->GetURLRequestContext());
}

#if defined(OS_WIN)
void ResourceMessageFilter::OnDuplicateSection(
    base::SharedMemoryHandle renderer_handle,
    base::SharedMemoryHandle* browser_handle) {
  // Duplicate the handle in this process right now so the memory is kept alive
  // (even if it is not mapped)
  base::SharedMemory shared_buf(renderer_handle, true, handle());
  shared_buf.GiveToProcess(base::GetCurrentProcessHandle(), browser_handle);
}
#endif

#if defined(OS_POSIX)
void ResourceMessageFilter::OnAllocateSharedMemoryBuffer(
    uint32 buffer_size,
    base::SharedMemoryHandle* handle) {
  base::SharedMemory shared_buf;
  shared_buf.Create(L"", false, false, buffer_size);
  if (!shared_buf.Map(buffer_size)) {
    *handle = base::SharedMemory::NULLHandle();
    NOTREACHED() << "Cannot map shared memory buffer";
    return;
  }
  shared_buf.GiveToProcess(base::GetCurrentProcessHandle(), handle);
}
#endif

void ResourceMessageFilter::OnResourceTypeStats(
    const WebCache::ResourceTypeStats& stats) {
  HISTOGRAM_COUNTS("WebCoreCache.ImagesSizeKB",
                   static_cast<int>(stats.images.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.CSSStylesheetsSizeKB",
                   static_cast<int>(stats.cssStyleSheets.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.ScriptsSizeKB",
                   static_cast<int>(stats.scripts.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.XSLStylesheetsSizeKB",
                   static_cast<int>(stats.xslStyleSheets.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.FontsSizeKB",
                   static_cast<int>(stats.fonts.size / 1024));
  // We need to notify the TaskManager of these statistics from the UI
  // thread.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableFunction(
          &ResourceMessageFilter::OnResourceTypeStatsOnUIThread,
          stats,
          base::GetProcId(handle())));
}

void ResourceMessageFilter::OnResourceTypeStatsOnUIThread(
    WebCache::ResourceTypeStats stats, base::ProcessId renderer_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  TaskManager::GetInstance()->model()->NotifyResourceTypeStats(
      renderer_id, stats);
}


void ResourceMessageFilter::OnV8HeapStats(int v8_memory_allocated,
                                          int v8_memory_used) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableFunction(&ResourceMessageFilter::OnV8HeapStatsOnUIThread,
                          v8_memory_allocated,
                          v8_memory_used,
                          base::GetProcId(handle())));
}

// static
void ResourceMessageFilter::OnV8HeapStatsOnUIThread(
    int v8_memory_allocated, int v8_memory_used, base::ProcessId renderer_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  TaskManager::GetInstance()->model()->NotifyV8HeapStats(
      renderer_id,
      static_cast<size_t>(v8_memory_allocated),
      static_cast<size_t>(v8_memory_used));
}

void ResourceMessageFilter::OnDidZoomURL(const GURL& url,
                                         int zoom_level) {
  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &ResourceMessageFilter::UpdateHostZoomLevelsOnUIThread,
                        url, zoom_level));
}

void ResourceMessageFilter::UpdateHostZoomLevelsOnUIThread(
    const GURL& url,
    int zoom_level) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  host_zoom_map_->SetZoomLevel(url, zoom_level);

  // Notify renderers.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* render_process_host = i.GetCurrentValue();
    render_process_host->Send(
        new ViewMsg_SetZoomLevelForCurrentURL(url, zoom_level));
  }
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
                             false,
                             true,
                             task);
}

void ResourceMessageFilter::OnGetDefaultPrintSettingsReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  ViewMsg_Print_Params params;
  if (printer_query->last_status() != printing::PrintingContext::OK) {
    memset(&params, 0, sizeof(params));
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params);
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

#if defined(OS_WIN) || defined(OS_MACOSX)

void ResourceMessageFilter::OnScriptedPrint(
    const ViewHostMsg_ScriptedPrint_Params& params,
    IPC::Message* reply_msg) {
#if defined(OS_WIN)
  HWND host_window = gfx::NativeViewFromId(params.host_window_id);
#elif defined(OS_MACOSX)
  gfx::NativeWindow host_window = NULL;
  // TODO: Get an actual window ref here, to allow a sheet-based print dialog.
#endif

  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager_->PopPrinterQuery(params.cookie, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &ResourceMessageFilter::OnScriptedPrintReply,
      printer_query,
      params.routing_id,
      reply_msg);
#if defined(OS_WIN)
  // Shows the Print... dialog box. This is asynchronous, only the IPC message
  // sender will hang until the Print dialog is dismissed.
  if (!host_window || !IsWindow(host_window)) {
    // TODO(maruel):  bug 1214347 Get the right browser window instead.
    host_window = GetDesktopWindow();
  } else {
    host_window = GetAncestor(host_window, GA_ROOTOWNER);
  }
  DCHECK(host_window);
#endif

  printer_query->GetSettings(printing::PrinterQuery::ASK_USER,
                             host_window,
                             params.expected_pages_count,
                             params.has_selection,
                             params.use_overlays,
                             task);
}

void ResourceMessageFilter::OnScriptedPrintReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    int routing_id,
    IPC::Message* reply_msg) {
  ViewMsg_PrintPages_Params params;
  if (printer_query->last_status() != printing::PrintingContext::OK ||
      !printer_query->settings().dpi()) {
    memset(&params, 0, sizeof(params));
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params.params);
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

#endif  // OS_WIN || OS_MACOSX

// static
Clipboard* ResourceMessageFilter::GetClipboard() {
  // We have a static instance of the clipboard service for use by all message
  // filters.  This instance lives for the life of the browser processes.
  static Clipboard* clipboard = new Clipboard;

  return clipboard;
}

ChromeURLRequestContext* ResourceMessageFilter::GetRequestContextForURL(
    const GURL& url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  URLRequestContextGetter* context_getter =
      url.SchemeIs(chrome::kExtensionScheme) ?
          extensions_request_context_ : request_context_;
  return static_cast<ChromeURLRequestContext*>(
      context_getter->GetURLRequestContext());
}

void ResourceMessageFilter::OnPlatformCheckSpelling(const string16& word,
                                                    int tag,
                                                    bool* correct) {
  *correct = SpellCheckerPlatform::CheckSpelling(word, tag);
}

void ResourceMessageFilter::OnPlatformFillSuggestionList(
    const string16& word,
    std::vector<string16>* suggestions) {
  SpellCheckerPlatform::FillSuggestionList(word, suggestions);
}

void ResourceMessageFilter::OnGetDocumentTag(IPC::Message* reply_msg) {
  int tag = SpellCheckerPlatform::GetDocumentTag();
  ViewHostMsg_GetDocumentTag::WriteReplyParams(reply_msg, tag);
  Send(reply_msg);
  return;
}

void ResourceMessageFilter::OnDocumentWithTagClosed(int tag) {
  SpellCheckerPlatform::CloseDocumentWithTag(tag);
}

void ResourceMessageFilter::OnShowSpellingPanel(bool show) {
  SpellCheckerPlatform::ShowSpellingPanel(show);
}

void ResourceMessageFilter::OnUpdateSpellingPanelWithMisspelledWord(
    const string16& word) {
  SpellCheckerPlatform::UpdateSpellingPanelWithMisspelledWord(word);
}

void ResourceMessageFilter::Observe(NotificationType type,
                                    const NotificationSource &source,
                                    const NotificationDetails &details) {
  if (type == NotificationType::BLACKLIST_NONVISUAL_RESOURCE_BLOCKED) {
    BlacklistUI::OnNonvisualContentBlocked(
        Details<const URLRequest>(details).ptr());
  }
}

void ResourceMessageFilter::OnDnsPrefetch(
    const std::vector<std::string>& hostnames) {
  chrome_browser_net::DnsPrefetchList(hostnames);
}

void ResourceMessageFilter::OnRendererHistograms(
    int sequence_number,
    const std::vector<std::string>& histograms) {
  HistogramSynchronizer::DeserializeHistogramList(sequence_number, histograms);
}

#if defined(OS_MACOSX)
void ResourceMessageFilter::OnAllocTransportDIB(
    size_t size, bool cache_in_browser, TransportDIB::Handle* handle) {
  render_widget_helper_->AllocTransportDIB(size, cache_in_browser, handle);
}

void ResourceMessageFilter::OnFreeTransportDIB(
    TransportDIB::Id dib_id) {
  render_widget_helper_->FreeTransportDIB(dib_id);
}
#endif

void ResourceMessageFilter::OnOpenChannelToExtension(
    int routing_id, const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name, int* port_id) {
  if (extensions_message_service_.get()) {
    *port_id = extensions_message_service_->
        OpenChannelToExtension(routing_id, source_extension_id,
                               target_extension_id, channel_name, this);
  } else {
    *port_id = -1;
  }
}

void ResourceMessageFilter::OnOpenChannelToTab(
    int routing_id, int tab_id, const std::string& extension_id,
    const std::string& channel_name, int* port_id) {
  if (extensions_message_service_.get()) {
    *port_id = extensions_message_service_->
        OpenChannelToTab(routing_id, tab_id, extension_id, channel_name, this);
  } else {
    *port_id = -1;
  }
}

bool ResourceMessageFilter::CheckBenchmarkingEnabled() {
  static bool checked = false;
  static bool result = false;
  if (!checked) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    result = command_line.HasSwitch(switches::kEnableBenchmarking);
    checked = true;
  }
  return result;
}

void ResourceMessageFilter::OnCloseCurrentConnections() {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled())
    return;
  request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache()->CloseCurrentConnections();
}

void ResourceMessageFilter::OnSetCacheMode(bool enabled) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled())
    return;

  net::HttpCache::Mode mode = enabled ?
      net::HttpCache::NORMAL : net::HttpCache::DISABLE;
  request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache()->set_mode(mode);
}

void ResourceMessageFilter::OnGetFileSize(const FilePath& path,
                                          IPC::Message* reply_msg) {
  // Get file size only when the child process has been granted permission to
  // upload the file.
  if (!ChildProcessSecurityPolicy::GetInstance()->CanUploadFile(id(), path)) {
    ViewHostMsg_GetFileSize::WriteReplyParams(
        reply_msg, static_cast<int64>(-1));
    Send(reply_msg);
    return;
  }

  // Getting file size could take long time if it lives on a network share,
  // so run it on FILE thread.
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::OnGetFileInfoOnFileThread, path,
          reply_msg, &WriteFileSize));
}

void ResourceMessageFilter::OnGetFileModificationTime(const FilePath& path,
                                                      IPC::Message* reply_msg) {
  // Get file modification time only when the child process has been granted
  // permission to upload the file.
  if (!ChildProcessSecurityPolicy::GetInstance()->CanUploadFile(id(), path)) {
    ViewHostMsg_GetFileModificationTime::WriteReplyParams(reply_msg,
                                                          base::Time());
    Send(reply_msg);
    return;
  }

  // Getting file modification time could take a long time if it lives on a
  // network share, so run it on the FILE thread.
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::OnGetFileInfoOnFileThread,
          path, reply_msg, &WriteFileModificationTime));
}

void ResourceMessageFilter::OnGetFileInfoOnFileThread(
    const FilePath& path,
    IPC::Message* reply_msg,
    FileInfoWriteFunc write_func) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  file_util::FileInfo file_info;
  file_info.size = 0;
  file_util::GetFileInfo(path, &file_info);

  (*write_func)(reply_msg, file_info);

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ResourceMessageFilter::Send, reply_msg));
}

void ResourceMessageFilter::OnKeygen(uint32 key_size_index,
                                     const std::string& challenge_string,
                                     const GURL& url,
                                     std::string* signed_public_key) {
  // Map displayed strings indicating level of keysecurity in the <keygen>
  // menu to the key size in bits. (See SSLKeyGeneratorChromium.cpp in WebCore.)
  int key_size_in_bits;
  switch (key_size_index) {
    case 0:
      key_size_in_bits = 2048;
      break;
    case 1:
      key_size_in_bits = 1024;
      break;
    default:
      DCHECK(false) << "Illegal key_size_index " << key_size_index;
      *signed_public_key = std::string();
      return;
  }
  net::KeygenHandler keygen_handler(key_size_in_bits, challenge_string);
  *signed_public_key = keygen_handler.GenKeyAndSignChallenge();
}

#if defined(USE_TCMALLOC)
void ResourceMessageFilter::OnRendererTcmalloc(base::ProcessId pid,
                                               const std::string& output) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableFunction(AboutTcmallocRendererCallback, pid, output));
}
#endif

void ResourceMessageFilter::OnEstablishGpuChannel() {
  GpuProcessHost::Get()->EstablishGpuChannel(id(), this);
}

void ResourceMessageFilter::OnSynchronizeGpu(IPC::Message* reply) {
  // We handle this message (and the other GPU process messages) here
  // rather than handing the message to the GpuProcessHost for
  // dispatch so that we can use the DELAY_REPLY macro to synthesize
  // the reply message, and also send down a "this" pointer so that
  // the GPU process host can send the reply later.
  GpuProcessHost::Get()->Synchronize(reply, this);
}

void ResourceMessageFilter::OnGetExtensionMessageBundle(
    const std::string& extension_id, IPC::Message* reply_msg) {
  ChromeURLRequestContext* context = static_cast<ChromeURLRequestContext*>(
    request_context_->GetURLRequestContext());

  FilePath extension_path = context->GetPathForExtension(extension_id);
  std::string default_locale =
    context->GetDefaultLocaleForExtension(extension_id);

  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::OnGetExtensionMessageBundleOnFileThread,
          extension_path, extension_id, default_locale, reply_msg));
}

void ResourceMessageFilter::OnGetExtensionMessageBundleOnFileThread(
    const FilePath& extension_path,
    const std::string& extension_id,
    const std::string& default_locale,
    IPC::Message* reply_msg) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  std::map<std::string, std::string> dictionary_map;
  if (!default_locale.empty()) {
    // Touch disk only if extension is localized.
    std::string error;
    scoped_ptr<ExtensionMessageBundle> bundle(
        extension_file_util::LoadExtensionMessageBundle(
            extension_path, default_locale, &error));

    if (bundle.get())
      dictionary_map = *bundle->dictionary();
  }

  // Add @@extension_id reserved message here, so it's available to
  // non-localized extensions too.
  dictionary_map.insert(
      std::make_pair(ExtensionMessageBundle::kExtensionIdKey, extension_id));

  ViewHostMsg_GetExtensionMessageBundle::WriteReplyParams(
      reply_msg, dictionary_map);

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ResourceMessageFilter::Send, reply_msg));
}
