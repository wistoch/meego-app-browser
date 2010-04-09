// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_manager.h"

#include "app/resource_bundle.h"
#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/language_state.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/translate/page_translated_details.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "grit/browser_resources.h"
#include "net/url_request/url_request_status.h"

namespace {

// Mapping from a locale name to a language code name.
// Locale names not included are translated as is.
struct LocaleToCLDLanguage {
  const char* locale_language;  // Language Chrome locale is in.
  const char* cld_language;     // Language the CLD reports.
};
LocaleToCLDLanguage kLocaleToCLDLanguages[] = {
    { "en-GB", "en" },
    { "en-US", "en" },
    { "es-419", "es" },
    { "pt-BR", "pt" },
    { "pt-PT", "pt" },
};

// The list of languages the Google translation server supports.
// For information, here is the list of languages that Chrome can be run in
// but that the translation server does not support:
// am Amharic
// bn Bengali
// gu Gujarati
// kn Kannada
// ml Malayalam
// mr Marathi
// ta Tamil
// te Telugu
const char* kSupportedLanguages[] = {
    "af",     // Afrikaans
    "sq",     // Albanian
    "ar",     // Arabic
    "be",     // Belarusian
    "bg",     // Bulgarian
    "ca",     // Catalan
    "zh-CN",  // Chinese (Simplified)
    "zh-TW",  // Chinese (Traditional)
    "hr",     // Croatian
    "cs",     // Czech
    "da",     // Danish
    "nl",     // Dutch
    "en",     // English
    "et",     // Estonian
    "fi",     // Finnish
    "fil",    // Filipino
    "fr",     // French
    "gl",     // Galician
    "de",     // German
    "el",     // Greek
    "he",     // Hebrew
    "hi",     // Hindi
    "hu",     // Hungarian
    "is",     // Icelandic
    "id",     // Indonesian
    "it",     // Italian
    "ga",     // Irish
    "ja",     // Japanese
    "ko",     // Korean
    "lv",     // Latvian
    "lt",     // Lithuanian
    "mk",     // Macedonian
    "ms",     // Malay
    "mt",     // Maltese
    "nb",     // Norwegian
    "fa",     // Persian
    "pl",     // Polish
    "pt",     // Portuguese
    "ro",     // Romanian
    "ru",     // Russian
    "sr",     // Serbian
    "sk",     // Slovak
    "sl",     // Slovenian
    "es",     // Spanish
    "sw",     // Swahili
    "sv",     // Swedish
    "th",     // Thai
    "tr",     // Turkish
    "uk",     // Ukrainian
    "vi",     // Vietnamese
    "cy",     // Welsh
    "yi",     // Yiddish
};

const char* const kTranslateScriptURL =
    "http://translate.google.com/translate_a/element.js?"
    "cb=cr.googleTranslate.onTranslateElementLoad";
const char* const kTranslateScriptHeader =
    "Google-Translate-Element-Mode: library";

}  // namespace

// static
base::LazyInstance<std::set<std::string> >
    TranslateManager::supported_languages_(base::LINKER_INITIALIZED);

TranslateManager::~TranslateManager() {
}

// static
bool TranslateManager::IsTranslatableURL(const GURL& url) {
  return !url.SchemeIs("chrome");
}

// static
void TranslateManager::GetSupportedLanguages(
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());
  for (size_t i = 0; i < arraysize(kSupportedLanguages); ++i)
    languages->push_back(kSupportedLanguages[i]);
}

// static
std::string TranslateManager::GetLanguageCode(
    const std::string& chrome_locale) {
  for (size_t i = 0; i < arraysize(kLocaleToCLDLanguages); ++i) {
    if (chrome_locale == kLocaleToCLDLanguages[i].locale_language)
      return kLocaleToCLDLanguages[i].cld_language;
  }
  return chrome_locale;
}

// static
bool TranslateManager::IsSupportedLanguage(const std::string& page_language) {
  if (supported_languages_.Pointer()->empty()) {
    for (size_t i = 0; i < arraysize(kSupportedLanguages); ++i)
      supported_languages_.Pointer()->insert(kSupportedLanguages[i]);
  }
  return supported_languages_.Pointer()->find(page_language) !=
      supported_languages_.Pointer()->end();
}

void TranslateManager::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::NAV_ENTRY_COMMITTED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      NavigationController::LoadCommittedDetails* load_details =
          Details<NavigationController::LoadCommittedDetails>(details).ptr();
      NavigationEntry* entry = controller->GetActiveEntry();
      if (!entry) {
        NOTREACHED();
        return;
      }
      if (entry->transition_type() != PageTransition::RELOAD &&
          load_details->type != NavigationType::SAME_PAGE) {
        return;
      }
      // When doing a page reload, we don't get a TAB_LANGUAGE_DETERMINED
      // notification.  So we need to explictly initiate the translation.
      // Note that we delay it as the TranslateManager gets this notification
      // before the TabContents and the TabContents processing might remove the
      // current infobars.  Since InitTranslation might add an infobar, it must
      // be done after that.
      MessageLoop::current()->PostTask(FROM_HERE,
          method_factory_.NewRunnableMethod(
              &TranslateManager::InitiateTranslationPosted,
              controller->tab_contents()->render_view_host()->process()->id(),
              controller->tab_contents()->render_view_host()->routing_id(),
              controller->tab_contents()->language_state().
                  original_language()));
      break;
    }
    case NotificationType::TAB_LANGUAGE_DETERMINED: {
      TabContents* tab = Source<TabContents>(source).ptr();
      std::string language = *(Details<std::string>(details).ptr());
      // We may get this notifications multiple times.  Make sure to translate
      // only once.
      LanguageState& language_state = tab->language_state();
      if (!language_state.translation_pending() &&
          !language_state.translation_declined() &&
          !language_state.IsPageTranslated()) {
        InitiateTranslation(tab, language);
      }
      break;
    }
    case NotificationType::PAGE_TRANSLATED: {
      // Only add translate infobar if it doesn't exist; if it already exists,
      // just update the state, the actual infobar would have received the same
      //  notification and update the visual display accordingly.
      TabContents* tab = Source<TabContents>(source).ptr();
      PageTranslatedDetails* page_translated_details =
          Details<PageTranslatedDetails>(details).ptr();
      TranslateInfoBarDelegate::TranslateState state =
          (page_translated_details->error_type == TranslateErrors::NONE ?
           TranslateInfoBarDelegate::kAfterTranslate :
           TranslateInfoBarDelegate::kTranslateError);
      int i;
      for (i = 0; i < tab->infobar_delegate_count(); ++i) {
        TranslateInfoBarDelegate* info_bar =
            tab->GetInfoBarDelegateAt(i)->AsTranslateInfoBarDelegate();
        if (info_bar) {
          info_bar->UpdateState(state, page_translated_details->error_type);
          break;
        }
      }
      if (i == tab->infobar_delegate_count()) {
        NavigationEntry* entry = tab->controller().GetActiveEntry();
        if (entry) {
          AddTranslateInfoBar(tab, state, entry->url(),
                              page_translated_details->source_language,
                              page_translated_details->target_language,
                              page_translated_details->error_type);
        }
      }
      break;
    }
    case NotificationType::PROFILE_DESTROYED: {
      Profile* profile = Source<Profile>(source).ptr();
      notification_registrar_.Remove(this, NotificationType::PROFILE_DESTROYED,
                                     source);
      size_t count = accept_languages_.erase(profile->GetPrefs());
      // We should know about this profile since we are listening for
      // notifications on it.
      DCHECK(count > 0);
      profile->GetPrefs()->RemovePrefObserver(prefs::kAcceptLanguages, this);
      break;
    }
    case NotificationType::PREF_CHANGED: {
      DCHECK(*Details<std::wstring>(details).ptr() == prefs::kAcceptLanguages);
      PrefService* prefs = Source<PrefService>(source).ptr();
      InitAcceptLanguages(prefs);
      break;
    }
    default:
      NOTREACHED();
  }
}

void TranslateManager::OnURLFetchComplete(const URLFetcher* source,
                                          const GURL& url,
                                          const URLRequestStatus& status,
                                          int response_code,
                                          const ResponseCookies& cookies,
                                          const std::string& data) {
  scoped_ptr<const URLFetcher> delete_ptr(source);
  DCHECK(translate_script_request_pending_);
  translate_script_request_pending_ = false;
  if (status.status() != URLRequestStatus::SUCCESS || response_code != 200)
    return;  // We could not retrieve the translate script.

  base::StringPiece str = ResourceBundle::GetSharedInstance().
      GetRawDataResource(IDR_TRANSLATE_JS);
  DCHECK(translate_script_.empty());
  str.CopyToString(&translate_script_);
  translate_script_ += "\n" + data;

  // Execute any pending requests.
  std::vector<PendingRequest>::const_iterator iter;
  for (iter = pending_requests_.begin(); iter != pending_requests_.end();
       ++iter) {
    const PendingRequest& request = *iter;
    TabContents* tab = tab_util::GetTabContentsByID(request.render_process_id,
                                                    request.render_view_id);
    if (!tab) {
      // The tab went away while we were retrieving the script.
      continue;
    }
    NavigationEntry* entry = tab->controller().GetActiveEntry();
    if (!entry || entry->page_id() != request.page_id) {
      // We navigated away from the page the translation was triggered on.
      continue;
    }

    // Translate the page.
    DoTranslatePage(tab, translate_script_,
                    request.source_lang, request.target_lang);
  }
  pending_requests_.clear();
}

// static
bool TranslateManager::IsShowingTranslateInfobar(TabContents* tab) {
  for (int i = 0; i < tab->infobar_delegate_count(); ++i) {
    if (tab->GetInfoBarDelegateAt(i)->AsTranslateInfoBarDelegate())
      return true;
  }
  return false;
}

TranslateManager::TranslateManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      translate_script_request_pending_(false) {
  notification_registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::TAB_LANGUAGE_DETERMINED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::PAGE_TRANSLATED,
                              NotificationService::AllSources());
}

void TranslateManager::InitiateTranslation(TabContents* tab,
                                           const std::string& page_lang) {
  PrefService* prefs = tab->profile()->GetPrefs();
  if (!prefs->GetBoolean(prefs::kEnableTranslate))
    return;

  NavigationEntry* entry = tab->controller().GetActiveEntry();
  if (!entry) {
    // This can happen for popups created with window.open("").
    return;
  }

  std::string target_lang = GetTargetLanguage();
  // Nothing to do if either the language Chrome is in or the language of the
  // page is not supported by the translation server.
  if (target_lang.empty() || !IsSupportedLanguage(page_lang)) {
    return;
  }

  // We don't want to translate:
  // - any Chrome specific page (New Tab Page, Download, History... pages).
  // - similar languages (ex: en-US to en).
  // - any user black-listed URLs or user selected language combination.
  // - any language the user configured as accepted languages.
  if (!IsTranslatableURL(entry->url()) || page_lang == target_lang ||
      !TranslatePrefs::CanTranslate(prefs, page_lang, entry->url()) ||
      IsAcceptLanguage(tab, page_lang)) {
    return;
  }

  // If the user has previously selected "always translate" for this language we
  // automatically translate.  Note that in incognito mode we disable that
  // feature; the user will get an infobar, so they can control whether the
  // page's text is sent to the translate server.
  std::string auto_target_lang;
  if (!tab->profile()->IsOffTheRecord() &&
      TranslatePrefs::ShouldAutoTranslate(prefs, page_lang,
          &auto_target_lang)) {
    TranslatePage(tab, page_lang, auto_target_lang);
    return;
  }

  std::string auto_translate_to = tab->language_state().AutoTranslateTo();
  if (!auto_translate_to.empty()) {
    // This page was navigated through a click from a translated page.
    TranslatePage(tab, page_lang, auto_translate_to);
    return;
  }

  // Prompts the user if he/she wants the page translated.
  AddTranslateInfoBar(tab, TranslateInfoBarDelegate::kBeforeTranslate,
                      entry->url(), page_lang, target_lang,
                      TranslateErrors::NONE);
}

void TranslateManager::InitiateTranslationPosted(int process_id,
                                                 int render_id,
                                                 const std::string& page_lang) {
  // The tab might have been closed.
  TabContents* tab = tab_util::GetTabContentsByID(process_id, render_id);
  if (!tab || tab->language_state().translation_pending())
    return;

  InitiateTranslation(tab, page_lang);
}

void TranslateManager::TranslatePage(TabContents* tab_contents,
                                     const std::string& source_lang,
                                     const std::string& target_lang) {
  NavigationEntry* entry = tab_contents->controller().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }
  if (!translate_script_.empty()) {
    DoTranslatePage(tab_contents, translate_script_, source_lang, target_lang);
    return;
  }

  // The script is not available yet.  Queue that request and query for the
  // script.  Once it is downloaded we'll do the translate.
  RenderViewHost* rvh = tab_contents->render_view_host();
  PendingRequest request;
  request.render_process_id = rvh->process()->id();
  request.render_view_id = rvh->routing_id();
  request.page_id = entry->page_id();
  request.source_lang = source_lang;
  request.target_lang = target_lang;
  pending_requests_.push_back(request);
  RequestTranslateScript();
}

void TranslateManager::RevertTranslation(TabContents* tab_contents) {
  NavigationEntry* entry = tab_contents->controller().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }
  tab_contents->render_view_host()->RevertTranslation(entry->page_id());
}

void TranslateManager::DoTranslatePage(TabContents* tab_contents,
                                       const std::string& translate_script,
                                       const std::string& source_lang,
                                       const std::string& target_lang) {
  NavigationEntry* entry = tab_contents->controller().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return;
  }

  tab_contents->language_state().set_translation_pending(true);
  tab_contents->render_view_host()->TranslatePage(entry->page_id(),
                                                  translate_script,
                                                  source_lang, target_lang);
}

bool TranslateManager::IsAcceptLanguage(TabContents* tab,
                                        const std::string& language) {
  PrefService* pref_service = tab->profile()->GetPrefs();
  PrefServiceLanguagesMap::const_iterator iter =
      accept_languages_.find(pref_service);
  if (iter == accept_languages_.end()) {
    InitAcceptLanguages(pref_service);
    // Listen for this profile going away, in which case we would need to clear
    // the accepted languages for the profile.
    notification_registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                                Source<Profile>(tab->profile()));
    // Also start listening for changes in the accept languages.
    tab->profile()->GetPrefs()->AddPrefObserver(prefs::kAcceptLanguages, this);

    iter = accept_languages_.find(pref_service);
  }

  return iter->second.count(language) != 0;
}

void TranslateManager::InitAcceptLanguages(PrefService* prefs) {
  // We have been asked for this profile, build the languages.
  std::wstring accept_langs_str = prefs->GetString(prefs::kAcceptLanguages);
  std::vector<std::string> accept_langs_list;
  LanguageSet accept_langs_set;
  SplitString(WideToASCII(accept_langs_str), ',', &accept_langs_list);
  std::vector<std::string>::const_iterator iter;
  std::string ui_lang =
      GetLanguageCode(g_browser_process->GetApplicationLocale());
  bool is_ui_english = StartsWithASCII(ui_lang, "en-", false);
  for (iter = accept_langs_list.begin();
       iter != accept_langs_list.end(); ++iter) {
    // Get rid of the locale extension if any (ex: en-US -> en), but for Chinese
    // for which the CLD reports zh-CN and zh-TW.
    std::string accept_lang(*iter);
    size_t index = iter->find("-");
    if (index != std::string::npos && *iter != "zh-CN" && *iter != "zh-TW")
      accept_lang = iter->substr(0, index);
    // Special-case English until we resolve bug 36182 properly.
    // Add English only if the UI language is not English. This will annoy
    // users of non-English Chrome who can comprehend English until English is
    // black-listed.
    // TODO(jungshik): Once we determine that it's safe to remove English from
    // the default Accept-Language values for most locales, remove this
    // special-casing.
    if (accept_lang != "en" || is_ui_english)
      accept_langs_set.insert(accept_lang);
  }
  accept_languages_[prefs] = accept_langs_set;
}

void TranslateManager::RequestTranslateScript() {
  if (translate_script_request_pending_)
    return;

  translate_script_request_pending_ = true;
  URLFetcher* fetcher = URLFetcher::Create(0, GURL(kTranslateScriptURL),
                                           URLFetcher::GET, this);
  fetcher->set_request_context(Profile::GetDefaultRequestContext());
  fetcher->set_extra_request_headers(kTranslateScriptHeader);
  fetcher->Start();
}

// static
void TranslateManager::AddTranslateInfoBar(
    TabContents* tab, TranslateInfoBarDelegate::TranslateState state,
    const GURL& url,
    const std::string& original_language, const std::string& target_language,
    TranslateErrors::Type error_type) {
  PrefService* prefs = tab->profile()->GetPrefs();
  TranslateInfoBarDelegate* infobar =
      TranslateInfoBarDelegate::Create(tab, prefs, state, url,
                                       original_language, target_language,
                                       error_type);
  if (!infobar) {
    NOTREACHED() << "Failed to create infobar for language " <<
        original_language << " and " << target_language;
    return;
  }
  tab->AddInfoBar(infobar);
}

// static
std::string TranslateManager::GetTargetLanguage() {
  std::string target_lang =
      GetLanguageCode(g_browser_process->GetApplicationLocale());
  if (IsSupportedLanguage(target_lang))
    return target_lang;
  return std::string();
}
