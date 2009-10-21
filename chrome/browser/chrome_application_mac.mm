// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/chrome_application_mac.h"

#import "base/histogram.h"
#import "base/logging.h"
#import "base/scoped_nsobject.h"
#import "base/sys_string_conversions.h"
#import "chrome/app/breakpad_mac.h"
#import "chrome/browser/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/cocoa/objc_method_swizzle.h"
#import "chrome/browser/renderer_host/render_widget_host_view_mac.h"

// The implementation of NSExceptions break various assumptions in the
// Chrome code.  This category defines a replacement for
// -initWithName:reason:userInfo: for purposes of forcing a break in
// the debugger when an exception is raised.  -raise sounds more
// obvious to intercept, but it doesn't catch the original throw
// because the objc runtime doesn't use it.
@interface NSException (NSExceptionSwizzle)
- (id)chromeInitWithName:(NSString *)aName
                  reason:(NSString *)aReason
                userInfo:(NSDictionary *)someUserInfo;
@end

static IMP gOriginalInitIMP = NULL;

@implementation NSException (NSExceptionSwizzle)
- (id)chromeInitWithName:(NSString *)aName
                  reason:(NSString *)aReason
                userInfo:(NSDictionary *)someUserInfo {
  // Method only called when swizzled.
  DCHECK(_cmd == @selector(initWithName:reason:userInfo:));

  // Dear reader: something you just did provoked an NSException.
  // Please check your backtrace and see if you can't file a bug with
  // a repro case.  You should be able to safely continue past the
  // NOTREACHED(), but feel free to comment it out locally if it is
  // making your job hard.
  DLOG(ERROR) << "Someone is preparing to raise an exception!  "
              << base::SysNSStringToUTF8(aName) << " *** "
              << base::SysNSStringToUTF8(aReason);
  NOTREACHED();

  // Forward to the original version.
  return gOriginalInitIMP(self, _cmd, aName, aReason, someUserInfo);
}
@end

namespace CrApplicationNSException {

// Maximum number of known named exceptions we'll support.  There is
// no central registration, but I only find about 75 possibilities in
// the system frameworks, and many of them are probably not
// interesting to track in aggregate (those relating to distributed
// objects, for instance).
const size_t kKnownNSExceptionCount = 25;

const size_t kUnknownNSException = kKnownNSExceptionCount;

size_t BinForException(NSException* exception) {
  // A list of common known exceptions.  The list position will
  // determine where they live in the histogram, so never move them
  // around, only add to the end.
  static const NSString* kKnownNSExceptionNames[] = {
    // ???
    NSGenericException,

    // Out-of-range on NSString or NSArray.
    NSRangeException,

    // Invalid arg to method, unrecognized selector.
    NSInvalidArgumentException,

    // malloc() returned null in object creation, I think.
    NSMallocException,

    nil
  };

  // Make sure our array hasn't outgrown our abilities to track it.
  DCHECK_LE(arraysize(kKnownNSExceptionNames), kKnownNSExceptionCount);

  const NSString* name = [exception name];
  for (int i = 0; kKnownNSExceptionNames[i]; ++i) {
    if (name == kKnownNSExceptionNames[i]) {
      return i;
    }
  }
  return kUnknownNSException;
}

void RecordExceptionWithUma(NSException* exception) {
  static LinearHistogram histogram("OSX.NSException", 0, kUnknownNSException,
                                   kUnknownNSException + 1);
  histogram.SetFlags(kUmaTargetedHistogramFlag);
  histogram.Add(BinForException(exception));
}

}  // CrApplicationNSException

namespace {

// Helper to make it easy to get crash keys right.
// TODO(shess): Find a better home for this.  app/breakpad_mac.h
// doesn't work.
class ScopedCrashKey {
 public:
  ScopedCrashKey(NSString* key, NSString* value)
      : crash_key_([key retain]) {
    SetCrashKeyValue(crash_key_.get(), value);
  }
  ~ScopedCrashKey() {
    ClearCrashKeyValue(crash_key_.get());
  }

 private:
  scoped_nsobject<NSString> crash_key_;
};

// Do-nothing wrapper so that we can arrange to only swizzle
// -[NSException raise] when DCHECK() is turned on (as opposed to
// replicating the preprocess logic which turns DCHECK() on).
BOOL SwizzleNSExceptionInit() {
  gOriginalInitIMP = ObjcEvilDoers::SwizzleImplementedInstanceMethods(
      [NSException class],
      @selector(initWithName:reason:userInfo:),
      @selector(chromeInitWithName:reason:userInfo:));
  return YES;
}

}  // namespace

@implementation CrApplication

- init {
  // TODO(shess): Push this somewhere where it can apply to the plugin
  // and renderer processes, and where it can intercept uncaught
  // exceptions.
  DCHECK(SwizzleNSExceptionInit());
  return [super init];
}

// -terminate: is the entry point for orderly "quit" operations in Cocoa.
// This includes the application menu's quit menu item and keyboard
// equivalent, the application's dock icon menu's quit menu item, "quit" (not
// "force quit") in the Activity Monitor, and quits triggered by user logout
// and system restart and shutdown.
//
// The default NSApplication -terminate: implementation will end the process
// by calling exit(), and thus never leave the main run loop.  This is
// unsuitable for Chrome's purposes.  Chrome depends on leaving the main
// run loop to perform a proper orderly shutdown.  This design is ingrained
// in the application and the assumptions that its code makes, and is
// entirely reasonable and works well on other platforms, but it's not
// compatible with the standard Cocoa quit sequence.  Quits originated from
// within the application can be redirected to not use -terminate:, but
// quits from elsewhere cannot be.
//
// To allow the Cocoa-based Chrome to support the standard Cocoa -terminate:
// interface, and allow all quits to cause Chrome to shut down properly
// regardless of their origin, -terminate: is overriden.  The custom
// -terminate: does not end the application with exit().  Instead, it simply
// returns after posting the normal NSApplicationWillTerminateNotification
// notification.  The application is responsible for exiting on its own in
// whatever way it deems appropriate.  In Chrome's case, the main run loop will
// end and the applicaton will exit by returning from main().
//
// This implementation of -terminate: is scaled back and is not as
// fully-featured as the implementation in NSApplication, nor is it a direct
// drop-in replacement -terminate: in most applications.  It is
// purpose-specific to Chrome.
- (void)terminate:(id)sender {
  NSApplicationTerminateReply shouldTerminate = NSTerminateNow;
  SEL selector = @selector(applicationShouldTerminate:);
  if ([[self delegate] respondsToSelector:selector])
    shouldTerminate = [[self delegate] applicationShouldTerminate:self];

  // If shouldTerminate is NSTerminateLater, the application is expected to
  // call -replyToApplicationShouldTerminate: when it knows whether or not it
  // should terminate.  If the argument is YES,
  // -replyToApplicationShouldTerminate: will call -terminate:.  This will
  // result in another call to the delegate's -applicationShouldTerminate:,
  // which would be expected to return NSTerminateNow at that point.
  if (shouldTerminate != NSTerminateNow)
    return;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSApplicationWillTerminateNotification
                    object:self];

  // Return, don't exit.  The application is responsible for exiting on its
  // own.
}

- (BOOL)sendAction:(SEL)anAction to:(id)aTarget from:(id)sender {
  // The Dock menu contains an automagic section where you can select
  // amongst open windows.  If a window is closed via JavaScript while
  // the menu is up, the menu item for that window continues to exist.
  // When a window is selected this method is called with the
  // now-freed window as |aTarget|.  Short-circuit the call if
  // |aTarget| is not a valid window.
  if (anAction == @selector(_selectWindow:)) {
    // Not using -[NSArray containsObject:] because |aTarget| may be a
    // freed object.
    BOOL found = NO;
    for (NSWindow* window in [self windows]) {
      if (window == aTarget) {
        found = YES;
        break;
      }
    }
    if (!found) {
      return NO;
    }
  }

  // When a Cocoa control is wired to a freed object, we get crashers
  // in the call to |super| with no useful information in the
  // backtrace.  Attempt to add some useful information.
  static const NSString* kActionKey = @"sendaction";

  // If the action is something generic like -commandDispatch:, then
  // the tag is essential.
  NSInteger tag = 0;
  if ([sender isKindOfClass:[NSControl class]]) {
    tag = [sender tag];
    if (tag == 0 || tag == -1) {
      tag = [sender selectedTag];
    }
  } else if ([sender isKindOfClass:[NSMenuItem class]]) {
    tag = [sender tag];
  }

  NSString* actionString = NSStringFromSelector(anAction);
  NSString* value =
        [NSString stringWithFormat:@"%@ tag %d sending %@ to %p",
                  [sender className], tag, actionString, aTarget];

  ScopedCrashKey key(kActionKey, value);
  return [super sendAction:anAction to:aTarget from:sender];
}

- (void)sendEvent:(NSEvent*)event {
  // The superclass's |sendEvent:| sends keyboard events to the menu and the key
  // view loop before dispatching them to |keyDown:|. Since we want to send keys
  // to the renderer before sending them to the menu, and we never want them to
  // the kev view loop when the web is focussed, we change this behavior.
  if ([[self keyWindow]
      isKindOfClass:[ChromeEventProcessingWindow class]]) {
    if ([static_cast<ChromeEventProcessingWindow*>([self keyWindow])
        shortcircuitEvent:event])
      return;
  }

  [super sendEvent:event];
}

// NSExceptions which are caught by the event loop are logged here.
// NSException uses setjmp/longjmp, which can be very bad for C++, so
// we attempt to track and report them.
- (void)reportException:(NSException *)anException {
  // If we throw an exception in this code, we can create an infinite
  // loop.  If we throw out of the if() without resetting
  // |reportException|, we'll stop reporting exceptions for this run.
  static BOOL reportingException = NO;
  DCHECK(!reportingException);
  if (!reportingException) {
    reportingException = YES;
    CrApplicationNSException::RecordExceptionWithUma(anException);

    // Store some human-readable information in breakpad keys in case
    // there is a crash.  Since breakpad does not provide infinite
    // storage, we track two exceptions.  The first exception thrown
    // is tracked because it may be the one which caused the system to
    // go off the rails.  The last exception thrown is tracked because
    // it may be the one most directly associated with the crash.
    static const NSString* kFirstExceptionKey = @"firstexception";
    static BOOL trackedFirstException = NO;
    static const NSString* kLastExceptionKey = @"lastexception";

    // TODO(shess): It would be useful to post some stacktrace info
    // from the exception.
    // 10.6 has -[NSException callStackSymbols]
    // 10.5 has -[NSException callStackReturnAddresses]
    // 10.5 has backtrace_symbols().
    // I've tried to combine the latter two, but got nothing useful.
    // The addresses are right, though, maybe we could train the crash
    // server to decode them for us.

    NSString* value = [NSString stringWithFormat:@"%@ reason %@",
                                [anException name], [anException reason]];
    if (!trackedFirstException) {
      SetCrashKeyValue(kFirstExceptionKey, value);
      trackedFirstException = YES;
    } else {
      SetCrashKeyValue(kLastExceptionKey, value);
    }

    reportingException = NO;
  }

  [super reportException:anException];
}

@end

namespace CrApplicationCC {

void Terminate() {
  [NSApp terminate:nil];
}

}  // namespace CrApplicationCC
