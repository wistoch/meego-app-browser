// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_message_filter.h"

#include <fcntl.h>
#include <map>

#include "app/x11_util.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "chrome/browser/browser_thread.h"
#if defined(TOOLKIT_GTK)
#include "chrome/browser/printing/print_dialog_gtk.h"
#else
#include "chrome/browser/printing/print_dialog_cloud.h"
#endif
#include "chrome/common/chrome_paths.h"
#include "chrome/common/render_messages.h"
#include "gfx/gtk_native_view_id_manager.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/x11/WebScreenInfoFactory.h"
#include "ui/base/clipboard/clipboard.h"

using WebKit::WebScreenInfo;
using WebKit::WebScreenInfoFactory;

namespace {

typedef std::map<int, FilePath> SequenceToPathMap;

struct PrintingSequencePathMap {
  SequenceToPathMap map;
  int sequence;
};

// No locking, only access on the FILE thread.
static base::LazyInstance<PrintingSequencePathMap>
    g_printing_file_descriptor_map(base::LINKER_INITIALIZED);

}  // namespace

// We get null window_ids passed into the two functions below; please see
// http://crbug.com/9060 for more details.

// Called on the BACKGROUND_X11 thread.
void RenderMessageFilter::DoOnGetScreenInfo(gfx::NativeViewId view,
                                            IPC::Message* reply_msg) {
  Display* display = x11_util::GetSecondaryDisplay();
  int screen = x11_util::GetDefaultScreen(display);
  WebScreenInfo results = WebScreenInfoFactory::screenInfo(display, screen);
  ViewHostMsg_GetScreenInfo::WriteReplyParams(reply_msg, results);
  Send(reply_msg);
}

// Called on the BACKGROUND_X11 thread.
void RenderMessageFilter::DoOnGetWindowRect(gfx::NativeViewId view,
                                            IPC::Message* reply_msg) {
  // This is called to get the x, y offset (in screen coordinates) of the given
  // view and its width and height.
  gfx::Rect rect;
  XID window;

  AutoLock lock(GtkNativeViewManager::GetInstance()->unrealize_lock());
  if (GtkNativeViewManager::GetInstance()->GetXIDForId(&window, view)) {
    if (window) {
      int x, y;
      unsigned width, height;
      if (x11_util::GetWindowGeometry(&x, &y, &width, &height, window))
        rect = gfx::Rect(x, y, width, height);
    }
  }

  ViewHostMsg_GetWindowRect::WriteReplyParams(reply_msg, rect);
  Send(reply_msg);
}

// Return the top-level parent of the given window. Called on the
// BACKGROUND_X11 thread.
static XID GetTopLevelWindow(XID window) {
  bool parent_is_root;
  XID parent_window;

  if (!x11_util::GetWindowParent(&parent_window, &parent_is_root, window))
    return 0;
  if (parent_is_root)
    return window;

  return GetTopLevelWindow(parent_window);
}

// Called on the BACKGROUND_X11 thread.
void RenderMessageFilter::DoOnGetRootWindowRect(gfx::NativeViewId view,
                                                IPC::Message* reply_msg) {
  // This is called to get the screen coordinates and size of the browser
  // window itself.
  gfx::Rect rect;
  XID window;

  AutoLock lock(GtkNativeViewManager::GetInstance()->unrealize_lock());
  if (GtkNativeViewManager::GetInstance()->GetXIDForId(&window, view)) {
    if (window) {
      const XID toplevel = GetTopLevelWindow(window);
      if (toplevel) {
        int x, y;
        unsigned width, height;
        if (x11_util::GetWindowGeometry(&x, &y, &width, &height, toplevel))
          rect = gfx::Rect(x, y, width, height);
      }
    }
  }

  ViewHostMsg_GetRootWindowRect::WriteReplyParams(reply_msg, rect);
  Send(reply_msg);
}

// Called on the UI thread.
void RenderMessageFilter::DoOnClipboardIsFormatAvailable(
    ui::Clipboard::FormatType format, ui::Clipboard::Buffer buffer,
    IPC::Message* reply_msg) {
  const bool result = GetClipboard()->IsFormatAvailable(format, buffer);

  ViewHostMsg_ClipboardIsFormatAvailable::WriteReplyParams(reply_msg, result);
  Send(reply_msg);
}

// Called on the UI thread.
void RenderMessageFilter::DoOnClipboardReadText(ui::Clipboard::Buffer buffer,
                                                IPC::Message* reply_msg) {
  string16 result;
  GetClipboard()->ReadText(buffer, &result);

  ViewHostMsg_ClipboardReadText::WriteReplyParams(reply_msg, result);
  Send(reply_msg);
}

// Called on the UI thread.
void RenderMessageFilter::DoOnClipboardReadAsciiText(
    ui::Clipboard::Buffer buffer, IPC::Message* reply_msg) {
  std::string result;
  GetClipboard()->ReadAsciiText(buffer, &result);

  ViewHostMsg_ClipboardReadAsciiText::WriteReplyParams(reply_msg, result);
  Send(reply_msg);
}

// Called on the UI thread.
void RenderMessageFilter::DoOnClipboardReadHTML(ui::Clipboard::Buffer buffer,
                                                IPC::Message* reply_msg) {
  std::string src_url_str;
  string16 markup;
  GetClipboard()->ReadHTML(buffer, &markup, &src_url_str);
  const GURL src_url = GURL(src_url_str);

  ViewHostMsg_ClipboardReadHTML::WriteReplyParams(reply_msg, markup, src_url);
  Send(reply_msg);
}

// Called on the UI thread.
void RenderMessageFilter::DoOnClipboardReadAvailableTypes(
    ui::Clipboard::Buffer buffer, IPC::Message* reply_msg) {
  Send(reply_msg);
}

// Called on the UI thread.
void RenderMessageFilter::DoOnClipboardReadData(ui::Clipboard::Buffer buffer,
                                                const string16& type,
                                                IPC::Message* reply_msg) {
  Send(reply_msg);
}
// Called on the UI thread.
void RenderMessageFilter::DoOnClipboardReadFilenames(
    ui::Clipboard::Buffer buffer, IPC::Message* reply_msg) {
  Send(reply_msg);
}

void RenderMessageFilter::DoOnAllocateTempFileForPrinting(
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FileDescriptor temp_file_fd(-1, false);
  bool allow_print =
#if defined(TOOLKIT_GTK)
    !PrintDialogGtk::DialogShowing();
#else
    true;
#endif
  FilePath path;
  SequenceToPathMap* map = &g_printing_file_descriptor_map.Get().map;
  const int sequence_number = g_printing_file_descriptor_map.Get().sequence++;

  if (allow_print &&
      file_util::CreateTemporaryFile(&path)) {
    int fd = open(path.value().c_str(), O_WRONLY);
    if (fd >= 0) {
      SequenceToPathMap::iterator it = map->find(sequence_number);
      if (it != map->end()) {
        NOTREACHED() << "Sequence number already in use. seq=" <<
            sequence_number;
      } else {
        (*map)[sequence_number] = path;
        temp_file_fd.fd = fd;
        temp_file_fd.auto_close = true;
      }
    }
  }

  ViewHostMsg_AllocateTempFileForPrinting::WriteReplyParams(
      reply_msg, temp_file_fd, sequence_number);
  Send(reply_msg);
}

void RenderMessageFilter::DoOnTempFileForPrintingWritten(int sequence_number) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SequenceToPathMap* map = &g_printing_file_descriptor_map.Get().map;
  SequenceToPathMap::iterator it = map->find(sequence_number);
  if (it == map->end()) {
    NOTREACHED() << "Got a sequence that we didn't pass to the "
                    "renderer: " << sequence_number;
    return;
  }

#if defined(TOOLKIT_GTK)
  PrintDialogGtk::CreatePrintDialogForPdf(it->second);
#else
  if (cloud_print_enabled_)
    PrintDialogCloud::CreatePrintDialogForPdf(it->second);
  else
    NOTIMPLEMENTED();
#endif

  // Erase the entry in the map.
  map->erase(it);
}

// Called on the IO thread.
void RenderMessageFilter::OnGetScreenInfo(gfx::NativeViewId view,
                                          IPC::Message* reply_msg) {
   BrowserThread::PostTask(
      BrowserThread::BACKGROUND_X11, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnGetScreenInfo, view, reply_msg));
}

// Called on the IO thread.
void RenderMessageFilter::OnGetWindowRect(gfx::NativeViewId view,
                                          IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::BACKGROUND_X11, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnGetWindowRect, view, reply_msg));
}

// Called on the IO thread.
void RenderMessageFilter::OnGetRootWindowRect(gfx::NativeViewId view,
                                              IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::BACKGROUND_X11, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnGetRootWindowRect, view, reply_msg));
}

// Called on the IO thread.
void RenderMessageFilter::OnClipboardIsFormatAvailable(
    ui::Clipboard::FormatType format, ui::Clipboard::Buffer buffer,
    IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardIsFormatAvailable, format,
          buffer, reply_msg));
}

// Called on the IO thread.
void RenderMessageFilter::OnClipboardReadText(ui::Clipboard::Buffer buffer,
                                              IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardReadText, buffer,
          reply_msg));
}

// Called on the IO thread.
void RenderMessageFilter::OnClipboardReadAsciiText(ui::Clipboard::Buffer buffer,
                                                   IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardReadAsciiText, buffer,
          reply_msg));
}

// Called on the IO thread.
void RenderMessageFilter::OnClipboardReadHTML(ui::Clipboard::Buffer buffer,
                                              IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardReadHTML, buffer,
          reply_msg));
}

// Called on the IO thread.
void RenderMessageFilter::OnClipboardReadAvailableTypes(
    ui::Clipboard::Buffer buffer, IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardReadAvailableTypes, buffer,
          reply_msg));
}

// Called on the IO thread.
void RenderMessageFilter::OnClipboardReadData(ui::Clipboard::Buffer buffer,
                                              const string16& type,
                                              IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardReadData, buffer, type,
          reply_msg));
}

// Called on the IO thread.
void RenderMessageFilter::OnClipboardReadFilenames(
    ui::Clipboard::Buffer buffer, IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnClipboardReadFilenames, buffer,
          reply_msg));
}

void RenderMessageFilter::OnAllocateTempFileForPrinting(
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnAllocateTempFileForPrinting,
          reply_msg));
}

void RenderMessageFilter::OnTempFileForPrintingWritten(int sequence_number) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::DoOnTempFileForPrintingWritten,
          sequence_number));
}
