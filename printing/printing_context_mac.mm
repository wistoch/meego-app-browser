// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context.h"

#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/sys_string_conversions.h"

namespace printing {

PrintingContext::PrintingContext()
    : context_(NULL),
      print_info_(nil),
#ifndef NDEBUG
      page_number_(-1),
#endif
      dialog_box_dismissed_(false),
      in_print_job_(false),
      abort_printing_(false) {
}

PrintingContext::~PrintingContext() {
  ResetSettings();
}


PrintingContext::Result PrintingContext::AskUserForSettings(
    gfx::NativeView parent_view, int max_pages, bool has_selection) {
  DCHECK([NSThread isMainThread]);

  // We deliberately don't feed max_pages into the dialog, because setting
  // NSPrintLastPage makes the print dialog pre-select the option to only print
  // a range.

  // TODO(stuartmorgan): implement 'print selection only' (probably requires
  // adding a new custom view to the panel on 10.5; 10.6 has
  // NSPrintPanelShowsPrintSelection).
  NSPrintPanel* panel = [NSPrintPanel printPanel];
  NSPrintInfo* printInfo = [NSPrintInfo sharedPrintInfo];

  NSPrintPanelOptions options = [panel options];
  options |= NSPrintPanelShowsPaperSize;
  options |= NSPrintPanelShowsOrientation;
  options |= NSPrintPanelShowsScaling;
  [panel setOptions:options];

  if (parent_view) {
    NSString* job_title = [[parent_view window] title];
    if (job_title) {
      PMPrintSettings printSettings =
          (PMPrintSettings)[printInfo PMPrintSettings];
      PMPrintSettingsSetJobName(printSettings, (CFStringRef)job_title);
      [printInfo updateFromPMPrintSettings];
    }
  }

  // TODO(stuartmorgan): We really want a tab sheet here, not a modal window.
  // Will require restructuring the PrintingContext API to use a callback.
  NSInteger selection = [panel runModalWithPrintInfo:printInfo];
  if (selection != NSOKButton) {
    return CANCEL;
  }

  ParsePrintInfo([panel printInfo]);
  return OK;
}

PrintingContext::Result PrintingContext::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  ParsePrintInfo([NSPrintInfo sharedPrintInfo]);

  return OK;
}

void PrintingContext::ParsePrintInfo(NSPrintInfo* print_info) {
  ResetSettings();
  print_info_ = [print_info retain];
  PageRanges page_ranges;
  NSDictionary* print_info_dict = [print_info_ dictionary];
  if (![[print_info_dict objectForKey:NSPrintAllPages] boolValue]) {
    PageRange range;
    range.from = [[print_info_dict objectForKey:NSPrintFirstPage] intValue] - 1;
    range.to = [[print_info_dict objectForKey:NSPrintLastPage] intValue] - 1;
    page_ranges.push_back(range);
  }
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_ PMPageFormat]);
  PMPrinter printer;
  PMSessionGetCurrentPrinter(print_session, &printer);

  settings_.Init(printer, page_format, page_ranges, false);
}

PrintingContext::Result PrintingContext::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);
  settings_ = settings;

  NOTIMPLEMENTED();

  return FAILED;
}

void PrintingContext::ResetSettings() {
  [print_info_ autorelease];
  print_info_ = nil;
  settings_.Clear();
#ifndef NDEBUG
  page_number_ = -1;
#endif
  dialog_box_dismissed_ = false;
  abort_printing_ = false;
  in_print_job_ = false;
  context_ = NULL;
}

PrintingContext::Result PrintingContext::NewDocument(
    const std::wstring& document_name) {
  DCHECK(!in_print_job_);

  in_print_job_ = true;

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_ PMPrintSettings]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_ PMPageFormat]);

  scoped_cftyperef<CFStringRef> job_title(
      base::SysWideToCFStringRef(document_name));
  PMPrintSettingsSetJobName(print_settings, job_title.get());

  OSStatus status = PMSessionBeginCGDocumentNoDialog(print_session,
                                                     print_settings,
                                                     page_format);
  if (status != noErr)
    return OnError();

#ifndef NDEBUG
  page_number_ = 0;
#endif

  return OK;
}

PrintingContext::Result PrintingContext::NewPage() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);
  DCHECK(!context_);

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_ PMPageFormat]);
  OSStatus status;
  status = PMSessionBeginPageNoDialog(print_session, page_format, NULL);
  if (status != noErr)
    return OnError();
  status = PMSessionGetCGGraphicsContext(print_session, &context_);
  if (status != noErr)
    return OnError();

#ifndef NDEBUG
  ++page_number_;
#endif

  return OK;
}

PrintingContext::Result PrintingContext::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);
  DCHECK(context_);

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  OSStatus status = PMSessionEndPageNoDialog(print_session);
  if (status != noErr)
    OnError();
  context_ = NULL;

  return OK;
}

PrintingContext::Result PrintingContext::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  OSStatus status = PMSessionEndDocumentNoDialog(print_session);
  if (status != noErr)
    OnError();

  ResetSettings();
  return OK;
}

void PrintingContext::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
  context_ = NULL;

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  PMSessionEndPageNoDialog(print_session);
}

void PrintingContext::DismissDialog() {
  NOTIMPLEMENTED();
}

PrintingContext::Result PrintingContext::OnError() {
  ResetSettings();
  return abort_printing_ ? CANCEL : FAILED;
}

}  // namespace printing
