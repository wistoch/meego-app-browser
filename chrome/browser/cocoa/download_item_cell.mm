// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_item_cell.h"

#include "app/gfx/canvas_paint.h"
#include "app/gfx/text_elider.h"
#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/download_item_cell.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

namespace {

// Distance from top border to icon
const CGFloat kImagePaddingTop = 7;

// Distance from left border to icon
const CGFloat kImagePaddingLeft = 9;

// Width of icon
const CGFloat kImageWidth = 16;

// Height of icon
const CGFloat kImageHeight = 16;

// x coordinate of download name string, in view coords
const CGFloat kTextPosLeft = kImagePaddingLeft +
    kImageWidth + download_util::kSmallProgressIconOffset;

// Distance from end of download name string to dropdown area
const CGFloat kTextPaddingRight = 3;

// y coordinate of download name string, in view coords, when status message
// is visible
const CGFloat kPrimaryTextPosTop = 3;

// y coordinate of download name string, in view coords, when status message
// is not visible
const CGFloat kPrimaryTextOnlyPosTop = 10;

// y coordinate of status message, in view coords
const CGFloat kSecondaryTextPosTop = 18;

// Grey value of status text
const CGFloat kSecondaryTextColor = 0.5;

// Width of dropdown area on the right (includes 1px for the border on each
// side).
const CGFloat kDropdownAreaWidth = 14;

// Width of dropdown arrow
const CGFloat kDropdownArrowWidth = 5;

// Height of dropdown arrow
const CGFloat kDropdownArrowHeight = 3;

// Vertical displacement of dropdown area, relative to the "centered" position.
const CGFloat kDropdownAreaY = -2;

// Duration of the two-lines-to-one-line animation, in seconds
NSTimeInterval kHideStatusDuration = 0.3;

// Duration of the 'download complete' animation, in seconds
const int kCompleteAnimationDuration = 2.5;

}

// This is a helper class to animate the fading out of the status text.
@interface DownloadItemCellAnimation : NSAnimation {
  DownloadItemCell* cell_;
}
- (id)initWithDownloadItemCell:(DownloadItemCell*)cell
                      duration:(NSTimeInterval)duration
                animationCurve:(NSAnimationCurve)animationCurve;
@end

@interface DownloadItemCell(Private)
- (void)updateTrackingAreas:(id)sender;
- (void)hideSecondaryTitle;
- (void)animation:(NSAnimation*)animation
       progressed:(NSAnimationProgress)progress;
- (NSString*)elideTitle:(int)availableWidth;
- (NSString*)elideStatus:(int)availableWidth;
@end

@implementation DownloadItemCell

@synthesize secondaryTitle = secondaryTitle_;
@synthesize secondaryFont = secondaryFont_;

- (void)setInitialState {
  isStatusTextVisible_ = NO;
  titleY_ = kPrimaryTextPosTop;
  statusAlpha_ = 1.0;

  [self setFont:[NSFont systemFontOfSize:
      [NSFont systemFontSizeForControlSize:NSSmallControlSize]]];
  [self setSecondaryFont:[NSFont systemFontOfSize:
      [NSFont systemFontSizeForControlSize:NSSmallControlSize]]];

  [self updateTrackingAreas:self];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(updateTrackingAreas:)
             name:NSViewFrameDidChangeNotification
           object:[self controlView]];
}

// For nib instantiations
- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder])) {
    [self setInitialState];
  }
  return self;
}

// For programmatic instantiations.
- (id)initTextCell:(NSString *)string {
  if ((self = [super initTextCell:string])) {
    [self setInitialState];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  if ([completionAnimation_ isAnimating])
    [completionAnimation_ stopAnimation];
  if ([hideStatusAnimation_ isAnimating])
    [hideStatusAnimation_ stopAnimation];
  [secondaryTitle_ release];
  [secondaryFont_ release];
  [super dealloc];
}

- (void)setStateFromDownload:(BaseDownloadItemModel*)downloadModel {
  // Set the name of the download.
  downloadPath_ = downloadModel->download()->GetFileName();

  std::wstring statusText = downloadModel->GetStatusText();
  if (statusText.empty()) {
    // Remove the status text label.
    [self hideSecondaryTitle];
    isStatusTextVisible_ = NO;
  } else {
    // Set status text.
    NSString* statusString = base::SysWideToNSString(statusText);
    [self setSecondaryTitle:statusString];
    isStatusTextVisible_ = YES;
  }

  switch (downloadModel->download()->state()) {
    case DownloadItem::COMPLETE:
      // Small downloads may start in a complete state due to asynchronous
      // notifications. In this case, we'll get a second complete notification
      // via the observers, so we ignore it and avoid creating a second complete
      // animation.
      if (completionAnimation_.get())
        break;
      completionAnimation_.reset([[DownloadItemCellAnimation alloc]
          initWithDownloadItemCell:self
                          duration:kCompleteAnimationDuration
                    animationCurve:NSAnimationLinear]);
      [completionAnimation_.get() setDelegate:self];
      [completionAnimation_.get() startAnimation];
      percentDone_ = -1;
      break;
    case DownloadItem::CANCELLED:
      percentDone_ = -1;
      break;
    case DownloadItem::IN_PROGRESS:
      percentDone_ = downloadModel->download()->is_paused() ?
          -1 : downloadModel->download()->PercentComplete();
      break;
    default:
      NOTREACHED();
  }

  [[self controlView] setNeedsDisplay:YES];
}

- (void)updateTrackingAreas:(id)sender {
  if (trackingAreaButton_) {
    [[self controlView] removeTrackingArea:trackingAreaButton_.get()];
      trackingAreaButton_.reset(nil);
  }
  if (trackingAreaDropdown_) {
    [[self controlView] removeTrackingArea:trackingAreaDropdown_.get()];
      trackingAreaDropdown_.reset(nil);
  }

  // Use two distinct tracking rects for left and right parts.
  NSRect bounds = [[self controlView] bounds];
  NSRect buttonRect, dropdownRect;
  NSDivideRect(bounds, &dropdownRect, &buttonRect,
      kDropdownAreaWidth, NSMaxXEdge);

    trackingAreaButton_.reset([[NSTrackingArea alloc]
                    initWithRect:buttonRect
                         options:(NSTrackingMouseEnteredAndExited |
                                  NSTrackingActiveInActiveApp)
                           owner:self
                      userInfo:nil]);
  [[self controlView] addTrackingArea:trackingAreaButton_.get()];

    trackingAreaDropdown_.reset([[NSTrackingArea alloc]
                    initWithRect:dropdownRect
                         options:(NSTrackingMouseEnteredAndExited |
                                  NSTrackingActiveInActiveApp)
                           owner:self
                      userInfo:nil]);
  [[self controlView] addTrackingArea:trackingAreaDropdown_.get()];
}

- (void)setShowsBorderOnlyWhileMouseInside:(BOOL)showOnly {
  // Override to make sure it doesn't do anything if it's called accidentally.
}

- (void)mouseEntered:(NSEvent*)theEvent {
  mouseInsideCount_++;
  if ([theEvent trackingArea] == trackingAreaButton_.get())
    mousePosition_ = kDownloadItemMouseOverButtonPart;
  else if ([theEvent trackingArea] == trackingAreaDropdown_.get())
    mousePosition_ = kDownloadItemMouseOverDropdownPart;
  [[self controlView] setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent *)theEvent {
  mouseInsideCount_--;
  if (mouseInsideCount_ == 0)
    mousePosition_ = kDownloadItemMouseOutside;
  [[self controlView] setNeedsDisplay:YES];
}

- (BOOL)isMouseInside {
  return mousePosition_ != kDownloadItemMouseOutside;
}

- (BOOL)isMouseOverButtonPart {
  return mousePosition_ == kDownloadItemMouseOverButtonPart;
}

- (BOOL)isButtonPartPressed {
  return [self isHighlighted]
      && mousePosition_ == kDownloadItemMouseOverButtonPart;
}

- (BOOL)isMouseOverDropdownPart {
  return mousePosition_ == kDownloadItemMouseOverDropdownPart;
}

- (BOOL)isDropdownPartPressed {
  return [self isHighlighted]
      && mousePosition_ == kDownloadItemMouseOverDropdownPart;
}

- (NSBezierPath*)leftRoundedPath:(CGFloat)radius inRect:(NSRect)rect {

  NSPoint topLeft = NSMakePoint(NSMinX(rect), NSMaxY(rect));
  NSPoint topRight = NSMakePoint(NSMaxX(rect), NSMaxY(rect));
  NSPoint bottomRight = NSMakePoint(NSMaxX(rect) , NSMinY(rect));

  NSBezierPath* path = [NSBezierPath bezierPath];
  [path moveToPoint:topRight];
  [path appendBezierPathWithArcFromPoint:topLeft
                                 toPoint:rect.origin
                                  radius:radius];
  [path appendBezierPathWithArcFromPoint:rect.origin
                                 toPoint:bottomRight
                                 radius:radius];
  [path lineToPoint:bottomRight];
  return path;
}

- (NSBezierPath*)rightRoundedPath:(CGFloat)radius inRect:(NSRect)rect {

  NSPoint topLeft = NSMakePoint(NSMinX(rect), NSMaxY(rect));
  NSPoint topRight = NSMakePoint(NSMaxX(rect), NSMaxY(rect));
  NSPoint bottomRight = NSMakePoint(NSMaxX(rect), NSMinY(rect));

  NSBezierPath* path = [NSBezierPath bezierPath];
  [path moveToPoint:rect.origin];
  [path appendBezierPathWithArcFromPoint:bottomRight
                                toPoint:topRight
                                  radius:radius];
  [path appendBezierPathWithArcFromPoint:topRight
                                toPoint:topLeft
                                 radius:radius];
  [path lineToPoint:topLeft];
  return path;
}

- (NSString*)elideTitle:(int)availableWidth {
  NSFont* font = [self font];
  gfx::Font font_chr =
      gfx::Font::CreateFont(base::SysNSStringToWide([font fontName]),
                            [font pointSize]);

  return base::SysWideToNSString(
      ElideFilename(downloadPath_, font_chr, availableWidth));
}

- (NSString*)elideStatus:(int)availableWidth {
  NSFont* font = [self secondaryFont];
  gfx::Font font_chr =
      gfx::Font::CreateFont(base::SysNSStringToWide([font fontName]),
                            [font pointSize]);

  return base::SysWideToNSString(ElideText(
      base::SysNSStringToWide([self secondaryTitle]),
      font_chr,
      availableWidth));
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  // Constants from Cole.  Will kConstant them once the feedback loop
  // is complete.
  NSRect drawFrame = NSInsetRect(cellFrame, 1.5, 1.5);
  NSRect innerFrame = NSInsetRect(cellFrame, 2, 2);

  const float radius = 5;
  NSWindow* window = [controlView window];
  BOOL active = [window isKeyWindow] || [window isMainWindow];

  GTMTheme* theme = [controlView gtm_theme];

  NSRect buttonDrawRect, dropdownDrawRect;
  NSDivideRect(drawFrame, &dropdownDrawRect, &buttonDrawRect,
      kDropdownAreaWidth, NSMaxXEdge);

  NSBezierPath* buttonInnerPath = [self
      leftRoundedPath:radius inRect:buttonDrawRect];
  NSBezierPath* buttonOuterPath = [self
      leftRoundedPath:(radius + 1)
               inRect:NSInsetRect(buttonDrawRect, -1, -1)];

  NSBezierPath* dropdownInnerPath = [self
      rightRoundedPath:radius inRect:dropdownDrawRect];
  NSBezierPath* dropdownOuterPath = [self
      rightRoundedPath:(radius + 1)
                inRect:NSInsetRect(dropdownDrawRect, -1, -1)];

  // Stroke the borders and appropriate fill gradient.
  [self drawBorderAndFillForTheme:theme
                      controlView:controlView
                        outerPath:buttonOuterPath
                        innerPath:buttonInnerPath
              showClickedGradient:[self isButtonPartPressed]
            showHighlightGradient:[self isMouseOverButtonPart]
                       hoverAlpha:0.0
                           active:active
                        cellFrame:cellFrame];

  [self drawBorderAndFillForTheme: theme
                      controlView:controlView
                        outerPath:dropdownOuterPath
                        innerPath:dropdownInnerPath
              showClickedGradient:[self isDropdownPartPressed]
            showHighlightGradient:[self isMouseOverDropdownPart]
                       hoverAlpha:0.0
                           active:active
                        cellFrame:cellFrame];

  [self drawInteriorWithFrame:innerFrame inView:controlView];
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  // Draw title
  CGFloat textWidth = cellFrame.size.width -
      (kTextPosLeft + kTextPaddingRight + kDropdownAreaWidth);
  [self setTitle:[self elideTitle:textWidth]];

  NSColor* themeTextColor = [[[self controlView] gtm_theme]
      textColorForStyle:GTMThemeStyleBookmarksBarButton
                  state:GTMThemeStateActiveWindow];

  NSColor* color = [self isButtonPartPressed]
      ? [NSColor alternateSelectedControlTextColor] : themeTextColor;
  NSString* primaryText = [self title];

  NSDictionary* primaryTextAttributes = [NSDictionary
      dictionaryWithObjectsAndKeys:
      color, NSForegroundColorAttributeName,
      [self font], NSFontAttributeName,
      nil];
  NSPoint primaryPos = NSMakePoint(
      cellFrame.origin.x + kTextPosLeft,
      titleY_);

  [primaryText drawAtPoint:primaryPos withAttributes:primaryTextAttributes];

  // Draw secondary title, if any
  if ([self secondaryTitle] != nil && statusAlpha_ > 0) {
    NSString* secondaryText = [self elideStatus:textWidth];
    NSColor* secondaryColor = [NSColor colorWithDeviceWhite:kSecondaryTextColor
                                                      alpha:statusAlpha_];
    NSDictionary* secondaryTextAttributes = [NSDictionary
        dictionaryWithObjectsAndKeys:
        secondaryColor, NSForegroundColorAttributeName,
        [self secondaryFont], NSFontAttributeName,
        nil];
    NSPoint secondaryPos = NSMakePoint(
        cellFrame.origin.x + kTextPosLeft,
        kSecondaryTextPosTop);
    [secondaryText drawAtPoint:secondaryPos
        withAttributes:secondaryTextAttributes];
  }

  // Draw progress disk
  {
    // CanvasPaint draws its content to the current NSGraphicsContext in its
    // destructor, which needs to be invoked before the icon is drawn below -
    // hence this nested block.

    // Always repaint the whole disk.
    NSPoint imagePosition = [self imageRectForBounds:cellFrame].origin;
    int x = imagePosition.x - download_util::kSmallProgressIconOffset;
    int y = imagePosition.y - download_util::kSmallProgressIconOffset;
    NSRect dirtyRect = NSMakeRect(
        x, y,
        download_util::kSmallProgressIconSize,
        download_util::kSmallProgressIconSize);

    gfx::CanvasPaint canvas(dirtyRect, false);
    canvas.set_composite_alpha(true);
    if (completionAnimation_.get()) {
      if ([completionAnimation_ isAnimating]) {
        download_util::PaintDownloadComplete(&canvas,
            x, y,
            [completionAnimation_ currentValue],
            download_util::SMALL);
      }
    } else if (percentDone_ >= 0) {
      download_util::PaintDownloadProgress(&canvas,
          x, y,
          download_util::kStartAngleDegrees,  // TODO(thakis): Animate
          percentDone_,
          download_util::SMALL);
    }
  }

  // Draw icon
  NSRect imageRect = NSZeroRect;
  imageRect.size = [[self image] size];
  [[self image] setFlipped:[controlView isFlipped]];
  [[self image] drawInRect:[self imageRectForBounds:cellFrame]
                  fromRect:imageRect
                 operation:NSCompositeSourceOver
                  fraction:[self isEnabled] ? 1.0 : 0.5];

  // Separator between button and popup parts
  CGFloat lx = NSMaxX(cellFrame) - kDropdownAreaWidth + 0.5;
  [[NSColor colorWithDeviceWhite:0.0 alpha:0.1] set];
  [NSBezierPath strokeLineFromPoint:NSMakePoint(lx, NSMinY(cellFrame) + 1)
                            toPoint:NSMakePoint(lx, NSMaxY(cellFrame) - 1)];
  [[NSColor colorWithDeviceWhite:1.0 alpha:0.1] set];
  [NSBezierPath strokeLineFromPoint:NSMakePoint(lx + 1, NSMinY(cellFrame) + 1)
                            toPoint:NSMakePoint(lx + 1, NSMaxY(cellFrame) - 1)];

  // Popup arrow. Put center of mass of the arrow in the center of the
  // dropdown area.
  CGFloat cx = NSMaxX(cellFrame) - kDropdownAreaWidth/2 + 0.5;
  CGFloat cy = NSMidY(cellFrame);
  NSPoint p1 = NSMakePoint(cx - kDropdownArrowWidth/2,
                           cy - kDropdownArrowHeight/3 + kDropdownAreaY);
  NSPoint p2 = NSMakePoint(cx + kDropdownArrowWidth/2,
                           cy - kDropdownArrowHeight/3 + kDropdownAreaY);
  NSPoint p3 = NSMakePoint(cx, cy + kDropdownArrowHeight*2/3 + kDropdownAreaY);
  NSBezierPath *triangle = [NSBezierPath bezierPath];
  [triangle moveToPoint:p1];
  [triangle lineToPoint:p2];
  [triangle lineToPoint:p3];
  [triangle closePath];

  NSGraphicsContext* context = [NSGraphicsContext currentContext];
  [context saveGraphicsState];

  scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
  [shadow setShadowColor:[NSColor whiteColor]];
  [shadow setShadowOffset:NSMakeSize(0, -1)];
  [shadow setShadowBlurRadius:0.0];
  [shadow set];

  NSColor* fill = [self isDropdownPartPressed]
      ? [NSColor alternateSelectedControlTextColor] : themeTextColor;
  [fill setFill];

  [triangle fill];

  [context restoreGraphicsState];
}

- (NSRect)imageRectForBounds:(NSRect)cellFrame {
  return NSMakeRect(cellFrame.origin.x + kImagePaddingLeft,
                    cellFrame.origin.y + kImagePaddingTop,
                    kImageWidth,
                    kImageHeight);
}

- (void)hideSecondaryTitle {
  if (isStatusTextVisible_) {
    // No core animation -- text in CA layers is not subpixel antialiased :-/
    hideStatusAnimation_.reset([[DownloadItemCellAnimation alloc]
        initWithDownloadItemCell:self
                        duration:kHideStatusDuration
                  animationCurve:NSAnimationEaseIn]);
    [hideStatusAnimation_.get() setDelegate:self];
    [hideStatusAnimation_.get() startAnimation];
  } else {
    // If the download is done so quickly that the status line is never visible,
    // don't show an animation
    [self animation:nil progressed:1.0];
  }
}

- (void)animation:(NSAnimation*)animation
      progressed:(NSAnimationProgress)progress {
  if (animation == hideStatusAnimation_ || animation == nil) {
    titleY_ = progress*kPrimaryTextOnlyPosTop +
        (1 - progress)*kPrimaryTextPosTop;
    statusAlpha_ = 1 - progress;
    [[self controlView] setNeedsDisplay:YES];
  } else if (animation == completionAnimation_) {
    [[self controlView] setNeedsDisplay:YES];
  }
}

- (void)animationDidEnd:(NSAnimation *)animation {
  if (animation == hideStatusAnimation_)
    hideStatusAnimation_.reset();
  else if (animation == completionAnimation_)
    completionAnimation_.reset();
}

@end

@implementation DownloadItemCellAnimation

- (id)initWithDownloadItemCell:(DownloadItemCell*)cell
                      duration:(NSTimeInterval)duration
                animationCurve:(NSAnimationCurve)animationCurve {
  if ((self = [super initWithDuration:duration
                       animationCurve:animationCurve])) {
    cell_ = cell;
    [self setAnimationBlockingMode:NSAnimationNonblocking];
  }
  return self;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress {
  [super setCurrentProgress:progress];
  [cell_ animation:self progressed:progress];
}

@end
