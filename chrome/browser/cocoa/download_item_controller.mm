// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/download_item_controller.h"

#include "base/mac_util.h"
#import "chrome/browser/cocoa/download_item_cell.h"
#include "chrome/browser/cocoa/download_item_mac.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/download_util.h"


// A class for the chromium-side part of the download shelf context menu.

class DownloadShelfContextMenuMac : public DownloadShelfContextMenu {
 public:
  DownloadShelfContextMenuMac(BaseDownloadItemModel* model)
      : DownloadShelfContextMenu(model) { }

  using DownloadShelfContextMenu::ExecuteItemCommand;
  using DownloadShelfContextMenu::ItemIsChecked;
  using DownloadShelfContextMenu::IsItemCommandEnabled;

  using DownloadShelfContextMenu::SHOW_IN_FOLDER;
  using DownloadShelfContextMenu::OPEN_WHEN_COMPLETE;
  using DownloadShelfContextMenu::ALWAYS_OPEN_TYPE;
  using DownloadShelfContextMenu::CANCEL;
};


// Implementation of DownloadItemController

@implementation DownloadItemController

- (id)initWithFrame:(NSRect)frameRect
              model:(BaseDownloadItemModel*)downloadModel
              shelf:(DownloadShelfController*)shelf {
  if ((self = [super initWithNibName:@"DownloadItem"
                              bundle:mac_util::MainAppBundle()])) {
    // Must be called before [self view], so that bridge_ is set in awakeFromNib
    bridge_.reset(new DownloadItemMac(downloadModel, self));
    menuBridge_.reset(new DownloadShelfContextMenuMac(downloadModel));

    shelf_ = shelf;

    [[self view] setFrame:frameRect];
  }
  return self;
}

- (void)awakeFromNib {
  [self setStateFromDownload:bridge_->download_model()];
  bridge_->LoadIcon();
}

- (void)setStateFromDownload:(BaseDownloadItemModel*)downloadModel {
  // TODO(thakis): handling of dangerous downloads -- crbug.com/14667

  // Set the correct popup menu.
  if (downloadModel->download()->state() == DownloadItem::COMPLETE)
    currentMenu_ = completeDownloadMenu_;
  else
    currentMenu_ = activeDownloadMenu_;

  [progressView_ setMenu:currentMenu_];  // for context menu
  [cell_ setStateFromDownload:downloadModel];
}

- (void)setIcon:(NSImage*)icon {
  [cell_ setImage:icon];
}

- (void)remove {
  // We are deleted after this!
  [shelf_ remove:self];
}

- (void)updateVisibility:(id)sender {
  // TODO(thakis): Make this prettier, by fading the items out or overlaying
  // the partial visible one with a horizontal alpha gradient -- crbug.com/17830
  NSView* view = [self view];
  NSRect containerFrame = [[view superview] frame];
  [view setHidden:(NSMaxX([view frame]) > NSWidth(containerFrame))];
}

- (IBAction)handleButtonClick:(id)sender {
  if ([cell_ isButtonPartPressed]) {
    DownloadItem* download = bridge_->download_model()->download();
    if (download->state() == DownloadItem::IN_PROGRESS) {
      download->set_open_when_complete(!download->open_when_complete());
    } else if (download->state() == DownloadItem::COMPLETE) {
      download_util::OpenDownload(download);
    }
  } else {
    // TODO(thakis): Align menu nicely with left view edge
    [NSMenu popUpContextMenu:currentMenu_
               withEvent:[NSApp currentEvent]
                 forView:progressView_];
  }
}

// Sets the enabled and checked state of a particular menu item for this
// download. We translate the NSMenuItem selection to menu selections understood
// by the non platform specific download context menu.
- (BOOL)validateMenuItem:(NSMenuItem *)item {
  SEL action = [item action];

  int actionId = 0;
  if (action == @selector(handleOpen:)) {
    actionId = DownloadShelfContextMenuMac::OPEN_WHEN_COMPLETE;
  } else if (action == @selector(handleAlwaysOpen:)) {
    actionId = DownloadShelfContextMenuMac::ALWAYS_OPEN_TYPE;
  } else if (action == @selector(handleReveal:)) {
    actionId = DownloadShelfContextMenuMac::SHOW_IN_FOLDER;
  } else if (action == @selector(handleCancel:)) {
    actionId = DownloadShelfContextMenuMac::CANCEL;
  } else {
    NOTREACHED();
    return YES;
  }

  if (menuBridge_->ItemIsChecked(actionId))
    [item setState:NSOnState];
  else
    [item setState:NSOffState];

  return menuBridge_->IsItemCommandEnabled(actionId) ? YES : NO;
}

- (IBAction)handleOpen:(id)sender {
  menuBridge_->ExecuteItemCommand(
      DownloadShelfContextMenuMac::OPEN_WHEN_COMPLETE);
}

- (IBAction)handleAlwaysOpen:(id)sender {
  menuBridge_->ExecuteItemCommand(
      DownloadShelfContextMenuMac::ALWAYS_OPEN_TYPE);
}

- (IBAction)handleReveal:(id)sender {
  menuBridge_->ExecuteItemCommand(DownloadShelfContextMenuMac::SHOW_IN_FOLDER);
}

- (IBAction)handleCancel:(id)sender {
  menuBridge_->ExecuteItemCommand(DownloadShelfContextMenuMac::CANCEL);
}

@end
