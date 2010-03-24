// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/cookie_prompt_window_controller.h"

#include <string>
#include <vector>

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#import "base/i18n/time_formatting.h"
#import "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/cookie_details_view_controller.h"
#include "chrome/browser/cocoa/cookie_tree_node.h"
#include "chrome/browser/cookie_modal_dialog.h"
#include "chrome/browser/cookies_tree_model.h"
#include "grit/generated_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

#pragma mark Selection Adapter

// The subpanes of the cookie details view expect to be able to bind to methods
// through a key path in the form |content.details.xxxx|. This class serves as
// an adapter that simply wraps a |CocoaCookieDetails| object. An instance of
// this class is set as the content object for cookie details view's object
// controller so that key paths are properly resolved through to the
// |CocoaCookieDetails| object for the cookie prompt.
@interface CookiePromptSelectionAdapter : NSObject {
 @private
  scoped_nsobject<CocoaCookieDetails> details_;
}

- (CocoaCookieDetails*)details;
- (id)initWithDetails:(CocoaCookieDetails*)details;
@end

@implementation CookiePromptSelectionAdapter

// The adapter assumes ownership of the details object
// in its initializer.
- (id)initWithDetails:(CocoaCookieDetails*)details {
  if ((self = [super init])) {
    details_.reset([details retain]);
  }
  return self;
}

- (CocoaCookieDetails*)details {
  return details_.get();
}

@end

#pragma mark Window Controller

@implementation CookiePromptWindowController

- (id)initWithDialog:(CookiePromptModalDialog*)dialog {
  NSString* nibpath = [mac_util::MainAppBundle()
      pathForResource:@"CookiePrompt"
               ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    dialog_ = dialog;
    CocoaCookieDetails* details = [CocoaCookieDetails
        createFromPromptModalDialog:dialog];
    selectionAdapterObject_.reset([[CookiePromptSelectionAdapter alloc]
        initWithDetails:details]);
  }
  return self;
}

// Ensures that all parameterized localized strings are filled in.
- (void)doLocalizationTweaks {
  int descriptionStringId = 0;
  switch (dialog_->dialog_type()) {
    case CookiePromptModalDialog::DIALOG_TYPE_COOKIE:
      descriptionStringId = IDS_COOKIE_ALERT_LABEL;
      break;
    case CookiePromptModalDialog::DIALOG_TYPE_LOCAL_STORAGE:
    case CookiePromptModalDialog::DIALOG_TYPE_DATABASE:
    case CookiePromptModalDialog::DIALOG_TYPE_APPCACHE:
      descriptionStringId = IDS_DATA_ALERT_LABEL;
      break;
    default:
      NOTREACHED();
      break;
  }

  string16 displayHost = UTF8ToUTF16(dialog_->origin().host());
  NSString* description = l10n_util::GetNSStringF(
      descriptionStringId, displayHost);
  [description_ setStringValue:description];

  // Add the host to the "remember for future prompts" radio button.
  NSString* allowText = l10n_util::GetNSStringWithFixup(
      IDS_COOKIE_ALERT_REMEMBER_RADIO);
  NSString* replacedCellText = base::SysUTF16ToNSString(
      ReplaceStringPlaceholders(base::SysNSStringToUTF16(allowText),
          displayHost, NULL));
  [rememberChoiceCell_ setTitle:replacedCellText];
}

// Adjust the vertical layout of the views in the window so that
// they are spaced correctly with their fully localized contents.
- (void)doLayoutTweaks {
  // Wrap the description text.
  CGFloat sizeDelta = [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedWidthTextField:description_];
  NSRect descriptionFrame = [description_ frame];
  descriptionFrame.origin.y -= sizeDelta;
  [description_ setFrame:descriptionFrame];

  // Wrap the radio buttons to fit if necessary.
  [GTMUILocalizerAndLayoutTweaker
      wrapRadioGroupForWidth:radioGroupMatrix_];
  sizeDelta += [GTMUILocalizerAndLayoutTweaker
      sizeToFitView:radioGroupMatrix_].height;
  NSRect radioGroupFrame = [radioGroupMatrix_ frame];
  radioGroupFrame.origin.y -= sizeDelta;
  [radioGroupMatrix_ setFrame:radioGroupFrame];

  // Adjust views location, they may have moved through the
  // expansion of the radio buttons and description text.
  NSRect disclosureViewFrame = [disclosureTriangleSuperView_ frame];
  disclosureViewFrame.origin.y -= sizeDelta;
  [disclosureTriangleSuperView_ setFrame:disclosureViewFrame];

  // Adjust the final window size by the size of the cookie details
  // view, since it will be initially hidden.
  NSRect detailsViewRect = [disclosedViewPlaceholder_ frame];
  sizeDelta -= detailsViewRect.size.height;

  // Final resize the window to fit all of the adjustments
  NSRect frame = [[self window] frame];
  frame.origin.y -= sizeDelta;
  frame.size.height += sizeDelta;
  [[self window] setFrame:frame display:NO animate:NO];
}

- (void)replaceCookieDetailsView {
  detailsViewController_.reset([[CookieDetailsViewController alloc] init]);

  NSRect viewFrameRect = [disclosedViewPlaceholder_ frame];
  [[disclosedViewPlaceholder_ superview]
      replaceSubview:disclosedViewPlaceholder_
                with:[detailsViewController_.get() view]];
}

- (void)awakeFromNib {
  DCHECK(disclosureTriangle_);
  DCHECK(radioGroupMatrix_);
  DCHECK(disclosedViewPlaceholder_);
  DCHECK(disclosureTriangleSuperView_);

  [self doLocalizationTweaks];
  [self doLayoutTweaks];
  [self replaceCookieDetailsView];

  [detailsViewController_ setContentObject:selectionAdapterObject_.get()];
  [detailsViewController_ shrinkViewToFit];

  [[detailsViewController_ view] setHidden:YES];
}

- (void)windowWillClose:(NSNotification*)notif {
  [self autorelease];
}

// |contextInfo| is the bridge back to the C++ CookiePromptModalDialog.
- (void)processModalDialogResult:(void*)contextInfo
                      returnCode:(int)returnCode  {
  CookiePromptModalDialog* bridge =
      reinterpret_cast<CookiePromptModalDialog*>(contextInfo);
  bool remember = [radioGroupMatrix_ selectedRow] == 0;
  switch (returnCode) {
    case NSAlertFirstButtonReturn: {  // OK
      bool sessionExpire = false;
      bridge->AllowSiteData(remember, sessionExpire);
      break;
    }
    case NSAlertSecondButtonReturn: {  // Cancel
      bridge->BlockSiteData(remember);
      break;
    }
    default:  {
      NOTREACHED();
      remember = false;
      bridge->BlockSiteData(remember);
    }
  }
}

- (void)doModalDialog:(void*)context {
  NSInteger returnCode = [NSApp runModalForWindow:[self window]];
  [self processModalDialogResult:context returnCode:returnCode];
}

- (IBAction)disclosureTrianglePressed:(id)sender {
  NSWindow* window = [self window];
  NSRect frame = [[self window] frame];
  CGFloat sizeChange = [[detailsViewController_.get() view] frame].size.height;
  switch ([sender state]) {
    case NSOnState:
      frame.size.height += sizeChange;
      frame.origin.y -= sizeChange;
      break;
    case NSOffState:
      frame.size.height -= sizeChange;
      frame.origin.y += sizeChange;
      break;
    default:
      NOTREACHED();
      break;
  }
  if ([sender state] == NSOffState) {
    [[detailsViewController_ view] setHidden:YES];
  }
  [window setFrame:frame display:YES animate:YES];
  if ([sender state] == NSOnState) {
    [[detailsViewController_ view] setHidden:NO];
  }
}

// Callback for "accept" button.
- (IBAction)accept:(id)sender {
  [[self window] close];
  [NSApp stopModalWithCode:NSAlertFirstButtonReturn];
}

// Callback for "block" button.
- (IBAction)block:(id)sender {
  [[self window] close];
  [NSApp  stopModalWithCode:NSAlertSecondButtonReturn];
}

@end
