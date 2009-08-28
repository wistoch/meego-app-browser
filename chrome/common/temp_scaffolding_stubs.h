// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
#define CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_

// This file provides declarations and stub definitions for classes we encouter
// during the porting effort. It is not meant to be permanent, and classes will
// be removed from here as they are fleshed out more completely.

#include <string>

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/size.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "build/build_config.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

class BookmarkContextMenu;
class BookmarkNode;
class Browser;
class CommandLine;
class DownloadItem;
class MessageLoop;
class NavigationController;
class ProcessSingleton;
class Profile;
class RenderViewHostDelegate;
class SiteInstance;
class URLRequest;
class TabContents;
struct ViewHostMsg_DidPrintPage_Params;

namespace gfx {
class Rect;
class Widget;
}

namespace IPC {
class Message;
}

//---------------------------------------------------------------------------
// These stubs are for Browser_main()

void InstallJankometer(const CommandLine&);

//---------------------------------------------------------------------------
// These stubs are for BrowserProcessImpl

class ViewMsg_Print_Params;

// Printing is all (obviously) not implemented.
// http://code.google.com/p/chromium/issues/detail?id=9847
namespace printing {

class PrintViewManager : public RenderViewHostDelegate::Printing {
 public:
  PrintViewManager(TabContents&) { }
  void Stop() { NOTIMPLEMENTED(); }
  void Destroy() { }
  bool OnRenderViewGone(RenderViewHost*) {
    NOTIMPLEMENTED();
    return true;  // Assume for now that all renderer crashes are important.
  }

  // RenderViewHostDelegate::Printing implementation.
  virtual void DidGetPrintedPagesCount(int cookie, int number_pages) {
    NOTIMPLEMENTED();
  }
  virtual void DidPrintPage(const ViewHostMsg_DidPrintPage_Params& params) {
    NOTIMPLEMENTED();
  }
};

class PrintingContext {
 public:
  enum Result { OK, CANCEL, FAILED };
};

class PrintSettings {
 public:
  void RenderParams(ViewMsg_Print_Params* params) const { NOTIMPLEMENTED(); }
  int dpi() const { NOTIMPLEMENTED(); return 92; }
};

class PrinterQuery : public base::RefCountedThreadSafe<PrinterQuery> {
 public:
  enum GetSettingsAskParam {
    DEFAULTS,
    ASK_USER,
  };

  void GetSettings(GetSettingsAskParam ask_user_for_settings,
                   int parent_window,
                   int expected_page_count,
                   bool has_selection,
                   CancelableTask* callback) { NOTIMPLEMENTED(); }
  PrintingContext::Result last_status() { return PrintingContext::FAILED; }
  const PrintSettings& settings() { NOTIMPLEMENTED(); return settings_; }
  int cookie() { NOTIMPLEMENTED(); return 0; }
  void StopWorker() { NOTIMPLEMENTED(); }

 private:
  PrintSettings settings_;
};

class PrintJobManager {
 public:
  void OnQuit() { }
  void PopPrinterQuery(int document_cookie, scoped_refptr<PrinterQuery>* job) {
    NOTIMPLEMENTED();
  }
  void QueuePrinterQuery(PrinterQuery* job) { NOTIMPLEMENTED(); }
};

}  // namespace printing

//---------------------------------------------------------------------------
// These stubs are for Browser

#if !defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
namespace download_util {
void DragDownload(const DownloadItem* download,
                  SkBitmap* icon,
                  gfx::NativeView view);
}  // namespace download_util
#endif

#if defined(OS_MACOSX)
class DockInfo {
 public:
  bool GetNewWindowBounds(gfx::Rect*, bool*) const;
  void AdjustOtherWindowBounds() const;
};
#else

#endif

//---------------------------------------------------------------------------
// These stubs are for TabContents

class RepostFormWarningDialog {
 public:
  static void RunRepostFormWarningDialog(NavigationController*) { }
  virtual ~RepostFormWarningDialog() { }
};

class BaseDragSource {
};

#endif  // CHROME_COMMON_TEMP_SCAFFOLDING_STUBS_H_
