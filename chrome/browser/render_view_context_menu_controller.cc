// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/render_view_context_menu_controller.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/browser/navigation_entry.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/clipboard_service.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/win_util.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"

#include "generated_resources.h"

RenderViewContextMenuController::RenderViewContextMenuController(
    WebContents* source_web_contents,
    const ViewHostMsg_ContextMenu_Params& params)
  : source_web_contents_(source_web_contents),
    params_(params) {
}

RenderViewContextMenuController::~RenderViewContextMenuController() {
}

///////////////////////////////////////////////////////////////////////////////
// Controller methods

void RenderViewContextMenuController::OpenURL(
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition) {
  source_web_contents_->OpenURL(url, disposition, transition);
}

void RenderViewContextMenuController::CopyImageAt(int x, int y) {
  source_web_contents_->CopyImageAt(x, y);
}

void RenderViewContextMenuController::Inspect(int x, int y) {
  source_web_contents_->InspectElementAt(x, y);
}

void RenderViewContextMenuController::WriteTextToClipboard(
    const std::wstring& text) {
  ClipboardService* clipboard = g_browser_process->clipboard_service();

  if (!clipboard)
    return;

  clipboard->Clear();
  clipboard->WriteText(text);
}

void RenderViewContextMenuController::WriteURLToClipboard(const GURL& url) {
  if (url.SchemeIs("mailto"))
    WriteTextToClipboard(UTF8ToWide(url.path()));
  else
    WriteTextToClipboard(UTF8ToWide(url.spec()));
}

///////////////////////////////////////////////////////////////////////////////
// Menu::Delegate methods

std::wstring RenderViewContextMenuController::GetLabel(int id) const {
  switch (id) {
    case IDS_CONTENT_CONTEXT_SEARCHWEBFOR: {
      const TemplateURL* const default_provider = source_web_contents_->
          profile()->GetTemplateURLModel()->GetDefaultSearchProvider();
      DCHECK(default_provider);  // The context menu should not contain this
                                 // item when there is no provider.
      return l10n_util::GetStringF(id, default_provider->short_name(),
          l10n_util::TruncateString(params_.selection_text, 50));
    }

    case IDS_CONTENT_CONTEXT_COPYLINKLOCATION:
      if (params_.link_url.SchemeIs("mailto"))
        return l10n_util::GetString(IDS_CONTENT_CONTEXT_COPYEMAILADDRESS);

    default:
      return l10n_util::GetString(id);
  }
}

bool RenderViewContextMenuController::IsCommandEnabled(int id) const {
  switch (id) {
    case IDS_CONTENT_CONTEXT_BACK:
      return source_web_contents_->controller()->CanGoBack();

    case IDS_CONTENT_CONTEXT_FORWARD:
      return source_web_contents_->controller()->CanGoForward();
    case IDS_CONTENT_CONTEXT_VIEWPAGESOURCE:
    case IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE:
    case IDS_CONTENT_CONTEXT_INSPECTELEMENT:
      return IsDevCommandEnabled(id);
    case IDS_CONTENT_CONTEXT_OPENLINKNEWTAB:
    case IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
    case IDS_CONTENT_CONTEXT_COPYLINKLOCATION:
      return params_.link_url.is_valid();

    case IDS_CONTENT_CONTEXT_SAVELINKAS:
      return params_.link_url.is_valid() &&
             URLRequest::IsHandledURL(params_.link_url);

    case IDS_CONTENT_CONTEXT_SAVEIMAGEAS:
      return params_.image_url.is_valid() &&
             URLRequest::IsHandledURL(params_.image_url);

    case IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDS_CONTENT_CONTEXT_COPYIMAGELOCATION:
      return params_.image_url.is_valid();
    case IDS_CONTENT_CONTEXT_SAVEPAGEAS:
      return SavePackage::IsSavableURL(source_web_contents_->GetURL());
    case IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB:
    case IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW:
      return params_.frame_url.is_valid();

    case IDS_CONTENT_CONTEXT_UNDO:
      return !!(params_.edit_flags & ContextNode::CAN_UNDO);

    case IDS_CONTENT_CONTEXT_REDO:
      return !!(params_.edit_flags & ContextNode::CAN_REDO);

    case IDS_CONTENT_CONTEXT_CUT:
      return !!(params_.edit_flags & ContextNode::CAN_CUT);

    case IDS_CONTENT_CONTEXT_COPY:
      return !!(params_.edit_flags & ContextNode::CAN_COPY);

    case IDS_CONTENT_CONTEXT_PASTE:
      return !!(params_.edit_flags & ContextNode::CAN_PASTE);

    case IDS_CONTENT_CONTEXT_DELETE:
      return !!(params_.edit_flags & ContextNode::CAN_DELETE);

    case IDS_CONTENT_CONTEXT_SELECTALL:
      return !!(params_.edit_flags & ContextNode::CAN_SELECT_ALL);

    case IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      return !source_web_contents_->profile()->IsOffTheRecord() &&
             params_.link_url.is_valid();

    case IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD:
      return !source_web_contents_->profile()->IsOffTheRecord() &&
             params_.frame_url.is_valid();

    case IDS_CONTENT_CONTEXT_COPYIMAGE:
    case IDS_CONTENT_CONTEXT_PRINT:
    case IDS_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDC_USESPELLCHECKSUGGESTION_0:
    case IDC_USESPELLCHECKSUGGESTION_1:
    case IDC_USESPELLCHECKSUGGESTION_2:
    case IDC_USESPELLCHECKSUGGESTION_3:
    case IDC_USESPELLCHECKSUGGESTION_4:
      return true;
    case IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY:
      return !params_.misspelled_word.empty();
    case IDS_CONTENT_CONTEXT_VIEWPAGEINFO:
    case IDS_CONTENT_CONTEXT_VIEWFRAMEINFO:
    case IDS_CONTENT_CONTEXT_SAVEFRAMEAS:
    case IDS_CONTENT_CONTEXT_PRINTFRAME:
    case IDS_CONTENT_CONTEXT_ADDSEARCHENGINE:  // Not implemented.
    default:
      return false;
  }
}

bool RenderViewContextMenuController::GetAcceleratorInfo(int id,
    ChromeViews::Accelerator* accel) {
  // There are no formally defined accelerators we can query so we assume
  // that Ctrl+C, Ctrl+V, Ctrl+X, Ctrl-A, etc do what they normally do.
  switch (id) {
    case IDS_CONTENT_CONTEXT_UNDO:
      *accel = ChromeViews::Accelerator(L'Z', false, true, false);
      return true;

    case IDS_CONTENT_CONTEXT_REDO:
      *accel = ChromeViews::Accelerator(L'Z', true, true, false);
      return true;

    case IDS_CONTENT_CONTEXT_CUT:
      *accel = ChromeViews::Accelerator(L'X', false, true, false);
      return true;

    case IDS_CONTENT_CONTEXT_COPY:
      *accel = ChromeViews::Accelerator(L'C', false, true, false);
      return true;

    case IDS_CONTENT_CONTEXT_PASTE:
      *accel = ChromeViews::Accelerator(L'V', false, true, false);
      return true;

    case IDS_CONTENT_CONTEXT_SELECTALL:
      *accel = ChromeViews::Accelerator(L'A', false, true, false);

    default:
      return false;
  }
}

void RenderViewContextMenuController::ExecuteCommand(int id) {
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

    // TODO(paulg): Prompt the user for file name when saving links and images.
    case IDS_CONTENT_CONTEXT_SAVEIMAGEAS:
    case IDS_CONTENT_CONTEXT_SAVELINKAS: {
      const GURL& referrer =
          params_.frame_url.is_empty() ? params_.page_url : params_.frame_url;
      const GURL& url = id == IDS_CONTENT_CONTEXT_SAVELINKAS ? params_.link_url :
                                                               params_.image_url;
      DownloadManager* dlm =
          source_web_contents_->profile()->GetDownloadManager();
      dlm->DownloadUrl(url, referrer, source_web_contents_);
      break;
    }

    case IDS_CONTENT_CONTEXT_COPYLINKLOCATION:
      WriteURLToClipboard(params_.link_url);
      break;

    case IDS_CONTENT_CONTEXT_COPYIMAGELOCATION:
      WriteURLToClipboard(params_.image_url);
      break;

    case IDS_CONTENT_CONTEXT_COPYIMAGE:
      CopyImageAt(params_.x, params_.y);
      break;

    case IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB:
      OpenURL(params_.image_url, NEW_BACKGROUND_TAB, PageTransition::LINK);
      break;

    case IDS_CONTENT_CONTEXT_BACK:
      source_web_contents_->controller()->GoBack();
      break;

    case IDS_CONTENT_CONTEXT_FORWARD:
      source_web_contents_->controller()->GoForward();
      break;

    case IDS_CONTENT_CONTEXT_SAVEPAGEAS:
      source_web_contents_->OnSavePage();
      break;

    case IDS_CONTENT_CONTEXT_PRINT:
      source_web_contents_->PrintPreview();
      break;

    case IDS_CONTENT_CONTEXT_VIEWPAGESOURCE:
      OpenURL(GURL("view-source:" + params_.page_url.spec()),
              NEW_FOREGROUND_TAB, PageTransition::GENERATED);
      break;

    case IDS_CONTENT_CONTEXT_INSPECTELEMENT:
      Inspect(params_.x, params_.y);
      break;

    case IDS_CONTENT_CONTEXT_VIEWPAGEINFO:
      win_util::MessageBox(NULL, L"Context Menu Action", L"View Page Info",
                           MB_OK);
      break;

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
      win_util::MessageBox(NULL, L"Context Menu Action", L"Save Frame As",
                           MB_OK);
      break;

    case IDS_CONTENT_CONTEXT_PRINTFRAME:
      win_util::MessageBox(NULL, L"Context Menu Action", L"Print Frame",
                           MB_OK);
      break;

    case IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      OpenURL(GURL("view-source:" + params_.frame_url.spec()),
              NEW_FOREGROUND_TAB, PageTransition::GENERATED);
      break;

    case IDS_CONTENT_CONTEXT_VIEWFRAMEINFO:
      win_util::MessageBox(NULL, L"Context Menu Action", L"View Frame Info",
                           MB_OK);
      break;

    case IDS_CONTENT_CONTEXT_UNDO:
      source_web_contents_->Undo();
      break;

    case IDS_CONTENT_CONTEXT_REDO:
      source_web_contents_->Redo();
      break;

    case IDS_CONTENT_CONTEXT_CUT:
      source_web_contents_->Cut();
      break;

    case IDS_CONTENT_CONTEXT_COPY:
      source_web_contents_->Copy();
      break;

    case IDS_CONTENT_CONTEXT_PASTE:
      source_web_contents_->Paste();
      break;

    case IDS_CONTENT_CONTEXT_DELETE:
      source_web_contents_->Delete();
      break;

    case IDS_CONTENT_CONTEXT_SELECTALL:
      source_web_contents_->SelectAll();
      break;

    case IDS_CONTENT_CONTEXT_SEARCHWEBFOR: {
      const TemplateURL* const default_provider = source_web_contents_->
          profile()->GetTemplateURLModel()->GetDefaultSearchProvider();
      DCHECK(default_provider);  // The context menu should not contain this
                                 // item when there is no provider.
      const TemplateURLRef* const search_url = default_provider->url();
      DCHECK(search_url->SupportsReplacement());
      OpenURL(GURL(search_url->ReplaceSearchTerms(*default_provider,
          params_.selection_text, TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
          std::wstring())), NEW_FOREGROUND_TAB, PageTransition::GENERATED);
      break;
    }

    case IDC_USESPELLCHECKSUGGESTION_0:
    case IDC_USESPELLCHECKSUGGESTION_1:
    case IDC_USESPELLCHECKSUGGESTION_2:
    case IDC_USESPELLCHECKSUGGESTION_3:
    case IDC_USESPELLCHECKSUGGESTION_4:
      source_web_contents_->Replace(params_.dictionary_suggestions[
          id - IDC_USESPELLCHECKSUGGESTION_0]);
      break;

    case IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY:
      source_web_contents_->AddToDictionary(params_.misspelled_word);
      break;

    case IDS_CONTENT_CONTEXT_ADDSEARCHENGINE:  // Not implemented.
    default:
      break;
  }
}

bool RenderViewContextMenuController::IsDevCommandEnabled(int id) const {
  CommandLine command_line;
  if (command_line.HasSwitch(switches::kAlwaysEnableDevTools))
    return true;

  NavigationEntry *active_entry =
      source_web_contents_->controller()->GetActiveEntry();
  if (!active_entry)
    return false;

  // Don't inspect HTML dialogs.
  if (source_web_contents_->type() == TAB_CONTENTS_HTML_DIALOG)
    return false;

  // Don't inspect view source.
  if (source_web_contents_->type() == TAB_CONTENTS_VIEW_SOURCE)
    return false;

  // Don't inspect inspector, new tab UI, etc.
  if (active_entry->url().SchemeIs("chrome-resource"))
    return false;

  // Don't inspect about:network, about:memory, etc.
  // However, we do want to inspect about:blank, which is often
  // used by ordinary web pages.
  if (active_entry->display_url().SchemeIs("about") &&
      !LowerCaseEqualsASCII(active_entry->display_url().path(), "blank"))
    return false;

  // Don't enable the web inspector if JavaScript is disabled
  if (id == IDS_CONTENT_CONTEXT_INSPECTELEMENT) {
    PrefService* prefs = source_web_contents_->profile()->GetPrefs();
    if (!prefs->GetBoolean(prefs::kWebKitJavascriptEnabled) ||
        command_line.HasSwitch(switches::kDisableJavaScript))
      return false;
  }

  return true;
}

