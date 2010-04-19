// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_address_model_mac.h"
#include "app/l10n_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "grit/generated_resources.h"

@implementation AutoFillAddressModel

@dynamic summary;
@synthesize label = label_;
@synthesize firstName = firstName_;
@synthesize middleName = middleName_;
@synthesize lastName = lastName_;
@synthesize email = email_;
@synthesize companyName = companyName_;
@synthesize addressLine1 = addressLine1_;
@synthesize addressLine2 = addressLine2_;
@synthesize addressCity = addressCity_;
@synthesize addressState = addressState_;
@synthesize addressZip = addressZip_;
@synthesize addressCountry = addressCountry_;
@synthesize phoneWholeNumber = phoneWholeNumber_;
@synthesize faxWholeNumber = faxWholeNumber_;

// Sets up the KVO dependency between "summary" and dependent fields.
+ (NSSet*)keyPathsForValuesAffectingValueForKey:(NSString*)key {
  NSSet* keyPaths = [super keyPathsForValuesAffectingValueForKey:key];

  if ([key isEqualToString:@"summary"]) {
    NSSet* affectingKeys =
        [NSSet setWithObjects:@"firstName", @"lastName", @"addressLine1", nil];
    keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKeys];
  }
  return keyPaths;
}

- (id)initWithProfile:(const AutoFillProfile&)profile {
  if ((self = [super init])) {
    [self setLabel:SysUTF16ToNSString(profile.Label())];
    [self setFirstName:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(NAME_FIRST)))];
    [self setMiddleName:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(NAME_MIDDLE)))];
    [self setLastName:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(NAME_LAST)))];
    [self setEmail:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(EMAIL_ADDRESS)))];
    [self setCompanyName:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(COMPANY_NAME)))];
    [self setAddressLine1:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE1)))];
    [self setAddressLine2:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE2)))];
    [self setAddressCity:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(ADDRESS_HOME_CITY)))];
    [self setAddressState:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(ADDRESS_HOME_STATE)))];
    [self setAddressZip:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(ADDRESS_HOME_ZIP)))];
    [self setAddressCountry:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(ADDRESS_HOME_COUNTRY)))];
    [self setPhoneWholeNumber:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(PHONE_HOME_WHOLE_NUMBER)))];
    [self setFaxWholeNumber:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(PHONE_FAX_WHOLE_NUMBER)))];
  }
  return self;
}

- (void)dealloc {
  [label_ release];
  [firstName_ release];
  [middleName_ release];
  [lastName_ release];
  [email_ release];
  [companyName_ release];
  [addressLine1_ release];
  [addressLine2_ release];
  [addressCity_ release];
  [addressState_ release];
  [addressZip_ release];
  [addressCountry_ release];
  [phoneWholeNumber_ release];
  [faxWholeNumber_ release];
  [super dealloc];
}

- (NSString*)summary {
  // Create a temporary |profile| to generate summary string.
  AutoFillProfile profile(string16(), 0);
  [self copyModelToProfile:&profile];
  return SysUTF16ToNSString(profile.PreviewSummary());
}

- (void)copyModelToProfile:(AutoFillProfile*)profile {
  DCHECK(profile);
  profile->set_label(base::SysNSStringToUTF16([self label]));
  profile->SetInfo(AutoFillType(NAME_FIRST),
      base::SysNSStringToUTF16([self firstName]));
  profile->SetInfo(AutoFillType(NAME_MIDDLE),
      base::SysNSStringToUTF16([self middleName]));
  profile->SetInfo(AutoFillType(NAME_LAST),
      base::SysNSStringToUTF16([self lastName]));
  profile->SetInfo(AutoFillType(EMAIL_ADDRESS),
      base::SysNSStringToUTF16([self email]));
  profile->SetInfo(AutoFillType(COMPANY_NAME),
      base::SysNSStringToUTF16([self companyName]));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_LINE1),
      base::SysNSStringToUTF16([self addressLine1]));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_LINE2),
      base::SysNSStringToUTF16([self addressLine2]));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_CITY),
      base::SysNSStringToUTF16([self addressCity]));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_STATE),
      base::SysNSStringToUTF16([self addressState]));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_ZIP),
      base::SysNSStringToUTF16([self addressZip]));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY),
      base::SysNSStringToUTF16([self addressCountry]));
  profile->SetInfo(AutoFillType(PHONE_HOME_WHOLE_NUMBER),
      base::SysNSStringToUTF16([self phoneWholeNumber]));
  profile->SetInfo(AutoFillType(PHONE_FAX_WHOLE_NUMBER),
      base::SysNSStringToUTF16([self faxWholeNumber]));
}

@end
