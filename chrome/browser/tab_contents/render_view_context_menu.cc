// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu.h"

#include "app/clipboard/clipboard.h"
#include "app/clipboard/scoped_clipboard_writer.h"
#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/fonts_languages_window.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/browser_url_util.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_versus_navigate_classifier.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/spellcheck_host.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/platform_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/webmenuitem.h"
#include "third_party/WebKit/WebKit/chromium/public/WebMediaPlayerAction.h"
#include "third_party/WebKit/WebKit/chromium/public/WebContextMenuData.h"

using WebKit::WebContextMenuData;
using WebKit::WebMediaPlayerAction;

// static
bool RenderViewContextMenu::IsDevToolsURL(const GURL& url) {
  return url.SchemeIs(chrome::kChromeUIScheme) &&
      url.host() == chrome::kChromeUIDevToolsHost;
}

RenderViewContextMenu::RenderViewContextMenu(
    TabContents* tab_contents,
    const ContextMenuParams& params)
    : params_(params),
      source_tab_contents_(tab_contents),
      profile_(tab_contents->profile()) {
}

RenderViewContextMenu::~RenderViewContextMenu() {
}

// Menu construction functions -------------------------------------------------

void RenderViewContextMenu::Init() {
  InitMenu();
  DoInit();
}

void RenderViewContextMenu::InitMenu() {
  bool has_link = !params_.link_url.is_empty();
  bool has_selection = !params_.selection_text.empty();

  if (AppendCustomItems()) {
    AppendSeparator();
    AppendDeveloperItems();
    return;
  }

  // When no special node or text is selected and selection has no link,
  // show page items.
  bool is_devtools = false;
  if (params_.media_type == WebContextMenuData::MediaTypeNone &&
      !has_link &&
      !params_.is_editable &&
      !has_selection) {
    // If context is in subframe, show subframe options instead.
    if (!params_.frame_url.is_empty()) {
      is_devtools = IsDevToolsURL(params_.frame_url);
      if (!is_devtools)
        AppendFrameItems();
    } else if (!params_.page_url.is_empty()) {
      is_devtools = IsDevToolsURL(params_.page_url);
      if (!is_devtools)
        AppendPageItems();
    }
  }

  if (has_link) {
    AppendLinkItems();
    if (params_.media_type != WebContextMenuData::MediaTypeNone)
      AppendSeparator();
  }

  switch (params_.media_type) {
    case WebContextMenuData::MediaTypeNone:
      break;
    case WebContextMenuData::MediaTypeImage:
      AppendImageItems();
      break;
    case WebContextMenuData::MediaTypeVideo:
      AppendVideoItems();
      break;
    case WebContextMenuData::MediaTypeAudio:
      AppendAudioItems();
      break;
  }

  if (params_.is_editable)
    AppendEditableItems();
  else if (has_selection || has_link)
    AppendCopyItem();

  if (has_selection)
    AppendSearchProvider();

  // In the DevTools popup menu, "developer items" is normally the only section,
  // so omit the separator there.
  if (!is_devtools)
    AppendSeparator();
  AppendDeveloperItems();
}

bool RenderViewContextMenu::AppendCustomItems() {
  std::vector<WebMenuItem>& custom_items = params_.custom_items;
  for (size_t i = 0; i < custom_items.size(); ++i) {
    DCHECK(IDC_CONTENT_CONTEXT_CUSTOM_FIRST + custom_items[i].action <
        IDC_CONTENT_CONTEXT_CUSTOM_LAST);
    AppendMenuItem(custom_items[i].action + IDC_CONTENT_CONTEXT_CUSTOM_FIRST,
                   custom_items[i].label);
  }
  return custom_items.size() > 0;
}

void RenderViewContextMenu::AppendDeveloperItems() {
  if (g_browser_process->have_inspector_files())
    AppendMenuItem(IDS_CONTENT_CONTEXT_INSPECTELEMENT);
}

void RenderViewContextMenu::AppendLinkItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVELINKAS);

  if (params_.link_url.SchemeIs(chrome::kMailToScheme)) {
    AppendMenuItem(IDS_CONTENT_CONTEXT_COPYLINKLOCATION,
                   l10n_util::GetStringUTF16(
                       IDS_CONTENT_CONTEXT_COPYEMAILADDRESS));
  } else {
    AppendMenuItem(IDS_CONTENT_CONTEXT_COPYLINKLOCATION);
  }
}

void RenderViewContextMenu::AppendImageItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPYIMAGELOCATION);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPYIMAGE);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
}

void RenderViewContextMenu::AppendAudioItems() {
  AppendMediaItems();
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEAUDIOAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPYAUDIOLOCATION);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB);
}

void RenderViewContextMenu::AppendVideoItems() {
  AppendMediaItems();
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEVIDEOAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB);
}

void RenderViewContextMenu::AppendMediaItems() {
  int media_flags = params_.media_flags;
  if (media_flags & WebContextMenuData::MediaPaused) {
    AppendMenuItem(IDS_CONTENT_CONTEXT_PLAY);
  } else {
    AppendMenuItem(IDS_CONTENT_CONTEXT_PAUSE);
  }

  if (media_flags & WebContextMenuData::MediaMuted) {
    AppendMenuItem(IDS_CONTENT_CONTEXT_UNMUTE);
  } else {
    AppendMenuItem(IDS_CONTENT_CONTEXT_MUTE);
  }

  AppendCheckboxMenuItem(IDS_CONTENT_CONTEXT_LOOP,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_LOOP));
}

void RenderViewContextMenu::AppendPageItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_BACK);
  AppendMenuItem(IDS_CONTENT_CONTEXT_FORWARD);
  AppendMenuItem(IDS_CONTENT_CONTEXT_RELOAD);
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEPAGEAS);
  AppendMenuItem(IDS_CONTENT_CONTEXT_PRINT);
  AppendMenuItem(IDS_CONTENT_CONTEXT_VIEWPAGESOURCE);
  AppendMenuItem(IDS_CONTENT_CONTEXT_VIEWPAGEINFO);
}

void RenderViewContextMenu::AppendFrameItems() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_BACK);
  AppendMenuItem(IDS_CONTENT_CONTEXT_FORWARD);
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW);
  AppendMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD);
  AppendSeparator();
  // These two menu items have yet to be implemented.
  // http://code.google.com/p/chromium/issues/detail?id=11827
  // AppendMenuItem(IDS_CONTENT_CONTEXT_SAVEFRAMEAS);
  // AppendMenuItem(IDS_CONTENT_CONTEXT_PRINTFRAME);
  AppendMenuItem(IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE);
  AppendMenuItem(IDS_CONTENT_CONTEXT_VIEWFRAMEINFO);
}

void RenderViewContextMenu::AppendCopyItem() {
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPY);
}

void RenderViewContextMenu::AppendSearchProvider() {
  DCHECK(profile_);

  if (params_.selection_text.empty())
    return;

  bool is_search;
  profile_->GetSearchVersusNavigateClassifier()->Classify(
      params_.selection_text, std::wstring(), &is_search,
      &selection_navigation_url_, NULL, NULL, NULL);
  if (!selection_navigation_url_.is_valid())
    return;

  string16 printable_selection_text(
      WideToUTF16(l10n_util::TruncateString(params_.selection_text, 50)));
  // Escape "&" as "&&".
  for (size_t i = printable_selection_text.find('&'); i != string16::npos;
       i = printable_selection_text.find('&', i + 2))
    printable_selection_text.insert(i, 1, '&');

  if (is_search) {
    const TemplateURL* const default_provider =
        profile_->GetTemplateURLModel()->GetDefaultSearchProvider();
    if (!default_provider)
      return;
    AppendMenuItem(IDS_CONTENT_CONTEXT_SEARCHWEBFOR,
                   l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHWEBFOR,
                       WideToUTF16(default_provider->short_name()),
                       printable_selection_text));
  } else {
    AppendMenuItem(IDS_CONTENT_CONTEXT_GOTOURL,
                   l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_GOTOURL,
                                              printable_selection_text));
  }
}

void RenderViewContextMenu::AppendEditableItems() {
  // Append Dictionary spell check suggestions.
  for (size_t i = 0; i < params_.dictionary_suggestions.size() &&
       IDC_SPELLCHECK_SUGGESTION_0 + i <= IDC_SPELLCHECK_SUGGESTION_LAST;
       ++i) {
    AppendMenuItem(IDC_SPELLCHECK_SUGGESTION_0 + static_cast<int>(i),
                   params_.dictionary_suggestions[i]);
  }
  if (params_.dictionary_suggestions.size() > 0)
    AppendSeparator();

  // If word is misspelled, give option for "Add to dictionary"
  if (!params_.misspelled_word.empty()) {
    if (params_.dictionary_suggestions.size() == 0) {
      AppendMenuItem(0,
          l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS));
    }
    AppendMenuItem(IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY);
    AppendSeparator();
  }

  AppendMenuItem(IDS_CONTENT_CONTEXT_UNDO);
  AppendMenuItem(IDS_CONTENT_CONTEXT_REDO);
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_CUT);
  AppendMenuItem(IDS_CONTENT_CONTEXT_COPY);
  AppendMenuItem(IDS_CONTENT_CONTEXT_PASTE);
  AppendMenuItem(IDS_CONTENT_CONTEXT_DELETE);
  AppendSeparator();

  // Add Spell Check options sub menu.
  StartSubMenu(IDC_SPELLCHECK_MENU,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLCHECK_MENU));

  // Add Spell Check languages to sub menu.
  std::vector<std::string> spellcheck_languages;
  SpellCheckHost::GetSpellCheckLanguages(profile_,
      &spellcheck_languages);
  DCHECK(spellcheck_languages.size() <
         IDC_SPELLCHECK_LANGUAGES_LAST - IDC_SPELLCHECK_LANGUAGES_FIRST);
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < spellcheck_languages.size(); ++i) {
    string16 display_name(l10n_util::GetDisplayNameForLocale(
        spellcheck_languages[i], app_locale, true));
    AppendRadioMenuItem(IDC_SPELLCHECK_LANGUAGES_FIRST + i, display_name);
  }

  // Add item in the sub menu to pop up the fonts and languages options menu.
  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS);

  // Add 'Check the spelling of this field' item in the sub menu.
  AppendCheckboxMenuItem(
      IDC_CHECK_SPELLING_OF_THIS_FIELD,
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_CHECK_SPELLING_OF_THIS_FIELD));

  // Add option for showing the spelling panel if the platform spellchecker
  // supports it.
  if (SpellCheckerPlatform::SpellCheckerAvailable() &&
      SpellCheckerPlatform::SpellCheckerProvidesPanel()) {
    AppendCheckboxMenuItem(IDC_SPELLPANEL_TOGGLE,  l10n_util::GetStringUTF16(
              SpellCheckerPlatform::SpellingPanelVisible() ?
              IDS_CONTENT_CONTEXT_HIDE_SPELLING_PANEL :
              IDS_CONTENT_CONTEXT_SHOW_SPELLING_PANEL));
  }
  FinishSubMenu();

  AppendSeparator();
  AppendMenuItem(IDS_CONTENT_CONTEXT_SELECTALL);
}

// Menu delegate functions -----------------------------------------------------

bool RenderViewContextMenu::IsItemCommandEnabled(int id) const {
  // Allow Spell Check language items on sub menu for text area context menu.
  if ((id >= IDC_SPELLCHECK_LANGUAGES_FIRST) &&
      (id < IDC_SPELLCHECK_LANGUAGES_LAST)) {
    return profile_->GetPrefs()->GetBoolean(prefs::kEnableSpellCheck);
  }

  // Process custom actions range.
  if ((id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST) &&
      (id < IDC_CONTENT_CONTEXT_CUSTOM_LAST)) {
    unsigned action = id - IDC_CONTENT_CONTEXT_CUSTOM_FIRST;
    for (size_t i = 0; i < params_.custom_items.size(); ++i) {
      if (params_.custom_items[i].action == action)
        return params_.custom_items[i].enabled;
    }
    NOTREACHED();
    return false;
  }

  switch (id) {
    case IDS_CONTENT_CONTEXT_BACK:
      return source_tab_contents_->controller().CanGoBack();

    case IDS_CONTENT_CONTEXT_FORWARD:
      return source_tab_contents_->controller().CanGoForward();

    case IDS_CONTENT_CONTEXT_RELOAD:
      return source_tab_contents_->delegate()->CanReloadContents(
          source_tab_contents_);

    case IDS_CONTENT_CONTEXT_VIEWPAGESOURCE:
    case IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      return source_tab_contents_->controller().CanViewSource();

    case IDS_CONTENT_CONTEXT_INSPECTELEMENT:
    // Viewing page info is not a developer command but is meaningful for the
    // same set of pages which developer commands are meaningful for.
    case IDS_CONTENT_CONTEXT_VIEWPAGEINFO:
      return IsDevCommandEnabled(id);

    case IDS_CONTENT_CONTEXT_OPENLINKNEWTAB:
    case IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
      return params_.link_url.is_valid();

    case IDS_CONTENT_CONTEXT_COPYLINKLOCATION:
      return params_.unfiltered_link_url.is_valid();

    case IDS_CONTENT_CONTEXT_SAVELINKAS:
      return params_.link_url.is_valid() &&
             URLRequest::IsHandledURL(params_.link_url);

    case IDS_CONTENT_CONTEXT_SAVEIMAGEAS:
      return params_.src_url.is_valid() &&
             URLRequest::IsHandledURL(params_.src_url);

    case IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB:
      // The images shown in the most visited thumbnails do not currently open
      // in a new tab as they should. Disabling this context menu option for
      // now, as a quick hack, before we resolve this issue (Issue = 2608).
      // TODO(sidchat): Enable this option once this issue is resolved.
      if (params_.src_url.scheme() == chrome::kChromeUIScheme)
        return false;
      return true;

    case IDS_CONTENT_CONTEXT_FULLSCREEN:
      // TODO(ajwong): Enable fullscreen after we actually implement this.
      return false;

    // Media control commands should all be disabled if the player is in an
    // error state.
    case IDS_CONTENT_CONTEXT_PLAY:
    case IDS_CONTENT_CONTEXT_PAUSE:
    case IDS_CONTENT_CONTEXT_LOOP:
      return (params_.media_flags &
              WebContextMenuData::MediaInError) == 0;

    // Mute and unmute should also be disabled if the player has no audio.
    case IDS_CONTENT_CONTEXT_MUTE:
    case IDS_CONTENT_CONTEXT_UNMUTE:
      return (params_.media_flags &
              WebContextMenuData::MediaHasAudio) != 0 &&
             (params_.media_flags &
              WebContextMenuData::MediaInError) == 0;

    case IDS_CONTENT_CONTEXT_SAVESCREENSHOTAS:
      // TODO(ajwong): Enable save screenshot after we actually implement
      // this.
      return false;

    case IDS_CONTENT_CONTEXT_COPYAUDIOLOCATION:
    case IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION:
    case IDS_CONTENT_CONTEXT_COPYIMAGELOCATION:
      return params_.src_url.is_valid();

    case IDS_CONTENT_CONTEXT_SAVEAUDIOAS:
    case IDS_CONTENT_CONTEXT_SAVEVIDEOAS:
      return (params_.media_flags &
              WebContextMenuData::MediaCanSave) &&
             params_.src_url.is_valid() &&
             URLRequest::IsHandledURL(params_.src_url);

    case IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB:
    case IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB:
      return true;

    case IDS_CONTENT_CONTEXT_SAVEPAGEAS: {
      // Instead of using GetURL here, we use url() (which is the "real" url of
      // the page) from the NavigationEntry because its reflects their origin
      // rather than the display one (returned by GetURL) which may be
      // different (like having "view-source:" on the front).
      NavigationEntry* active_entry =
          source_tab_contents_->controller().GetActiveEntry();
      return SavePackage::IsSavableURL(
          (active_entry) ? active_entry->url() : GURL());
    }

    case IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB:
    case IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW:
      return params_.frame_url.is_valid();

    case IDS_CONTENT_CONTEXT_UNDO:
      return !!(params_.edit_flags & WebContextMenuData::CanUndo);

    case IDS_CONTENT_CONTEXT_REDO:
      return !!(params_.edit_flags & WebContextMenuData::CanRedo);

    case IDS_CONTENT_CONTEXT_CUT:
      return !!(params_.edit_flags & WebContextMenuData::CanCut);

    case IDS_CONTENT_CONTEXT_COPY:
      return !!(params_.edit_flags & WebContextMenuData::CanCopy);

    case IDS_CONTENT_CONTEXT_PASTE:
      return !!(params_.edit_flags & WebContextMenuData::CanPaste);

    case IDS_CONTENT_CONTEXT_DELETE:
      return !!(params_.edit_flags & WebContextMenuData::CanDelete);

    case IDS_CONTENT_CONTEXT_SELECTALL:
      return !!(params_.edit_flags & WebContextMenuData::CanSelectAll);

    case IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      return !profile_->IsOffTheRecord() && params_.link_url.is_valid();

    case IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD:
      return !profile_->IsOffTheRecord() && params_.frame_url.is_valid();

    case IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY:
      return !params_.misspelled_word.empty();

    case IDS_CONTENT_CONTEXT_COPYIMAGE:
    case IDS_CONTENT_CONTEXT_PRINT:
    case IDS_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDS_CONTENT_CONTEXT_GOTOURL:
    case IDC_SPELLCHECK_SUGGESTION_0:
    case IDC_SPELLCHECK_SUGGESTION_1:
    case IDC_SPELLCHECK_SUGGESTION_2:
    case IDC_SPELLCHECK_SUGGESTION_3:
    case IDC_SPELLCHECK_SUGGESTION_4:
    case IDC_SPELLCHECK_MENU:
    case IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
    case IDS_CONTENT_CONTEXT_VIEWFRAMEINFO:
      return true;

    case IDC_CHECK_SPELLING_OF_THIS_FIELD:
      return profile_->GetPrefs()->GetBoolean(prefs::kEnableSpellCheck);

    case IDS_CONTENT_CONTEXT_SAVEFRAMEAS:
    case IDS_CONTENT_CONTEXT_PRINTFRAME:
    case IDS_CONTENT_CONTEXT_ADDSEARCHENGINE:  // Not implemented.
    default:
      return false;
  }
}

bool RenderViewContextMenu::ItemIsChecked(int id) const {
  // See if the video is set to looping.
  if (id == IDS_CONTENT_CONTEXT_LOOP) {
    return (params_.media_flags &
            WebContextMenuData::MediaLoop) != 0;
  }

  // Check box for 'Check the Spelling of this field'.
  if (id == IDC_CHECK_SPELLING_OF_THIS_FIELD) {
    return (params_.spellcheck_enabled &&
            profile_->GetPrefs()->GetBoolean(prefs::kEnableSpellCheck));
  }

  // Don't bother getting the display language vector if this isn't a spellcheck
  // language.
  if ((id < IDC_SPELLCHECK_LANGUAGES_FIRST) ||
      (id >= IDC_SPELLCHECK_LANGUAGES_LAST))
    return false;

  std::vector<std::string> languages;
  return SpellCheckHost::GetSpellCheckLanguages(profile_, &languages) ==
      (id - IDC_SPELLCHECK_LANGUAGES_FIRST);
}

void RenderViewContextMenu::ExecuteItemCommand(int id) {
  // Check to see if one of the spell check language ids have been clicked.
  if (id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      id < IDC_SPELLCHECK_LANGUAGES_LAST) {
    const size_t language_number = id - IDC_SPELLCHECK_LANGUAGES_FIRST;
    std::vector<std::string> languages;
    SpellCheckHost::GetSpellCheckLanguages(profile_, &languages);
    if (language_number < languages.size()) {
      StringPrefMember dictionary_language;
      dictionary_language.Init(prefs::kSpellCheckDictionary,
          profile_->GetPrefs(), NULL);
      dictionary_language.SetValue(ASCIIToWide(languages[language_number]));
    }
    return;
  }

  // Process custom actions range.
  if ((id >= IDC_CONTENT_CONTEXT_CUSTOM_FIRST) &&
      (id < IDC_CONTENT_CONTEXT_CUSTOM_LAST)) {
    unsigned action = id - IDC_CONTENT_CONTEXT_CUSTOM_FIRST;
    source_tab_contents_->render_view_host()->
        PerformCustomContextMenuAction(action);
    return;
  }

  switch (id) {
    case IDS_CONTENT_CONTEXT_OPENLINKNEWTAB:
      OpenURL(params_.link_url, NEW_BACKGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
      OpenURL(params_.link_url, NEW_WINDOW, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      OpenURL(params_.link_url, OFF_THE_RECORD, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_SAVEAUDIOAS:
    case IDS_CONTENT_CONTEXT_SAVEVIDEOAS:
    case IDS_CONTENT_CONTEXT_SAVEIMAGEAS:
    case IDS_CONTENT_CONTEXT_SAVELINKAS: {
      const GURL& referrer =
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url;
      const GURL& url =
          (id == IDS_CONTENT_CONTEXT_SAVELINKAS ? params_.link_url :
                                                  params_.src_url);
      DownloadManager* dlm = profile_->GetDownloadManager();
      dlm->DownloadUrl(url, referrer, params_.frame_charset,
                       source_tab_contents_);
      break;
    }

    case IDS_CONTENT_CONTEXT_COPYLINKLOCATION:
      WriteURLToClipboard(params_.unfiltered_link_url);
      break;

    case IDS_CONTENT_CONTEXT_COPYAUDIOLOCATION:
    case IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION:
    case IDS_CONTENT_CONTEXT_COPYIMAGELOCATION:
      WriteURLToClipboard(params_.src_url);
      break;

    case IDS_CONTENT_CONTEXT_COPYIMAGE:
      CopyImageAt(params_.x, params_.y);
      break;

    case IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB:
    case IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB:
    case IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB:
      OpenURL(params_.src_url, NEW_BACKGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_PLAY:
      UserMetrics::RecordAction("MediaContextMenu_Play", profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Play, true));
      break;

    case IDS_CONTENT_CONTEXT_PAUSE:
      UserMetrics::RecordAction("MediaContextMenu_Pause", profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Play, false));
      break;

    case IDS_CONTENT_CONTEXT_MUTE:
      UserMetrics::RecordAction("MediaContextMenu_Mute", profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Mute, true));
      break;

    case IDS_CONTENT_CONTEXT_UNMUTE:
      UserMetrics::RecordAction("MediaContextMenu_Unmute", profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Mute, false));
      break;

    case IDS_CONTENT_CONTEXT_LOOP:
      UserMetrics::RecordAction("MediaContextMenu_Loop", profile_);
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Loop,
                              !ItemIsChecked(IDS_CONTENT_CONTEXT_LOOP)));
      break;

    case IDS_CONTENT_CONTEXT_BACK:
      source_tab_contents_->controller().GoBack();
      break;

    case IDS_CONTENT_CONTEXT_FORWARD:
      source_tab_contents_->controller().GoForward();
      break;

    case IDS_CONTENT_CONTEXT_SAVEPAGEAS:
      source_tab_contents_->OnSavePage();
      break;

    case IDS_CONTENT_CONTEXT_RELOAD:
      source_tab_contents_->controller().Reload(true);
      break;

    case IDS_CONTENT_CONTEXT_PRINT:
      source_tab_contents_->PrintPreview();
      break;

    case IDS_CONTENT_CONTEXT_VIEWPAGESOURCE:
      OpenURL(GURL("view-source:" + params_.page_url.spec()),
              NEW_FOREGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_INSPECTELEMENT:
      Inspect(params_.x, params_.y);
      break;

    case IDS_CONTENT_CONTEXT_VIEWPAGEINFO: {
      NavigationEntry* nav_entry =
          source_tab_contents_->controller().GetActiveEntry();
      source_tab_contents_->ShowPageInfo(nav_entry->url(), nav_entry->ssl(),
                                         true);
      break;
    }

    case IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB:
      OpenURL(params_.frame_url, NEW_BACKGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW:
      OpenURL(params_.frame_url, NEW_WINDOW, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD:
      OpenURL(params_.frame_url, OFF_THE_RECORD, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_SAVEFRAMEAS:
      // http://code.google.com/p/chromium/issues/detail?id=11827
      NOTIMPLEMENTED() << "IDS_CONTENT_CONTEXT_SAVEFRAMEAS";
      break;

    case IDS_CONTENT_CONTEXT_PRINTFRAME:
      // http://code.google.com/p/chromium/issues/detail?id=11827
      NOTIMPLEMENTED() << "IDS_CONTENT_CONTEXT_PRINTFRAME";
      break;

    case IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      OpenURL(GURL("view-source:" + params_.frame_url.spec()),
              NEW_FOREGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_VIEWFRAMEINFO: {
      // Deserialize the SSL info.
      NavigationEntry::SSLStatus ssl;
      if (!params_.security_info.empty()) {
        int cert_id, cert_status, security_bits;
        SSLManager::DeserializeSecurityInfo(params_.security_info,
                                            &cert_id,
                                            &cert_status,
                                            &security_bits);
        ssl.set_cert_id(cert_id);
        ssl.set_cert_status(cert_status);
        ssl.set_security_bits(security_bits);
      }
      source_tab_contents_->ShowPageInfo(params_.frame_url, ssl,
                                         false);  // Don't show the history.
      break;
    }

    case IDS_CONTENT_CONTEXT_UNDO:
      source_tab_contents_->render_view_host()->Undo();
      break;

    case IDS_CONTENT_CONTEXT_REDO:
      source_tab_contents_->render_view_host()->Redo();
      break;

    case IDS_CONTENT_CONTEXT_CUT:
      source_tab_contents_->render_view_host()->Cut();
      break;

    case IDS_CONTENT_CONTEXT_COPY:
      source_tab_contents_->render_view_host()->Copy();
      break;

    case IDS_CONTENT_CONTEXT_PASTE:
      source_tab_contents_->render_view_host()->Paste();
      break;

    case IDS_CONTENT_CONTEXT_DELETE:
      source_tab_contents_->render_view_host()->Delete();
      break;

    case IDS_CONTENT_CONTEXT_SELECTALL:
      source_tab_contents_->render_view_host()->SelectAll();
      break;

    case IDS_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDS_CONTENT_CONTEXT_GOTOURL: {
      OpenURL(selection_navigation_url_, NEW_FOREGROUND_TAB,
              PageTransition::LINK);
      break;
    }

    case IDC_SPELLCHECK_SUGGESTION_0:
    case IDC_SPELLCHECK_SUGGESTION_1:
    case IDC_SPELLCHECK_SUGGESTION_2:
    case IDC_SPELLCHECK_SUGGESTION_3:
    case IDC_SPELLCHECK_SUGGESTION_4:
      source_tab_contents_->render_view_host()->Replace(
          params_.dictionary_suggestions[id - IDC_SPELLCHECK_SUGGESTION_0]);
      break;

    case IDC_CHECK_SPELLING_OF_THIS_FIELD:
      source_tab_contents_->render_view_host()->ToggleSpellCheck();
      break;
    case IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY: {
      SpellCheckHost* spellcheck_host = profile_->GetSpellCheckHost();
      if (!spellcheck_host) {
        NOTREACHED();
        break;
      }
      spellcheck_host->AddWord(UTF16ToUTF8(params_.misspelled_word));
      SpellCheckerPlatform::AddWord(params_.misspelled_word);
      break;
    }

    case IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
      ShowFontsLanguagesWindow(
          platform_util::GetTopLevel(
              source_tab_contents_->GetContentNativeView()),
          LANGUAGES_PAGE, profile_);
      break;

    case IDC_SPELLPANEL_TOGGLE:
      source_tab_contents_->render_view_host()->ToggleSpellPanel(
          SpellCheckerPlatform::SpellingPanelVisible());
      break;
    case IDS_CONTENT_CONTEXT_ADDSEARCHENGINE:  // Not implemented.
    default:
      break;
  }
}

bool RenderViewContextMenu::IsDevCommandEnabled(int id) const {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kAlwaysEnableDevTools))
    return true;

  NavigationEntry *active_entry =
      source_tab_contents_->controller().GetActiveEntry();
  if (!active_entry)
    return false;

  // Don't inspect view source.
  if (active_entry->IsViewSourceMode())
    return false;

  // Don't inspect HTML dialogs (doesn't work anyway).
  if (active_entry->url().SchemeIs(chrome::kGearsScheme))
    return false;

#if defined NDEBUG
  bool debug_mode = false;
#else
  bool debug_mode = true;
#endif
  // Don't inspect new tab UI, etc.
  if (active_entry->url().SchemeIs(chrome::kChromeUIScheme) && !debug_mode &&
      active_entry->url().host() != chrome::kChromeUIDevToolsHost)
    return false;

  // Don't inspect about:network, about:memory, etc.
  // However, we do want to inspect about:blank, which is often
  // used by ordinary web pages.
  if (active_entry->virtual_url().SchemeIs(chrome::kAboutScheme) &&
      !LowerCaseEqualsASCII(active_entry->virtual_url().path(), "blank"))
    return false;

  if (id == IDS_CONTENT_CONTEXT_INSPECTELEMENT) {
    // Don't enable the web inspector if JavaScript is disabled.
    if (!profile_->GetPrefs()->GetBoolean(prefs::kWebKitJavascriptEnabled) ||
        command_line.HasSwitch(switches::kDisableJavaScript))
      return false;
    // Don't enable the web inspector on web inspector if there is no process
    // per tab flag set.
    if (IsDevToolsURL(active_entry->url()) &&
        !command_line.HasSwitch(switches::kProcessPerTab))
      return false;
  }

  return true;
}

// Controller functions --------------------------------------------------------

void RenderViewContextMenu::OpenURL(
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition) {
  source_tab_contents_->OpenURL(url, GURL(), disposition, transition);
}

void RenderViewContextMenu::CopyImageAt(int x, int y) {
  source_tab_contents_->render_view_host()->CopyImageAt(x, y);
}

void RenderViewContextMenu::Inspect(int x, int y) {
  UserMetrics::RecordAction("DevTools_InspectElement", profile_);
  DevToolsManager::GetInstance()->InspectElement(
      source_tab_contents_->render_view_host(), x, y);
}

void RenderViewContextMenu::WriteURLToClipboard(const GURL& url) {
  chrome_browser_net::WriteURLToClipboard(
      url,
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
      g_browser_process->clipboard());
}

void RenderViewContextMenu::MediaPlayerActionAt(
    const gfx::Point& location,
    const WebMediaPlayerAction& action) {
  source_tab_contents_->render_view_host()->MediaPlayerActionAt(
      location, action);
}
