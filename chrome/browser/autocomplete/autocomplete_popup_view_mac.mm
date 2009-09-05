// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_view_mac.h"

#include "app/gfx/text_elider.h"
#include "base/sys_string_conversions.h"
#include "base/gfx/rect.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/cocoa/event_utils.h"
#include "chrome/browser/cocoa/nsimage_cache.h"

namespace {

// The size delta between the font used for the edit and the result
// rows.
const int kEditFontAdjust = -1;

// How much to adjust the cell sizing up from the default determined
// by the font.
const int kCellHeightAdjust = 7.0;

// How to round off the popup's corners.  Goal is to match star and go
// buttons.
const CGFloat kPopupRoundingRadius = 3.5;

// Gap between the field and the popup.
const CGFloat kPopupFieldGap = 2.0;

// How opaque the popup window should be.  This matches Windows (see
// autocomplete_popup_contents_view.cc, kGlassPopupTransparency).
const CGFloat kPopupAlpha = 240.0 / 255.0;

// How much space to leave for the left and right margins.
const CGFloat kLeftRightMargin = 8.0;

// How far to offset the text column from the left.
const CGFloat kTextXOffset = 33.0;

// Animation duration when animating the popup window smaller.
const float kShrinkAnimationDuration = 0.1;

// Maximum fraction of the popup width that can be used to display match
// contents.
const float kMaxMatchContentsWidth = 0.7;

// NSEvent -buttonNumber for middle mouse button.
const static NSInteger kMiddleButtonNumber(2);

// Background colors for different states of the popup elements.
NSColor* BackgroundColor() {
  return [[NSColor controlBackgroundColor] colorWithAlphaComponent:kPopupAlpha];
}
NSColor* SelectedBackgroundColor() {
  return [[NSColor selectedControlColor] colorWithAlphaComponent:kPopupAlpha];
}
NSColor* HoveredBackgroundColor() {
  return [[NSColor controlHighlightColor] colorWithAlphaComponent:kPopupAlpha];
}

// TODO(shess): These are totally unprincipled.  I experimented with
// +controlTextColor and the like, but found myself wondering whether
// that was really appropriate.  Circle back after consulting with
// someone more knowledgeable about the ins and outs of this.
static const NSColor* ContentTextColor() {
  return [NSColor blackColor];
}
static const NSColor* URLTextColor() {
  return [NSColor colorWithCalibratedRed:0.0 green:0.55 blue:0.0 alpha:1.0];
}
static const NSColor* DescriptionTextColor() {
  return [NSColor darkGrayColor];
}

// Return the appropriate icon for the given match.  Derived from the
// gtk code.
NSImage* MatchIcon(const AutocompleteMatch& match) {
  if (match.starred) {
    return nsimage_cache::ImageNamed(@"o2_star.png");
  }

  switch (match.type) {
    case AutocompleteMatch::URL_WHAT_YOU_TYPED:
    case AutocompleteMatch::NAVSUGGEST: {
      return nsimage_cache::ImageNamed(@"o2_globe.png");
    }
    case AutocompleteMatch::HISTORY_URL:
    case AutocompleteMatch::HISTORY_TITLE:
    case AutocompleteMatch::HISTORY_BODY:
    case AutocompleteMatch::HISTORY_KEYWORD: {
      return nsimage_cache::ImageNamed(@"o2_history.png");
    }
    case AutocompleteMatch::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatch::SEARCH_HISTORY:
    case AutocompleteMatch::SEARCH_SUGGEST:
    case AutocompleteMatch::SEARCH_OTHER_ENGINE: {
      return nsimage_cache::ImageNamed(@"o2_search.png");
    }
    case AutocompleteMatch::OPEN_HISTORY_PAGE: {
      return nsimage_cache::ImageNamed(@"o2_more.png");
    }
    default:
      NOTREACHED();
      break;
  }

  return nil;
}

}  // namespace

// Helper for MatchText() to allow sharing code between the contents
// and description cases.  Returns NSMutableAttributedString as a
// convenience for MatchText().
NSMutableAttributedString* AutocompletePopupViewMac::DecorateMatchedString(
    const std::wstring &matchString,
    const AutocompleteMatch::ACMatchClassifications &classifications,
    NSColor* textColor, gfx::Font& font) {
  // Cache for on-demand computation of the bold version of |font|.
  NSFont* boldFont = nil;

  // Start out with a string using the default style info.
  NSString* s = base::SysWideToNSString(matchString);
  NSDictionary* attributes = [NSDictionary dictionaryWithObjectsAndKeys:
                                  font.nativeFont(), NSFontAttributeName,
                                  textColor, NSForegroundColorAttributeName,
                                  nil];
  NSMutableAttributedString* as =
      [[[NSMutableAttributedString alloc] initWithString:s
                                              attributes:attributes]
        autorelease];

  // Mark up the runs which differ from the default.
  for (ACMatchClassifications::const_iterator i = classifications.begin();
       i != classifications.end(); ++i) {
    const BOOL isLast = (i+1) == classifications.end();
    const size_t nextOffset = (isLast ? matchString.length() : (i+1)->offset);
    const NSInteger location = static_cast<NSInteger>(i->offset);
    const NSInteger length = static_cast<NSInteger>(nextOffset - i->offset);
    const NSRange range = NSMakeRange(location, length);

    if (0 != (i->style & ACMatchClassification::URL)) {
      [as addAttribute:NSForegroundColorAttributeName
                 value:URLTextColor() range:range];
    }

    if (0 != (i->style & ACMatchClassification::MATCH)) {
      if (!boldFont) {
        NSFontManager* fontManager = [NSFontManager sharedFontManager];
        boldFont = [fontManager convertFont:font.nativeFont()
                                toHaveTrait:NSBoldFontMask];
      }
      [as addAttribute:NSFontAttributeName value:boldFont range:range];
    }
  }

  return as;
}

// Return the text to show for the match, based on the match's
// contents and description.  Result will be in |font|, with the
// boldfaced version used for matches.
NSAttributedString* AutocompletePopupViewMac::MatchText(
    const AutocompleteMatch& match, gfx::Font& font, float cellWidth) {
  // If there is a description, then the URL can take at most 70% of the
  // available width, with 30% being reserved for the description.  If there is
  // no description, then the URL can take the full 100%.
  float availableWidth = cellWidth - kTextXOffset - kLeftRightMargin;
  BOOL hasDescription = match.description.empty() ? NO : YES;
  float urlWidth = hasDescription ? availableWidth * kMaxMatchContentsWidth
                                  : availableWidth;

  NSMutableAttributedString *as =
      DecorateMatchedString(gfx::ElideText(match.contents, font, urlWidth),
                            match.contents_class,
                            ContentTextColor(), font);

  // If there is a description, append it, separated from the contents
  // with an en dash, and decorated with a distinct color.
  if (!match.description.empty()) {
    NSDictionary* attributes =
        [NSDictionary dictionaryWithObjectsAndKeys:
             font.nativeFont(), NSFontAttributeName,
             ContentTextColor(), NSForegroundColorAttributeName,
             nil];
    NSString* rawEnDash = [NSString stringWithFormat:@" %C ", 0x2013];
    NSAttributedString* enDash =
        [[[NSAttributedString alloc] initWithString:rawEnDash
                                         attributes:attributes] autorelease];

    NSAttributedString* description =
        DecorateMatchedString(match.description, match.description_class,
                              DescriptionTextColor(), font);

    [as appendAttributedString:enDash];
    [as appendAttributedString:description];
  }

  NSMutableParagraphStyle* style =
      [[[NSMutableParagraphStyle alloc] init] autorelease];
  [style setLineBreakMode:NSLineBreakByTruncatingTail];
  [as addAttribute:NSParagraphStyleAttributeName value:style
             range:NSMakeRange(0, [as length])];

  return as;
}

// AutocompleteButtonCell overrides how backgrounds are displayed to
// handle hover versus selected.  So long as we're in there, it also
// provides some default initialization.

@interface AutocompleteButtonCell : NSButtonCell {
}
@end

// AutocompleteMatrix sets up a tracking area to implement hover by
// highlighting the cell the mouse is over.

@interface AutocompleteMatrix : NSMatrix {
  SEL middleClickAction_;
}

// Action to be called when the middle mouse button is released.
- (void)setMiddleClickAction:(SEL)anAction;

// Return the currently highlighted row.  Returns -1 if no row is
// highlighted.
- (NSInteger)highlightedRow;

@end

// Thin Obj-C bridge class between the target of the popup window's
// AutocompleteMatrix and the AutocompletePopupView implementation.

// TODO(shess): Now that I'm using AutocompleteMatrix, I could instead
// subvert the target/action stuff and have it message popup_view_
// directly.

@interface AutocompleteMatrixTarget : NSObject {
 @private
  AutocompletePopupViewMac* popup_view_;  // weak, owns us.
}
- initWithPopupView:(AutocompletePopupViewMac*)view;

// Tell popup model via popup_view_ about the selected row.
- (void)select:(id)sender;

// Call |popup_view_| OnMiddleClick().
- (void)middleSelect:(id)sender;

// Resize the popup when the field's window resizes.
- (void)windowDidResize:(NSNotification*)notification;

@end

AutocompletePopupViewMac::AutocompletePopupViewMac(
    AutocompleteEditViewMac* edit_view,
    AutocompleteEditModel* edit_model,
    AutocompletePopupPositioner* positioner,
    Profile* profile,
    NSTextField* field)
    : model_(new AutocompletePopupModel(this, edit_model, profile)),
      edit_view_(edit_view),
      positioner_(positioner),
      field_(field),
      matrix_target_([[AutocompleteMatrixTarget alloc] initWithPopupView:this]),
      popup_(nil) {
  DCHECK(edit_view);
  DCHECK(edit_model);
  DCHECK(profile);
  edit_model->SetPopupModel(model_.get());
}

AutocompletePopupViewMac::~AutocompletePopupViewMac() {
  // TODO(shess): Having to be aware of destructor ordering in this
  // way seems brittle.  There must be a better way.

  // Destroy the popup model before this object is destroyed, because
  // it can call back to us in the destructor.
  model_.reset();

  // Break references to matrix_target_ before it is released.
  NSMatrix* matrix = [popup_ contentView];
  [matrix setTarget:nil];
}

bool AutocompletePopupViewMac::IsOpen() const {
  return [popup_ isVisible] ? true : false;
}

void AutocompletePopupViewMac::CreatePopupIfNeeded() {
  if (!popup_) {
    popup_.reset([[NSWindow alloc] initWithContentRect:NSZeroRect
                                             styleMask:NSBorderlessWindowMask
                                               backing:NSBackingStoreBuffered
                                                 defer:YES]);
    [popup_ setMovableByWindowBackground:NO];
    // The window shape is determined by the content view
    // (AutocompleteMatrix).
    [popup_ setAlphaValue:1.0];
    [popup_ setOpaque:NO];
    [popup_ setBackgroundColor:[NSColor clearColor]];
    [popup_ setHasShadow:YES];
    [popup_ setLevel:NSNormalWindowLevel];

    AutocompleteMatrix* matrix =
        [[[AutocompleteMatrix alloc] initWithFrame:NSZeroRect] autorelease];
    [matrix setTarget:matrix_target_];
    [matrix setAction:@selector(select:)];
    [matrix setMiddleClickAction:@selector(middleSelect:)];
    [popup_ setContentView:matrix];

    // We need the popup to follow window resize.
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:matrix_target_
           selector:@selector(windowDidResize:)
               name:NSWindowDidResizeNotification
             object:[field_ window]];
  }
}

void AutocompletePopupViewMac::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  if (result.empty()) {
    [[popup_ parentWindow] removeChildWindow:popup_];
    [popup_ orderOut:nil];

    // Break references to matrix_target_ before releasing popup_.
    NSMatrix* matrix = [popup_ contentView];
    [matrix setTarget:nil];

    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc removeObserver:matrix_target_
                  name:NSWindowDidResizeNotification
                object:[field_ window]];

    popup_.reset(nil);

    return;
  }

  CreatePopupIfNeeded();

  // Layout the popup and size it to land underneath the field.
  NSRect r = NSRectFromCGRect(positioner_->GetPopupBounds().ToCGRect());
  r.origin = [[field_ window] convertBaseToScreen:r.origin];
  DCHECK_GT(r.size.width, 0.0);

  // The popup's font is a slightly smaller version of what |field_|
  // uses.
  NSFont* fieldFont = [field_ font];
  const CGFloat resultFontSize = [fieldFont pointSize] + kEditFontAdjust;
  gfx::Font resultFont = gfx::Font::CreateFont(
      base::SysNSStringToWide([fieldFont fontName]), (int)resultFontSize);

  // Load the results into the popup's matrix.  The popup window must be
  // correctly sized before calling MatchText().
  AutocompleteMatrix* matrix = [popup_ contentView];
  const size_t rows = model_->result().size();
  DCHECK_GT(rows, 0U);
  [matrix renewRows:rows columns:1];
  for (size_t ii = 0; ii < rows; ++ii) {
    AutocompleteButtonCell* cell = [matrix cellAtRow:ii column:0];
    const AutocompleteMatch& match = model_->result().match_at(ii);
    [cell setImage:MatchIcon(match)];
    [cell setAttributedTitle:MatchText(match, resultFont, r.size.width)];
  }

  // Set the cell size to fit a line of text in the cell's font.  All
  // cells should use the same font and each should layout in one
  // line, so they should all be about the same height.
  const NSSize cellSize = [[matrix cellAtRow:0 column:0] cellSize];
  DCHECK_GT(cellSize.height, 0.0);
  [matrix setCellSize:NSMakeSize(r.size.width,
                                 cellSize.height + kCellHeightAdjust)];

  // Make the matrix big enough to hold all the cells.
  [matrix sizeToCells];

  // Make the window just as big.
  r.size.height = [matrix frame].size.height;
  r.origin.y -= r.size.height + kPopupFieldGap;

  // Update the selection.
  PaintUpdatesNow();

  // Animate the frame change if the only change is that the height got smaller.
  // Otherwise, resize immediately.
  NSRect oldFrame = [popup_ frame];
  if (r.size.height < oldFrame.size.height &&
      r.origin.x == oldFrame.origin.x &&
      r.size.width == oldFrame.size.width) {
    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext] setDuration:kShrinkAnimationDuration];
    [[popup_ animator] setFrame:r display:YES];
    [NSAnimationContext endGrouping];
  } else {
    [popup_ setFrame:r display:YES];
  }

  if (!IsOpen()) {
    [[field_ window] addChildWindow:popup_ ordered:NSWindowAbove];
  }
}

// This is only called by model in SetSelectedLine() after updating
// everything.  Popup should already be visible.
void AutocompletePopupViewMac::PaintUpdatesNow() {
  AutocompleteMatrix* matrix = [popup_ contentView];
  [matrix selectCellAtRow:model_->selected_line() column:0];
}

AutocompletePopupModel* AutocompletePopupViewMac::GetModel() {
  return model_.get();
}

void AutocompletePopupViewMac::AcceptInput() {
  const NSInteger selectedRow = [[popup_ contentView] selectedRow];

  // -1 means no cells were selected.  This can happen if the user
  // clicked and then dragged their mouse off the popup before
  // releasing, so reset the selection and ignore the event.
  if (selectedRow == -1) {
    PaintUpdatesNow();
  } else {
    model_->SetSelectedLine(selectedRow, false);
    WindowOpenDisposition disposition =
        event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
    edit_view_->AcceptInput(disposition, false);
  }
}

void AutocompletePopupViewMac::OnMiddleClick() {
  const NSInteger row = [[popup_ contentView] highlightedRow];
  if (row == -1) {
    return;
  }

  // OpenURL() may close the popup, which will clear the result set
  // and, by extension, |match| and its contents.  So copy the
  // relevant strings out to make sure they stay alive until the call
  // completes.
  const AutocompleteMatch& match = model_->result().match_at(row);
  const GURL url(match.destination_url);
  std::wstring keyword;
  const bool is_keyword_hint = model_->GetKeywordForMatch(match, &keyword);
  edit_view_->OpenURL(url, NEW_BACKGROUND_TAB, match.transition, GURL(), row,
                      is_keyword_hint ? std::wstring() : keyword);
}

@implementation AutocompleteButtonCell

- init {
  self = [super init];
  if (self) {
    [self setImagePosition:NSImageLeft];
    [self setBordered:NO];
    [self setButtonType:NSRadioButton];

    // Without this highlighting messes up white areas of images.
    [self setHighlightsBy:NSNoCellMask];
  }
  return self;
}

- (NSColor*)backgroundColor {
  if ([self state] == NSOnState) {
    return SelectedBackgroundColor();
  } else if ([self isHighlighted]) {
    return HoveredBackgroundColor();
  }
  return BackgroundColor();
}

// The default NSButtonCell drawing leaves the image flush left and
// the title next to the image.  This spaces things out to line up
// with the star button and autocomplete field.
// TODO(shess): Determine if the star button can change size (other
// than scaling coordinates), in which case this needs to be more
// dynamic.
- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
  [[self backgroundColor] set];
  NSRectFill(cellFrame);

  // Put the image centered vertically but in a fixed column.
  // TODO(shess) Currently, the images are all 16x16 png files, so
  // left-justified works out fine.  If that changes, it may be
  // appropriate to align them on their centers instead of their
  // left-hand sides.
  NSImage* image = [self image];
  if (image) {
    NSRect imageRect = cellFrame;
    imageRect.size = [image size];
    imageRect.origin.y +=
        floor((NSHeight(cellFrame) - NSHeight(imageRect)) / 2);
    imageRect.origin.x += kLeftRightMargin;
    [self drawImage:image withFrame:imageRect inView:controlView];
  }

  // Adjust the title position to be lined up under the field's text.
  NSAttributedString* title = [self attributedTitle];
  if (title) {
    NSRect titleRect = cellFrame;
    titleRect.size.width -= (kTextXOffset + kLeftRightMargin);
    titleRect.origin.x += kTextXOffset;
    [self drawTitle:title withFrame:titleRect inView:controlView];
  }
}

@end

@implementation AutocompleteMatrix

// Remove all tracking areas and initialize the one we want.  Removing
// all might be overkill, but it's unclear why there would be others
// for the popup window.
- (void)resetTrackingArea {
  for (NSTrackingArea* trackingArea in [self trackingAreas]) {
    [self removeTrackingArea:trackingArea];
  }

  // TODO(shess): Consider overriding -acceptsFirstMouse: and changing
  // NSTrackingActiveInActiveApp to NSTrackingActiveAlways.
  NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited;
  options |= NSTrackingMouseMoved;
  options |= NSTrackingActiveInActiveApp;
  options |= NSTrackingInVisibleRect;

  NSTrackingArea* trackingArea =
      [[[NSTrackingArea alloc] initWithRect:[self frame]
                                    options:options
                                      owner:self
                                   userInfo:nil] autorelease];
  [self addTrackingArea:trackingArea];
}

- (void)updateTrackingAreas {
  [self resetTrackingArea];
  [super updateTrackingAreas];
}

- initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setCellClass:[AutocompleteButtonCell class]];

    // Cells pack with no spacing.
    [self setIntercellSpacing:NSMakeSize(0.0, 0.0)];

    [self setDrawsBackground:YES];
    [self setBackgroundColor:BackgroundColor()];
    [self renewRows:0 columns:1];
    [self setAllowsEmptySelection:YES];
    [self setMode:NSRadioModeMatrix];
    [self deselectAllCells];

    [self resetTrackingArea];
  }
  return self;
}

- (void)highlightRowAt:(NSInteger)rowIndex {
  // highlightCell will be nil if rowIndex is out of range, so no cell
  // will be highlighted.
  NSCell* highlightCell = [self cellAtRow:rowIndex column:0];

  for (NSCell* cell in [self cells]) {
    [cell setHighlighted:(cell == highlightCell)];
  }
}

- (void)highlightRowUnder:(NSEvent*)theEvent {
  NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
  NSInteger row, column;
  if ([self getRow:&row column:&column forPoint:point]) {
    [self highlightRowAt:row];
  } else {
    [self highlightRowAt:-1];
  }
}

// Callbacks from NSTrackingArea.
- (void)mouseEntered:(NSEvent*)theEvent {
  [self highlightRowUnder:theEvent];
}
- (void)mouseMoved:(NSEvent*)theEvent {
  [self highlightRowUnder:theEvent];
}
- (void)mouseExited:(NSEvent*)theEvent {
  [self highlightRowAt:-1];
}

// The tracking area events aren't forwarded during a drag, so handle
// highlighting manually for middle-click and middle-drag.
- (void)otherMouseDown:(NSEvent*)theEvent {
  if ([theEvent buttonNumber] == kMiddleButtonNumber) {
    [self highlightRowUnder:theEvent];
  }
  [super otherMouseDown:theEvent];
}
- (void)otherMouseDragged:(NSEvent*)theEvent {
  if ([theEvent buttonNumber] == kMiddleButtonNumber) {
    [self highlightRowUnder:theEvent];
  }
  [super otherMouseDragged:theEvent];
}

- (void)otherMouseUp:(NSEvent*)theEvent {
  // Only intercept middle button.
  if ([theEvent buttonNumber] != kMiddleButtonNumber) {
    [super otherMouseUp:theEvent];
    return;
  }

  // -otherMouseDragged: should always have been called at this
  // location, but make sure the user is getting the right feedback.
  [self highlightRowUnder:theEvent];

  // This does the right thing if target is nil.
  [NSApp sendAction:middleClickAction_ to:[self target] from:self];
}

- (NSInteger)highlightedRow {
  NSArray* cells = [self cells];
  const NSUInteger count = [cells count];
  for(NSUInteger i = 0; i < count; ++i) {
    if ([[cells objectAtIndex:i] isHighlighted]) {
      return i;
    }
  }
  return -1;
}

- (void)setMiddleClickAction:(SEL)anAction {
  middleClickAction_ = anAction;
}

- (BOOL)isOpaque {
  return NO;
}

// This handles drawing the decorations of the rounded popup window,
// calling on NSMatrix to draw the actual contents.
- (void)drawRect:(NSRect)rect {
  NSBezierPath* path =
      [NSBezierPath bezierPathWithRoundedRect:[self bounds]
                                      xRadius:kPopupRoundingRadius
                                      yRadius:kPopupRoundingRadius];

  // Draw the matrix clipped to our border.
  [NSGraphicsContext saveGraphicsState];
  [path addClip];
  [super drawRect:rect];
  [NSGraphicsContext restoreGraphicsState];
}

@end

@implementation AutocompleteMatrixTarget

- initWithPopupView:(AutocompletePopupViewMac*)view {
  self = [super init];
  if (self) {
    popup_view_ = view;
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)select:(id)sender {
  DCHECK(popup_view_);
  popup_view_->AcceptInput();
}

- (void)middleSelect:(id)sender {
  DCHECK(popup_view_);
  popup_view_->OnMiddleClick();
}

- (void)windowDidResize:(NSNotification*)notification {
  DCHECK(popup_view_);

  // TODO(shess): UpdatePopupAppearance() is called frequently, so it
  // should be really cheap, but in this case we could probably make
  // things even cheaper by refactoring between the popup-placement
  // code and the matrix-population code.
  popup_view_->UpdatePopupAppearance();
}

@end
