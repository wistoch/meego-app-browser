// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_credit_card_model_mac.h"
#include "app/l10n_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/autofill/credit_card.h"
#include "grit/generated_resources.h"


@implementation AutoFillCreditCardModel

@synthesize label = label_;
@synthesize nameOnCard = nameOnCard_;
@synthesize creditCardNumber = creditCardNumber_;
@synthesize expirationMonth = expirationMonth_;
@synthesize expirationYear = expirationYear_;
@synthesize billingAddress = billingAddress_;

- (id)initWithCreditCard:(const CreditCard&)creditCard {
  if ((self = [super init])) {
    [self setLabel:SysUTF16ToNSString(creditCard.Label())];
    [self setNameOnCard:SysUTF16ToNSString(
        creditCard.GetFieldText(AutoFillType(CREDIT_CARD_NAME)))];
    [self setCreditCardNumber:SysUTF16ToNSString(
        creditCard.GetFieldText(AutoFillType(CREDIT_CARD_NUMBER)))];
    [self setExpirationMonth:SysUTF16ToNSString(
        creditCard.GetFieldText(AutoFillType(CREDIT_CARD_EXP_MONTH)))];
    [self setExpirationYear:SysUTF16ToNSString(
        creditCard.GetFieldText(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR)))];
    [self setBillingAddress:SysUTF16ToNSString(
        creditCard.billing_address())];
  }
  return self;
}

- (void)dealloc {
  [label_ release];
  [nameOnCard_ release];
  [creditCardNumber_ release];
  [expirationMonth_ release];
  [expirationYear_ release];
  [billingAddress_ release];
  [super dealloc];
}

- (void)copyModelToCreditCard:(CreditCard*)creditCard {
  DCHECK(creditCard);
  creditCard->set_label(base::SysNSStringToUTF16([self label]));
  creditCard->SetInfo(AutoFillType(CREDIT_CARD_NAME),
      base::SysNSStringToUTF16([self nameOnCard]));
  creditCard->SetInfo(AutoFillType(CREDIT_CARD_NUMBER),
      base::SysNSStringToUTF16([self creditCardNumber]));
  creditCard->SetInfo(AutoFillType(CREDIT_CARD_EXP_MONTH),
      base::SysNSStringToUTF16([self expirationMonth]));
  creditCard->SetInfo(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
      base::SysNSStringToUTF16([self expirationYear]));
  creditCard->set_billing_address(
      base::SysNSStringToUTF16([self billingAddress]));
}

@end
