// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "app/text_elider.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/time.h"
#include "base/i18n/time_formatting.h"
#include "gfx/font.h"
#include "printing/page_number.h"
#include "printing/page_overlays.h"
#include "printing/printed_pages_source.h"
#include "printing/printed_page.h"
#include "printing/units.h"
#include "skia/ext/platform_device.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

using base::Time;

namespace {

struct PrintDebugDumpPath {
  PrintDebugDumpPath()
    : enabled(false) {
  }

  bool enabled;
  std::wstring debug_dump_path;
};

Singleton<PrintDebugDumpPath> g_debug_dump_info;

}  // namespace

namespace printing {

PrintedDocument::PrintedDocument(const PrintSettings& settings,
                                 PrintedPagesSource* source,
                                 int cookie)
    : mutable_(source),
      immutable_(settings, source, cookie) {

  // Records the expected page count if a range is setup.
  if (!settings.ranges.empty()) {
    // If there is a range, set the number of page
    for (unsigned i = 0; i < settings.ranges.size(); ++i) {
      const PageRange& range = settings.ranges[i];
      mutable_.expected_page_count_ += range.to - range.from + 1;
    }
  }
}

PrintedDocument::~PrintedDocument() {
}

void PrintedDocument::SetPage(int page_number,
                              NativeMetafile* metafile,
                              double shrink,
                              const gfx::Size& paper_size,
                              const gfx::Rect& page_rect,
                              bool has_visible_overlays) {
  // Notice the page_number + 1, the reason is that this is the value that will
  // be shown. Users dislike 0-based counting.
  scoped_refptr<PrintedPage> page(
      new PrintedPage(page_number + 1,
                      metafile,
                      paper_size,
                      page_rect,
                      has_visible_overlays));
  {
    AutoLock lock(lock_);
    mutable_.pages_[page_number] = page;
    if (mutable_.shrink_factor == 0) {
      mutable_.shrink_factor = shrink;
    } else {
      DCHECK_EQ(mutable_.shrink_factor, shrink);
    }
  }
  DebugDump(*page);
}

bool PrintedDocument::GetPage(int page_number,
                              scoped_refptr<PrintedPage>* page) {
  AutoLock lock(lock_);
  PrintedPages::const_iterator itr = mutable_.pages_.find(page_number);
  if (itr != mutable_.pages_.end()) {
    if (itr->second.get()) {
      *page = itr->second;
      return true;
    }
  }
  return false;
}

bool PrintedDocument::RenderPrintedPageNumber(
    int page_number, gfx::NativeDrawingContext context) {
  scoped_refptr<PrintedPage> page;
  if (!GetPage(page_number, &page))
    return false;
  RenderPrintedPage(*page.get(), context);
  return true;
}

bool PrintedDocument::IsComplete() const {
  AutoLock lock(lock_);
  if (!mutable_.page_count_)
    return false;
  PageNumber page(immutable_.settings_, mutable_.page_count_);
  if (page == PageNumber::npos())
    return false;
  for (; page != PageNumber::npos(); ++page) {
    PrintedPages::const_iterator itr = mutable_.pages_.find(page.ToInt());
    if (itr == mutable_.pages_.end() || !itr->second.get() ||
        !itr->second->native_metafile())
      return false;
  }
  return true;
}

void PrintedDocument::DisconnectSource() {
  AutoLock lock(lock_);
  mutable_.source_ = NULL;
}

uint32 PrintedDocument::MemoryUsage() const {
  std::vector< scoped_refptr<PrintedPage> > pages_copy;
  {
    AutoLock lock(lock_);
    pages_copy.reserve(mutable_.pages_.size());
    PrintedPages::const_iterator end = mutable_.pages_.end();
    for (PrintedPages::const_iterator itr = mutable_.pages_.begin();
         itr != end; ++itr) {
      if (itr->second.get()) {
        pages_copy.push_back(itr->second);
      }
    }
  }
  uint32 total = 0;
  for (size_t i = 0; i < pages_copy.size(); ++i) {
    total += pages_copy[i]->native_metafile()->GetDataSize();
  }
  return total;
}

void PrintedDocument::set_page_count(int max_page) {
  AutoLock lock(lock_);
  DCHECK_EQ(0, mutable_.page_count_);
  mutable_.page_count_ = max_page;
  if (immutable_.settings_.ranges.empty()) {
    mutable_.expected_page_count_ = max_page;
  } else {
    // If there is a range, don't bother since expected_page_count_ is already
    // initialized.
    DCHECK_NE(mutable_.expected_page_count_, 0);
  }
}

int PrintedDocument::page_count() const {
  AutoLock lock(lock_);
  return mutable_.page_count_;
}

int PrintedDocument::expected_page_count() const {
  AutoLock lock(lock_);
  return mutable_.expected_page_count_;
}

void PrintedDocument::PrintHeaderFooter(gfx::NativeDrawingContext context,
                                        const PrintedPage& page,
                                        PageOverlays::HorizontalPosition x,
                                        PageOverlays::VerticalPosition y,
                                        const gfx::Font& font) const {
  const PrintSettings& settings = immutable_.settings_;
  if (!settings.use_overlays || !page.has_visible_overlays()) {
    return;
  }
  const std::wstring& line = settings.overlays.GetOverlay(x, y);
  if (line.empty()) {
    return;
  }
  std::wstring output(PageOverlays::ReplaceVariables(line, *this, page));
  if (output.empty()) {
    // May happen if document name or url is empty.
    return;
  }
  const gfx::Size string_size(font.GetStringWidth(output), font.height());
  gfx::Rect bounding;
  bounding.set_height(string_size.height());
  const gfx::Rect& overlay_area(
      settings.page_setup_device_units().overlay_area());
  // Hard code .25 cm interstice between overlays. Make sure that some space is
  // kept between each headers.
  const int interstice = ConvertUnit(250, kHundrethsMMPerInch,
                                     settings.device_units_per_inch());
  const int max_width = overlay_area.width() / 3 - interstice;
  const int actual_width = std::min(string_size.width(), max_width);
  switch (x) {
    case PageOverlays::LEFT:
      bounding.set_x(overlay_area.x());
      bounding.set_width(max_width);
      break;
    case PageOverlays::CENTER:
      bounding.set_x(overlay_area.x() +
                     (overlay_area.width() - actual_width) / 2);
      bounding.set_width(actual_width);
      break;
    case PageOverlays::RIGHT:
      bounding.set_x(overlay_area.right() - actual_width);
      bounding.set_width(actual_width);
      break;
  }

  DCHECK_LE(bounding.right(), overlay_area.right());

  switch (y) {
    case PageOverlays::BOTTOM:
      bounding.set_y(overlay_area.bottom() - string_size.height());
      break;
    case PageOverlays::TOP:
      bounding.set_y(overlay_area.y());
      break;
  }

  if (string_size.width() > bounding.width()) {
    if (line == PageOverlays::kUrl) {
      output = gfx::ElideUrl(url(), font, bounding.width(), std::wstring());
    } else {
      output = gfx::ElideText(output, font, bounding.width(), false);
    }
  }

  // TODO(stuartmorgan): Factor out this platform-specific part into another
  // method that can be moved into the platform files.
#if defined(OS_WIN)
  // Save the state (again) for the clipping region.
  int saved_state = SaveDC(context);
  DCHECK_NE(saved_state, 0);

  int result = IntersectClipRect(context, bounding.x(), bounding.y(),
                                 bounding.right() + 1, bounding.bottom() + 1);
  DCHECK(result == SIMPLEREGION || result == COMPLEXREGION);
  TextOut(context,
          bounding.x(), bounding.y(),
          output.c_str(),
          static_cast<int>(output.size()));
  int res = RestoreDC(context, saved_state);
  DCHECK_NE(res, 0);
#else  // OS_WIN
  NOTIMPLEMENTED();
#endif  // OS_WIN
}

void PrintedDocument::DebugDump(const PrintedPage& page) {
  if (!g_debug_dump_info->enabled)
    return;

  std::wstring filename;
  filename += date();
  filename += L"_";
  filename += time();
  filename += L"_";
  filename += name();
  filename += L"_";
  filename += StringPrintf(L"%02d", page.page_number());
  filename += L"_.emf";
#if defined(OS_WIN)
  file_util::ReplaceIllegalCharactersInPath(&filename, '_');
#else
  std::string narrow_filename = WideToUTF8(filename);
  file_util::ReplaceIllegalCharactersInPath(&narrow_filename, '_');
  filename = UTF8ToWide(narrow_filename);
#endif
  FilePath path = FilePath::FromWStringHack(
      g_debug_dump_info->debug_dump_path);
#if defined(OS_WIN)
  page.native_metafile()->SaveTo(path.Append(filename).ToWStringHack());
#else  // OS_WIN
  NOTIMPLEMENTED();
#endif  // OS_WIN
}

void PrintedDocument::set_debug_dump_path(const std::wstring& debug_dump_path) {
  g_debug_dump_info->enabled = !debug_dump_path.empty();
  g_debug_dump_info->debug_dump_path = debug_dump_path;
}

const std::wstring& PrintedDocument::debug_dump_path() {
  return g_debug_dump_info->debug_dump_path;
}

PrintedDocument::Mutable::Mutable(PrintedPagesSource* source)
    : source_(source),
      expected_page_count_(0),
      page_count_(0),
      shrink_factor(0) {
}

PrintedDocument::Immutable::Immutable(const PrintSettings& settings,
                                      PrintedPagesSource* source,
                                      int cookie)
    : settings_(settings),
      source_message_loop_(MessageLoop::current()),
      name_(source->RenderSourceName()),
      url_(source->RenderSourceUrl()),
      cookie_(cookie) {
  // Setup the document's date.
#if defined(OS_WIN)
  // On Windows, use the native time formatting for printing.
  SYSTEMTIME systemtime;
  GetLocalTime(&systemtime);
  date_ = win_util::FormatSystemDate(systemtime, std::wstring());
  time_ = win_util::FormatSystemTime(systemtime, std::wstring());
#else  // OS_WIN
  Time now = Time::Now();
  date_ = base::TimeFormatShortDateNumeric(now);
  time_ = base::TimeFormatTimeOfDay(now);
#endif  // OS_WIN
}

}  // namespace printing
