// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stack>

#import "chrome/browser/cocoa/bookmark_editor_base_controller.h"
#include "app/l10n_util.h"
#include "app/l10n_util_mac.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/cocoa/bookmark_all_tabs_controller.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"
#import "chrome/browser/cocoa/bookmark_tree_browser_cell.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

@interface BookmarkEditorBaseController ()

@property (retain, readwrite) NSArray* folderTreeArray;

// Return the folder tree object for the given path.
- (BookmarkFolderInfo*)folderForIndexPath:(NSIndexPath*)path;

// (Re)build the folder tree from the BookmarkModel's current state.
- (void)buildFolderTree;

// Notifies the controller that the bookmark model has changed.
- (void)modelChanged;

// Notifies the controller that a node has been removed.
- (void)nodeRemoved:(const BookmarkNode*)node
         fromParent:(const BookmarkNode*)parent;

// Given a folder node, collect an array containing BookmarkFolderInfos
// describing its subchildren which are also folders.
- (NSMutableArray*)addChildFoldersFromNode:(const BookmarkNode*)node;

// Scan the folder tree stemming from the given tree folder and create
// any newly added folders.  Pass down info for the folder which was
// selected before we began creating folders.
- (void)createNewFoldersForFolder:(BookmarkFolderInfo*)treeFolder
               selectedFolderInfo:(BookmarkFolderInfo*)selectedFolderInfo;

// Scan the folder tree looking for the given bookmark node and return
// the selection path thereto.
- (NSIndexPath*)selectionPathForNode:(const BookmarkNode*)node;

@end

// static; implemented for each platform.  Update this function for new
// classes derived from BookmarkEditorBaseController.
void BookmarkEditor::Show(gfx::NativeWindow parent_hwnd,
                          Profile* profile,
                          const BookmarkNode* parent,
                          const EditDetails& details,
                          Configuration configuration,
                          Handler* handler) {
  BookmarkEditorBaseController* controller = nil;
  if (details.type == EditDetails::NEW_FOLDER) {
    controller = [[BookmarkAllTabsController alloc]
                  initWithParentWindow:parent_hwnd
                               profile:profile
                                parent:parent
                         configuration:configuration
                               handler:handler];
  } else {
    controller = [[BookmarkEditorController alloc]
                  initWithParentWindow:parent_hwnd
                               profile:profile
                                parent:parent
                                  node:details.existing_node
                         configuration:configuration
                               handler:handler];
  }
  [controller runAsModalSheet];
}

// Adapter to tell BookmarkEditorBaseController when bookmarks change.
class BookmarkEditorBaseControllerBridge : public BookmarkModelObserver {
 public:
  BookmarkEditorBaseControllerBridge(BookmarkEditorBaseController* controller)
      : controller_(controller),
        importing_(false)
  { }

  virtual void Loaded(BookmarkModel* model) {
    [controller_ modelChanged];
  }

  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) {
    if (!importing_ && new_parent->GetChild(new_index)->is_folder())
      [controller_ modelChanged];
  }

  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) {
    if (!importing_ && parent->GetChild(index)->is_folder())
      [controller_ modelChanged];
  }

  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) {
    [controller_ nodeRemoved:node fromParent:parent];
    if (node->is_folder())
      [controller_ modelChanged];
  }

  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) {
    if (!importing_ && node->is_folder())
      [controller_ modelChanged];
  }

  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) {
    if (!importing_)
      [controller_ modelChanged];
  }

  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {
    // I care nothing for these 'favicons': I only show folders.
  }

  virtual void BookmarkImportBeginning(BookmarkModel* model) {
    importing_ = true;
  }

  // Invoked after a batch import finishes.  This tells observers to update
  // themselves if they were waiting for the update to finish.
  virtual void BookmarkImportEnding(BookmarkModel* model) {
    importing_ = false;
    [controller_ modelChanged];
  }

 private:
  BookmarkEditorBaseController* controller_;  // weak
  bool importing_;
};


#pragma mark -

@implementation BookmarkEditorBaseController

@synthesize initialName = initialName_;
@synthesize displayName = displayName_;
@synthesize okEnabled = okEnabled_;

- (id)initWithParentWindow:(NSWindow*)parentWindow
                   nibName:(NSString*)nibName
                   profile:(Profile*)profile
                    parent:(const BookmarkNode*)parent
             configuration:(BookmarkEditor::Configuration)configuration
                   handler:(BookmarkEditor::Handler*)handler {
  NSString* nibpath = [mac_util::MainAppBundle()
                        pathForResource:nibName
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    parentWindow_ = parentWindow;
    profile_ = profile;
    parentNode_ = parent;
    configuration_ = configuration;
    handler_.reset(handler);
    initialName_ = [@"" retain];
    observer_.reset(new BookmarkEditorBaseControllerBridge(self));
    [self bookmarkModel]->AddObserver(observer_.get());
  }
  return self;
}

- (void)dealloc {
  [self bookmarkModel]->RemoveObserver(observer_.get());
  [initialName_ release];
  [displayName_ release];
  [super dealloc];
}

- (void)awakeFromNib {
  [self setDisplayName:[self initialName]];

  if (configuration_ != BookmarkEditor::SHOW_TREE) {
    // Remember the tree view's height; we will shrink our frame by that much.
    NSRect frame = [[self window] frame];
    CGFloat browserHeight = [folderTreeView_ frame].size.height;
    frame.size.height -= browserHeight;
    frame.origin.y += browserHeight;
    // Remove the folder tree and "new folder" button.
    [folderTreeView_ removeFromSuperview];
    [newFolderButton_ removeFromSuperview];
    // Finally, commit the size change.
    [[self window] setFrame:frame display:YES];
  }

  // Build up a tree of the current folder configuration.
  [self buildFolderTree];
}

- (void)windowDidLoad {
  if (configuration_ == BookmarkEditor::SHOW_TREE) {
    [self selectNodeInBrowser:parentNode_];
  }
}

/* TODO(jrg):
// Implementing this informal protocol allows us to open the sheet
// somewhere other than at the top of the window.  NOTE: this means
// that I, the controller, am also the window's delegate.
- (NSRect)window:(NSWindow*)window willPositionSheet:(NSWindow*)sheet
        usingRect:(NSRect)rect {
  // adjust rect.origin.y to be the bottom of the toolbar
  return rect;
}
*/

// TODO(jrg): consider NSModalSession.
- (void)runAsModalSheet {
  [NSApp beginSheet:[self window]
     modalForWindow:parentWindow_
      modalDelegate:self
     didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
        contextInfo:nil];
}

- (BOOL)okEnabled {
  return YES;
}

- (IBAction)ok:(id)sender {
  // At least one of these two functions should be provided by derived classes.
  BOOL hasWillCommit = [self respondsToSelector:@selector(willCommit)];
  BOOL hasDidCommit = [self respondsToSelector:@selector(didCommit)];
  DCHECK(hasWillCommit || hasDidCommit);
  BOOL shouldContinue = YES;
  if (hasWillCommit) {
    NSNumber* hasWillContinue = [self performSelector:@selector(willCommit)];
    if (hasWillContinue && [hasWillContinue isKindOfClass:[NSNumber class]])
      shouldContinue = [hasWillContinue boolValue];
  }
  if (shouldContinue)
    [self createNewFolders];
  if (hasDidCommit) {
    NSNumber* hasDidContinue = [self performSelector:@selector(didCommit)];
    if (hasDidContinue && [hasDidContinue isKindOfClass:[NSNumber class]])
      shouldContinue = [hasDidContinue boolValue];
  }
  if (shouldContinue)
    [NSApp endSheet:[self window]];
}

- (IBAction)cancel:(id)sender {
  [NSApp endSheet:[self window]];
}

- (void)didEndSheet:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  [sheet close];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
}

#pragma mark Folder Tree Management

- (BookmarkModel*)bookmarkModel {
  return profile_->GetBookmarkModel();
}

- (const BookmarkNode*)parentNode {
  return parentNode_;
}

- (BookmarkFolderInfo*)folderForIndexPath:(NSIndexPath*)indexPath {
  NSUInteger pathCount = [indexPath length];
  BookmarkFolderInfo* item = nil;
  NSArray* treeNode = [self folderTreeArray];
  for (NSUInteger i = 0; i < pathCount; ++i) {
    item = [treeNode objectAtIndex:[indexPath indexAtPosition:i]];
    treeNode = [item children];
  }
  return item;
}

- (NSIndexPath*)selectedIndexPath {
  NSIndexPath* selectedIndexPath = nil;
  NSArray* selections = [self tableSelectionPaths];
  if ([selections count]) {
    DCHECK([selections count] == 1);  // Should be exactly one selection.
    selectedIndexPath = [selections objectAtIndex:0];
  }
  return selectedIndexPath;
}

- (BookmarkFolderInfo*)selectedFolder {
  BookmarkFolderInfo* item = nil;
  NSIndexPath* selectedIndexPath = [self selectedIndexPath];
  if (selectedIndexPath) {
    item = [self folderForIndexPath:selectedIndexPath];
  }
  return item;
}

- (const BookmarkNode*)selectedNode {
  const BookmarkNode* selectedNode = NULL;
  // Determine a new parent node only if the browser is showing.
  if (configuration_ == BookmarkEditor::SHOW_TREE) {
    BookmarkFolderInfo* folderInfo = [self selectedFolder];
    if (folderInfo)
      selectedNode = [folderInfo folderNode];
  } else {
    // If the tree is not showing then we use the original parent.
    selectedNode = parentNode_;
  }
  return selectedNode;
}

- (void)notifyHandlerCreatedNode:(const BookmarkNode*)node {
  if (handler_.get())
    handler_->NodeCreated(node);
}

- (NSArray*)folderTreeArray {
  return folderTreeArray_.get();
}

- (void)setFolderTreeArray:(NSArray*)folderTreeArray {
  folderTreeArray_.reset([folderTreeArray retain]);
}

- (NSArray*)tableSelectionPaths {
  return tableSelectionPaths_.get();
}

- (void)setTableSelectionPath:(NSIndexPath*)tableSelectionPath {
  [self setTableSelectionPaths:[NSArray arrayWithObject:tableSelectionPath]];
}

- (void)setTableSelectionPaths:(NSArray*)tableSelectionPaths {
  tableSelectionPaths_.reset([tableSelectionPaths retain]);
}

- (void)selectNodeInBrowser:(const BookmarkNode*)node {
  DCHECK(configuration_ == BookmarkEditor::SHOW_TREE);
  NSIndexPath* selectionPath = [self selectionPathForNode:node];
  [self willChangeValueForKey:@"okEnabled"];
  [self setTableSelectionPath:selectionPath];
  [self didChangeValueForKey:@"okEnabled"];
}

- (NSIndexPath*)selectionPathForNode:(const BookmarkNode*)desiredNode {
  // Back up the parent chaing for desiredNode, building up a stack
  // of ancestor nodes.  Then crawl down the folderTreeArray looking
  // for each ancestor in order while building up the selectionPath.
  std::stack<const BookmarkNode*> nodeStack;
  BookmarkModel* model = profile_->GetBookmarkModel();
  const BookmarkNode* rootNode = model->root_node();
  const BookmarkNode* node = desiredNode;
  while (node != rootNode) {
    DCHECK(node);
    nodeStack.push(node);
    node = node->GetParent();
  }
  NSUInteger stackSize = nodeStack.size();

  NSIndexPath* path = nil;
  NSArray* folders = [self folderTreeArray];
  while (!nodeStack.empty()) {
    node = nodeStack.top();
    nodeStack.pop();
    // Find node in the current folders array.
    NSUInteger i = 0;
    for (BookmarkFolderInfo *folderInfo in folders) {
      const BookmarkNode* testNode = [folderInfo folderNode];
      if (testNode == node) {
        path = path ? [path indexPathByAddingIndex:i] :
        [NSIndexPath indexPathWithIndex:i];
        folders = [folderInfo children];
        break;
      }
      ++i;
    }
  }
  DCHECK([path length] == stackSize);
  return path;
}

- (NSMutableArray*)addChildFoldersFromNode:(const BookmarkNode*)node {
  NSMutableArray* childFolders = nil;
  int childCount = node->GetChildCount();
  for (int i = 0; i < childCount; ++i) {
    const BookmarkNode* childNode = node->GetChild(i);
    if (childNode->type() != BookmarkNode::URL) {
      NSString* childName = base::SysWideToNSString(childNode->GetTitle());
      NSMutableArray* children = [self addChildFoldersFromNode:childNode];
      BookmarkFolderInfo* folderInfo =
          [BookmarkFolderInfo bookmarkFolderInfoWithFolderName:childName
                                                    folderNode:childNode
                                                      children:children];
      if (!childFolders)
        childFolders = [NSMutableArray arrayWithObject:folderInfo];
      else
        [childFolders addObject:folderInfo];
    }
  }
  return childFolders;
}

- (void)buildFolderTree {
  // Build up a tree of the current folder configuration.
  BookmarkModel* model = profile_->GetBookmarkModel();
  const BookmarkNode* rootNode = model->root_node();
  NSMutableArray* baseArray = [self addChildFoldersFromNode:rootNode];
  DCHECK(baseArray);
  [self setFolderTreeArray:baseArray];
}

- (void)modelChanged {
  const BookmarkNode* selectedNode = [self selectedNode];
  [self buildFolderTree];
  if (selectedNode && configuration_ == BookmarkEditor::SHOW_TREE)
    [self selectNodeInBrowser:selectedNode];
}

- (void)nodeRemoved:(const BookmarkNode*)node
         fromParent:(const BookmarkNode*)parent {
  if (node->is_folder()) {
    if (parentNode_ == node || parentNode_->HasAncestor(node)) {
      parentNode_ = [self bookmarkModel]->GetBookmarkBarNode();
      if (configuration_ != BookmarkEditor::SHOW_TREE) {
        // The user can't select a different folder, so just close up shop.
        [self cancel:self];
        return;
      }
    }

    if (configuration_ == BookmarkEditor::SHOW_TREE) {
      // For safety's sake, in case deleted node was an ancestor of selection,
      // go back to a known safe place.
      [self selectNodeInBrowser:parentNode_];
    }
  }
}

#pragma mark New Folder Handler

- (void)createNewFoldersForFolder:(BookmarkFolderInfo*)folderInfo
               selectedFolderInfo:(BookmarkFolderInfo*)selectedFolderInfo {
  NSArray* subfolders = [folderInfo children];
  const BookmarkNode* parentNode = [folderInfo folderNode];
  DCHECK(parentNode);
  NSUInteger i = 0;
  for (BookmarkFolderInfo* subFolderInfo in subfolders) {
    if ([subFolderInfo newFolder]) {
      BookmarkModel* model = [self bookmarkModel];
      const BookmarkNode* newFolder =
      model->AddGroup(parentNode, i,
                      base::SysNSStringToWide([subFolderInfo folderName]));
      [self notifyHandlerCreatedNode:newFolder];
      // Update our dictionary with the actual folder node just created.
      [subFolderInfo setFolderNode:newFolder];
      [subFolderInfo setNewFolder:NO];
      // If the newly created folder was selected, update the selection path.
      if (subFolderInfo == selectedFolderInfo) {
        NSIndexPath* selectionPath = [self selectionPathForNode:newFolder];
        [self setTableSelectionPath:selectionPath];
      }
    }
    [self createNewFoldersForFolder:subFolderInfo
                 selectedFolderInfo:selectedFolderInfo];
    ++i;
  }
}

- (IBAction)newFolder:(id)sender {
  // Create a new folder off of the selected folder node.
  BookmarkFolderInfo* parentInfo = [self selectedFolder];
  if (parentInfo) {
    NSIndexPath* selection = [self selectedIndexPath];
    NSString* newFolderName =
        l10n_util::GetNSStringWithFixup(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME);
    BookmarkFolderInfo* folderInfo =
        [BookmarkFolderInfo bookmarkFolderInfoWithFolderName:newFolderName];
    [self willChangeValueForKey:@"folderTreeArray"];
    NSMutableArray* children = [parentInfo children];
    if (children) {
      [children addObject:folderInfo];
    } else {
      children = [NSMutableArray arrayWithObject:folderInfo];
      [parentInfo setChildren:children];
    }
    [self didChangeValueForKey:@"folderTreeArray"];

    // Expose the parent folder children.
    [folderTreeView_ expandItem:parentInfo];

    // Select the new folder node and put the folder name into edit mode.
    selection = [selection indexPathByAddingIndex:[children count] - 1];
    [self setTableSelectionPath:selection];
    NSInteger row = [folderTreeView_ selectedRow];
    DCHECK(row >= 0);
    [folderTreeView_ editColumn:0 row:row withEvent:nil select:YES];
  }
}

- (void)createNewFolders {
  // Scan the tree looking for nodes marked 'newFolder' and create those nodes.
  NSArray* folderTreeArray = [self folderTreeArray];
  for (BookmarkFolderInfo *folderInfo in folderTreeArray) {
    [self createNewFoldersForFolder:folderInfo
                 selectedFolderInfo:[self selectedFolder]];
  }
}

#pragma mark For Unit Test Use Only

- (BOOL)okButtonEnabled {
  return [okButton_ isEnabled];
}

- (void)selectTestNodeInBrowser:(const BookmarkNode*)node {
  [self selectNodeInBrowser:node];
}

@end  // BookmarkEditorBaseController

@implementation BookmarkFolderInfo

@synthesize folderName = folderName_;
@synthesize folderNode = folderNode_;
@synthesize children = children_;
@synthesize newFolder = newFolder_;

+ (id)bookmarkFolderInfoWithFolderName:(NSString*)folderName
                            folderNode:(const BookmarkNode*)folderNode
                              children:(NSMutableArray*)children {
  return [[[BookmarkFolderInfo alloc] initWithFolderName:folderName
                                              folderNode:folderNode
                                                children:children
                                               newFolder:NO]
          autorelease];
}

+ (id)bookmarkFolderInfoWithFolderName:(NSString*)folderName {
  return [[[BookmarkFolderInfo alloc] initWithFolderName:folderName
                                              folderNode:NULL
                                                children:nil
                                               newFolder:YES]
          autorelease];
}

- (id)initWithFolderName:(NSString*)folderName
              folderNode:(const BookmarkNode*)folderNode
                children:(NSMutableArray*)children
               newFolder:(BOOL)newFolder {
  if ((self = [super init])) {
    // A folderName is always required, and if newFolder is NO then there
    // should be a folderNode.  Children is optional.
    DCHECK(folderName && (newFolder || folderNode));
    if (folderName && (newFolder || folderNode)) {
      folderName_ = [folderName copy];
      folderNode_ = folderNode;
      children_ = [children retain];
      newFolder_ = newFolder;
    } else {
      NOTREACHED();  // Invalid init.
      [self release];
      self = nil;
    }
  }
  return self;
}

- (id)init {
  NOTREACHED();  // Should never be called.
  return [self initWithFolderName:nil folderNode:nil children:nil newFolder:NO];
}

- (void)dealloc {
  [folderName_ release];
  [children_ release];
  [super dealloc];
}

// Implementing isEqual: allows the NSTreeController to preserve the selection
// and open/shut state of outline items when the data changes.
- (BOOL)isEqual:(id)other {
  return [other isKindOfClass:[BookmarkFolderInfo class]] &&
      folderNode_ == [(BookmarkFolderInfo*)other folderNode];
}

@end
