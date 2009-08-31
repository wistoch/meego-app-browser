// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extensions_ui.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_util.h"

#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

////////////////////////////////////////////////////////////////////////////////
//
// ExtensionsHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

ExtensionsUIHTMLSource::ExtensionsUIHTMLSource()
    : DataSource(chrome::kChromeUIExtensionsHost, MessageLoop::current()) {
}

void ExtensionsUIHTMLSource::StartDataRequest(const std::string& path,
                                              int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString(L"title",
      l10n_util::GetString(IDS_EXTENSIONS_TITLE));

  static const StringPiece extensions_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_EXTENSIONS_UI_HTML));
  std::string full_html(extensions_html.data(), extensions_html.size());
  jstemplate_builder::AppendJsonHtml(&localized_strings, &full_html);
  jstemplate_builder::AppendI18nTemplateSourceHtml(&full_html);
  jstemplate_builder::AppendI18nTemplateProcessHtml(&full_html);
  jstemplate_builder::AppendJsTemplateSourceHtml(&full_html);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

///////////////////////////////////////////////////////////////////////////////
//
// ExtensionsDOMHandler
//
///////////////////////////////////////////////////////////////////////////////

ExtensionsDOMHandler::ExtensionsDOMHandler(ExtensionsService* extension_service)
    : extensions_service_(extension_service) {
}

void ExtensionsDOMHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("requestExtensionsData",
      NewCallback(this, &ExtensionsDOMHandler::HandleRequestExtensionsData));
  dom_ui_->RegisterMessageCallback("inspect",
      NewCallback(this, &ExtensionsDOMHandler::HandleInspectMessage));
  dom_ui_->RegisterMessageCallback("reload",
      NewCallback(this, &ExtensionsDOMHandler::HandleReloadMessage));
  dom_ui_->RegisterMessageCallback("enable",
      NewCallback(this, &ExtensionsDOMHandler::HandleEnableMessage));
  dom_ui_->RegisterMessageCallback("uninstall",
      NewCallback(this, &ExtensionsDOMHandler::HandleUninstallMessage));
  dom_ui_->RegisterMessageCallback("load",
      NewCallback(this, &ExtensionsDOMHandler::HandleLoadMessage));
  dom_ui_->RegisterMessageCallback("pack",
      NewCallback(this, &ExtensionsDOMHandler::HandlePackMessage));
}

void ExtensionsDOMHandler::HandleRequestExtensionsData(const Value* value) {
  DictionaryValue results;

  // Add the extensions to the results structure.
  ListValue *extensions_list = new ListValue();
  const ExtensionList* extensions = extensions_service_->extensions();
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    // Don't show the themes since this page's UI isn't really useful for
    // themes.
    if (!(*extension)->IsTheme()) {
      extensions_list->Append(CreateExtensionDetailValue(
          *extension, GetActivePagesForExtension((*extension)->id()), true));
    }
  }
  extensions = extensions_service_->disabled_extensions();
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    if (!(*extension)->IsTheme()) {
      extensions_list->Append(CreateExtensionDetailValue(
          *extension, GetActivePagesForExtension((*extension)->id()), false));
    }
  }
  results.Set(L"extensions", extensions_list);

  dom_ui_->CallJavascriptFunction(L"returnExtensionsData", results);

  // Register for notifications that we need to reload the page.
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
      NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
      NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UPDATE_DISABLED,
      NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED_DISABLED,
      NotificationService::AllSources());
}

void ExtensionsDOMHandler::HandleInspectMessage(const Value* value) {
  std::string render_process_id_str;
  std::string render_view_id_str;
  int render_process_id;
  int render_view_id;
  CHECK(value->IsType(Value::TYPE_LIST));
  const ListValue* list = static_cast<const ListValue*>(value);
  CHECK(list->GetSize() == 2);
  CHECK(list->GetString(0, &render_process_id_str));
  CHECK(list->GetString(1, &render_view_id_str));
  CHECK(StringToInt(render_process_id_str, &render_process_id));
  CHECK(StringToInt(render_view_id_str, &render_view_id));
  RenderViewHost* host = RenderViewHost::FromID(render_process_id,
                                                render_view_id);
  if (!host) {
    // This can happen if the host has gone away since the page was displayed.
    return;
  }

  DevToolsManager::GetInstance()->OpenDevToolsWindow(host);
}

void ExtensionsDOMHandler::HandleReloadMessage(const Value* value) {
  CHECK(value->IsType(Value::TYPE_LIST));
  const ListValue* list = static_cast<const ListValue*>(value);
  CHECK(list->GetSize() == 1);
  std::string extension_id;
  CHECK(list->GetString(0, &extension_id));
  extensions_service_->ReloadExtension(extension_id);
}

void ExtensionsDOMHandler::HandleEnableMessage(const Value* value) {
  CHECK(value->IsType(Value::TYPE_LIST));
  const ListValue* list = static_cast<const ListValue*>(value);
  CHECK(list->GetSize() == 1);
  std::string extension_id;
  CHECK(list->GetString(0, &extension_id));
  extensions_service_->EnableExtension(extension_id);
}

void ExtensionsDOMHandler::HandleUninstallMessage(const Value* value) {
  CHECK(value->IsType(Value::TYPE_LIST));
  const ListValue* list = static_cast<const ListValue*>(value);
  CHECK(list->GetSize() == 1);
  std::string extension_id;
  CHECK(list->GetString(0, &extension_id));
  extensions_service_->UninstallExtension(extension_id, false);
}

void ExtensionsDOMHandler::HandleLoadMessage(const Value* value) {
  load_extension_dialog_ = SelectFileDialog::Create(this);
  load_extension_dialog_->SelectFile(
      SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY),
      FilePath(), NULL, 0, FILE_PATH_LITERAL(""),
      NULL, NULL);
}

void ExtensionsDOMHandler::HandlePackMessage(const Value* value) {
#if defined(OS_WIN)
  ShowPackDialog();
#endif
}

void ExtensionsDOMHandler::FileSelected(const FilePath& path, int index,
                                        void* params) {
  extensions_service_->LoadExtension(path);
}

void ExtensionsDOMHandler::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_LOADED:
    case NotificationType::EXTENSION_UNLOADED:
    case NotificationType::EXTENSION_UPDATE_DISABLED:
    case NotificationType::EXTENSION_UNLOADED_DISABLED:
      if (dom_ui_->tab_contents())
        dom_ui_->tab_contents()->controller().Reload(false);
      registrar_.RemoveAll();
      break;

    default:
      NOTREACHED();
  }
}

static void CreateScriptFileDetailValue(
    const FilePath& extension_path, const UserScript::FileList& scripts,
    const wchar_t* key, DictionaryValue* script_data) {
  if (scripts.empty())
    return;

  ListValue *list = new ListValue();
  for (size_t i = 0; i < scripts.size(); ++i) {
    const UserScript::File &file = scripts[i];
    // We are passing through GURLs to canonicalize the output to a valid
    // URL path fragment.
    GURL script_url = net::FilePathToFileURL(file.path());
    GURL extension_url = net::FilePathToFileURL(extension_path);
    std::string relative_path =
        script_url.spec().substr(extension_url.spec().length() + 1);

    list->Append(new StringValue(relative_path));
  }
  script_data->Set(key, list);
}

// Static
DictionaryValue* ExtensionsDOMHandler::CreateContentScriptDetailValue(
  const UserScript& script, const FilePath& extension_path) {
  DictionaryValue* script_data = new DictionaryValue();
  CreateScriptFileDetailValue(extension_path, script.js_scripts(), L"js",
    script_data);
  CreateScriptFileDetailValue(extension_path, script.css_scripts(), L"css",
    script_data);

  // Get list of glob "matches" strings
  ListValue *url_pattern_list = new ListValue();
  const std::vector<URLPattern>& url_patterns = script.url_patterns();
  for (std::vector<URLPattern>::const_iterator url_pattern =
      url_patterns.begin();
    url_pattern != url_patterns.end(); ++url_pattern) {
    url_pattern_list->Append(new StringValue(url_pattern->GetAsString()));
  }

  script_data->Set(L"matches", url_pattern_list);

  return script_data;
}

// Static
DictionaryValue* ExtensionsDOMHandler::CreateExtensionDetailValue(
    const Extension *extension, const std::vector<ExtensionPage>& pages,
    bool enabled) {
  DictionaryValue* extension_data = new DictionaryValue();

  extension_data->SetString(L"id", extension->id());
  extension_data->SetString(L"name", extension->name());
  extension_data->SetString(L"description", extension->description());
  extension_data->SetString(L"version", extension->version()->GetString());
  extension_data->SetBoolean(L"enabled", enabled);

  // Add list of content_script detail DictionaryValues
  ListValue *content_script_list = new ListValue();
  UserScriptList content_scripts = extension->content_scripts();
  for (UserScriptList::const_iterator script = content_scripts.begin();
    script != content_scripts.end(); ++script) {
      content_script_list->Append(CreateContentScriptDetailValue(*script,
          extension->path()));
  }
  extension_data->Set(L"content_scripts", content_script_list);

  // Add permissions
  ListValue *permission_list = new ListValue;
  std::vector<URLPattern> permissions = extension->host_permissions();
  for (std::vector<URLPattern>::iterator permission = permissions.begin();
       permission != permissions.end(); ++permission) {
    permission_list->Append(Value::CreateStringValue(
        permission->GetAsString()));
  }
  extension_data->Set(L"permissions", permission_list);

  // Add views
  ListValue* views = new ListValue;
  for (std::vector<ExtensionPage>::const_iterator iter = pages.begin();
       iter != pages.end(); ++iter) {
    DictionaryValue* view_value = new DictionaryValue;
    view_value->SetString(L"path",
        iter->url.path().substr(1, std::string::npos));  // no leading slash
    view_value->SetInteger(L"renderViewId", iter->render_view_id);
    view_value->SetInteger(L"renderProcessId", iter->render_process_id);
    views->Append(view_value);
  }
  extension_data->Set(L"views", views);

  return extension_data;
}

std::vector<ExtensionPage> ExtensionsDOMHandler::GetActivePagesForExtension(
    const std::string& extension_id) {
  std::vector<ExtensionPage> result;
  std::set<ExtensionFunctionDispatcher*>* all_instances =
      ExtensionFunctionDispatcher::all_instances();

  for (std::set<ExtensionFunctionDispatcher*>::iterator iter =
       all_instances->begin(); iter != all_instances->end(); ++iter) {
    RenderViewHost* view = (*iter)->render_view_host();
    if ((*iter)->extension_id() == extension_id && view) {
      result.push_back(ExtensionPage((*iter)->url(),
                                     view->process()->pid(),
                                     view->routing_id()));
    }
  }

  return result;
}

ExtensionsDOMHandler::~ExtensionsDOMHandler() {
}

// ExtensionsDOMHandler, public: -----------------------------------------------

ExtensionsUI::ExtensionsUI(TabContents* contents) : DOMUI(contents) {
  ExtensionsService *exstension_service =
      GetProfile()->GetOriginalProfile()->GetExtensionsService();

  ExtensionsDOMHandler* handler = new ExtensionsDOMHandler(exstension_service);
  AddMessageHandler(handler);
  handler->Attach(this);

  ExtensionsUIHTMLSource* html_source = new ExtensionsUIHTMLSource();

  // Set up the chrome://extensions/ source.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
          &ChromeURLDataManager::AddDataSource, html_source));
}
