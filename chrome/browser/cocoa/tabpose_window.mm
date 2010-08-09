// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/tabpose_window.h"

#import <QuartzCore/QuartzCore.h>

#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"

const int kTopGradientHeight  = 15;

NSString* const kAnimationIdKey = @"AnimationId";
NSString* const kAnimationIdFadeIn = @"FadeIn";
NSString* const kAnimationIdFadeOut = @"FadeOut";

const CGFloat kDefaultAnimationDuration = 0.25;  // In seconds.
const CGFloat kSlomoFactor = 4;

// CAGradientLayer is 10.6-only -- roll our own.
@interface DarkGradientLayer : CALayer
- (void)drawInContext:(CGContextRef)context;
@end

@implementation DarkGradientLayer
- (void)drawInContext:(CGContextRef)context {
  scoped_cftyperef<CGColorSpaceRef> grayColorSpace(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericGray));
  CGFloat grays[] = { 0.277, 1.0, 0.39, 1.0 };
  CGFloat locations[] = { 0, 1 };
  scoped_cftyperef<CGGradientRef> gradient(CGGradientCreateWithColorComponents(
      grayColorSpace.get(), grays, locations, arraysize(locations)));
  CGPoint topLeft = CGPointMake(0.0, kTopGradientHeight);
  CGContextDrawLinearGradient(context, gradient.get(), topLeft, CGPointZero, 0);
}
@end

namespace {

class ScopedCAActionDisabler {
 public:
  ScopedCAActionDisabler() {
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithBool:YES]
                     forKey:kCATransactionDisableActions];
  }

  ~ScopedCAActionDisabler() {
    [CATransaction commit];
  }
};

class ScopedCAActionSetDuration {
 public:
  explicit ScopedCAActionSetDuration(CGFloat duration) {
    [CATransaction begin];
    [CATransaction setValue:[NSNumber numberWithFloat:duration]
                     forKey:kCATransactionAnimationDuration];
  }

  ~ScopedCAActionSetDuration() {
    [CATransaction commit];
  }
};

}  // namespace

// Given the number |n| of tiles with a desired aspect ratio of |a| and a
// desired distance |dx|, |dy| between tiles, returns how many tiles fit
// vertically into a rectangle with the dimensions |w_c|, |h_c|. This returns
// an exact solution, which is usually a fractional number.
static float FitNRectsWithAspectIntoBoundingSizeWithConstantPadding(
    int n, double a, int w_c, int h_c, int dx, int dy) {
  // We want to have the small rects have the same aspect ratio a as a full
  // tab. Let w, h be the size of a small rect, and w_c, h_c the size of the
  // container. dx, dy are the distances between small rects in x, y direction.

  // Geometry yields:
  // w_c = nx * (w + dx) - dx <=> w = (w_c + d_x) / nx - d_x
  // h_c = ny * (h + dy) - dy <=> h = (h_c + d_y) / ny - d_t
  // Plugging this into
  // a := tab_width / tab_height = w / h
  // yields
  // a = ((w_c - (nx - 1)*d_x)*ny) / (nx*(h_c - (ny - 1)*d_y))
  // Plugging in nx = n/ny and pen and paper (or wolfram alpha:
  // http://www.wolframalpha.com/input/?i=(-sqrt((d+n-a+f+n)^2-4+(a+f%2Ba+h)+(-d+n-n+w))%2Ba+f+n-d+n)/(2+a+(f%2Bh)) , (solution for nx)
  // http://www.wolframalpha.com/input/?i=+(-sqrt((a+f+n-d+n)^2-4+(d%2Bw)+(-a+f+n-a+h+n))-a+f+n%2Bd+n)/(2+(d%2Bw)) , (solution for ny)
  // ) gives us nx and ny (but the wrong root -- s/-sqrt(FOO)/sqrt(FOO)/.

  // This function returns ny.
  return (sqrt(pow(n * (a * dy - dx), 2) +
               4 * n * a * (dx + w_c) * (dy + h_c)) -
          n * (a * dy - dx))
      /
         (2 * (dx + w_c));
}

namespace tabpose {

// A tile is what is shown for a single tab in tabpose mode. It consists of a
// title, favicon, thumbnail image, and pre- and postanimation rects.
// TODO(thakis): Right now, it only consists of a thumb rect.
class Tile {
 public:
  // Returns the rectangle this thumbnail is at at the beginning of the zoom-in
  // animation. |tile| is the rectangle that's covering the whole tab area when
  // the animation starts.
  NSRect GetStartRectRelativeTo(const Tile& tile) const;
  NSRect thumb_rect() const { return thumb_rect_; }

 private:
  friend class TileSet;

  // The thumb rect includes infobars, detached thumbnail bar, web contents,
  // and devtools.
  NSRect thumb_rect_;
  NSRect start_thumb_rect_;
};

NSRect Tile::GetStartRectRelativeTo(const Tile& tile) const {
  NSRect rect = start_thumb_rect_;
  rect.origin.x -= tile.start_thumb_rect_.origin.x;
  rect.origin.y -= tile.start_thumb_rect_.origin.y;
  return rect;
}

// A tileset is responsible for owning and laying out all |Tile|s shown in a
// tabpose window.
class TileSet {
 public:
  // Fills in |tiles_|.
  void Build(TabStripModel* source_model);

  // Computes coordinates for |tiles_|.
  void Layout(NSRect containing_rect);

  int selected_index() const { return selected_index_; }
  void set_selected_index(int index);
  void ResetSelectedIndex() { selected_index_ = initial_index_; }

  const Tile& selected_tile() const { return tiles_[selected_index()]; }
  const Tile& tile_at(int index) const { return tiles_[index]; }

 private:
  std::vector<Tile> tiles_;  // Doesn't change often, hence values are fine.

  int selected_index_;
  int initial_index_;
};

void TileSet::Build(TabStripModel* source_model) {
  selected_index_ = initial_index_ = source_model->selected_index();
  tiles_.resize(source_model->count());
}

void TileSet::Layout(NSRect containing_rect) {
  int tile_count = tiles_.size();

  // Room around the tiles insde of |containing_rect|.
  const int kSmallPaddingTop = 30;
  const int kSmallPaddingLeft = 30;
  const int kSmallPaddingRight = 30;
  const int kSmallPaddingBottom = 30;

  // Room between the tiles.
  const int kSmallPaddingX = 15;
  const int kSmallPaddingY = 13;

  // Aspect ratio of the containing rect.
  CGFloat aspect = NSWidth(containing_rect) / NSHeight(containing_rect);

  // Room left in container after the outer padding is removed.
  double container_width =
      NSWidth(containing_rect) - kSmallPaddingLeft - kSmallPaddingRight;
  double container_height =
      NSHeight(containing_rect) - kSmallPaddingTop - kSmallPaddingBottom;

  // The tricky part is figuring out the size of a tab thumbnail, or since the
  // size of the containing rect is known, the number of tiles in x and y
  // direction.
  // Given are the size of the containing rect, and the number of thumbnails
  // that need to fit into that rect. The aspect ratio of the thumbnails needs
  // to be the same as that of |containing_rect|, else they will look distorted.
  // The thumbnails need to be distributed such that
  // |count_x * count_y >= tile_count|, and such that wasted space is minimized.
  //  See the comments in
  // |FitNRectsWithAspectIntoBoundingSizeWithConstantPadding()| for a more
  // detailed discussion.
  // TODO(thakis): It might be good enough to choose |count_x| and |count_y|
  //   such that count_x / count_y is roughly equal to |aspect|?
  double fny = FitNRectsWithAspectIntoBoundingSizeWithConstantPadding(
      tile_count, aspect, container_width, container_height,
      kSmallPaddingX, kSmallPaddingY);
  int count_y(roundf(fny));
  int count_x(ceilf(tile_count / float(count_y)));
  int last_row_count_x = tile_count - count_x * (count_y - 1);

  // Now that |count_x| and |count_y| are known, it's straightforward to compute
  // thumbnail width/height. See comment in
  // |FitNRectsWithAspectIntoBoundingSizeWithConstantPadding| for the derivation
  // of these two formulas.
  int small_width =
      floor((container_width + kSmallPaddingX) / float(count_x) -
            kSmallPaddingX);
  int small_height =
      floor((container_height + kSmallPaddingY) / float(count_y) -
            kSmallPaddingY);

  // |small_width / small_height| has only roughly an aspect ratio of |aspect|.
  // Shrink the thumbnail rect to make the aspect ratio fit exactly, and add
  // the extra space won by shrinking to the outer padding.
  int smallExtraPaddingLeft = 0;
  int smallExtraPaddingTop = 0;
  if (aspect > small_width/float(small_height)) {
    small_height = small_width / aspect;
    CGFloat all_tiles_height =
        (small_height + kSmallPaddingY) * count_y - kSmallPaddingY;
    smallExtraPaddingTop = (container_height - all_tiles_height)/2;
  } else {
    small_width = small_height * aspect;
    CGFloat all_tiles_width =
        (small_width + kSmallPaddingX) * count_x - kSmallPaddingX;
    smallExtraPaddingLeft = (container_width - all_tiles_width)/2;
  }

  // Compute inter-tile padding in the zoomed-out view.
  CGFloat scale_small_to_big = NSWidth(containing_rect) / float(small_width);
  CGFloat big_padding_x = kSmallPaddingX * scale_small_to_big;
  CGFloat big_padding_y = kSmallPaddingY * scale_small_to_big;

  // Now all dimensions are known. Lay out all tiles on a regular grid:
  // X X X X
  // X X X X
  // X X
  for (int row = 0, i = 0; i < tile_count; ++row) {
    for (int col = 0; col < count_x && i < tile_count; ++col, ++i) {
      // Compute the smalled, zoomed-out thumbnail rect.
      tiles_[i].thumb_rect_.size = NSMakeSize(small_width, small_height);

      int small_x = col * (small_width + kSmallPaddingX) +
                    kSmallPaddingLeft + smallExtraPaddingLeft;
      int small_y = row * (small_height + kSmallPaddingY) +
                    kSmallPaddingTop + smallExtraPaddingTop;

      tiles_[i].thumb_rect_.origin = NSMakePoint(
          small_x, NSHeight(containing_rect) - small_y - small_height);

      // Compute the big, pre-zoom thumbnail rect.
      tiles_[i].start_thumb_rect_.size = containing_rect.size;

      int big_x = col * (NSWidth(containing_rect) + big_padding_x);
      int big_y = row * (NSHeight(containing_rect) + big_padding_y);
      tiles_[i].start_thumb_rect_.origin = NSMakePoint(big_x, -big_y);
    }
  }

  // Go through last row and center it:
  // X X X X
  // X X X X
  //   X X
  int last_row_empty_tiles_x = count_x - last_row_count_x;
  CGFloat small_last_row_shift_x =
      last_row_empty_tiles_x * (small_width + kSmallPaddingX) / 2;
  CGFloat big_last_row_shift_x =
      last_row_empty_tiles_x * (NSWidth(containing_rect) + big_padding_x) / 2;
  for (int i = tile_count - last_row_count_x; i < tile_count; ++i) {
    tiles_[i].thumb_rect_.origin.x += small_last_row_shift_x;
    tiles_[i].start_thumb_rect_.origin.x += big_last_row_shift_x;
  }
}

void TileSet::set_selected_index(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, static_cast<int>(tiles_.size()));
  selected_index_ = index;
}

}  // namespace tabpose

void AnimateCALayerFrameFromTo(
    CALayer* layer, const NSRect& from, const NSRect& to,
    NSTimeInterval duration, id boundsAnimationDelegate) {
  // http://developer.apple.com/mac/library/qa/qa2008/qa1620.html
  CABasicAnimation* animation;

  animation = [CABasicAnimation animationWithKeyPath:@"bounds"];
  animation.fromValue = [NSValue valueWithRect:from];
  animation.toValue = [NSValue valueWithRect:to];
  animation.duration = duration;
  animation.timingFunction =
      [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
  animation.delegate = boundsAnimationDelegate;

  // Update the layer's bounds so the layer doesn't snap back when the animation
  // completes.
  layer.bounds = NSRectToCGRect(to);

  // Add the animation, overriding the implicit animation.
  [layer addAnimation:animation forKey:@"bounds"];

  // Prepare the animation from the current position to the new position.
  NSPoint opoint = from.origin;
  NSPoint point = to.origin;

  // Adapt to anchorPoint.
  opoint.x += NSWidth(from) * layer.anchorPoint.x;
  opoint.y += NSHeight(from) * layer.anchorPoint.y;
  point.x += NSWidth(to) * layer.anchorPoint.x;
  point.y += NSHeight(to) * layer.anchorPoint.y;

  animation = [CABasicAnimation animationWithKeyPath:@"position"];
  animation.fromValue = [NSValue valueWithPoint:opoint];
  animation.toValue = [NSValue valueWithPoint:point];
  animation.duration = duration;
  animation.timingFunction =
      [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];

  // Update the layer's position so that the layer doesn't snap back when the
  // animation completes.
  layer.position = NSPointToCGPoint(point);

  // Add the animation, overriding the implicit animation.
  [layer addAnimation:animation forKey:@"position"];
}

@interface TabposeWindow (Private)
- (id)initForWindow:(NSWindow*)parent
               rect:(NSRect)rect
              slomo:(BOOL)slomo
      tabStripModel:(TabStripModel*)tabStripModel;
- (void)setUpLayers:(NSRect)bgLayerRect slomo:(BOOL)slomo;
- (void)fadeAway:(BOOL)slomo;
- (void)selectTileAtIndex:(int)newIndex;
@end

@implementation TabposeWindow

+ (id)openTabposeFor:(NSWindow*)parent
                rect:(NSRect)rect
               slomo:(BOOL)slomo
       tabStripModel:(TabStripModel*)tabStripModel {
  // Releases itself when closed.
  return [[TabposeWindow alloc]
      initForWindow:parent rect:rect slomo:slomo tabStripModel:tabStripModel];
}

- (id)initForWindow:(NSWindow*)parent
               rect:(NSRect)rect
              slomo:(BOOL)slomo
      tabStripModel:(TabStripModel*)tabStripModel {
  NSRect frame = [parent frame];
  if ((self = [super initWithContentRect:frame
                               styleMask:NSBorderlessWindowMask
                                 backing:NSBackingStoreBuffered
                                   defer:NO])) {
    // TODO(thakis): Add a TabStripModelObserver to |tabStripModel_|.
    tabStripModel_ = tabStripModel;
    state_ = tabpose::kFadingIn;
    tileSet_.reset(new tabpose::TileSet);
    [self setReleasedWhenClosed:YES];
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setUpLayers:rect slomo:slomo];
    [self setAcceptsMouseMovedEvents:YES];
    [parent addChildWindow:self ordered:NSWindowAbove];
    [self makeKeyAndOrderFront:self];
  }
  return self;
}

- (CALayer*)selectedLayer {
  return [allLayers_ objectAtIndex:tileSet_->selected_index()];
}

- (void)selectTileAtIndex:(int)newIndex {
  // TODO(thakis): Have a nicer indicator for the current selection
  // (a blue outline, probably).
  int oldIndex = tileSet_->selected_index();
  CALayer* oldSelectedLayer = [allLayers_ objectAtIndex:oldIndex];
  oldSelectedLayer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
  CALayer* newSelectedLayer = [allLayers_ objectAtIndex:newIndex];
  newSelectedLayer.backgroundColor = CGColorGetConstantColor(kCGColorWhite);

  tileSet_->set_selected_index(newIndex);
}

- (void)setUpLayers:(NSRect)bgLayerRect slomo:(BOOL)slomo {
  // Root layer -- covers whole window.
  rootLayer_ = [CALayer layer];
  [[self contentView] setLayer:rootLayer_];
  [[self contentView] setWantsLayer:YES];

  // Background layer -- the visible part of the window.
  gray_.reset(CGColorCreateGenericGray(0.39, 1.0));
  bgLayer_ = [CALayer layer];
  bgLayer_.backgroundColor = gray_;
  bgLayer_.frame = NSRectToCGRect(bgLayerRect);
  bgLayer_.masksToBounds = YES;
  [rootLayer_ addSublayer:bgLayer_];

  // Top gradient.
  CALayer* gradientLayer = [DarkGradientLayer layer];
  gradientLayer.frame = CGRectMake(
      0,
      NSHeight(bgLayerRect) - kTopGradientHeight,
      NSWidth(bgLayerRect),
      kTopGradientHeight);
  [gradientLayer setNeedsDisplay];  // Draw once.
  [bgLayer_ addSublayer:gradientLayer];

  // Layers for the tab thumbnails.
  tileSet_->Build(tabStripModel_);
  tileSet_->Layout(bgLayerRect);

  allLayers_.reset(
      [[NSMutableArray alloc] initWithCapacity:tabStripModel_->count()]);
  for (int i = 0; i < tabStripModel_->count(); ++i) {
    CALayer* layer = [CALayer layer];

    // Background color as placeholder for now.
    layer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);

    AnimateCALayerFrameFromTo(
        layer,
        tileSet_->tile_at(i).GetStartRectRelativeTo(tileSet_->selected_tile()),
        tileSet_->tile_at(i).thumb_rect(),
        kDefaultAnimationDuration * (slomo ? kSlomoFactor : 1),
        i == tileSet_->selected_index() ? self : nil);

    // Add a delegate to one of the animations to get a notification once the
    // animations are done.
    if (i == tileSet_->selected_index()) {
      CAAnimation* animation = [layer animationForKey:@"bounds"];
      DCHECK(animation);
      [animation setValue:kAnimationIdFadeIn forKey:kAnimationIdKey];
    }

    layer.shadowRadius = 10;
    layer.shadowOffset = CGSizeMake(0, -10);

    [bgLayer_ addSublayer:layer];
    [allLayers_ addObject:layer];
  }
  [self selectTileAtIndex:tileSet_->selected_index()];
}

- (BOOL)canBecomeKeyWindow {
 return YES;
}

- (void)keyDown:(NSEvent*)event {
  // Overridden to prevent beeps.
}

- (void)keyUp:(NSEvent*)event {
  if (state_ == tabpose::kFadingOut)
    return;

  NSString* characters = [event characters];
  if ([characters length] < 1)
    return;

  unichar character = [characters characterAtIndex:0];
  switch (character) {
    case NSEnterCharacter:
    case NSNewlineCharacter:
    case NSCarriageReturnCharacter:
    case ' ':
      [self fadeAway:([event modifierFlags] & NSShiftKeyMask) != 0];
      break;
    case '\e':  // Escape
      tileSet_->ResetSelectedIndex();
      [self fadeAway:([event modifierFlags] & NSShiftKeyMask) != 0];
      break;
    // TODO(thakis): Support moving the selection via arrow keys.
  }
}

- (void)mouseMoved:(NSEvent*)event {
  int newIndex = -1;
  CGPoint p = NSPointToCGPoint([event locationInWindow]);
  for (NSUInteger i = 0; i < [allLayers_ count]; ++i) {
    CALayer* layer = [allLayers_ objectAtIndex:i];
    CGPoint lp = [layer convertPoint:p fromLayer:rootLayer_];
    if ([static_cast<CALayer*>([layer presentationLayer]) containsPoint:lp])
      newIndex = i;
  }
  if (newIndex >= 0)
    [self selectTileAtIndex:newIndex];
}

- (void)mouseDown:(NSEvent*)event {
  [self fadeAway:([event modifierFlags] & NSShiftKeyMask) != 0];
}

- (void)swipeWithEvent:(NSEvent*)event {
  if ([event deltaY] > 0.5)  // Swipe up
    [self fadeAway:([event modifierFlags] & NSShiftKeyMask) != 0];
}

- (void)close {
  // Prevent parent window from disappearing.
  [[self parentWindow] removeChildWindow:self];
  [super close];
}

- (void)commandDispatch:(id)sender {
  // Without this, -validateUserInterfaceItem: is not called.
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  // Disable all browser-related menu items.
  return NO;
}

- (void)fadeAway:(BOOL)slomo {
  if (state_ == tabpose::kFadingOut)
    return;

  state_ = tabpose::kFadingOut;
  [self setAcceptsMouseMovedEvents:NO];

  // Select chosen tab.
  tabStripModel_->SelectTabContentsAt(tileSet_->selected_index(),
                                      /*user_gesture=*/true);

  {
    ScopedCAActionDisabler disableCAActions;

    // Move the selected layer on top of all other layers.
    [self selectedLayer].zPosition = 1;

    // Running animations with shadows is slow, so turn shadows off before
    // running the exit animation.
    for (CALayer* layer in allLayers_.get())
      layer.shadowOpacity = 0.0;
  }

  // Animate layers out, all in one transaction.
  CGFloat duration = kDefaultAnimationDuration * (slomo ? kSlomoFactor : 1);
  ScopedCAActionSetDuration durationSetter(duration);
  for (NSUInteger i = 0; i < [allLayers_ count]; ++i) {
    CALayer* layer = [allLayers_ objectAtIndex:i];
    // |start_thumb_rect_| was relative to |initial_index_|, now this needs to
    // be relative to |selectedIndex_| (whose start rect was relative to
    // |initial_index_| too)
    CGRect newFrame = NSRectToCGRect(
        tileSet_->tile_at(i).GetStartRectRelativeTo(tileSet_->selected_tile()));

    // Add a delegate to one of the implicit animations to get a notification
    // once the animations are done.
    if (static_cast<int>(i) == tileSet_->selected_index()) {
      CAAnimation* animation = [CAAnimation animation];
      animation.delegate = self;
      [animation setValue:kAnimationIdFadeOut forKey:kAnimationIdKey];
      [layer addAnimation:animation forKey:@"frame"];
    }

    layer.frame = newFrame;
  }
}

- (void)animationDidStop:(CAAnimation*)animation finished:(BOOL)finished {
  NSString* animationId = [animation valueForKey:kAnimationIdKey];
  if ([animationId isEqualToString:kAnimationIdFadeIn]) {
    if (finished) {
      // If the user clicks while the fade in animation is still running,
      // |state_| is already kFadingOut. In that case, don't do anything.
      DCHECK_EQ(tabpose::kFadingIn, state_);
      state_ = tabpose::kFadedIn;

      // Running animations with shadows is slow, so turn shadows on only after
      // the animation is done.
      ScopedCAActionDisabler disableCAActions;
      for (CALayer* layer in allLayers_.get())
        layer.shadowOpacity = 0.5;
    }
  } else if ([animationId isEqualToString:kAnimationIdFadeOut]) {
    DCHECK_EQ(tabpose::kFadingOut, state_);
    [self close];
  }
}

@end
