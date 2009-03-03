// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_blocking_page.h"

#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
SSLBlockingPage::SSLBlockingPage(SSLManager::CertError* error,
                                 Delegate* delegate)
    : InterstitialPage(error->GetWebContents(), true, error->request_url()),
      error_(error),
      delegate_(delegate),
      delegate_has_been_notified_(false) {
}

SSLBlockingPage::~SSLBlockingPage() {
  if (!delegate_has_been_notified_) {
    // The page is closed without the user having chosen what to do, default to
    // deny.
    NotifyDenyCertificate();
  }
}

std::string SSLBlockingPage::GetHTMLContents() {
  // Let's build the html error page.
  DictionaryValue strings;
  SSLErrorInfo error_info = delegate_->GetSSLErrorInfo(error_);
  strings.SetString(
      ASCIIToUTF16("title"),
      WideToUTF16Hack(l10n_util::GetString(IDS_SSL_BLOCKING_PAGE_TITLE)));
  strings.SetString(ASCIIToUTF16("headLine"),
                    WideToUTF16Hack(error_info.title()));
  strings.SetString(ASCIIToUTF16("description"),
                    WideToUTF16Hack(error_info.details()));

  strings.SetString(
      ASCIIToUTF16("moreInfoTitle"),
      WideToUTF16Hack(l10n_util::GetString(IDS_CERT_ERROR_EXTRA_INFO_TITLE)));
  SetExtraInfo(&strings, error_info.extra_information());

  strings.SetString(
      ASCIIToUTF16("proceed"),
      WideToUTF16Hack(l10n_util::GetString(IDS_SSL_BLOCKING_PAGE_PROCEED)));
  strings.SetString(
      ASCIIToUTF16("exit"),
      WideToUTF16Hack(l10n_util::GetString(IDS_SSL_BLOCKING_PAGE_EXIT)));

  strings.SetString(
      ASCIIToUTF16("textdirection"),
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
      ASCIIToUTF16("rtl") : ASCIIToUTF16("ltr"));

  static const StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_SSL_ROAD_BLOCK_HTML));

  return jstemplate_builder::GetTemplateHtml(html, &strings, "template_root");
}

void SSLBlockingPage::UpdateEntry(NavigationEntry* entry) {
#if defined(OS_WIN)
  DCHECK(tab()->type() == TAB_CONTENTS_WEB);
  WebContents* web = tab()->AsWebContents();
  const net::SSLInfo& ssl_info = error_->ssl_info();
  int cert_id = CertStore::GetSharedInstance()->StoreCert(
      ssl_info.cert, web->render_view_host()->process()->host_id());

  entry->ssl().set_security_style(SECURITY_STYLE_AUTHENTICATION_BROKEN);
  entry->ssl().set_cert_id(cert_id);
  entry->ssl().set_cert_status(ssl_info.cert_status);
  entry->ssl().set_security_bits(ssl_info.security_bits);
  NotificationService::current()->Notify(
      NotificationType::SSL_STATE_CHANGED,
      Source<NavigationController>(web->controller()),
      NotificationService::NoDetails());
#else
  NOTIMPLEMENTED();
#endif
}

void SSLBlockingPage::CommandReceived(const std::string& command) {
  if (command == "1") {
    Proceed();
  } else {
    DontProceed();
  }
}

void SSLBlockingPage::Proceed() {
  // Accepting the certificate resumes the loading of the page.
  NotifyAllowCertificate();

  // This call hides and deletes the interstitial.
  InterstitialPage::Proceed();
}

void SSLBlockingPage::DontProceed() {
  NotifyDenyCertificate();
  InterstitialPage::DontProceed();
}


void SSLBlockingPage::NotifyDenyCertificate() {
  DCHECK(!delegate_has_been_notified_);

  delegate_->OnDenyCertificate(error_);
  delegate_has_been_notified_ = true;
}

void SSLBlockingPage::NotifyAllowCertificate() {
  DCHECK(!delegate_has_been_notified_);

  delegate_->OnAllowCertificate(error_);
  delegate_has_been_notified_ = true;
}

// static
void SSLBlockingPage::SetExtraInfo(
    DictionaryValue* strings,
    const std::vector<std::wstring>& extra_info) {
  DCHECK(extra_info.size() < 5);  // We allow 5 paragraphs max.
  const string16 keys[5] = {
    ASCIIToUTF16("moreInfo1"), ASCIIToUTF16("moreInfo2"),
    ASCIIToUTF16("moreInfo3"), ASCIIToUTF16("moreInfo4"),
    ASCIIToUTF16("moreInfo5")
  };
  int i;
  for (i = 0; i < static_cast<int>(extra_info.size()); i++) {
    strings->SetString(keys[i], WideToUTF16Hack(extra_info[i]));
  }
  for (;i < 5; i++) {
    strings->SetString(keys[i], ASCIIToUTF16(""));
  }
}
