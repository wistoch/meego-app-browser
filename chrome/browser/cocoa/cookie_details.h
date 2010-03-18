// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "base/scoped_nsobject.h"
#include "net/base/cookie_monster.h"

class CookieTreeNode;
class CookiePromptModalDialog;

// This enum specifies the type of information contained in the
// cookie details.
enum CocoaCookieDetailsType {
  // Represents grouping of cookie data, used in the cookie tree.
  kCocoaCookieDetailsTypeFolder = 0,

  // Detailed information about a cookie, used both in the cookie
  // tree and the cookie prompt.
  kCocoaCookieDetailsTypeCookie = 1,

  // Detailed information about a web database used for
  // display in the cookie tree.
  kCocoaCookieDetailsTypeTreeDatabase = 2,

  // Detailed information about local storage used for
  // display in the cookie tree.
  kCocoaCookieDetailsTypeTreeLocalStorage = 3,

  // Detailed information about a web database used for display
  // in the cookie prompt dialog.
  kCocoaCookieDetailsTypePromptDatabase = 4,

  // Detailed information about local storage used for display
  // in the cookie prompt dialog.
  kCocoaCookieDetailsTypePromptLocalStorage = 5
};

// This class contains all of the information that can be displayed in
// a cookie details view. Because the view uses bindings to display
// the cookie information, the methods that provide that information
// for display must be implemented directly on this class and not on any
// of its subclasses.
// If this system is rewritten to not use bindings, this class should be
// subclassed and specialized, rather than using an enum to determine type.
@interface CocoaCookieDetails : NSObject {
 @private
  CocoaCookieDetailsType type_;

  // These members are only set for type kCocoaCookieDetailsTypeCookie.
  scoped_nsobject<NSString> content_;
  scoped_nsobject<NSString> path_;
  scoped_nsobject<NSString> sendFor_;
  // Stringifed dates.
  scoped_nsobject<NSString> created_;
  scoped_nsobject<NSString> expires_;

  // These members are only set for types kCocoaCookieDetailsTypeCookie,
  // kCocoaCookieDetailsTypePromptDatabase.
  scoped_nsobject<NSString> name_;

  // Only set for types kCocoaCookieDetailsTypeTreeLocalStorage and
  // kCocoaCookieDetailsTypeTreeDatabase nodes.
  scoped_nsobject<NSString> fileSize_;
  scoped_nsobject<NSString> lastModified_;

  // These members are only set for types kCocoaCookieDetailsTypeCookie,
  // kCocoaCookieDetailsTypePromptDatabase and
  // kCocoaCookieDetailsTypePromptLocalStorage nodes.
  scoped_nsobject<NSString> domain_;

  // Used only for type kCocoaCookieTreeNodeTypeDatabaseStorage.
  scoped_nsobject<NSString> databaseDescription_;

  // Used only for type kCocoaCookieDetailsTypePromptLocalStorage
  scoped_nsobject<NSString> localStorageKey_;
  scoped_nsobject<NSString> localStorageValue_;
}

@property (readonly) CocoaCookieDetailsType type;

// The following methods are used in the bindings of subviews inside
// the cookie detail view. Note that the method that tests the
// visibility of the subview for cookie-specific information has a different
// polarity than the other visibility testing methods. This ensures that
// this subview is shown when there is no selection in the cookie tree,
// because a hidden value of |false| is generated when the key value binding
// is evaluated through a nil object. The other methods are bound using a
// |NSNegateBoolean| transformer, so that when there is a empty selection the
// hidden value is |true|.
- (BOOL)shouldHideCookieDetailsView;
- (BOOL)shouldShowLocalStorageTreeDetailsView;
- (BOOL)shouldShowDatabaseTreeDetailsView;
- (BOOL)shouldShowDatabasePromptDetailsView;
- (BOOL)shouldShowLocalStoragePromptDetailsView;

- (NSString*)name;
- (NSString*)content;
- (NSString*)domain;
- (NSString*)path;
- (NSString*)sendFor;
- (NSString*)created;
- (NSString*)expires;
- (NSString*)fileSize;
- (NSString*)lastModified;
- (NSString*)databaseDescription;
- (NSString*)localStorageKey;
- (NSString*)localStorageValue;

// Used for folders in the cookie tree.
- (id)initAsFolder;

// Used for cookie details in both the cookie tree and the cookie prompt dialog.
- (id)initWithCookie:(const net::CookieMonster::CanonicalCookie*)treeNode
              origin:(NSString*)origin;

// Used for database details in the cookie tree.
- (id)initWithDatabase:
    (const BrowsingDataDatabaseHelper::DatabaseInfo*)databaseInfo;

// Used for local storage details in the cookie tree.
- (id)initWithLocalStorage:
    (const BrowsingDataLocalStorageHelper::LocalStorageInfo*)localStorageInfo;

// Used for database details in the cookie prompt dialog.
- (id)initWithDatabase:(const std::string&)domain
                  name:(const string16&)name;

// Used for local storage details in the cookie prompt dialog.
- (id)initWithLocalStorage:(const std::string&)domain
                       key:(const string16&)key
                     value:(const string16&)value;

// A factory method to create a configured instance given a node from
// the cookie tree in |treeNode|.
+ (CocoaCookieDetails*)createFromCookieTreeNode:(CookieTreeNode*)treeNode;

// A factory method to create a configured instance given a cookie prompt
// modal dialog in |dialog|.
+ (CocoaCookieDetails*)createFromPromptModalDialog:
    (CookiePromptModalDialog*)dialog;

@end
