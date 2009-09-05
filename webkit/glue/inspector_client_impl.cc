// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "DOMWindow.h"
#include "FloatRect.h"
#include "InspectorController.h"
#include "Page.h"
#include "Settings.h"
#include <wtf/Vector.h>
MSVC_POP_WARNING();

#undef LOG
#include "base/logging.h"
#include "base/gfx/rect.h"
#include "base/string_util.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/inspector_client_impl.h"
#include "webkit/glue/webdevtoolsagent_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview_impl.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

using namespace WebCore;

using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebURLRequest;

static const float kDefaultInspectorXPos = 10;
static const float kDefaultInspectorYPos = 50;
static const float kDefaultInspectorHeight = 640;
static const float kDefaultInspectorWidth = 480;

InspectorClientImpl::InspectorClientImpl(WebViewImpl* webView)
    : inspected_web_view_(webView) {
  ASSERT(inspected_web_view_);
}

InspectorClientImpl::~InspectorClientImpl() {
}

void InspectorClientImpl::inspectorDestroyed() {
  // Our lifetime is bound to the WebViewImpl.
}

Page* InspectorClientImpl::createPage() {
  // This method should never be called in Chrome as inspector front-end lives
  // in a separate process.
  NOTREACHED();
  return NULL;
}

void InspectorClientImpl::showWindow() {
  DCHECK(inspected_web_view_->GetWebDevToolsAgentImpl());
  InspectorController* inspector =
      inspected_web_view_->page()->inspectorController();
  inspector->setWindowVisible(true);
}

void InspectorClientImpl::closeWindow() {
  if (inspected_web_view_->page())
    inspected_web_view_->page()->inspectorController()->setWindowVisible(false);
}

bool InspectorClientImpl::windowVisible() {
  DCHECK(inspected_web_view_->GetWebDevToolsAgentImpl());
  return false;
}

void InspectorClientImpl::attachWindow() {
  // TODO(jackson): Implement this
}

void InspectorClientImpl::detachWindow() {
  // TODO(jackson): Implement this
}

void InspectorClientImpl::setAttachedWindowHeight(unsigned int height) {
  // TODO(dglazkov): Implement this
  NOTIMPLEMENTED();
}

static void invalidateNodeBoundingRect(WebViewImpl* web_view) {
  // TODO(ojan): http://b/1143996 Is it important to just invalidate the rect
  // of the node region given that this is not on a critical codepath?
  // In order to do so, we'd have to take scrolling into account.
  const WebSize& size = web_view->size();
  WebRect damaged_rect(0, 0, size.width, size.height);
  if (web_view->GetDelegate())
    web_view->GetDelegate()->didInvalidateRect(damaged_rect);
}

void InspectorClientImpl::highlight(Node* node) {
  // InspectorController does the actually tracking of the highlighted node
  // and the drawing of the highlight. Here we just make sure to invalidate
  // the rects of the old and new nodes.
  hideHighlight();
}

void InspectorClientImpl::hideHighlight() {
  // TODO: Should be able to invalidate a smaller rect.
  invalidateNodeBoundingRect(inspected_web_view_);
}

void InspectorClientImpl::inspectedURLChanged(const String& newURL) {
  // TODO(jackson): Implement this
}

String InspectorClientImpl::localizedStringsURL() {
  NOTIMPLEMENTED();
  return String();
}

String InspectorClientImpl::hiddenPanels() {
  // Enumerate tabs that are currently disabled.
  return "scripts,profiles,databases";
}

void InspectorClientImpl::populateSetting(
    const String& key,
    InspectorController::Setting& setting) {
  LoadSettings();
  if (settings_->contains(key))
    setting = settings_->get(key);
}

void InspectorClientImpl::storeSetting(
    const String& key,
    const InspectorController::Setting& setting) {
  LoadSettings();
  settings_->set(key, setting);
  SaveSettings();
}

void InspectorClientImpl::removeSetting(const String& key) {
  LoadSettings();
  settings_->remove(key);
  SaveSettings();
}

void InspectorClientImpl::inspectorWindowObjectCleared() {
  NOTIMPLEMENTED();
}

void InspectorClientImpl::LoadSettings() {
  if (settings_)
    return;

  settings_.set(new SettingsMap);
  String data = webkit_glue::StdWStringToString(
      inspected_web_view_->GetInspectorSettings());
  if (data.isEmpty())
    return;

  Vector<String> entries;
  data.split("\n", entries);
  for (Vector<String>::iterator it = entries.begin();
       it != entries.end(); ++it) {
    Vector<String> tokens;
    it->split(":", tokens);
    if (tokens.size() != 3)
      continue;

    String name = decodeURLEscapeSequences(tokens[0]);
    String type = tokens[1];
    InspectorController::Setting setting;
    bool ok = true;
    if (type == "string")
      setting.set(decodeURLEscapeSequences(tokens[2]));
    else if (type == "double")
      setting.set(tokens[2].toDouble(&ok));
    else if (type == "integer")
      setting.set(static_cast<long>(tokens[2].toInt(&ok)));
    else if (type == "boolean")
      setting.set(tokens[2] == "true");
    else
      continue;

    if (ok)
      settings_->set(name, setting);
  }
}

void InspectorClientImpl::SaveSettings() {
  String data;
  for (SettingsMap::iterator it = settings_->begin(); it != settings_->end();
       ++it) {
    String entry;
    InspectorController::Setting value = it->second;
    String name = encodeWithURLEscapeSequences(it->first);
    switch (value.type()) {
      case InspectorController::Setting::StringType:
        entry = String::format(
            "%s:string:%s",
            name.utf8().data(),
            encodeWithURLEscapeSequences(value.string()).utf8().data());
        break;
      case InspectorController::Setting::DoubleType:
        entry = String::format(
            "%s:double:%f",
            name.utf8().data(),
            value.doubleValue());
        break;
      case InspectorController::Setting::IntegerType:
        entry = String::format(
            "%s:integer:%ld",
            name.utf8().data(),
            value.integerValue());
        break;
      case InspectorController::Setting::BooleanType:
        entry = String::format("%s:boolean:%s",
                               name.utf8().data(),
                               value.booleanValue() ? "true" : "false");
        break;
      case InspectorController::Setting::StringVectorType:
        NOTIMPLEMENTED();
        break;
      default:
        NOTREACHED();
        break;
    }
    data.append(entry);
    data.append("\n");
  }
  inspected_web_view_->delegate()->UpdateInspectorSettings(
      webkit_glue::StringToStdWString(data));
}
