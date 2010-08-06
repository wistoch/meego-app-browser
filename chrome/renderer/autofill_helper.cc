// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill_helper.h"

#include "app/l10n_util.h"
#include "chrome/renderer/form_manager.h"
#include "chrome/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/password_form.h"

using WebKit::WebFormControlElement;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebNode;
using WebKit::WebString;

AutoFillHelper::AutoFillHelper(RenderView* render_view)
    : render_view_(render_view),
      autofill_query_id_(0),
      autofill_action_(AUTOFILL_NONE),
      suggestions_clear_index_(-1),
      suggestions_options_index_(-1) {
}

void AutoFillHelper::QueryAutocompleteSuggestions(const WebNode& node,
                                                  const WebString& name,
                                                  const WebString& value) {
  static int query_counter = 0;
  autofill_query_id_ = query_counter++;
  autofill_query_node_ = node;

  const WebFormControlElement& element = node.toConst<WebFormControlElement>();
  webkit_glue::FormField field;
  FormManager::WebFormControlElementToFormField(element, true, &field);

  // WebFormControlElementToFormField does not scrape the DOM for the field
  // label, so find the label here.
  // TODO(jhawkins): Add form and field identities so we can use the cached form
  // data in FormManager.
  field.set_label(FormManager::LabelForElement(element));

  bool form_autofilled = form_manager_.FormWithNodeIsAutoFilled(node);
  render_view_->Send(new ViewHostMsg_QueryFormFieldAutoFill(
      render_view_->routing_id(), autofill_query_id_, form_autofilled, field));
}

void AutoFillHelper::RemoveAutocompleteSuggestion(
    const WebKit::WebString& name, const WebKit::WebString& value) {
  // The index of clear & options will have shifted down.
  if (suggestions_clear_index_ != -1)
    suggestions_clear_index_--;
  if (suggestions_options_index_ != -1)
    suggestions_options_index_--;

  render_view_->Send(new ViewHostMsg_RemoveAutocompleteEntry(
      render_view_->routing_id(), name, value));
}

void AutoFillHelper::SuggestionsReceived(int query_id,
                                         const std::vector<string16>& values,
                                         const std::vector<string16>& labels,
                                         const std::vector<string16>& icons,
                                         const std::vector<int>& unique_ids) {
  WebKit::WebView* web_view = render_view_->webview();
  if (!web_view || query_id != autofill_query_id_)
    return;

  // Any popup currently showing is now obsolete.
  web_view->hidePopups();

  // No suggestions: nothing to do.
  if (values.empty())
    return;

  std::vector<string16> v(values);
  std::vector<string16> l(labels);
  std::vector<string16> i(icons);
  std::vector<int> ids(unique_ids);
  int separator_index = -1;

  // The form has been auto-filled, so give the user the chance to clear the
  // form.  Append the 'Clear form' menu item.
  if (form_manager_.FormWithNodeIsAutoFilled(autofill_query_node_)) {
    v.push_back(l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM));
    l.push_back(string16());
    i.push_back(string16());
    ids.push_back(0);
    suggestions_clear_index_ = v.size() - 1;
    separator_index = values.size();
  }

  // Only include "AutoFill Options" special menu item if we have AutoFill
  // items, identified by |unique_ids| having at least one valid value.
  bool show_options = false;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (ids[i] != 0) {
      show_options = true;
      break;
    }
  }
  if (show_options) {
    // Append the 'AutoFill Options...' menu item.
    v.push_back(l10n_util::GetStringUTF16(IDS_AUTOFILL_OPTIONS));
    l.push_back(string16());
    i.push_back(string16());
    ids.push_back(0);
    suggestions_options_index_ = v.size() - 1;
    separator_index = values.size();
  }

  // Send to WebKit for display.
  if (!v.empty()) {
    web_view->applyAutoFillSuggestions(
        autofill_query_node_, v, l, i, ids, separator_index);
  }
}

void AutoFillHelper::FormDataFilled(int query_id,
                                    const webkit_glue::FormData& form) {
  if (!render_view_->webview() || query_id != autofill_query_id_)
    return;

  switch (autofill_action_) {
    case AUTOFILL_FILL:
      form_manager_.FillForm(form, autofill_query_node_);
      break;
    case AUTOFILL_PREVIEW:
      form_manager_.PreviewForm(form);
      break;
    default:
      NOTREACHED();
  }
  autofill_action_ = AUTOFILL_NONE;
}

void AutoFillHelper::DidSelectAutoFillSuggestion(const WebNode& node,
                                                 const WebString& value,
                                                 const WebString& label,
                                                 int unique_id) {
  DidClearAutoFillSelection(node);
  QueryAutoFillFormData(node, value, label, unique_id, AUTOFILL_PREVIEW);
}

void AutoFillHelper::DidAcceptAutoFillSuggestion(const WebNode& node,
                                                 const WebString& value,
                                                 const WebString& label,
                                                 int unique_id,
                                                 unsigned index) {
  if (suggestions_options_index_ != -1 &&
      index == static_cast<unsigned>(suggestions_options_index_)) {
    // User selected 'AutoFill Options'.
    render_view_->Send(new ViewHostMsg_ShowAutoFillDialog(
        render_view_->routing_id()));
  } else if (suggestions_clear_index_ != -1 &&
             index == static_cast<unsigned>(suggestions_clear_index_)) {
    // User selected 'Clear form'.
    // The form has been auto-filled, so give the user the chance to clear the
    // form.
    form_manager_.ClearFormWithNode(node);
  } else if (form_manager_.FormWithNodeIsAutoFilled(node) || !unique_id) {
    // User selected an Autocomplete entry, so we fill directly.
    WebInputElement element = node.toConst<WebInputElement>();

    // Set the suggested value to update input element value immediately in UI.
    // The |setValue| call has update delayed until element loses focus.
    element.setSuggestedValue(value);
    element.setValue(value);

    WebFrame* webframe = node.document().frame();
    if (webframe)
      webframe->notifiyPasswordListenerOfAutocomplete(element);
  } else {
    // Fill the values for the whole form.
    QueryAutoFillFormData(node, value, label, unique_id, AUTOFILL_FILL);
  }

  suggestions_clear_index_ = -1;
  suggestions_options_index_ = -1;
}

void AutoFillHelper::DidClearAutoFillSelection(const WebNode& node) {
  form_manager_.ClearPreviewedFormWithNode(node);
}

void AutoFillHelper::FrameContentsAvailable(WebFrame* frame) {
  form_manager_.ExtractForms(frame);
  SendForms(frame);
}

void AutoFillHelper::FrameWillClose(WebFrame* frame) {
  form_manager_.ResetFrame(frame);
}

void AutoFillHelper::QueryAutoFillFormData(const WebNode& node,
                                           const WebString& value,
                                           const WebString& label,
                                           int unique_id,
                                           AutoFillAction action) {
  static int query_counter = 0;
  autofill_query_id_ = query_counter++;

  webkit_glue::FormData form;
  const WebInputElement element = node.toConst<WebInputElement>();
  if (!form_manager_.FindFormWithFormControlElement(
          element, FormManager::REQUIRE_NONE, &form))
    return;

  autofill_action_ = action;
  render_view_->Send(new ViewHostMsg_FillAutoFillFormData(
      render_view_->routing_id(), autofill_query_id_, form, value, label,
      unique_id));
}

void AutoFillHelper::SendForms(WebFrame* frame) {
  // TODO(jhawkins): Use FormManager once we have strict ordering of form
  // control elements in the cache.
  WebKit::WebVector<WebFormElement> web_forms;
  frame->forms(web_forms);

  std::vector<webkit_glue::FormData> forms;
  for (size_t i = 0; i < web_forms.size(); ++i) {
    const WebFormElement& web_form = web_forms[i];

    webkit_glue::FormData form;
    if (FormManager::WebFormElementToFormData(
            web_form, FormManager::REQUIRE_NONE, false, &form)) {
      forms.push_back(form);
    }
  }

  if (!forms.empty()) {
    render_view_->Send(new ViewHostMsg_FormsSeen(render_view_->routing_id(),
                                                 forms));
  }
}
