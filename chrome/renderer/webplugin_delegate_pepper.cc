// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define PEPPER_APIS_ENABLED 1

#include "build/build_config.h"
#if defined(OS_WIN)
#include <vsstyle.h>
#endif

#include "chrome/renderer/webplugin_delegate_pepper.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#if defined(OS_MACOSX)
#include "base/mac_util.h"
#endif
#include "base/md5.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#if defined(OS_MACOSX)
#include "base/scoped_cftyperef.h"
#endif
#include "base/scoped_ptr.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/time.h"
#if defined(OS_WIN)
#include "base/win_util.h"
#endif
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/webplugin_delegate_proxy.h"
#include "gfx/blit.h"
#if defined(OS_WIN)
#include "gfx/codec/jpeg_codec.h"
#include "gfx/gdi_util.h"
#include "gfx/native_theme_win.h"
#include "skia/ext/vector_platform_device.h"
#endif
#include "third_party/npapi/bindings/npapi_extensions.h"
#include "third_party/npapi/bindings/npapi_extensions_private.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/webkit_glue.h"

#if defined(ENABLE_GPU)
#include "webkit/glue/plugins/plugin_constants_win.h"
#endif

using gpu::Buffer;
using webkit_glue::WebPlugin;
using webkit_glue::WebPluginDelegate;
using webkit_glue::WebPluginResourceClient;
using WebKit::WebCursorInfo;
using WebKit::WebKeyboardEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

namespace {

// Implementation artifacts for a context
struct Device2DImpl {
  TransportDIB* dib;
};

struct Device3DImpl {
  gpu::CommandBuffer* command_buffer;
  bool dynamically_created;
};

const int32 kDefaultCommandBufferSize = 1024 * 1024;

#if defined(OS_WIN)
struct ScrollbarThemeMapping {
  NPThemeItem item;
  NPThemeState state;
  int state_id;  // Used by uxtheme.
};
static const ScrollbarThemeMapping scrollbar_mappings[] = {
  { NPThemeItemScrollbarDownArrow, NPThemeStateDisabled, ABS_DOWNDISABLED},
  { NPThemeItemScrollbarDownArrow, NPThemeStateHot, ABS_DOWNHOT},
  { NPThemeItemScrollbarDownArrow, NPThemeStateHover, ABS_DOWNHOVER},
  { NPThemeItemScrollbarDownArrow, NPThemeStateNormal, ABS_DOWNNORMAL},
  { NPThemeItemScrollbarDownArrow, NPThemeStatePressed, ABS_DOWNPRESSED},
  { NPThemeItemScrollbarLeftArrow, NPThemeStateDisabled, ABS_LEFTDISABLED},
  { NPThemeItemScrollbarLeftArrow, NPThemeStateHot, ABS_LEFTHOT},
  { NPThemeItemScrollbarLeftArrow, NPThemeStateHover, ABS_LEFTHOVER},
  { NPThemeItemScrollbarLeftArrow, NPThemeStateNormal, ABS_LEFTNORMAL},
  { NPThemeItemScrollbarLeftArrow, NPThemeStatePressed, ABS_LEFTPRESSED},
  { NPThemeItemScrollbarRightArrow, NPThemeStateDisabled, ABS_RIGHTDISABLED},
  { NPThemeItemScrollbarRightArrow, NPThemeStateHot, ABS_RIGHTHOT},
  { NPThemeItemScrollbarRightArrow, NPThemeStateHover, ABS_RIGHTHOVER},
  { NPThemeItemScrollbarRightArrow, NPThemeStateNormal, ABS_RIGHTNORMAL},
  { NPThemeItemScrollbarRightArrow, NPThemeStatePressed, ABS_RIGHTPRESSED},
  { NPThemeItemScrollbarUpArrow, NPThemeStateDisabled, ABS_UPDISABLED},
  { NPThemeItemScrollbarUpArrow, NPThemeStateHot, ABS_UPHOT},
  { NPThemeItemScrollbarUpArrow, NPThemeStateHover, ABS_UPHOVER},
  { NPThemeItemScrollbarUpArrow, NPThemeStateNormal, ABS_UPNORMAL},
  { NPThemeItemScrollbarUpArrow, NPThemeStatePressed, ABS_UPPRESSED},
};

int GetStateIdFromNPState(int state) {
  switch (state) {
    case NPThemeStateDisabled:
      return SCRBS_DISABLED;
    case NPThemeStateHot:
      return SCRBS_HOT;
    case NPThemeStateHover:
      return SCRBS_HOVER;
    case NPThemeStateNormal:
      return SCRBS_NORMAL;
    case NPThemeStatePressed:
      return SCRBS_PRESSED;
    default:
      return -1;
  };
}
#else
  // TODO(port)
#endif

} // namespace

WebPluginDelegatePepper* WebPluginDelegatePepper::Create(
    const FilePath& filename,
    const std::string& mime_type,
    const base::WeakPtr<RenderView>& render_view) {
  scoped_refptr<NPAPI::PluginLib> plugin_lib =
      NPAPI::PluginLib::CreatePluginLib(filename);
  if (plugin_lib.get() == NULL)
    return NULL;

  NPError err = plugin_lib->NP_Initialize();
  if (err != NPERR_NO_ERROR)
    return NULL;

  scoped_refptr<NPAPI::PluginInstance> instance =
      plugin_lib->CreateInstance(mime_type);
  return new WebPluginDelegatePepper(render_view,
                                     instance.get());
}

void WebPluginDelegatePepper::didChooseFile(
    const WebKit::WebVector<WebKit::WebString>& file_names) {
  if (file_names.isEmpty()) {
    current_choose_file_callback_(NULL, 0, current_choose_file_user_data_);
  } else {
    // Construct a bunch of 8-bit strings for the callback.
    std::vector<std::string> file_strings;
    file_strings.resize(file_names.size());
    for (size_t i = 0; i < file_names.size(); i++)
      file_strings[i] = file_names[0].utf8();

    // Construct an array of pointers to each of the strings.
    std::vector<const char*> pointers_to_strings;
    pointers_to_strings.resize(file_strings.size());
    for (size_t i = 0; i < file_strings.size(); i++)
      pointers_to_strings[i] = file_strings[i].c_str();

    current_choose_file_callback_(
        &pointers_to_strings[0],
        static_cast<int>(pointers_to_strings.size()),
        current_choose_file_user_data_);
  }
}

bool WebPluginDelegatePepper::Initialize(
    const GURL& url,
    const std::vector<std::string>& arg_names,
    const std::vector<std::string>& arg_values,
    WebPlugin* plugin,
    bool load_manually) {
  plugin_ = plugin;

  instance_->set_web_plugin(plugin_);
  int argc = 0;
  scoped_array<char*> argn(new char*[arg_names.size()]);
  scoped_array<char*> argv(new char*[arg_names.size()]);
  for (size_t i = 0; i < arg_names.size(); ++i) {
    argn[argc] = const_cast<char*>(arg_names[i].c_str());
    argv[argc] = const_cast<char*>(arg_values[i].c_str());
    argc++;
  }

  bool start_result = instance_->Start(
      url, argn.get(), argv.get(), argc, load_manually);
  if (!start_result)
    return false;

  plugin_url_ = url.spec();

  return true;
}

void WebPluginDelegatePepper::DestroyInstance() {
  if (instance_ && (instance_->npp()->ndata != NULL)) {
    // Shutdown all streams before destroying so that
    // no streams are left "in progress".  Need to do
    // this before calling set_web_plugin(NULL) because the
    // instance uses the helper to do the download.
    instance_->CloseStreams();

    window_.window = NULL;
    instance_->NPP_SetWindow(&window_);

    instance_->NPP_Destroy();

    instance_->set_web_plugin(NULL);

    instance_ = 0;
  }

  // Destroy the nested GPU plugin only after first destroying the underlying
  // Pepper plugin. This is so the Pepper plugin does not attempt to issue
  // rendering commands after the GPU plugin has stopped processing them and
  // responding to them.
  if (nested_delegate_) {
#if defined(ENABLE_GPU)
    if (command_buffer_) {
      nested_delegate_->DestroyCommandBuffer(command_buffer_);
      command_buffer_ = NULL;
    }
#endif

    nested_delegate_->PluginDestroyed();
    nested_delegate_ = NULL;
  }
}

void WebPluginDelegatePepper::UpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  // Only resend to the instance if the geometry has changed.
  if (window_rect == window_rect_ && clip_rect == clip_rect_)
    return;

  clip_rect_ = clip_rect;
  cutout_rects_.clear();

  if (window_rect_ == window_rect)
    return;
  window_rect_ = window_rect;

  // TODO(brettw) figure out how to tell the plugin that the size changed and it
  // needs to repaint?
  SkBitmap new_committed;
  new_committed.setConfig(SkBitmap::kARGB_8888_Config,
                          window_rect_.width(), window_rect_.height());
  new_committed.allocPixels();
  committed_bitmap_ = new_committed;

  // Forward the new geometry to the nested plugin instance.
  if (nested_delegate_)
    nested_delegate_->UpdateGeometry(window_rect, clip_rect);

#if defined(ENABLE_GPU)
#if defined(OS_MACOSX)
  // Send the new window size to the command buffer service code so it
  // can allocate a new backing store. The handle to the new backing
  // store is sent back to the browser asynchronously.
  if (command_buffer_) {
    command_buffer_->SetWindowSize(window_rect_.size());
  }
#endif  // OS_MACOSX
#endif  // ENABLE_GPU

  if (!instance())
    return;

  ForwardSetWindow();
}

NPObject* WebPluginDelegatePepper::GetPluginScriptableObject() {
  return instance_->GetPluginScriptableObject();
}

void WebPluginDelegatePepper::DidFinishLoadWithReason(
    const GURL& url, NPReason reason, int notify_id) {
  instance()->DidFinishLoadWithReason(url, reason, notify_id);
}

int WebPluginDelegatePepper::GetProcessId() {
  // We are in process, so the plugin pid is this current process pid.
  return base::GetCurrentProcId();
}

void WebPluginDelegatePepper::SendJavaScriptStream(
    const GURL& url,
    const std::string& result,
    bool success,
    int notify_id) {
  instance()->SendJavaScriptStream(url, result, success, notify_id);
}

void WebPluginDelegatePepper::DidReceiveManualResponse(
    const GURL& url, const std::string& mime_type,
    const std::string& headers, uint32 expected_length, uint32 last_modified) {
  instance()->DidReceiveManualResponse(url, mime_type, headers,
                                       expected_length, last_modified);
}

void WebPluginDelegatePepper::DidReceiveManualData(const char* buffer,
                                                       int length) {
  instance()->DidReceiveManualData(buffer, length);
}

void WebPluginDelegatePepper::DidFinishManualLoading() {
  instance()->DidFinishManualLoading();
}

void WebPluginDelegatePepper::DidManualLoadFail() {
  instance()->DidManualLoadFail();
}

FilePath WebPluginDelegatePepper::GetPluginPath() {
  return instance()->plugin_lib()->plugin_info().path;
}

void WebPluginDelegatePepper::RenderViewInitiatedPaint() {
  // Broadcast event to all 2D contexts.
  Graphics2DMap::iterator iter2d(&graphic2d_contexts_);
  while (!iter2d.IsAtEnd()) {
    iter2d.GetCurrentValue()->RenderViewInitiatedPaint();
    iter2d.Advance();
  }
}

void WebPluginDelegatePepper::RenderViewFlushedPaint() {
  // Broadcast event to all 2D contexts.
  Graphics2DMap::iterator iter2d(&graphic2d_contexts_);
  while (!iter2d.IsAtEnd()) {
    iter2d.GetCurrentValue()->RenderViewFlushedPaint();
    iter2d.Advance();
  }
}

WebPluginResourceClient* WebPluginDelegatePepper::CreateResourceClient(
    unsigned long resource_id, const GURL& url, int notify_id) {
  return instance()->CreateStream(resource_id, url, std::string(), notify_id);
}

WebPluginResourceClient* WebPluginDelegatePepper::CreateSeekableResourceClient(
    unsigned long resource_id, int range_request_id) {
  return instance()->GetRangeRequest(range_request_id);
}

void WebPluginDelegatePepper::StartFind(const std::string& search_text,
                                        bool case_sensitive,
                                        int identifier) {
  find_identifier_ = identifier;
  GetFindExtensions()->startFind(
      instance()->npp(), search_text.c_str(), case_sensitive);
}

void WebPluginDelegatePepper::SelectFindResult(bool forward) {
  GetFindExtensions()->selectFindResult(instance()->npp(), forward);
}

void WebPluginDelegatePepper::StopFind() {
  find_identifier_ = -1;
  GetFindExtensions()->stopFind(instance()->npp());
}

void WebPluginDelegatePepper::NumberOfFindResultsChanged(int total,
                                                         bool final_result) {
  DCHECK(find_identifier_ != -1);

  if (total == 0) {
    render_view_->ReportNoFindInPageResults(find_identifier_);
  } else {
    render_view_->reportFindInPageMatchCount(
        find_identifier_, total, final_result);
  }
}

void WebPluginDelegatePepper::SelectedFindResultChanged(int index) {
  render_view_->reportFindInPageSelection(
        find_identifier_, index + 1, WebKit::WebRect());
}

void WebPluginDelegatePepper::Zoom(int factor) {
  NPPExtensions* extensions = NULL;
  instance()->NPP_GetValue(NPPVPepperExtensions, &extensions);
  if (extensions && extensions->zoom)
    extensions->zoom(instance()->npp(), factor);
}

bool WebPluginDelegatePepper::ChooseFile(const char* mime_types,
                                         int mode,
                                         NPChooseFileCallback callback,
                                         void* user_data) {
  if (!render_view_ || !callback)
    return false;

  if (current_choose_file_callback_)
    return false;  // Reentrant call to browse, only one can be outstanding
                   // per plugin.

  // TODO(brettw) do something with the mime types!
  current_choose_file_callback_ = callback;
  current_choose_file_user_data_ = user_data;

  ViewHostMsg_RunFileChooser_Params ipc_params;
  switch (mode) {
    case NPChooseFile_Open:
      ipc_params.mode = ViewHostMsg_RunFileChooser_Params::Open;
      break;
    case NPChooseFile_OpenMultiple:
      ipc_params.mode = ViewHostMsg_RunFileChooser_Params::OpenMultiple;
      break;
    case NPChooseFile_Save:
      ipc_params.mode = ViewHostMsg_RunFileChooser_Params::Save;
      break;
    default:
      return false;
  }
  return render_view_->ScheduleFileChooser(ipc_params, this);
}

NPError WebPluginDelegatePepper::Device2DQueryCapability(int32 capability,
                                                         int32* value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device2DQueryConfig(
    const NPDeviceContext2DConfig* request,
    NPDeviceContext2DConfig* obtain) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device2DInitializeContext(
    const NPDeviceContext2DConfig* config,
    NPDeviceContext2D* context) {

  if (!render_view_) {
    return NPERR_GENERIC_ERROR;
  }

  // This is a windowless plugin, so set it to have no handle. Defer this
  // until we know the plugin will use the 2D device. If it uses the 3D device
  // it will have a window handle.
  plugin_->SetWindow(0);

  scoped_ptr<Graphics2DDeviceContext> g2d(new Graphics2DDeviceContext(this));
  NPError status = g2d->Initialize(window_rect_, config, context);
  if (NPERR_NO_ERROR == status) {
    context->reserved = reinterpret_cast<void *>(
        graphic2d_contexts_.Add(g2d.release()));
  }
  return status;
}

NPError WebPluginDelegatePepper::Device2DSetStateContext(
    NPDeviceContext2D* context,
    int32 state,
    intptr_t value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device2DGetStateContext(
    NPDeviceContext2D* context,
    int32 state,
    intptr_t* value) {
  if (state == NPExtensionsReservedStateSharedMemory) {
    if (!context)
      return NPERR_INVALID_PARAM;
    Graphics2DDeviceContext* ctx = graphic2d_contexts_.Lookup(
        reinterpret_cast<intptr_t>(context->reserved));
    if (!ctx)
      return NPERR_INVALID_PARAM;
    *value = reinterpret_cast<intptr_t>(ctx->transport_dib());
    return NPERR_NO_ERROR;
  } else if (state == NPExtensionsReservedStateSharedMemoryChecksum) {
    if (!context)
      return NPERR_INVALID_PARAM;
    // Bytes per pixel.
    static const int kBytesPixel = 4;
    int32 row_count = context->dirty.bottom - context->dirty.top;
    int32 stride = context->dirty.right - context->dirty.left;
    size_t length = row_count * stride * kBytesPixel;
    MD5Digest md5_result;   // 128-bit digest
    MD5Sum(context->region, length, &md5_result);
    std::string hex_md5 = MD5DigestToBase16(md5_result);
    // Return the least significant 8 characters (i.e. 4 bytes)
    // of the 32 character hexadecimal result as an int.
    *value = HexStringToInt(hex_md5.substr(24));
    return NPERR_NO_ERROR;
  }
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device2DFlushContext(
    NPP id,
    NPDeviceContext2D* context,
    NPDeviceFlushContextCallbackPtr callback,
    void* user_data) {
  if (!context)
    return NPERR_INVALID_PARAM;

  Graphics2DDeviceContext* ctx = graphic2d_contexts_.Lookup(
      reinterpret_cast<intptr_t>(context->reserved));
  if (!ctx)
    return NPERR_INVALID_PARAM;  // TODO(brettw) call callback.

  return ctx->Flush(&committed_bitmap_, context, callback, id, user_data);
}

NPError WebPluginDelegatePepper::Device2DDestroyContext(
    NPDeviceContext2D* context) {

  if (!context || !graphic2d_contexts_.Lookup(
      reinterpret_cast<intptr_t>(context->reserved))) {
    return NPERR_INVALID_PARAM;
  }
  graphic2d_contexts_.Remove(reinterpret_cast<intptr_t>(context->reserved));
  memset(context, 0, sizeof(NPDeviceContext2D));
  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device2DThemeGetSize(NPThemeItem item,
                                                      int* width,
                                                      int* height) {
#if defined(OS_WIN)
  switch (item) {
    case NPThemeItemScrollbarDownArrow:
    case NPThemeItemScrollbarUpArrow:
      *width = GetSystemMetrics(SM_CXVSCROLL);
      *height = GetSystemMetrics(SM_CYVSCROLL);
      break;
    case NPThemeItemScrollbarLeftArrow:
    case NPThemeItemScrollbarRightArrow:
      *width = GetSystemMetrics(SM_CXHSCROLL);
      *height = GetSystemMetrics(SM_CYHSCROLL);
      break;
    case NPThemeItemScrollbarHorizontalThumb:
      *width = GetSystemMetrics(SM_CXHTHUMB);
      *height = *width;  // Make the min size a square.
      break;
    case NPThemeItemScrollbarVerticalThumb:
      *height = GetSystemMetrics(SM_CYVTHUMB);
      *width = *height;  // Make the min size a square.
      break;
    case NPThemeItemScrollbarHoriztonalTrack:
      *height = GetSystemMetrics(SM_CYHSCROLL);
      *width = 0;
      break;
    case NPThemeItemScrollbarVerticalTrack:
      *width = GetSystemMetrics(SM_CXVSCROLL);
      *height = 0;
      break;
    default:
      return NPERR_GENERIC_ERROR;
  }
  return NPERR_NO_ERROR;
#else
  NOTIMPLEMENTED();
  return NPERR_GENERIC_ERROR;
#endif
}

NPError WebPluginDelegatePepper::Device2DThemePaint(NPDeviceContext2D* context,
                                                    NPThemeParams* params) {
  if (!context)
    return NPERR_INVALID_PARAM;

  Graphics2DDeviceContext* ctx = graphic2d_contexts_.Lookup(
      reinterpret_cast<intptr_t>(context->reserved));
  if (!ctx)
    return NPERR_INVALID_PARAM;

  NPError rv = NPERR_GENERIC_ERROR;
  gfx::Rect rect(params->location.left,
                 params->location.top,
                 params->location.right - params->location.left,
                 params->location.bottom - params->location.top);
  skia::PlatformCanvas* canvas = ctx->canvas();

#if defined(OS_WIN)
  int state = -1;
  int part = -1;
  int classic_state = 0;
  skia::PlatformDevice::PlatformSurface surface = canvas->beginPlatformPaint();
#endif

  switch (params->item) {
    case NPThemeItemScrollbarDownArrow:
    case NPThemeItemScrollbarLeftArrow:
    case NPThemeItemScrollbarRightArrow:
    case NPThemeItemScrollbarUpArrow: {
      int state_to_use = params->state;
      if (state_to_use == NPThemeStateHover
#if defined(OS_WIN)
          && win_util::GetWinVersion() < win_util::WINVERSION_VISTA
#endif
          ) {
        state_to_use = NPThemeStateHover;
      }

#if defined(OS_WIN)
      for (size_t i = 0; i < arraysize(scrollbar_mappings); ++i) {
        if (scrollbar_mappings[i].item == params->item &&
            scrollbar_mappings[i].state == state_to_use) {
          state = scrollbar_mappings[i].state_id;
          gfx::NativeTheme::instance()->PaintScrollbarArrow(
              surface, state, classic_state, &rect.ToRECT());
          rv = NPERR_NO_ERROR;
          break;
        }
      }
#else
          // TODO(port)
#endif
      break;
    }
    case NPThemeItemScrollbarHorizontalThumb:
    case NPThemeItemScrollbarVerticalThumb:
#if defined(OS_WIN)
      // First draw the thumb, then the gripper.
      part = params->item == NPThemeItemScrollbarHorizontalThumb ?
          SBP_THUMBBTNHORZ : SBP_THUMBBTNVERT;
      state = GetStateIdFromNPState(params->state);
      gfx::NativeTheme::instance()->PaintScrollbarThumb(
          surface, part, state, classic_state, &rect.ToRECT());

      part = params->item == NPThemeItemScrollbarHorizontalThumb ?
        SBP_GRIPPERHORZ : SBP_GRIPPERVERT;
      gfx::NativeTheme::instance()->PaintScrollbarThumb(
          surface, part, state, classic_state, &rect.ToRECT());
      rv = NPERR_NO_ERROR;
#else
          // TODO(port)
#endif
      break;
    case NPThemeItemScrollbarHoriztonalTrack:
    case NPThemeItemScrollbarVerticalTrack: {
#if defined(OS_WIN)
      part = params->item == NPThemeItemScrollbarHoriztonalTrack ?
          SBP_LOWERTRACKHORZ : SBP_LOWERTRACKVERT;
      state = GetStateIdFromNPState(params->state);
      RECT align;
      align.left = align.right = params->align.x;
      align.top = align.bottom = params->align.y;
      gfx::NativeTheme::instance()->PaintScrollbarTrack(
          surface, part, state, classic_state, &rect.ToRECT(), &align, canvas);
      rv = NPERR_NO_ERROR;
#else
      // TODO(port)
      NOTIMPLEMENTED();
#endif
      break;
    }
    default:
      NOTREACHED();
  }

  canvas->endPlatformPaint();
  return rv;
}

NPError WebPluginDelegatePepper::Device3DQueryCapability(int32 capability,
                                                         int32* value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device3DQueryConfig(
    const NPDeviceContext3DConfig* request,
    NPDeviceContext3DConfig* obtain) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device3DInitializeContext(
    const NPDeviceContext3DConfig* config,
    NPDeviceContext3D* context) {
  if (!context)
    return NPERR_GENERIC_ERROR;

#if defined(ENABLE_GPU)
  // Check to see if the GPU plugin is already initialized and fail if so.
  if (nested_delegate_)
    return NPERR_GENERIC_ERROR;

  // Create an instance of the GPU plugin that is responsible for 3D
  // rendering.
  nested_delegate_ = new WebPluginDelegateProxy(kGPUPluginMimeType,
                                                render_view_);

  // TODO(apatrick): should the GPU plugin be attached to plugin_?
  if (nested_delegate_->Initialize(GURL(),
                                   std::vector<std::string>(),
                                   std::vector<std::string>(),
                                   plugin_,
                                   false)) {
    plugin_->SetAcceptsInputEvents(true);

    // Ask the GPU plugin to create a command buffer and return a proxy.
    command_buffer_ = nested_delegate_->CreateCommandBuffer();
    if (command_buffer_) {
      // Initialize the proxy command buffer.
      if (command_buffer_->Initialize(config->commandBufferSize)) {
        // Get the initial command buffer state.
        gpu::CommandBuffer::State state = command_buffer_->GetState();

        // Initialize the 3D context.
        context->reserved = NULL;
        context->waitForProgress = true;
        Buffer ring_buffer = command_buffer_->GetRingBuffer();
        context->commandBuffer = ring_buffer.ptr;
        context->commandBufferSize = state.size;
        context->repaintCallback = NULL;
        Synchronize3DContext(context, state);

        ScheduleHandleRepaint(instance_->npp(), context);

        // Ensure the service knows the window size before rendering anything.
        nested_delegate_->UpdateGeometry(window_rect_, clip_rect_);
#if defined(OS_MACOSX)
        command_buffer_->SetWindowSize(window_rect_.size());
#endif  // OS_MACOSX

        // Make sure the nested delegate shows up in the right place
        // on the page.
        SendNestedDelegateGeometryToBrowser(window_rect_, clip_rect_);

        // Save the implementation information (the CommandBuffer).
        Device3DImpl* impl = new Device3DImpl;
        impl->command_buffer = command_buffer_;
        impl->dynamically_created = false;
        context->reserved = impl;

        return NPERR_NO_ERROR;
      }
    }

    nested_delegate_->DestroyCommandBuffer(command_buffer_);
    command_buffer_ = NULL;
  }

  nested_delegate_->PluginDestroyed();
  nested_delegate_ = NULL;
#endif  // ENABLE_GPU

  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device3DSetStateContext(
    NPDeviceContext3D* context,
    int32 state,
    intptr_t value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device3DGetStateContext(
    NPDeviceContext3D* context,
    int32 state,
    intptr_t* value) {
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::Device3DFlushContext(
    NPP id,
    NPDeviceContext3D* context,
    NPDeviceFlushContextCallbackPtr callback,
    void* user_data) {
  if (!context)
    return NPERR_GENERIC_ERROR;

#if defined(ENABLE_GPU)
  gpu::CommandBuffer::State state;

  if (context->waitForProgress) {
    if (callback) {
      command_buffer_->AsyncFlush(
          context->putOffset,
          method_factory3d_.NewRunnableMethod(
              &WebPluginDelegatePepper::Device3DUpdateState,
              id,
              context,
              callback,
              user_data));
    } else {
      state = command_buffer_->Flush(context->putOffset);
      Synchronize3DContext(context, state);
    }
  } else {
    if (callback) {
      command_buffer_->AsyncGetState(
          method_factory3d_.NewRunnableMethod(
              &WebPluginDelegatePepper::Device3DUpdateState,
              id,
              context,
              callback,
              user_data));
    } else {
      state = command_buffer_->GetState();
      Synchronize3DContext(context, state);
    }
  }
#endif  // ENABLE_GPU
  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DDestroyContext(
    NPDeviceContext3D* context) {
  if (!context)
    return NPERR_GENERIC_ERROR;

#if defined(ENABLE_GPU)
  // Prevent any async flush callbacks from being invoked after the context
  // has been destroyed.
  method_factory3d_.RevokeAll();

  // TODO(apatrick): this will be much simpler when we switch to the new device
  // API. There should be no need for the Device3DImpl and the context will
  // always be destroyed dynamically.
  Device3DImpl* impl = static_cast<Device3DImpl*>(context->reserved);
  bool dynamically_created = impl->dynamically_created;
  delete impl;
  context->reserved = NULL;
  if (dynamically_created) {
    delete context;
  }

  if (nested_delegate_) {
    if (command_buffer_) {
      nested_delegate_->DestroyCommandBuffer(command_buffer_);
      command_buffer_ = NULL;
    }

    nested_delegate_->PluginDestroyed();
    nested_delegate_ = NULL;
  }
#endif  // ENABLE_GPU

  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DCreateBuffer(
    NPDeviceContext3D* context,
    size_t size,
    int32* id) {
  if (!context)
    return NPERR_GENERIC_ERROR;

#if defined(ENABLE_GPU)
  *id = command_buffer_->CreateTransferBuffer(size);
  if (*id < 0)
    return NPERR_GENERIC_ERROR;
#endif  // ENABLE_GPU

  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DDestroyBuffer(
    NPDeviceContext3D* context,
    int32 id) {
  if (!context)
    return NPERR_GENERIC_ERROR;

#if defined(ENABLE_GPU)
  command_buffer_->DestroyTransferBuffer(id);
#endif  // ENABLE_GPU
  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DMapBuffer(
    NPDeviceContext3D* context,
    int32 id,
    NPDeviceBuffer* np_buffer) {
  if (!context)
    return NPERR_GENERIC_ERROR;

#if defined(ENABLE_GPU)
  Buffer gpu_buffer;
  if (id == NP3DCommandBufferId) {
    gpu_buffer = command_buffer_->GetRingBuffer();
  } else {
    gpu_buffer = command_buffer_->GetTransferBuffer(id);
  }

  np_buffer->ptr = gpu_buffer.ptr;
  np_buffer->size = gpu_buffer.size;
  if (!np_buffer->ptr)
    return NPERR_GENERIC_ERROR;
#endif  // ENABLE_GPU

  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DGetNumConfigs(int32* num_configs) {
  if (!num_configs)
    return NPERR_GENERIC_ERROR;

  *num_configs = 1;
  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DGetConfigAttribs(
    int32 config,
    int32* attrib_list) {
  // Only one config available currently.
  if (config != 0)
    return NPERR_GENERIC_ERROR;

  if (attrib_list) {
    for (int32* attrib_pair = attrib_list; *attrib_pair; attrib_pair += 2) {
      switch (attrib_pair[0]) {
        case NP3DAttrib_BufferSize:
          attrib_pair[1] = 32;
          break;
        case NP3DAttrib_AlphaSize:
        case NP3DAttrib_BlueSize:
        case NP3DAttrib_GreenSize:
        case NP3DAttrib_RedSize:
          attrib_pair[1] = 8;
          break;
        case NP3DAttrib_DepthSize:
          attrib_pair[1] = 24;
          break;
        case NP3DAttrib_StencilSize:
          attrib_pair[1] = 8;
          break;
        case NP3DAttrib_SurfaceType:
          attrib_pair[1] = 0;
          break;
        default:
          return NPERR_GENERIC_ERROR;
      }
    }
  }

  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DCreateContext(
    int32 config,
    int32* attrib_list,
    NPDeviceContext3D** context) {
  if (!context)
    return NPERR_GENERIC_ERROR;

  // Only one config available currently.
  if (config != 0)
    return NPERR_GENERIC_ERROR;

  // For now, just use the old API to initialize the context.
  NPDeviceContext3DConfig old_config;
  old_config.commandBufferSize = kDefaultCommandBufferSize;
  if (attrib_list) {
    for (int32* attrib_pair = attrib_list; *attrib_pair; attrib_pair += 2) {
      switch (attrib_pair[0]) {
        case NP3DAttrib_CommandBufferSize:
          old_config.commandBufferSize = attrib_pair[1];
          break;
        default:
          return NPERR_GENERIC_ERROR;
      }
    }
  }

  *context = new NPDeviceContext3D;
  Device3DInitializeContext(&old_config, *context);

  // Flag the context as dynamically created by the browser. TODO(apatrick):
  // take this out when all contexts are dynamically created.
  Device3DImpl* impl = static_cast<Device3DImpl*>((*context)->reserved);
  impl->dynamically_created = true;

  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DRegisterCallback(
    NPP id,
    NPDeviceContext3D* context,
    int32 callback_type,
    NPDeviceGenericCallbackPtr callback,
    void* callback_data) {
  if (!context)
    return NPERR_GENERIC_ERROR;

  switch (callback_type) {
    case NP3DCallback_Repaint:
      context->repaintCallback = reinterpret_cast<NPDeviceContext3DRepaintPtr>(
          callback);
      break;
    default:
      return NPERR_GENERIC_ERROR;
  }

  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::Device3DSynchronizeContext(
    NPP id,
    NPDeviceContext3D* context,
    NPDeviceSynchronizationMode mode,
    const int32* input_attrib_list,
    int32* output_attrib_list,
    NPDeviceSynchronizeContextCallbackPtr callback,
    void* callback_data) {
  if (!context)
    return NPERR_GENERIC_ERROR;

  // Copy input attributes into context.
  if (input_attrib_list) {
    for (const int32* attrib_pair = input_attrib_list;
         *attrib_pair;
         attrib_pair += 2) {
      switch (attrib_pair[0]) {
        case NP3DAttrib_PutOffset:
          context->putOffset = attrib_pair[1];
          break;
        default:
          return NPERR_GENERIC_ERROR;
      }
    }
  }

  // Use existing flush mechanism for now.
  if (mode != NPDeviceSynchronizationMode_Cached) {
    context->waitForProgress = mode == NPDeviceSynchronizationMode_Flush;
    Device3DFlushContext(id, context, callback, callback_data);
  }

  // Copy most recent output attributes from context.
  // To read output attributes after the completion of an asynchronous flush,
  // invoke SynchronizeContext again with mode
  // NPDeviceSynchronizationMode_Cached from the callback function.
  if (output_attrib_list) {
    for (int32* attrib_pair = output_attrib_list;
         *attrib_pair;
         attrib_pair += 2) {
      switch (attrib_pair[0]) {
        case NP3DAttrib_CommandBufferSize:
          attrib_pair[1] = context->commandBufferSize;
          break;
        case NP3DAttrib_GetOffset:
          attrib_pair[1] = context->getOffset;
          break;
        case NP3DAttrib_PutOffset:
          attrib_pair[1] = context->putOffset;
          break;
        case NP3DAttrib_Token:
          attrib_pair[1] = context->token;
          break;
        default:
          return NPERR_GENERIC_ERROR;
      }
    }
  }

  return NPERR_NO_ERROR;
}

NPError WebPluginDelegatePepper::DeviceAudioQueryCapability(int32 capability,
                                                            int32* value) {
  // TODO(neb,cpu) implement QueryCapability
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::DeviceAudioQueryConfig(
    const NPDeviceContextAudioConfig* request,
    NPDeviceContextAudioConfig* obtain) {
  // TODO(neb,cpu) implement QueryConfig
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::DeviceAudioInitializeContext(
    const NPDeviceContextAudioConfig* config,
    NPDeviceContextAudio* context) {

  if (!render_view_) {
    return NPERR_GENERIC_ERROR;
  }

  scoped_ptr<AudioDeviceContext> audio(new AudioDeviceContext());
  NPError status = audio->Initialize(render_view_->audio_message_filter(),
                                     config, context);
  if (NPERR_NO_ERROR == status) {
    context->reserved =
        reinterpret_cast<void *>(audio_contexts_.Add(audio.release()));
  }
  return status;
}

NPError WebPluginDelegatePepper::DeviceAudioSetStateContext(
    NPDeviceContextAudio* context,
    int32 state,
    intptr_t value) {
  // TODO(neb,cpu) implement SetStateContext
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::DeviceAudioGetStateContext(
    NPDeviceContextAudio* context,
    int32 state,
    intptr_t* value) {
  if (state == NPExtensionsReservedStateSharedMemory) {
    if (!context)
      return NPERR_INVALID_PARAM;
    AudioDeviceContext* ctx = audio_contexts_.Lookup(
        reinterpret_cast<intptr_t>(context->reserved));
    if (!ctx)
      return NPERR_INVALID_PARAM;
    *value = reinterpret_cast<intptr_t>(ctx->shared_memory());
    return NPERR_NO_ERROR;
  } else if (state == NPExtensionsReservedStateSharedMemorySize) {
    if (!context)
      return NPERR_INVALID_PARAM;
    AudioDeviceContext* ctx = audio_contexts_.Lookup(
        reinterpret_cast<intptr_t>(context->reserved));
    if (!ctx)
      return NPERR_INVALID_PARAM;
    *value = static_cast<intptr_t>(ctx->shared_memory_size());
    return NPERR_NO_ERROR;
  } else if (state == NPExtensionsReservedStateSyncChannel) {
    if (!context)
      return NPERR_INVALID_PARAM;
    AudioDeviceContext* ctx = audio_contexts_.Lookup(
        reinterpret_cast<intptr_t>(context->reserved));
    if (!ctx)
      return NPERR_INVALID_PARAM;
    *value = reinterpret_cast<intptr_t>(ctx->socket());
    return NPERR_NO_ERROR;
  }
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::DeviceAudioFlushContext(
    NPP id,
    NPDeviceContextAudio* context,
    NPDeviceFlushContextCallbackPtr callback,
    void* user_data) {
  // TODO(neb,cpu) implement FlushContext
  return NPERR_GENERIC_ERROR;
}

NPError WebPluginDelegatePepper::DeviceAudioDestroyContext(
    NPDeviceContextAudio* context) {
  if (!context || !audio_contexts_.Lookup(
      reinterpret_cast<intptr_t>(context->reserved))) {
    return NPERR_INVALID_PARAM;
  }
  audio_contexts_.Remove(reinterpret_cast<intptr_t>(context->reserved));
  memset(context, 0, sizeof(NPDeviceContextAudio));
  return NPERR_NO_ERROR;
}

bool WebPluginDelegatePepper::PrintSupportsPrintExtension() {
  return GetPrintExtensions() != NULL;
}

int WebPluginDelegatePepper::PrintBegin(const gfx::Rect& printable_area,
                                        int printer_dpi) {
  int32 num_pages = 0;
  NPPPrintExtensions* print_extensions = GetPrintExtensions();
  if (print_extensions) {
    NPRect np_printable_area = {0};
    np_printable_area.left = printable_area.x();
    np_printable_area.top = printable_area.y();
    np_printable_area.right = np_printable_area.left + printable_area.width();
    np_printable_area.bottom = np_printable_area.top + printable_area.height();
    if (NPERR_NO_ERROR == print_extensions->printBegin(instance()->npp(),
                                                       &np_printable_area,
                                                       printer_dpi,
                                                       &num_pages)) {
      current_printable_area_ = printable_area;
    }
  }
  return num_pages;
}

bool WebPluginDelegatePepper::PrintPage(int page_number,
                                        WebKit::WebCanvas* canvas) {
#if defined(OS_WIN) || defined(OS_LINUX)
  NPPPrintExtensions* print_extensions = GetPrintExtensions();
  if (!print_extensions)
    return false;

  DCHECK(!current_printable_area_.IsEmpty());

  // Calculate the width and height needed for the raster image.
  NPRect np_printable_area = {0};
  np_printable_area.left = current_printable_area_.x();
  np_printable_area.top = current_printable_area_.y();
  np_printable_area.right =
      current_printable_area_.x() + current_printable_area_.width();
  np_printable_area.bottom =
      current_printable_area_.y() + current_printable_area_.height();
  gfx::Size size_in_pixels;
  if (!CalculatePrintedPageDimensions(page_number, print_extensions,
                                      &size_in_pixels)) {
    return false;
  }

  // Now print the page onto a 2d device context.
  scoped_ptr<Graphics2DDeviceContext> g2d(new Graphics2DDeviceContext(this));
  NPDeviceContext2DConfig config;
  NPDeviceContext2D context;
  gfx::Rect surface_rect(gfx::Point(0, 0), size_in_pixels);
  NPError err = g2d->Initialize(surface_rect, &config, &context);
  if (err != NPERR_NO_ERROR) {
    NOTREACHED();
    return false;
  }
  err = print_extensions->printPageRaster(instance()->npp(), page_number,
                                          &context);
  if (err !=  NPERR_NO_ERROR)
    return false;

  SkBitmap committed;
  committed.setConfig(SkBitmap::kARGB_8888_Config, size_in_pixels.width(),
                      size_in_pixels.height());
  committed.allocPixels();
  err = g2d->Flush(&committed, &context, NULL, instance()->npp(), NULL);
  if (err !=  NPERR_NO_ERROR) {
    NOTREACHED();
    return false;
  }
  // Draw the printed image into the supplied canvas.
  SkIRect src_rect;
  src_rect.set(0, 0, size_in_pixels.width(), size_in_pixels.height());
  SkRect dest_rect;
  dest_rect.set(SkIntToScalar(current_printable_area_.x()),
                SkIntToScalar(current_printable_area_.y()),
                SkIntToScalar(current_printable_area_.x() +
                              current_printable_area_.width()),
                SkIntToScalar(current_printable_area_.y() +
                              current_printable_area_.height()));
  bool draw_to_canvas = true;
#if defined(OS_WIN)
  // Since this is a raster output, the size of the bitmap can be
  // huge (especially at high printer DPIs). On Windows, this can
  // result in a HUGE EMF (on Mac and Linux the output goes to PDF
  // which appears to Flate compress the bitmap). So, if this bitmap
  // is larger than 20 MB, we save the bitmap as a JPEG into the EMF
  // DC. Note: We chose JPEG over PNG because JPEG compression seems
  // way faster (about 4 times faster).
  static const int kCompressionThreshold = 20 * 1024 * 1024;
  if (committed.getSize() > kCompressionThreshold) {
    DrawJPEGToPlatformDC(committed, current_printable_area_, canvas);
    draw_to_canvas = false;
  }
#endif  // OS_WIN

  if (draw_to_canvas)
    canvas->drawBitmapRect(committed, &src_rect, dest_rect);

  return true;
#else  // defined(OS_WIN) || defined(OS_LINUX)
  NOTIMPLEMENTED();
  return false;
#endif  // defined(OS_WIN) || defined(OS_LINUX)
}

void WebPluginDelegatePepper::PrintEnd() {
  NPPPrintExtensions* print_extensions = GetPrintExtensions();
  if (print_extensions)
    print_extensions->printEnd(instance()->npp());
  current_printable_area_ = gfx::Rect();
}

bool WebPluginDelegatePepper::SupportsFind() {
  return GetFindExtensions() != NULL;
}


WebPluginDelegatePepper::WebPluginDelegatePepper(
    const base::WeakPtr<RenderView>& render_view,
    NPAPI::PluginInstance *instance)
    : render_view_(render_view),
      plugin_(NULL),
      instance_(instance),
      nested_delegate_(NULL),
#if defined(ENABLE_GPU)
      command_buffer_(NULL),
#endif
      find_identifier_(-1),
      method_factory3d_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      current_choose_file_callback_(NULL),
      current_choose_file_user_data_(NULL) {
  // For now we keep a window struct, although it isn't used.
  memset(&window_, 0, sizeof(window_));
  // All Pepper plugins are windowless and transparent.
  // TODO(sehr): disable resetting these NPPVs by plugins.
  instance->set_windowless(true);
  instance->set_transparent(true);
}

WebPluginDelegatePepper::~WebPluginDelegatePepper() {
  DestroyInstance();

  if (render_view_)
    render_view_->OnPepperPluginDestroy(this);
}

void WebPluginDelegatePepper::ForwardSetWindow() {
  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();
  window_.type = NPWindowTypeDrawable;
  instance()->NPP_SetWindow(&window_);
}

void WebPluginDelegatePepper::PluginDestroyed() {
  delete this;
}

void WebPluginDelegatePepper::Paint(WebKit::WebCanvas* canvas,
                                    const gfx::Rect& rect) {
  if (nested_delegate_) {
    // TODO(apatrick): The GPU plugin will render to an offscreen render target.
    //    Need to copy it to the screen here.
  } else {
    // Blit from background_context to context.
    if (!committed_bitmap_.isNull()) {
#if defined(OS_MACOSX)
      SkAutoLockPixels lock(committed_bitmap_);

      scoped_cftyperef<CGDataProviderRef> data_provider(
          CGDataProviderCreateWithData(
              NULL, committed_bitmap_.getAddr32(0, 0),
              committed_bitmap_.rowBytes() * committed_bitmap_.height(), NULL));
      scoped_cftyperef<CGImageRef> image(
          CGImageCreate(
              committed_bitmap_.width(), committed_bitmap_.height(),
              8, 32, committed_bitmap_.rowBytes(),
              mac_util::GetSystemColorSpace(),
              kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
              data_provider, NULL, false, kCGRenderingIntentDefault));

      // Flip the transform
      CGContextSaveGState(canvas);
      float window_height = static_cast<float>(CGBitmapContextGetHeight(canvas));
      CGContextTranslateCTM(canvas, 0, window_height);
      CGContextScaleCTM(canvas, 1.0, -1.0);

      CGRect bounds;
      bounds.origin.x = window_rect_.origin().x();
      bounds.origin.y = window_height - window_rect_.origin().y() -
          committed_bitmap_.height();
      bounds.size.width = committed_bitmap_.width();
      bounds.size.height = committed_bitmap_.height();

      CGContextDrawImage(canvas, bounds, image);
      CGContextRestoreGState(canvas);
#else
      gfx::Point origin(window_rect_.origin().x(), window_rect_.origin().y());
      canvas->drawBitmap(committed_bitmap_,
                         SkIntToScalar(window_rect_.origin().x()),
                         SkIntToScalar(window_rect_.origin().y()));
#endif
    }
  }
}

void WebPluginDelegatePepper::Print(gfx::NativeDrawingContext context) {
  NOTIMPLEMENTED();
}

void WebPluginDelegatePepper::InstallMissingPlugin() {
  NOTIMPLEMENTED();
}

void WebPluginDelegatePepper::SetFocus(bool focused) {
  if (!focused)
    return;

  NPPepperEvent npevent;

  npevent.type = NPEventType_Focus;
  npevent.size = sizeof(npevent);
  // TODO(sehr): what timestamp should this have?
  npevent.timeStampSeconds = 0.0;
  // Currently this API only supports gaining focus.
  npevent.u.focus.value = 1;
  instance()->NPP_HandleEvent(&npevent);
}

// Anonymous namespace for functions converting WebInputEvents to NPAPI types.
namespace {
NPEventTypes ConvertEventTypes(WebInputEvent::Type wetype) {
  switch (wetype) {
    case WebInputEvent::MouseDown:
      return NPEventType_MouseDown;
    case WebInputEvent::MouseUp:
      return NPEventType_MouseUp;
    case WebInputEvent::MouseMove:
      return NPEventType_MouseMove;
    case WebInputEvent::MouseEnter:
      return NPEventType_MouseEnter;
    case WebInputEvent::MouseLeave:
      return NPEventType_MouseLeave;
    case WebInputEvent::MouseWheel:
      return NPEventType_MouseWheel;
    case WebInputEvent::RawKeyDown:
      return NPEventType_RawKeyDown;
    case WebInputEvent::KeyDown:
      return NPEventType_KeyDown;
    case WebInputEvent::KeyUp:
      return NPEventType_KeyUp;
    case WebInputEvent::Char:
      return NPEventType_Char;
    case WebInputEvent::Undefined:
    default:
      return NPEventType_Undefined;
  }
}

void BuildKeyEvent(const WebInputEvent* event, NPPepperEvent* npevent) {
  const WebKeyboardEvent* key_event =
      reinterpret_cast<const WebKeyboardEvent*>(event);
  npevent->u.key.modifier = key_event->modifiers;
  npevent->u.key.normalizedKeyCode = key_event->windowsKeyCode;
}

void BuildCharEvent(const WebInputEvent* event, NPPepperEvent* npevent) {
  const WebKeyboardEvent* key_event =
      reinterpret_cast<const WebKeyboardEvent*>(event);
  npevent->u.character.modifier = key_event->modifiers;
  // For consistency, check that the sizes of the texts agree.
  DCHECK(sizeof(npevent->u.character.text) == sizeof(key_event->text));
  DCHECK(sizeof(npevent->u.character.unmodifiedText) ==
         sizeof(key_event->unmodifiedText));
  for (size_t i = 0; i < WebKeyboardEvent::textLengthCap; ++i) {
    npevent->u.character.text[i] = key_event->text[i];
    npevent->u.character.unmodifiedText[i] = key_event->unmodifiedText[i];
  }
}

void BuildMouseEvent(const WebInputEvent* event, NPPepperEvent* npevent) {
  const WebMouseEvent* mouse_event =
      reinterpret_cast<const WebMouseEvent*>(event);
  npevent->u.mouse.modifier = mouse_event->modifiers;
  npevent->u.mouse.button = mouse_event->button;
  npevent->u.mouse.x = mouse_event->x;
  npevent->u.mouse.y = mouse_event->y;
  npevent->u.mouse.clickCount = mouse_event->clickCount;
}

void BuildMouseWheelEvent(const WebInputEvent* event, NPPepperEvent* npevent) {
  const WebMouseWheelEvent* mouse_wheel_event =
      reinterpret_cast<const WebMouseWheelEvent*>(event);
  npevent->u.wheel.modifier = mouse_wheel_event->modifiers;
  npevent->u.wheel.deltaX = mouse_wheel_event->deltaX;
  npevent->u.wheel.deltaY = mouse_wheel_event->deltaY;
  npevent->u.wheel.wheelTicksX = mouse_wheel_event->wheelTicksX;
  npevent->u.wheel.wheelTicksY = mouse_wheel_event->wheelTicksY;
  npevent->u.wheel.scrollByPage = mouse_wheel_event->scrollByPage;
}
}  // namespace

bool WebPluginDelegatePepper::HandleInputEvent(const WebInputEvent& event,
                                               WebCursorInfo* cursor_info) {
  NPPepperEvent npevent;

  npevent.type = ConvertEventTypes(event.type);
  npevent.size = sizeof(npevent);
  npevent.timeStampSeconds = event.timeStampSeconds;
  switch (npevent.type) {
    case NPEventType_Undefined:
      return false;
    case NPEventType_MouseDown:
    case NPEventType_MouseUp:
    case NPEventType_MouseMove:
    case NPEventType_MouseEnter:
    case NPEventType_MouseLeave:
      BuildMouseEvent(&event, &npevent);
      break;
    case NPEventType_MouseWheel:
      BuildMouseWheelEvent(&event, &npevent);
      break;
    case NPEventType_RawKeyDown:
    case NPEventType_KeyDown:
    case NPEventType_KeyUp:
      BuildKeyEvent(&event, &npevent);
      break;
    case NPEventType_Char:
      BuildCharEvent(&event, &npevent);
      break;
    case NPEventType_Minimize:
    case NPEventType_Focus:
    case NPEventType_Device:
      // NOTIMPLEMENTED();
      break;
  }
  return instance()->NPP_HandleEvent(&npevent) != 0;
}

#if defined(ENABLE_GPU)

void WebPluginDelegatePepper::ScheduleHandleRepaint(
    NPP npp, NPDeviceContext3D* context) {
  command_buffer_->SetNotifyRepaintTask(method_factory3d_.NewRunnableMethod(
      &WebPluginDelegatePepper::ForwardHandleRepaint,
      npp,
      context));
}

void WebPluginDelegatePepper::ForwardHandleRepaint(
    NPP npp, NPDeviceContext3D* context) {
  if (context->repaintCallback)
    context->repaintCallback(npp, context);
  ScheduleHandleRepaint(npp, context);
}

void WebPluginDelegatePepper::Synchronize3DContext(
    NPDeviceContext3D* context,
    gpu::CommandBuffer::State state) {
  context->getOffset = state.get_offset;
  context->putOffset = state.put_offset;
  context->token = state.token;
  context->error = static_cast<NPDeviceContext3DError>(state.error);
}

void WebPluginDelegatePepper::Device3DUpdateState(
    NPP npp,
    NPDeviceContext3D* context,
    NPDeviceFlushContextCallbackPtr callback,
    void* user_data) {
  if (command_buffer_) {
    Synchronize3DContext(context, command_buffer_->GetLastState());
    if (callback)
      callback(npp, context, NPERR_NO_ERROR, user_data);
  }
}

#endif  // ENABLE_GPU

void WebPluginDelegatePepper::SendNestedDelegateGeometryToBrowser(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  // Inform the browser about the location of the plugin on the page.
  // It appears that initially the plugin does not get laid out correctly --
  // possibly due to lazy creation of the nested delegate.
  if (!nested_delegate_ ||
      !nested_delegate_->GetPluginWindowHandle() ||
      !render_view_) {
    return;
  }

  webkit_glue::WebPluginGeometry geom;
  geom.window = nested_delegate_->GetPluginWindowHandle();
  geom.window_rect = window_rect;
  geom.clip_rect = clip_rect;
  // Rects_valid must be true for this to work in the Gtk port;
  // hopefully not having the cutout rects will not cause incorrect
  // clipping.
  geom.rects_valid = true;
  geom.visible = true;
  render_view_->DidMovePlugin(geom);
}

bool WebPluginDelegatePepper::CalculatePrintedPageDimensions(
    int page_number,
    NPPPrintExtensions* print_extensions,
    gfx::Size* page_dimensions) {
  int32 width_in_pixels = 0;
  int32 height_in_pixels = 0;
  NPError err = print_extensions->getRasterDimensions(
      instance()->npp(), page_number, &width_in_pixels, &height_in_pixels);
  if (err != NPERR_NO_ERROR)
    return false;

  DCHECK(width_in_pixels && height_in_pixels);
  page_dimensions->SetSize(width_in_pixels, height_in_pixels);
  return true;
}

NPPPrintExtensions* WebPluginDelegatePepper::GetPrintExtensions() {
  NPPPrintExtensions* ret = NULL;
  NPPExtensions* extensions = NULL;
  instance()->NPP_GetValue(NPPVPepperExtensions, &extensions);
  if (extensions && extensions->getPrintExtensions)
    ret = extensions->getPrintExtensions(instance()->npp());
  return ret;
}

NPPFindExtensions* WebPluginDelegatePepper::GetFindExtensions() {
  NPPFindExtensions* ret = NULL;
  NPPExtensions* extensions = NULL;
  instance()->NPP_GetValue(NPPVPepperExtensions, &extensions);
  if (extensions && extensions->getFindExtensions)
    ret = extensions->getFindExtensions(instance()->npp());
  return ret;
}

#if defined(OS_WIN)
bool WebPluginDelegatePepper::DrawJPEGToPlatformDC(
    const SkBitmap& bitmap,
    const gfx::Rect& printable_area,
    WebKit::WebCanvas* canvas) {
  skia::VectorPlatformDevice& device =
      static_cast<skia::VectorPlatformDevice&>(
          canvas->getTopPlatformDevice());
  HDC dc = device.getBitmapDC();
  // TODO(sanjeevr): This is a temporary hack. If we output a JPEG
  // to the EMF, the EnumEnhMetaFile call fails in the browser
  // process. The failure also happens if we output nothing here.
  // We need to investigate the reason for this failure and fix it.
  // In the meantime this temporary hack of drawing an empty
  // rectangle in the DC gets us by.
  Rectangle(dc, 0, 0, 0, 0);

  // Ideally we should add JPEG compression to the VectorPlatformDevice class
  // However, Skia currently has no JPEG compression code and we cannot
  // depend on gfx/jpeg_codec.h in Skia. So we do the compression here.
  SkAutoLockPixels lock(bitmap);
  DCHECK(bitmap.getConfig() == SkBitmap::kARGB_8888_Config);
  const uint32_t* pixels =
      static_cast<const uint32_t*>(bitmap.getPixels());
  std::vector<unsigned char> compressed_image;
  base::TimeTicks start_time = base::TimeTicks::Now();
  bool encoded = gfx::JPEGCodec::Encode(
      reinterpret_cast<const unsigned char*>(pixels),
      gfx::JPEGCodec::FORMAT_BGRA, bitmap.width(), bitmap.height(),
      static_cast<int>(bitmap.rowBytes()), 100, &compressed_image);
  UMA_HISTOGRAM_TIMES("PepperPluginPrint.RasterBitmapCompressTime",
                      base::TimeTicks::Now() - start_time);
  if (!encoded) {
    NOTREACHED();
    return false;
  }
  BITMAPINFOHEADER bmi = {0};
  gfx::CreateBitmapHeader(bitmap.width(), bitmap.height(), &bmi);
  bmi.biCompression = BI_JPEG;
  bmi.biSizeImage = compressed_image.size();
  bmi.biHeight = -bmi.biHeight;
  StretchDIBits(dc, printable_area.x(), printable_area.y(),
                printable_area.width(), printable_area.height(),
                0, 0, bitmap.width(), bitmap.height(),
                &compressed_image.front(),
                reinterpret_cast<const BITMAPINFO*>(&bmi),
                DIB_RGB_COLORS, SRCCOPY);
  return true;
}
#endif  // OS_WIN

