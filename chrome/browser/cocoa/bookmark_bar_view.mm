// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_view.h"

#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "third_party/GTM/AppKit/GTMTheme.h"
#import "third_party/mozilla/include/NSPasteboard+Utils.h"

@interface BookmarkBarView (Private)
- (void)themeDidChangeNotification:(NSNotification*)aNotification;
- (void)updateTheme:(GTMTheme*)theme;
@end

@implementation BookmarkBarView

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  // This probably isn't strictly necessary, but can't hurt.
  [self unregisterDraggedTypes];
  [super dealloc];
}

- (void)awakeFromNib {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(themeDidChangeNotification:)
                        name:kGTMThemeDidChangeNotification
                      object:nil];

  DCHECK(controller_ && "Expected this to be hooked up via Interface Builder");
  NSArray* types = [NSArray arrayWithObjects:
                              NSStringPboardType,
                              NSHTMLPboardType,
                              NSURLPboardType,
                              kBookmarkButtonDragType,
                              nil];
  [self registerForDraggedTypes:types];
}

- (void)viewDidMoveToWindow {
  if ([self window])
    [self updateTheme:[self gtm_theme]];
}

// Called after the current theme has changed.
- (void)themeDidChangeNotification:(NSNotification*)aNotification {
  GTMTheme* theme = [aNotification object];
  [self updateTheme:theme];
}

// Adapt appearance to the current theme. Called after theme changes and before
// this is shown for the first time.
- (void)updateTheme:(GTMTheme*)theme {
  NSColor* color = [theme textColorForStyle:GTMThemeStyleBookmarksBarButton
                                      state:GTMThemeStateActiveWindow];
  [noItemTextfield_ setTextColor:color];
}

// Mouse down events on the bookmark bar should not allow dragging the parent
// window around.
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

-(NSTextField*)noItemTextfield {
  return noItemTextfield_;
}

// NSDraggingDestination methods

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  if ([[info draggingPasteboard] containsURLData])
    return NSDragOperationCopy;
  if ([[info draggingPasteboard] dataForType:kBookmarkButtonDragType])
    return NSDragOperationMove;
  return NSDragOperationNone;
}

- (BOOL)wantsPeriodicDraggingUpdates {
  // TODO(port): This should probably return |YES| and the controller should
  // slide the existing bookmark buttons interactively to the side to make
  // room for the about-to-be-dropped bookmark.
  return NO;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info {
  // For now it's the same as draggingEntered:.
  // TODO(jrg): once we return YES for wantsPeriodicDraggingUpdates,
  // this should ping the controller_ to perform animations.
  return [self draggingEntered:info];
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)info {
  return YES;
}

// Implement NSDraggingDestination protocol method
// performDragOperation: for URLs.
- (BOOL)performDragOperationForURL:(id<NSDraggingInfo>)info {
  NSPasteboard* pboard = [info draggingPasteboard];
  DCHECK([pboard containsURLData]);

  NSArray* urls = nil;
  NSArray* titles = nil;
  [pboard getURLs:&urls andTitles:&titles];

  return [controller_ addURLs:urls
                   withTitles:titles
                           at:[info draggingLocation]];
}

// Implement NSDraggingDestination protocol method
// performDragOperation: for bookmarks.
- (BOOL)performDragOperationForBookmark:(id<NSDraggingInfo>)info {
  BOOL rtn = NO;
  NSData* data = [[info draggingPasteboard]
                   dataForType:kBookmarkButtonDragType];
  // [info draggingSource] is nil if not the same application.
  if (data && [info draggingSource]) {
    BookmarkButton* button = nil;
    [data getBytes:&button length:sizeof(button)];
    rtn = [controller_ dragButton:button to:[info draggingLocation]];
  }
  return rtn;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info {
  NSPasteboard* pboard = [info draggingPasteboard];
  if ([pboard containsURLData]) {
    return [self performDragOperationForURL:info];
  } else if ([pboard dataForType:kBookmarkButtonDragType]) {
    return [self performDragOperationForBookmark:info];
  } else {
    NOTREACHED() << "Unknown drop type onto bookmark bar.";
    return NO;
  }
}

@end  // @implementation BookmarkBarView


@implementation BookmarkBarView(TestingAPI)

- (void)setController:(id)controller {
  controller_ = controller;
}

@end  // @implementation BookmarkBarView(TestingAPI)
