// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_root_view.h"

#include "app/drag_drop_types.h"
#include "app/os_exchange_data.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/tabs/tab_strip_wrapper.h"

BrowserRootView::BrowserRootView(BrowserView* browser_view,
                                 views::Widget* widget)
    : views::RootView(widget),
      browser_view_(browser_view),
      forwarding_to_tab_strip_(false) {
}

bool BrowserRootView::GetDropFormats(
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats) {
  if (tabstrip() && tabstrip()->GetView()->IsVisible() &&
      !tabstrip()->IsAnimating()) {
    *formats = OSExchangeData::URL | OSExchangeData::STRING;
    return true;
  }
  return false;
}

bool BrowserRootView::AreDropTypesRequired() {
  return true;
}

bool BrowserRootView::CanDrop(const OSExchangeData& data) {
  if (!tabstrip() || !tabstrip()->GetView()->IsVisible() ||
      tabstrip()->IsAnimating())
    return false;

  // If there is a URL, we'll allow the drop.
  if (data.HasURL())
    return true;

  // If there isn't a URL, see if we can 'paste and go'.
  return GetPasteAndGoURL(data, NULL);
}

void BrowserRootView::OnDragEntered(const views::DropTargetEvent& event) {
  if (ShouldForwardToTabStrip(event)) {
    forwarding_to_tab_strip_ = true;
    scoped_ptr<views::DropTargetEvent> mapped_event(
        MapEventToTabStrip(event, event.GetData()));
    tabstrip()->GetView()->OnDragEntered(*mapped_event.get());
  }
}

int BrowserRootView::OnDragUpdated(const views::DropTargetEvent& event) {
  if (ShouldForwardToTabStrip(event)) {
    scoped_ptr<views::DropTargetEvent> mapped_event(
        MapEventToTabStrip(event, event.GetData()));
    if (!forwarding_to_tab_strip_) {
      tabstrip()->GetView()->OnDragEntered(*mapped_event.get());
      forwarding_to_tab_strip_ = true;
    }
    return tabstrip()->GetView()->OnDragUpdated(*mapped_event.get());
  } else if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    tabstrip()->GetView()->OnDragExited();
  }
  return DragDropTypes::DRAG_NONE;
}

void BrowserRootView::OnDragExited() {
  if (forwarding_to_tab_strip_) {
    forwarding_to_tab_strip_ = false;
    tabstrip()->GetView()->OnDragExited();
  }
}

int BrowserRootView::OnPerformDrop(const views::DropTargetEvent& event) {
  if (!forwarding_to_tab_strip_)
    return DragDropTypes::DRAG_NONE;

  // Extract the URL and create a new OSExchangeData containing the URL. We do
  // this as the TabStrip doesn't know about the autocomplete edit and neeeds
  // to know about it to handle 'paste and go'.
  GURL url;
  std::wstring title;
  OSExchangeData mapped_data;
  if (!event.GetData().GetURLAndTitle(&url, &title) || !url.is_valid()) {
    // The url isn't valid. Use the paste and go url.
    if (GetPasteAndGoURL(event.GetData(), &url))
      mapped_data.SetURL(url, std::wstring());
    // else case: couldn't extract a url or 'paste and go' url. This ends up
    // passing through an OSExchangeData with nothing in it. We need to do this
    // so that the tab strip cleans up properly.
  } else {
    mapped_data.SetURL(url, std::wstring());
  }
  forwarding_to_tab_strip_ = false;
  scoped_ptr<views::DropTargetEvent> mapped_event(
      MapEventToTabStrip(event, mapped_data));
  return tabstrip()->GetView()->OnPerformDrop(*mapped_event);
}

bool BrowserRootView::ShouldForwardToTabStrip(
    const views::DropTargetEvent& event) {
  if (!tabstrip()->GetView()->IsVisible())
    return false;

  // Allow the drop as long as the mouse is over the tabstrip or vertically
  // before it.
  gfx::Point tab_loc_in_host;
  ConvertPointToView(tabstrip()->GetView(), this, &tab_loc_in_host);
  return event.y() < tab_loc_in_host.y() + tabstrip()->GetView()->height();
}

views::DropTargetEvent* BrowserRootView::MapEventToTabStrip(
    const views::DropTargetEvent& event,
    const OSExchangeData& data) {
  gfx::Point tab_strip_loc(event.location());
  ConvertPointToView(this, tabstrip()->GetView(), &tab_strip_loc);
  return new views::DropTargetEvent(data, tab_strip_loc.x(),
                                    tab_strip_loc.y(),
                                    event.GetSourceOperations());
}

TabStripWrapper* BrowserRootView::tabstrip() const {
  return browser_view_->tabstrip();
}

bool BrowserRootView::GetPasteAndGoURL(const OSExchangeData& data,
                                       GURL* url) {
  if (!data.HasString())
    return false;

  LocationBar* location_bar = browser_view_->GetLocationBar();
  if (!location_bar)
    return false;

  AutocompleteEditView* edit = location_bar->location_entry();
  if (!edit)
    return false;

  std::wstring text;
  if (!data.GetString(&text) || text.empty() ||
      !edit->model()->CanPasteAndGo(text)) {
    return false;
  }
  if (url)
    *url = edit->model()->paste_and_go_url();
  return true;
}
