// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "gfx/font.h"
#include "grit/theme_resources.h"

@interface AutocompleteTextAttachmentCell : NSTextAttachmentCell {
}

// TODO(shess):
// Override -cellBaselineOffset to allow the image to be shifted up or
// down relative to the containing text's baseline.

// Draw the image using |DrawImageInRect()| helper function for
// |-setFlipped:| consistency with other image drawing.
- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)aView;

@end

namespace {

const CGFloat kBaselineAdjust = 2.0;

// How far to offset the keyword token into the field.
const NSInteger kKeywordXOffset = 3;

// How much width (beyond text) to add to the keyword token on each
// side.
const NSInteger kKeywordTokenInset = 3;

// Gap to leave between hint and right-hand-side of cell.
const NSInteger kHintXOffset = 4;

// How far to shift bounding box of hint down from top of field.
// Assumes -setFlipped:YES.
const NSInteger kHintYOffset = 4;

// How far to inset the keywork token from sides.
const NSInteger kKeywordYInset = 4;

// TODO(shess): The keyword hint image wants to sit on the baseline.
// This moves it down so that there is approximately as much image
// above the lowercase ascender as below the baseline.  A better
// technique would be nice to have, though.
const NSInteger kKeywordHintImageBaseline = -6;

// Drops the magnifying glass icon so that it looks centered in the
// keyword-search bubble.
const NSInteger kKeywordSearchImageBaseline = -5;

// The amount of padding on either side reserved for drawing an icon.
const NSInteger kIconHorizontalPad = 3;

// How far to shift bounding box of hint icon label down from top of field.
const NSInteger kIconLabelYOffset = 7;

// How far the editor insets itself, for purposes of determining if
// decorations need to be trimmed.
const CGFloat kEditorHorizontalInset = 3.0;

// Cause the location icon to line up above the icons in the popup.
const CGFloat kLocationIconXOffset = 6.0;
const CGFloat kLocationIconXPad = 1.0;

// How long to wait for mouse-up on the location icon before assuming
// that the user wants to drag.
const NSTimeInterval kLocationIconDragTimeout = 0.25;

// Conveniences to centralize width+offset calculations.
CGFloat WidthForHint(NSAttributedString* hintString) {
  return kHintXOffset + ceil([hintString size].width);
}
CGFloat WidthForKeyword(NSAttributedString* keywordString) {
  return kKeywordXOffset + ceil([keywordString size].width) +
      2 * kKeywordTokenInset;
}

// Convenience to draw |image| in the |rect| portion of |view|.
void DrawImageInRect(NSImage* image, NSView* view, const NSRect& rect) {
  // If there is an image, make sure we calculated the target size
  // correctly.
  DCHECK(!image || NSEqualSizes([image size], rect.size));
  [image setFlipped:[view isFlipped]];
  [image drawInRect:rect
           fromRect:NSZeroRect  // Entire image
          operation:NSCompositeSourceOver
           fraction:1.0];
}

// Helper function to generate an attributed string containing
// |anImage|.  If |baselineAdjustment| is 0, the image sits on the
// text baseline, positive values shift it up, negative values shift
// it down.
NSAttributedString* AttributedStringForImage(NSImage* anImage,
                                             CGFloat baselineAdjustment) {
  scoped_nsobject<AutocompleteTextAttachmentCell> attachmentCell(
      [[AutocompleteTextAttachmentCell alloc] initImageCell:anImage]);
  scoped_nsobject<NSTextAttachment> attachment(
      [[NSTextAttachment alloc] init]);
  [attachment setAttachmentCell:attachmentCell];

  scoped_nsobject<NSMutableAttributedString> as(
      [[NSAttributedString attributedStringWithAttachment:attachment]
        mutableCopy]);
  [as addAttribute:NSBaselineOffsetAttributeName
      value:[NSNumber numberWithFloat:baselineAdjustment]
      range:NSMakeRange(0, [as length])];

  return [[as copy] autorelease];
}

}  // namespace

@implementation AutocompleteTextAttachmentCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)aView {
  // Draw image with |DrawImageInRect()| to get consistent
  // |-setFlipped:| treatment.
  DrawImageInRect([self image], aView, cellFrame);
}

@end

@implementation AutocompleteTextFieldIcon

@synthesize rect = rect_;
@synthesize view = view_;

// Private helper.
- (id)initWithView:(LocationBarViewMac::LocationBarImageView*)view
           isLabel:(BOOL)isLabel {
  self = [super init];
  if (self) {
    isLabel_ = isLabel;
    view_ = view;
    rect_ = NSZeroRect;
  }
  return self;
}

- (id)initImageWithView:(LocationBarViewMac::LocationBarImageView*)view {
  return [self initWithView:view isLabel:NO];
}

- (id)initLabelWithView:(LocationBarViewMac::LocationBarImageView*)view {
  return [self initWithView:view isLabel:YES];
}

- (void)positionInFrame:(NSRect)frame {
  if (isLabel_) {
    NSAttributedString* label = view_->GetLabel();
    DCHECK(label);
    const CGFloat labelWidth = ceil([label size].width);
    rect_ = NSMakeRect(NSMaxX(frame) - labelWidth,
                       NSMinY(frame) + kIconLabelYOffset,
                       labelWidth, NSHeight(frame) - kIconLabelYOffset);
  } else {
    const NSSize imageSize = view_->GetImageSize();
    const CGFloat yOffset = floor((NSHeight(frame) - imageSize.height) / 2);
    rect_ = NSMakeRect(NSMaxX(frame) - imageSize.width,
                       NSMinY(frame) + yOffset,
                       imageSize.width, imageSize.height);
  }
}

- (void)drawInView:(NSView*)controlView {
  // Make sure someone called |-positionInFrame:|.
  DCHECK(!NSIsEmptyRect(rect_));
  if (isLabel_) {
    NSAttributedString* label = view_->GetLabel();
    [label drawInRect:rect_];
  } else {
    DrawImageInRect(view_->GetImage(), controlView, rect_);
  }
}

@end

@implementation AutocompleteTextFieldCell

// @synthesize doesn't seem to compile for this transition.
- (NSAttributedString*)keywordString {
  return keywordString_.get();
}
- (NSAttributedString*)hintString {
  return hintString_.get();
}

- (CGFloat)baselineAdjust {
  return kBaselineAdjust;
}

- (void)setKeywordString:(NSString*)fullString
           partialString:(NSString*)partialString
          availableWidth:(CGFloat)width {
  DCHECK(fullString != nil);

  hintString_.reset();

  // Adjust for space between editor and decorations.
  width -= 2 * kEditorHorizontalInset;

  // Get the magnifying glass to put at the front of the string.
  NSImage* image =
      AutocompleteEditViewMac::ImageForResource(IDR_OMNIBOX_SEARCH);
  const NSSize imageSize = [image size];

  // Based on what fits, choose |fullString| with the image,
  // |fullString| without the image, or |partialString|.
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:[self font]
                                  forKey:NSFontAttributeName];
  NSString* s = fullString;
  const CGFloat sWidth = [s sizeWithAttributes:attributes].width;
  if (sWidth + imageSize.width > width) {
    image = nil;
  }
  if (sWidth > width) {
    if (partialString) {
      s = partialString;
    }
  }

  scoped_nsobject<NSMutableAttributedString> as(
      [[NSMutableAttributedString alloc] initWithString:s
                                             attributes:attributes]);

  // Insert the image at the front of the string if it didn't make
  // things too wide.
  if (image) {
    NSAttributedString* is =
        AttributedStringForImage(image, kKeywordSearchImageBaseline);
    [as insertAttributedString:is atIndex:0];
  }

  keywordString_.reset([as copy]);
}

// Convenience for the attributes used in the right-justified info
// cells.
- (NSDictionary*)hintAttributes {
  scoped_nsobject<NSMutableParagraphStyle> style(
      [[NSMutableParagraphStyle alloc] init]);
  [style setAlignment:NSRightTextAlignment];

  return [NSDictionary dictionaryWithObjectsAndKeys:
              [self font], NSFontAttributeName,
              [NSColor lightGrayColor], NSForegroundColorAttributeName,
              style.get(), NSParagraphStyleAttributeName,
              nil];
}

- (void)setKeywordHintPrefix:(NSString*)prefixString
                       image:(NSImage*)anImage
                      suffix:(NSString*)suffixString
              availableWidth:(CGFloat)width {
  DCHECK(prefixString != nil);
  DCHECK(anImage != nil);
  DCHECK(suffixString != nil);

  keywordString_.reset();

  // Adjust for space between editor and decorations.
  width -= 2 * kEditorHorizontalInset;

  // If |hintString_| is now too wide, clear it so that we don't pass
  // the equality tests.
  if (hintString_ && WidthForHint(hintString_) > width) {
    [self clearKeywordAndHint];
  }

  // TODO(shess): Also check the length?
  if (![[hintString_ string] hasPrefix:prefixString] ||
      ![[hintString_ string] hasSuffix:suffixString]) {

    // Build an attributed string with the concatenation of the prefix
    // and suffix.
    NSString* s = [prefixString stringByAppendingString:suffixString];
    scoped_nsobject<NSMutableAttributedString> as(
        [[NSMutableAttributedString alloc]
          initWithString:s attributes:[self hintAttributes]]);

    // Build an attachment containing the hint image.
    NSAttributedString* is =
        AttributedStringForImage(anImage, kKeywordHintImageBaseline);

    // Stuff the image attachment between the prefix and suffix.
    [as insertAttributedString:is atIndex:[prefixString length]];

    // If too wide, just show the image.
    hintString_.reset(WidthForHint(as) > width ? [is copy] : [as copy]);
  }
}

- (void)setSearchHintString:(NSString*)aString
             availableWidth:(CGFloat)width {
  DCHECK(aString != nil);

  // Adjust for space between editor and decorations.
  width -= 2 * kEditorHorizontalInset;

  keywordString_.reset();

  // If |hintString_| is now too wide, clear it so that we don't pass
  // the equality tests.
  if (hintString_ && WidthForHint(hintString_) > width) {
    [self clearKeywordAndHint];
  }

  if (![[hintString_ string] isEqualToString:aString]) {
    scoped_nsobject<NSAttributedString> as(
        [[NSAttributedString alloc] initWithString:aString
                                        attributes:[self hintAttributes]]);

    // If too wide, don't keep the hint.
    hintString_.reset(WidthForHint(as) > width ? nil : [as copy]);
  }
}

- (void)clearKeywordAndHint {
  keywordString_.reset();
  hintString_.reset();
}

- (void)setPageActionViewList:(LocationBarViewMac::PageActionViewList*)list {
  page_action_views_ = list;
}

- (void)setLocationIconView:(LocationBarViewMac::LocationIconView*)view {
  locationIconView_ = view;
}

- (void)setStarIconView:(LocationBarViewMac::LocationBarImageView*)view {
  starIconView_ = view;
}

- (void)setSecurityLabelView:(LocationBarViewMac::LocationBarImageView*)view {
  securityLabelView_ = view;
}

- (void)setContentSettingViewsList:
    (LocationBarViewMac::ContentSettingViews*)views {
  content_setting_views_ = views;
}

// Overriden to account for the hint strings and hint icons.
- (NSRect)textFrameForFrame:(NSRect)cellFrame {
  NSRect textFrame([super textFrameForFrame:cellFrame]);

  // TODO(shess): Check the set of exclusions in |LocationBarViewMac|.
  // If |keywordString_| takes precedence over other cases, then this
  // code could be made simpler.
  if (!keywordString_ && locationIconView_ && locationIconView_->IsVisible()) {
    const NSSize imageSize = locationIconView_->GetImageSize();
    const CGFloat locationIconWidth =
        kLocationIconXOffset + kLocationIconXPad + imageSize.width;
    textFrame.origin.x += locationIconWidth;
    textFrame.size.width -= locationIconWidth;
  }

  if (hintString_) {
    DCHECK(!keywordString_);
    const CGFloat hintWidth(WidthForHint(hintString_));

    // TODO(shess): This could be better.  Show the hint until the
    // non-hint text bumps against it?
    if (hintWidth < NSWidth(textFrame)) {
      textFrame.size.width -= hintWidth;
    }
  } else if (keywordString_) {
    DCHECK(!hintString_);
    const CGFloat keywordWidth(WidthForKeyword(keywordString_));

    if (keywordWidth < NSWidth(textFrame)) {
      textFrame.origin.x += keywordWidth;
      textFrame.size.width -= keywordWidth;
    }
  } else {
    // Leave room for items on the right (SSL label, page actions,
    // etc).  Icons are laid out in |cellFrame| rather than
    // |textFrame| for consistency with drawing code.
    NSArray* iconFrames = [self layedOutIcons:cellFrame];
    if ([iconFrames count]) {
      const CGFloat minX = NSMinX([[iconFrames objectAtIndex:0] rect]);
      if (minX >= NSMinX(textFrame)) {
        textFrame.size.width = minX - NSMinX(textFrame);
      }
    }
  }

  return textFrame;
}

- (NSRect)locationIconFrameForFrame:(NSRect)cellFrame {
  if (!locationIconView_ || !locationIconView_->IsVisible())
    return NSZeroRect;

  const NSSize imageSize = locationIconView_->GetImageSize();
  const CGFloat yOffset = floor((NSHeight(cellFrame) - imageSize.height) / 2);
  return NSMakeRect(NSMinX(cellFrame) + kLocationIconXOffset,
                    NSMinY(cellFrame) + yOffset,
                    imageSize.width, imageSize.height);
}

- (NSRect)starIconFrameForFrame:(NSRect)cellFrame {
  if (!starIconView_ || !starIconView_->IsVisible())
    return NSZeroRect;

  // The star icon is always at the RHS.
  scoped_nsobject<AutocompleteTextFieldIcon> icon(
        [[AutocompleteTextFieldIcon alloc] initImageWithView:starIconView_]);
  cellFrame.size.width -= kHintXOffset;
  [icon positionInFrame:cellFrame];
  return [icon rect];
}

- (size_t)pageActionCount {
  // page_action_views_ may be NULL during testing, or if the
  // containing LocationViewMac object has already been destructed
  // (happens sometimes during window shutdown).
  if (!page_action_views_)
    return 0;
  return page_action_views_->Count();
}

- (NSRect)pageActionFrameForIndex:(size_t)index inFrame:(NSRect)cellFrame {
  LocationBarViewMac::PageActionImageView* view =
      page_action_views_->ViewAt(index);

  // When this method is called, all the icon images are still loading, so
  // just check to see whether the view is visible when deciding whether
  // its NSRect should be made available.
  if (!view->IsVisible())
    return NSZeroRect;

  for (AutocompleteTextFieldIcon* icon in [self layedOutIcons:cellFrame]) {
    if (view == [icon view])
      return [icon rect];
  }
  NOTREACHED();
  return NSZeroRect;
}

- (NSRect)pageActionFrameForExtensionAction:(ExtensionAction*)action
                                    inFrame:(NSRect)cellFrame {
  const size_t pageActionCount = [self pageActionCount];
  size_t pos = 0;
  while (pos < pageActionCount &&
      action != page_action_views_->ViewAt(pos)->page_action())
    ++pos;
  return (pos == pageActionCount) ? NSZeroRect :
      [self pageActionFrameForIndex:pos inFrame:cellFrame];
}

- (void)drawHintWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  DCHECK(hintString_);

  NSRect textFrame = [self textFrameForFrame:cellFrame];
  NSRect infoFrame(NSMakeRect(NSMaxX(textFrame),
                              cellFrame.origin.y + kHintYOffset,
                              ceil([hintString_ size].width),
                              cellFrame.size.height - kHintYOffset));
  [hintString_.get() drawInRect:infoFrame];
}

- (void)drawKeywordWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  DCHECK(keywordString_);

  NSRect textFrame = [self textFrameForFrame:cellFrame];
  const CGFloat x = NSMinX(cellFrame) + kKeywordXOffset;
  NSRect infoFrame(NSMakeRect(x,
                              cellFrame.origin.y + kKeywordYInset,
                              NSMinX(textFrame) - x,
                              cellFrame.size.height - 2 * kKeywordYInset));

  // Draw the token rectangle with rounded corners.
  NSRect frame(NSInsetRect(infoFrame, 0.5, 0.5));
  NSBezierPath* path =
      [NSBezierPath bezierPathWithRoundedRect:frame xRadius:4.0 yRadius:4.0];

  // Matches the color of the highlighted line in the popup.
  [[NSColor selectedControlColor] set];
  [path fill];

  // Border around token rectangle, match focus ring's inner color.
  [[[NSColor keyboardFocusIndicatorColor] colorWithAlphaComponent:0.5] set];
  [path setLineWidth:1.0];
  [path stroke];

  // Draw text w/in the rectangle.
  infoFrame.origin.x += 3.0;
  [keywordString_.get() drawInRect:infoFrame];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  if (!keywordString_ && locationIconView_ && locationIconView_->IsVisible()) {
    DrawImageInRect(locationIconView_->GetImage(), controlView,
                    [self locationIconFrameForFrame:cellFrame]);
  }

  if (hintString_) {
    [self drawHintWithFrame:cellFrame inView:controlView];
  } else if (keywordString_) {
    [self drawKeywordWithFrame:cellFrame inView:controlView];
  } else {
    for (AutocompleteTextFieldIcon* icon in [self layedOutIcons:cellFrame]) {
      [icon drawInView:controlView];
    }
  }

  [super drawInteriorWithFrame:cellFrame inView:controlView];
}

- (NSArray*)layedOutIcons:(NSRect)cellFrame {
  // The set of views to display right-justified in the cell, from
  // left to right.
  NSMutableArray* result = [NSMutableArray array];

  // Security label floats to left of page actions.
  if (securityLabelView_ && securityLabelView_->IsVisible() &&
      securityLabelView_->GetLabel()) {
    scoped_nsobject<AutocompleteTextFieldIcon> icon(
        [[AutocompleteTextFieldIcon alloc]
          initLabelWithView:securityLabelView_]);
    [result addObject:icon];
  }

  // Collect the image views for bulk processing.
  // TODO(shess): Refactor with LocationBarViewMac to make the
  // different types of items more consistent.
  std::vector<LocationBarViewMac::LocationBarImageView*> views;

  if (content_setting_views_) {
    views.insert(views.end(),
                 content_setting_views_->begin(),
                 content_setting_views_->end());
  }

  // TODO(shess): Previous implementation of this method made a
  // right-to-left array, so add the page-action items in that order.
  // As part of the refactor mentioned above, lay everything out
  // nicely left-to-right.
  for (size_t i = [self pageActionCount]; i-- > 0;) {
    views.push_back(page_action_views_->ViewAt(i));
  }

  // The star icon should always come last.
  if (starIconView_)
    views.push_back(starIconView_);

  // Load the visible views into |result|.
  for (std::vector<LocationBarViewMac::LocationBarImageView*>::const_iterator
           iter = views.begin(); iter != views.end(); ++iter) {
    if ((*iter)->IsVisible()) {
      scoped_nsobject<AutocompleteTextFieldIcon> icon(
          [[AutocompleteTextFieldIcon alloc] initImageWithView:*iter]);
      [result addObject:icon];
    }
  }

  // Leave a boundary at RHS of field.
  cellFrame.size.width -= kHintXOffset;

  // Position each view within the frame from right to left.
  for (AutocompleteTextFieldIcon* icon in [result reverseObjectEnumerator]) {
    [icon positionInFrame:cellFrame];

    // Trim the icon's space from the frame.
    cellFrame.size.width = NSMinX([icon rect]) - kIconHorizontalPad;
  }
  return result;
}

- (AutocompleteTextFieldIcon*)iconForEvent:(NSEvent*)theEvent
                                    inRect:(NSRect)cellFrame
                                    ofView:(AutocompleteTextField*)controlView {
  const BOOL flipped = [controlView isFlipped];
  const NSPoint location =
      [controlView convertPoint:[theEvent locationInWindow] fromView:nil];

  // Special check for location image, it is not in |-layedOutIcons:|.
  const NSRect locationIconFrame = [self locationIconFrameForFrame:cellFrame];
  if (NSMouseInRect(location, locationIconFrame, flipped)) {
    // Make up an icon to return.
    AutocompleteTextFieldIcon* icon =
        [[[AutocompleteTextFieldIcon alloc]
           initImageWithView:locationIconView_] autorelease];
    [icon setRect:locationIconFrame];
    return icon;
  }

  for (AutocompleteTextFieldIcon* icon in [self layedOutIcons:cellFrame]) {
    if (NSMouseInRect(location, [icon rect], flipped))
      return icon;
  }

  return nil;
}

- (NSMenu*)actionMenuForEvent:(NSEvent*)theEvent
                       inRect:(NSRect)cellFrame
                       ofView:(AutocompleteTextField*)controlView {
  AutocompleteTextFieldIcon*
      icon = [self iconForEvent:theEvent inRect:cellFrame ofView:controlView];
  if (icon)
    return [icon view]->GetMenu();
  return nil;
}

- (BOOL)mouseDown:(NSEvent*)theEvent
           inRect:(NSRect)cellFrame
           ofView:(AutocompleteTextField*)controlView {
  AutocompleteTextFieldIcon* icon =
      [self iconForEvent:theEvent inRect:cellFrame ofView:controlView];
  if (!icon)
    return NO;

  // If the icon is draggable, then initiate a drag if the user drags
  // or holds the mouse down for awhile.
  if ([icon view]->IsDraggable()) {
    NSDate* timeout =
        [NSDate dateWithTimeIntervalSinceNow:kLocationIconDragTimeout];
    NSEvent* event = [NSApp nextEventMatchingMask:(NSLeftMouseDraggedMask |
                                                   NSLeftMouseUpMask)
                                        untilDate:timeout
                                           inMode:NSEventTrackingRunLoopMode
                                          dequeue:YES];
    if (!event || [event type] == NSLeftMouseDragged) {
      NSPasteboard* pboard = [icon view]->GetDragPasteboard();
      DCHECK(pboard);

      // TODO(shess): My understanding is that the -isFlipped
      // adjustment should not be necessary.  But without it, the
      // image is nowhere near the cursor.  Perhaps the icon's rect is
      // incorrectly calculated?
      // http://crbug.com/40711
      NSPoint dragPoint = [icon rect].origin;
      if ([controlView isFlipped])
        dragPoint.y += NSHeight([icon rect]);

      [controlView dragImage:[icon view]->GetImage()
                          at:dragPoint
                      offset:NSZeroSize
                       event:event ? event : theEvent
                  pasteboard:pboard
                      source:self
                   slideBack:YES];
      return YES;
    }

    // On mouse-up fall through to mouse-pressed case.
    DCHECK_EQ([event type], NSLeftMouseUp);
  }

  [icon view]->OnMousePressed([icon rect]);
  return YES;
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  return NSDragOperationCopy;
}

@end
