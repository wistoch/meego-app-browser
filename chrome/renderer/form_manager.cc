// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/form_manager.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"

using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebNode;
using WebKit::WebString;
using WebKit::WebVector;

FormManager::FormManager() {
}

FormManager::~FormManager() {
  Reset();
}

void FormManager::ExtractForms(WebFrame* frame) {
  // Reset the vector of FormElements for this frame.
  ResetFrame(frame);

  WebVector<WebFormElement> web_forms;
  frame->forms(web_forms);

  // Form loop.
  for (size_t i = 0; i < web_forms.size(); ++i) {
    FormElement* form_elements = new FormElement;
    form_elements->form_element = web_forms[i];

    // Form elements loop.
    WebVector<WebInputElement> input_elements;
    form_elements->form_element.getInputElements(input_elements);
    for (size_t j = 0; j < input_elements.size(); ++j) {
      WebInputElement element = input_elements[j];
      form_elements->input_elements[element.nameForAutofill()] = element;
    }

    form_elements_map_[frame].push_back(form_elements);
  }
}

void FormManager::GetForms(std::vector<FormData>* forms,
                           RequirementsMask requirements) {
  // Frame loop.
  for (WebFrameFormElementMap::iterator iter = form_elements_map_.begin();
       iter != form_elements_map_.end(); ++iter) {
    WebFrame* frame = iter->first;

    // Form loop.
    for (std::vector<FormElement*>::iterator form_iter = iter->second.begin();
         form_iter != iter->second.end(); ++form_iter) {
      FormElement* form_element = *form_iter;

      if (requirements & REQUIRE_AUTOCOMPLETE &&
          !form_element->form_element.autoComplete())
        continue;

      FormData form;
      FormElementToFormData(frame, form_element, requirements, &form);
      forms->push_back(form);
    }
  }
}

bool FormManager::FindForm(const WebInputElement& element, FormData* form) {
  // Frame loop.
  for (WebFrameFormElementMap::iterator iter = form_elements_map_.begin();
       iter != form_elements_map_.end(); ++iter) {
    WebFrame* frame = iter->first;

    // Form loop.
    for (std::vector<FormElement*>::iterator form_iter = iter->second.begin();
         form_iter != iter->second.end(); ++form_iter) {
      FormElement* form_element = *form_iter;

      if (form_element->input_elements.find(element.nameForAutofill()) !=
          form_element->input_elements.end()) {
        FormElementToFormData(frame, form_element, REQUIRE_NONE, form);
        return true;
      }
    }
  }

  return false;
}

bool FormManager::FillForm(const FormData& form) {
  FormElement* form_element = NULL;

  // Frame loop.
  for (WebFrameFormElementMap::iterator iter = form_elements_map_.begin();
       iter != form_elements_map_.end(); ++iter) {
    // Form loop.
    for (std::vector<FormElement*>::iterator form_iter = iter->second.begin();
         form_iter != iter->second.end(); ++form_iter) {
      if ((*form_iter)->form_element.name() == form.name) {
        form_element = *form_iter;
        break;
      }
    }
  }

  if (!form_element)
    return false;

  DCHECK(form_element->input_elements.size() == form.elements.size());
  DCHECK(form.elements.size() == form.values.size());

  size_t i = 0;
  for (FormInputElementMap::iterator iter =
           form_element->input_elements.begin();
      iter != form_element->input_elements.end(); ++iter, ++i) {
    DCHECK(iter->second.nameForAutofill() == form.elements[i]);

    iter->second.setValue(form.values[i]);
    iter->second.setAutofilled(true);
  }

  return true;
}

void FormManager::Reset() {
  for (WebFrameFormElementMap::iterator iter = form_elements_map_.begin();
       iter != form_elements_map_.end(); ++iter) {
    STLDeleteElements(&iter->second);
  }
  form_elements_map_.clear();
}

void FormManager::ResetFrame(WebFrame* frame) {
  WebFrameFormElementMap::iterator iter = form_elements_map_.find(frame);
  if (iter != form_elements_map_.end()) {
    STLDeleteElements(&iter->second);
    form_elements_map_.erase(iter);
  }
}

void FormManager::FormElementToFormData(WebFrame* frame,
                                        const FormElement* form_element,
                                        RequirementsMask requirements,
                                        FormData* form) {
  form->name = form_element->form_element.name();
  form->origin = frame->url();
  form->action = frame->completeURL(form_element->form_element.action());

  // If the completed ULR is not valid, just use the action we get from
  // WebKit.
  if (!form->action.is_valid())
    form->action = GURL(form_element->form_element.action());

  // Form elements loop.
  for (FormInputElementMap::const_iterator element_iter =
           form_element->input_elements.begin();
       element_iter != form_element->input_elements.end(); ++element_iter) {
    const WebInputElement& input_element = element_iter->second;

    if (requirements & REQUIRE_AUTOCOMPLETE &&
        !input_element.autoComplete())
      continue;

    if (requirements & REQUIRE_ELEMENTS_ENABLED &&
        !input_element.isEnabledFormControl())
      continue;

    form->elements.push_back(input_element.nameForAutofill());
    form->values.push_back(input_element.value());

    // TODO(jhawkins): It's possible for a form to have more than one submit
    // input element.  The FormData structure probably doesn't need to keep
    // track of the name of any submit button.
    if (input_element.inputType() == WebInputElement::Submit)
      form->submit = input_element.nameForAutofill();
  }
}
