// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_shelf_controller.h"

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#include "chrome/browser/cocoa/download_item_controller.h"
#include "chrome/browser/cocoa/download_shelf_mac.h"
#import "chrome/browser/cocoa/download_shelf_view.h"
#include "grit/generated_resources.h"

namespace {

// Max number of download views we'll contain. Any time a view is added and
// we already have this many download views, one is removed.
const size_t kMaxDownloadItemCount = 16;

// Border padding of a download item.
const int kDownloadItemBorderPadding = 3;

// Width of a download item, must match width in DownloadItem.xib.
const int kDownloadItemWidth = 200;

// Height of a download item, must match height in DownloadItem.xib.
const int kDownloadItemHeight = 34;

// Horizontal padding between two download items.
const int kDownloadItemPadding = 10;

// Duration for the open-new-leftmost-item animation, in seconds.
const NSTimeInterval kDownloadItemOpenDuration = 0.8;

}  // namespace

@interface DownloadShelfController(Private)
- (void)applyContentAreaOffset:(BOOL)apply;
- (void)positionBar;
- (void)showDownloadShelf:(BOOL)enable;
- (void)resizeDownloadLinkToFit;
@end


@implementation DownloadShelfController

- (id)initWithBrowser:(Browser*)browser
          contentArea:(NSView*)content {
  if ((self = [super initWithNibName:@"DownloadShelf"
                              bundle:mac_util::MainAppBundle()])) {
    contentArea_ = content;
    shelfHeight_ = [[self view] bounds].size.height;

    [self positionBar];
    [[[contentArea_ window] contentView] addSubview:[self view]];

    downloadItemControllers_.reset([[NSMutableArray alloc] init]);

    // This calls show:, so it needs to be last.
    bridge_.reset(new DownloadShelfMac(browser, self));
  }
  return self;
}

- (void)awakeFromNib {
  // Initialize "Show all downloads" link.

  scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy]);
  // TODO(thakis): left-align for RTL languages?
  [paragraphStyle.get() setAlignment:NSRightTextAlignment];

  NSDictionary* linkAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
      @"", NSLinkAttributeName,
      [NSCursor pointingHandCursor], NSCursorAttributeName,
      paragraphStyle.get(), NSParagraphStyleAttributeName,
      nil];
  NSString* text =
      base::SysWideToNSString(l10n_util::GetString(IDS_SHOW_ALL_DOWNLOADS));
  scoped_nsobject<NSAttributedString> linkText([[NSAttributedString alloc]
      initWithString:text attributes:linkAttributes]);

  [[showAllDownloadsLink_ textStorage] setAttributedString:linkText.get()];
  [showAllDownloadsLink_ setDelegate:self];

  [self resizeDownloadLinkToFit];
}

- (void)dealloc {
  for (DownloadItemController* itemController
      in downloadItemControllers_.get()) {
    [[NSNotificationCenter defaultCenter] removeObserver:itemController];
  }
  [super dealloc];
}

- (void)resizeDownloadLinkToFit {
  // Get width required by localized download link text.
  // http://developer.apple.com/documentation/Cocoa/Conceptual/TextLayout/Tasks/StringHeight.html
  [[showAllDownloadsLink_ textContainer] setLineFragmentPadding:0.0];
  (void)[[showAllDownloadsLink_ layoutManager]glyphRangeForTextContainer:
      [showAllDownloadsLink_ textContainer]];
  NSRect textRect = [[showAllDownloadsLink_ layoutManager]
      usedRectForTextContainer:[showAllDownloadsLink_ textContainer]];

  int offsetX = [showAllDownloadsLink_ frame].size.width - textRect.size.width;

  // Fit link itself.
  NSRect linkFrame = [linkContainer_ frame];
  linkFrame.origin.x += offsetX;
  linkFrame.size.width -= offsetX;
  [linkContainer_ setFrame:linkFrame];
  [linkContainer_ setNeedsDisplay:YES];

  // Move image.
  NSRect imageFrame = [image_ frame];
  imageFrame.origin.x += offsetX;
  [image_ setFrame:imageFrame];
  [image_ setNeedsDisplay:YES];

  // Change item container size.
  NSRect itemFrame = [itemContainerView_ frame];
  itemFrame.size.width += offsetX;
  [itemContainerView_ setFrame:itemFrame];
  [itemContainerView_ setNeedsDisplay:YES];
}

- (BOOL)textView:(NSTextView *)aTextView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  bridge_->browser()->ShowDownloadsTab();
  return YES;
}

// Initializes the download shelf at the bottom edge of |contentArea_|.
- (void)positionBar {
  // Set the bar's height to zero and position it at the bottom of the content
  // area, within the window's content view (as opposed to the tab strip, which
  // is a sibling). We'll enlarge it and slide the content area up when we need
  // to show this strip.
  NSRect contentFrame = [contentArea_ frame];
  NSRect barFrame = NSMakeRect(0, 0, contentFrame.size.width, shelfHeight_);
  [[self view] setFrame:barFrame];
}

// Called when the contentArea's frame changes.  Enlarge the view to stay with
// the bottom of the contentArea.
- (void)resizeDownloadShelf {
  NSRect barFrame = [[self view] frame];
  barFrame.origin.y = 0;
  barFrame.size.height = NSMinY([contentArea_ frame]);
  [[self view] setFrame:barFrame];
}

- (void)remove:(DownloadItemController*)download {
  // Look for the download in our controller array and remove it. This will
  // explicity release it so that it removes itself as an Observer of the
  // DownloadItem. We don't want to wait for autorelease since the DownloadItem
  // we are observing will likely be gone by then.
  [[NSNotificationCenter defaultCenter] removeObserver:download];
  [[download view] removeFromSuperview];
  [downloadItemControllers_ removeObject:download];

  // TODO(thakis): Need to relayout the remaining item views here (
  // crbug.com/17831 ).

  // Check to see if we have any downloads remaining and if not, hide the shelf.
  if (![downloadItemControllers_ count])
    [self showDownloadShelf:NO];
}

// We need to explicitly release our download controllers here since they need
// to remove themselves as observers before the remaining shutdown happens.
- (void)exiting {
  downloadItemControllers_.reset();
}

// Show or hide the bar based on the value of |enable|. Handles animating the
// resize of the content view.
- (void)showDownloadShelf:(BOOL)enable {
  if ([self isVisible] == enable)
    return;

  contentAreaHasOffset_ = enable;
  [[self view] setHidden:enable ? NO : YES];
  [self applyContentAreaOffset:enable];

  barIsVisible_ = enable;
}

// Apply a contents box offset to make (or remove) room for the download shelf.
// If apply is YES, always make room (the contentView_ is "full size"). If apply
// is NO, we are trying to undo an offset. If no offset there is nothing to undo.
- (void)applyContentAreaOffset:(BOOL)apply {
  if (!contentAreaHasOffset_ && apply) {
    // There is no offset to unconditionally apply.
    return;
  }

  NSRect frame = [contentArea_ frame];
  if (apply) {
    frame.origin.y += shelfHeight_;
    frame.size.height -= shelfHeight_;
  } else {
    frame.origin.y -= shelfHeight_;
    frame.size.height += shelfHeight_;
  }

  [[contentArea_ animator] setFrame:frame];
  [[self view] setNeedsDisplay:YES];
  [contentArea_ setNeedsDisplay:YES];
}

- (DownloadShelf*)bridge {
  return bridge_.get();
}

- (BOOL)isVisible {
  return barIsVisible_;
}

- (void)show:(id)sender {
  [self showDownloadShelf:YES];
}

- (void)hide:(id)sender {
  // If |sender| isn't nil, then we're being closed from the UI by the user and
  // we need to tell our shelf implementation to close. Otherwise, we're being
  // closed programmatically by our shelf implementation.
  if (sender)
    bridge_->Close();
  else
    [self showDownloadShelf:NO];
}

- (float)height {
  return shelfHeight_;
}

- (void)addDownloadItem:(BaseDownloadItemModel*)model {
  // TODO(thakis): RTL support?
  // (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
  // Shift all existing items to the right
  for (DownloadItemController* itemController
      in downloadItemControllers_.get()) {
    NSRect frame = [[itemController view] frame];
    frame.origin.x += kDownloadItemWidth + kDownloadItemPadding;
    [[[itemController view] animator] setFrame:frame];
  }

  // Insert new item at the left.
  // Start at width 0...
  NSRect position = NSMakeRect(0, kDownloadItemBorderPadding,
                               0, kDownloadItemHeight);
  scoped_nsobject<DownloadItemController> controller(
      [[DownloadItemController alloc] initWithFrame:position
                                              model:model
                                              shelf:self]);
  [downloadItemControllers_ addObject:controller.get()];

  [itemContainerView_ addSubview:[controller.get() view]];

  [[NSNotificationCenter defaultCenter]
    addObserver:controller
       selector:@selector(updateVisibility:)
           name:NSViewFrameDidChangeNotification
         object:[controller view]];
  [[NSNotificationCenter defaultCenter]
    addObserver:controller
       selector:@selector(updateVisibility:)
           name:NSViewFrameDidChangeNotification
         object:itemContainerView_];

  // ...then animate in
  NSRect frame = [[controller.get() view] frame];
  frame.size.width = kDownloadItemWidth;

  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:kDownloadItemOpenDuration];
  [[[controller.get() view] animator] setFrame:frame];
  [NSAnimationContext endGrouping];

  // Keep only a limited number of items in the shelf.
  if ([downloadItemControllers_ count] > kMaxDownloadItemCount) {
    DCHECK(kMaxDownloadItemCount > 0);

    // Since no user will ever see the item being removed (needs a horizontal
    // screen resolution greater than 3200 at 16 items at 200 pixels each),
    // there's no point in animating the removal.
    [self remove:[downloadItemControllers_ objectAtIndex:0]];
  }
}

@end
