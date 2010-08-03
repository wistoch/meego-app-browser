// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/localized_error.h"

#include "app/l10n_util.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLError.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebURLError;

namespace {

static const char* kRedirectLoopLearnMoreUrl =
    "http://www.google.com/support/chrome/bin/answer.py?answer=95626";

enum NAV_SUGGESTIONS {
  SUGGEST_NONE     = 0,
  SUGGEST_RELOAD   = 1 << 0,
  SUGGEST_HOSTNAME = 1 << 1,
  SUGGEST_LEARNMORE = 1 << 2,
};

struct WebErrorNetErrorMap {
  const int error_code;
  const unsigned int title_resource_id;
  const unsigned int heading_resource_id;
  const unsigned int summary_resource_id;
  const unsigned int details_resource_id;
  const int suggestions;  // Bitmap of SUGGEST_* values.
};

WebErrorNetErrorMap net_error_options[] = {
  {net::ERR_TIMED_OUT,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_TIMED_OUT,
   SUGGEST_RELOAD,
  },
  {net::ERR_CONNECTION_TIMED_OUT,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_TIMED_OUT,
   SUGGEST_RELOAD,
  },
  {net::ERR_CONNECTION_FAILED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_CONNECT_FAILED,
   SUGGEST_RELOAD,
  },
  {net::ERR_NAME_NOT_RESOLVED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_NAME_NOT_RESOLVED,
   SUGGEST_RELOAD,
  },
  {net::ERR_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_DISCONNECTED,
   SUGGEST_RELOAD,
  },
  {net::ERR_FILE_NOT_FOUND,
   IDS_ERRORPAGES_TITLE_NOT_FOUND,
   IDS_ERRORPAGES_HEADING_NOT_FOUND,
   IDS_ERRORPAGES_SUMMARY_NOT_FOUND,
   IDS_ERRORPAGES_DETAILS_FILE_NOT_FOUND,
   SUGGEST_NONE,
  },
  {net::ERR_TOO_MANY_REDIRECTS,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_TOO_MANY_REDIRECTS,
   IDS_ERRORPAGES_SUMMARY_TOO_MANY_REDIRECTS,
   IDS_ERRORPAGES_DETAILS_TOO_MANY_REDIRECTS,
   SUGGEST_RELOAD | SUGGEST_LEARNMORE,
  },
  {net::ERR_SSL_PROTOCOL_ERROR,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_SSL_PROTOCOL_ERROR,
   IDS_ERRORPAGES_SUMMARY_SSL_PROTOCOL_ERROR,
   IDS_ERRORPAGES_DETAILS_SSL_PROTOCOL_ERROR,
   SUGGEST_NONE,
  },
  {net::ERR_SSL_UNSAFE_NEGOTIATION,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_SSL_PROTOCOL_ERROR,
   IDS_ERRORPAGES_SUMMARY_SSL_PROTOCOL_ERROR,
   IDS_ERRORPAGES_DETAILS_SSL_UNSAFE_NEGOTIATION,
   SUGGEST_NONE,
  },
  {net::ERR_BAD_SSL_CLIENT_AUTH_CERT,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_BAD_SSL_CLIENT_AUTH_CERT,
   IDS_ERRORPAGES_SUMMARY_BAD_SSL_CLIENT_AUTH_CERT,
   IDS_ERRORPAGES_DETAILS_BAD_SSL_CLIENT_AUTH_CERT,
   SUGGEST_NONE,
  },
};

bool LocaleIsRTL() {
#if defined(TOOLKIT_GTK)
  // base::i18n::IsRTL() uses the GTK text direction, which doesn't work within
  // the renderer sandbox.
  return base::i18n::ICUIsRTL();
#else
  return base::i18n::IsRTL();
#endif
}

}  // namespace

void GetLocalizedErrorValues(const WebURLError& error,
                             DictionaryValue* error_strings) {
  bool rtl = LocaleIsRTL();
  error_strings->SetString("textdirection", rtl ? "rtl" : "ltr");

  // Grab strings that are applicable to all error pages
  error_strings->SetStringFromUTF16("detailsLink",
    l10n_util::GetStringUTF16(IDS_ERRORPAGES_DETAILS_LINK));
  error_strings->SetStringFromUTF16("detailsHeading",
    l10n_util::GetStringUTF16(IDS_ERRORPAGES_DETAILS_HEADING));

  // Grab the strings and settings that depend on the error type.  Init
  // options with default values.
  WebErrorNetErrorMap options = {
    0,
    IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
    IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
    IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
    IDS_ERRORPAGES_DETAILS_UNKNOWN,
    SUGGEST_NONE,
  };
  int error_code = error.reason;
  for (size_t i = 0; i < arraysize(net_error_options); ++i) {
    if (net_error_options[i].error_code == error_code) {
      memcpy(&options, &net_error_options[i], sizeof(WebErrorNetErrorMap));
      break;
    }
  }

  string16 suggestions_heading;
  if (options.suggestions != SUGGEST_NONE) {
    suggestions_heading =
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_HEADING);
  }
  error_strings->SetStringFromUTF16("suggestionsHeading", suggestions_heading);

  string16 failed_url(ASCIIToUTF16(error.unreachableURL.spec()));
  // URLs are always LTR.
  if (rtl)
    base::i18n::WrapStringWithLTRFormatting(&failed_url);
  error_strings->SetStringFromUTF16("title",
      l10n_util::GetStringFUTF16(options.title_resource_id, failed_url));
  error_strings->SetStringFromUTF16("heading",
      l10n_util::GetStringUTF16(options.heading_resource_id));

  DictionaryValue* summary = new DictionaryValue;
  summary->SetStringFromUTF16("msg",
      l10n_util::GetStringUTF16(options.summary_resource_id));
  // TODO(tc): we want the unicode url here since it's being displayed
  summary->SetStringFromUTF16("failedUrl", failed_url);
  error_strings->Set("summary", summary);

  // Error codes are expected to be negative
  DCHECK(error_code < 0);
  string16 details = l10n_util::GetStringUTF16(options.details_resource_id);
  error_strings->SetStringFromUTF16("details",
      l10n_util::GetStringFUTF16(IDS_ERRORPAGES_DETAILS_TEMPLATE,
                                 base::IntToString16(-error_code),
                                 ASCIIToUTF16(net::ErrorToString(error_code)),
                                 details));

  if (options.suggestions & SUGGEST_RELOAD) {
    DictionaryValue* suggest_reload = new DictionaryValue;
    suggest_reload->SetStringFromUTF16("msg",
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_RELOAD));
    suggest_reload->SetStringFromUTF16("reloadUrl", failed_url);
    error_strings->Set("suggestionsReload", suggest_reload);
  }

  if (options.suggestions & SUGGEST_HOSTNAME) {
    // Only show the "Go to hostname" suggestion if the failed_url has a path.
    const GURL& failed_url = error.unreachableURL;
    if (std::string() == failed_url.path()) {
      DictionaryValue* suggest_home_page = new DictionaryValue;
      suggest_home_page->SetStringFromUTF16("suggestionsHomepageMsg",
          l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_HOMEPAGE));
      string16 homepage(ASCIIToUTF16(failed_url.GetWithEmptyPath().spec()));
      // URLs are always LTR.
      if (rtl)
        base::i18n::WrapStringWithLTRFormatting(&homepage);
      suggest_home_page->SetStringFromUTF16("homePage", homepage);
      // TODO(tc): we actually want the unicode hostname
      suggest_home_page->SetString("hostName", failed_url.host());
      error_strings->Set("suggestionsHomepage", suggest_home_page);
    }
  }

  if (options.suggestions & SUGGEST_LEARNMORE) {
    GURL learn_more_url;
    switch (options.error_code) {
      case net::ERR_TOO_MANY_REDIRECTS:
        learn_more_url = GURL(kRedirectLoopLearnMoreUrl);
        break;
      default:
        break;
    }

    if (learn_more_url.is_valid()) {
      // Add the language parameter to the URL.
      std::string query = learn_more_url.query() + "&hl=" +
          WideToASCII(webkit_glue::GetWebKitLocale());
      GURL::Replacements repl;
      repl.SetQueryStr(query);
      learn_more_url = learn_more_url.ReplaceComponents(repl);

      DictionaryValue* suggest_learn_more = new DictionaryValue;
      suggest_learn_more->SetStringFromUTF16("msg",
          l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_LEARNMORE));
      suggest_learn_more->SetString("learnMoreUrl", learn_more_url.spec());
      error_strings->Set("suggestionsLearnMore", suggest_learn_more);
    }
  }
}

void GetFormRepostErrorValues(const GURL& display_url,
                              DictionaryValue* error_strings) {
  bool rtl = LocaleIsRTL();
  error_strings->SetString("textdirection", rtl ? "rtl" : "ltr");

  string16 failed_url(ASCIIToUTF16(display_url.spec()));
  // URLs are always LTR.
  if (rtl)
    base::i18n::WrapStringWithLTRFormatting(&failed_url);
  error_strings->SetStringFromUTF16(
      "title", l10n_util::GetStringFUTF16(IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
                                          failed_url));
  error_strings->SetStringFromUTF16(
      "heading", l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_TITLE));
  error_strings->SetString("suggestionsHeading", "");
  DictionaryValue* summary = new DictionaryValue;
  summary->SetStringFromUTF16(
      "msg", l10n_util::GetStringUTF16(IDS_ERRORPAGES_HTTP_POST_WARNING));
  error_strings->Set("summary", summary);
}
