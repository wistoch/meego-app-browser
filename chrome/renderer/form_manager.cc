// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/form_manager.h"

#include "base/logging.h"
#include "base/scoped_vector.h"
#include "base/string_util.h"
#include "base/stl_util-inl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebLabelElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNodeList.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSelectElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;
using webkit_glue::FormField;
using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFormControlElement;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebLabelElement;
using WebKit::WebNode;
using WebKit::WebNodeList;
using WebKit::WebSelectElement;
using WebKit::WebString;
using WebKit::WebVector;

namespace {

// The number of fields required by AutoFill.  Ideally we could send the forms
// to AutoFill no matter how many fields are in the forms; however, finding the
// label for each field is a costly operation and we can't spare the cycles if
// it's not necessary.
const size_t kRequiredAutoFillFields = 3;

// This is a helper function for the FindChildText() function.
// Returns the node value of the descendant or sibling of |node| that is a
// non-empty text node.  This is a faster alternative to |innerText()| for
// performance critical operations.  It does a full depth-first search so
// can be used when the structure is not directly known.  It does not aggregate
// the text of multiple nodes, it just returns the value of the first found.
// "Non-empty" in this case means non-empty after the whitespace has been
// stripped.
string16 FindChildTextInner(const WebNode& node) {
  string16 element_text;
  if (node.isNull())
    return element_text;

  element_text = node.nodeValue();
  TrimWhitespace(element_text, TRIM_ALL, &element_text);
  if (!element_text.empty())
    return element_text;

  element_text = FindChildTextInner(node.firstChild());
  if (!element_text.empty())
    return element_text;

  element_text = FindChildTextInner(node.nextSibling());
  if (!element_text.empty())
    return element_text;

  return element_text;
}

// Returns the node value of the first decendant of |element| that is a
// non-empty text node.  "Non-empty" in this case means non-empty after the
// whitespace has been stripped.
string16 FindChildText(const WebElement& element) {
  WebNode child = element.firstChild();
  return FindChildTextInner(child);
}

}  // namespace

FormManager::FormManager() {
}

FormManager::~FormManager() {
  Reset();
}

// static
void FormManager::WebFormControlElementToFormField(
    const WebFormControlElement& element, bool get_value, FormField* field) {
  DCHECK(field);

  // The label is not officially part of a WebFormControlElement; however, the
  // labels for all form control elements are scraped from the DOM and set in
  // WebFormElementToFormData.
  field->set_name(element.nameForAutofill());
  field->set_form_control_type(element.formControlType());

  if (element.formControlType() == WebString::fromUTF8("text")) {
    const WebInputElement& input_element = element.toConst<WebInputElement>();
    field->set_size(input_element.size());
  }

  if (!get_value)
    return;

  // TODO(jhawkins): In WebKit, move value() and setValue() to
  // WebFormControlElement.
  string16 value;
  if (element.formControlType() == WebString::fromUTF8("text")) {
    const WebInputElement& input_element =
        element.toConst<WebInputElement>();
    value = input_element.value();
  } else if (element.formControlType() == WebString::fromUTF8("select-one")) {
    // TODO(jhawkins): This is ugly.  WebSelectElement::value() is a non-const
    // method.  Look into fixing this on the WebKit side.
    WebFormControlElement& e = const_cast<WebFormControlElement&>(element);
    WebSelectElement select_element = e.to<WebSelectElement>();
    value = select_element.value();
  }
  field->set_value(value);
}

// static
string16 FormManager::LabelForElement(const WebFormControlElement& element) {
  WebNodeList labels = element.document().getElementsByTagName("label");
  for (unsigned i = 0; i < labels.length(); ++i) {
    WebElement e = labels.item(i).to<WebElement>();
    if (e.hasTagName("label")) {
      WebLabelElement label = e.to<WebLabelElement>();
      if (label.correspondingControl() == element)
        return FindChildText(label);
    }
  }

  // Infer the label from context if not found in label element.
  return FormManager::InferLabelForElement(element);
}

// static
bool FormManager::WebFormElementToFormData(const WebFormElement& element,
                                           RequirementsMask requirements,
                                           bool get_values,
                                           FormData* form) {
  DCHECK(form);

  const WebFrame* frame = element.document().frame();
  if (!frame)
    return false;

  if (requirements & REQUIRE_AUTOCOMPLETE && !element.autoComplete())
    return false;

  form->name = element.name();
  form->method = element.method();
  form->origin = frame->url();
  form->action = frame->document().completeURL(element.action());

  // If the completed URL is not valid, just use the action we get from
  // WebKit.
  if (!form->action.is_valid())
    form->action = GURL(element.action());

  // A map from a FormField's name to the FormField itself.
  std::map<string16, FormField*> name_map;

  // The extracted FormFields.  We use pointers so we can store them in
  // |name_map|.
  ScopedVector<FormField> form_fields;

  WebVector<WebFormControlElement> control_elements;
  element.getFormControlElements(control_elements);

  // A vector of bools that indicate whether each field in the form meets the
  // requirements and thus will be in the resulting |form|.
  std::vector<bool> fields_extracted(control_elements.size(), false);

  for (size_t i = 0; i < control_elements.size(); ++i) {
    const WebFormControlElement& control_element = control_elements[i];

    if (requirements & REQUIRE_AUTOCOMPLETE &&
        control_element.formControlType() == WebString::fromUTF8("text")) {
      const WebInputElement& input_element =
          control_element.toConst<WebInputElement>();
      if (!input_element.autoComplete())
        continue;
    }

    if (requirements & REQUIRE_ELEMENTS_ENABLED && !control_element.isEnabled())
      continue;

    // Create a new FormField, fill it out and map it to the field's name.
    FormField* field = new FormField;
    WebFormControlElementToFormField(control_element, get_values, field);
    form_fields.push_back(field);
    // TODO(jhawkins): A label element is mapped to a form control element's id.
    // field->name() will contain the id only if the name does not exist.  Add
    // an id() method to WebFormControlElement and use that here.
    name_map[field->name()] = field;
    fields_extracted[i] = true;
  }

  // Don't extract field labels if we have no fields.
  if (form_fields.empty())
    return false;

  // Loop through the label elements inside the form element.  For each label
  // element, get the corresponding form control element, use the form control
  // element's name as a key into the <name, FormField> map to find the
  // previously created FormField and set the FormField's label to the
  // label.firstChild().nodeValue() of the label element.
  WebNodeList labels = element.getElementsByTagName("label");
  for (unsigned i = 0; i < labels.length(); ++i) {
    WebLabelElement label = labels.item(i).to<WebLabelElement>();
    WebFormControlElement field_element =
        label.correspondingControl().to<WebFormControlElement>();
    if (field_element.isNull() || !field_element.isFormControlElement())
      continue;

    std::map<string16, FormField*>::iterator iter =
        name_map.find(field_element.nameForAutofill());
    if (iter != name_map.end())
      iter->second->set_label(FindChildText(label));
  }

  // Loop through the form control elements, extracting the label text from the
  // DOM.  We use the |fields_extracted| vector to make sure we assign the
  // extracted label to the correct field, as it's possible |form_fields| will
  // not contain all of the elements in |control_elements|.
  for (size_t i = 0, field_idx = 0;
       i < control_elements.size() && field_idx < form_fields.size(); ++i) {
    // This field didn't meet the requirements, so don't try to find a label for
    // it.
    if (!fields_extracted[i])
      continue;

    const WebFormControlElement& control_element = control_elements[i];
    if (form_fields[field_idx]->label().empty())
      form_fields[field_idx]->set_label(
          FormManager::InferLabelForElement(control_element));

    ++field_idx;
  }

  // Copy the created FormFields into the resulting FormData object.
  for (ScopedVector<FormField>::const_iterator iter = form_fields.begin();
       iter != form_fields.end(); ++iter) {
    form->fields.push_back(**iter);
  }

  return true;
}

void FormManager::ExtractForms(const WebFrame* frame) {
  DCHECK(frame);

  // Reset the vector of FormElements for this frame.
  ResetFrame(frame);

  WebVector<WebFormElement> web_forms;
  frame->forms(web_forms);

  for (size_t i = 0; i < web_forms.size(); ++i) {
    FormElement* form_elements = new FormElement;
    form_elements->form_element = web_forms[i];

    WebVector<WebFormControlElement> control_elements;
    form_elements->form_element.getFormControlElements(control_elements);
    for (size_t j = 0; j < control_elements.size(); ++j) {
      WebFormControlElement element = control_elements[j];
      form_elements->control_elements.push_back(element);
    }

    form_elements_map_[frame].push_back(form_elements);
  }
}

void FormManager::GetForms(RequirementsMask requirements,
                           std::vector<FormData>* forms) {
  DCHECK(forms);

  for (WebFrameFormElementMap::iterator iter = form_elements_map_.begin();
       iter != form_elements_map_.end(); ++iter) {
    for (std::vector<FormElement*>::iterator form_iter = iter->second.begin();
         form_iter != iter->second.end(); ++form_iter) {
      FormData form;
      if (WebFormElementToFormData((*form_iter)->form_element,
                                   requirements,
                                   true,
                                   &form))
        forms->push_back(form);
    }
  }
}

void FormManager::GetFormsInFrame(const WebFrame* frame,
                                  RequirementsMask requirements,
                                  std::vector<FormData>* forms) {
  DCHECK(frame);
  DCHECK(forms);

  WebFrameFormElementMap::iterator iter = form_elements_map_.find(frame);
  if (iter == form_elements_map_.end())
    return;

  // TODO(jhawkins): Factor this out and use it here and in GetForms.
  const std::vector<FormElement*>& form_elements = iter->second;
  for (std::vector<FormElement*>::const_iterator form_iter =
           form_elements.begin();
       form_iter != form_elements.end(); ++form_iter) {
    FormElement* form_element = *form_iter;

    // We need at least |kRequiredAutoFillFields| fields before appending this
    // form to |forms|.
    if (form_element->control_elements.size() < kRequiredAutoFillFields)
      continue;

    if (requirements & REQUIRE_AUTOCOMPLETE &&
        !form_element->form_element.autoComplete())
      continue;

    FormData form;
    FormElementToFormData(frame, form_element, requirements, &form);
    if (form.fields.size() >= kRequiredAutoFillFields)
      forms->push_back(form);
  }
}

bool FormManager::FindForm(const WebFormElement& element,
                           RequirementsMask requirements,
                           FormData* form) {
  DCHECK(form);

  const WebFrame* frame = element.document().frame();
  if (!frame)
    return false;

  WebFrameFormElementMap::const_iterator frame_iter =
      form_elements_map_.find(frame);
  if (frame_iter == form_elements_map_.end())
    return false;

  for (std::vector<FormElement*>::const_iterator iter =
           frame_iter->second.begin();
       iter != frame_iter->second.end(); ++iter) {
    if ((*iter)->form_element.name() != element.name())
      continue;

    return FormElementToFormData(frame, *iter, requirements, form);
  }

  return false;
}

bool FormManager::FindFormWithFormControlElement(
    const WebFormControlElement& element,
    RequirementsMask requirements,
    FormData* form) {
  DCHECK(form);

  const WebFrame* frame = element.document().frame();
  if (!frame)
    return false;

  if (form_elements_map_.find(frame) == form_elements_map_.end())
    return false;

  const std::vector<FormElement*> forms = form_elements_map_[frame];
  for (std::vector<FormElement*>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    const FormElement* form_element = *iter;

    for (std::vector<WebFormControlElement>::const_iterator iter =
             form_element->control_elements.begin();
         iter != form_element->control_elements.end(); ++iter) {
      if (iter->nameForAutofill() == element.nameForAutofill()) {
        WebFormElementToFormData(
            form_element->form_element, requirements, true, form);
        return true;
      }
    }
  }

  return false;
}

bool FormManager::FillForm(const FormData& form) {
  FormElement* form_element = NULL;

  for (WebFrameFormElementMap::iterator iter = form_elements_map_.begin();
       iter != form_elements_map_.end(); ++iter) {
    const WebFrame* frame = iter->first;

    for (std::vector<FormElement*>::iterator form_iter = iter->second.begin();
         form_iter != iter->second.end(); ++form_iter) {
      // TODO(dhollowa): matching on form name here which is not guaranteed to
      // be unique for the page, nor is it guaranteed to be non-empty.  Need to
      // find a way to uniquely identify the form cross-process.  For now we'll
      // check form name and form action for identity.
      // http://crbug.com/37990 test file sample8.html.
      // Also note that WebString() == WebString(string16()) does not seem to
      // evaluate to |true| for some reason TBD, so forcing to string16.
      string16 element_name((*form_iter)->form_element.name());
      GURL action(
          frame->document().completeURL((*form_iter)->form_element.action()));
      if (element_name == form.name && action == form.action) {
        form_element = *form_iter;
        break;
      }
    }
  }

  if (!form_element)
    return false;

  // It's possible that the site has injected fields into the form after the
  // page has loaded, so we can't assert that the size of the cached control
  // elements is equal to the size of the fields in |form|.  Fortunately, the
  // one case in the wild where this happens, paypal.com signup form, the fields
  // are appended to the end of the form and are not visible.

  for (size_t i = 0, j = 0;
       i < form_element->control_elements.size() && j < form.fields.size();
       ++i, ++j) {
    // Once again, empty WebString != empty string16, so we have to explicitly
    // check for this case.
    if (form_element->control_elements[i].nameForAutofill().length() == 0 &&
        form.fields[j].name().empty())
      continue;

    // We assume that the intersection of the fields in
    // |form_element->control_elements| and |form.fields| is ordered, but it's
    // possible that one or the other sets may have more fields than the other,
    // so loop past non-matching fields in the set with more elements.
    while (form_element->control_elements[i].nameForAutofill() !=
           form.fields[j].name()) {
      if (form_element->control_elements.size() > form.fields.size()) {
        // We're at the end of the elements already.
        if (i + 1 == form_element->control_elements.size())
          break;
        ++i;
      } else if (form.fields.size() > form_element->control_elements.size()) {
        // We're at the end of the elements already.
        if (j + 1 == form.fields.size())
          break;
        ++j;
      } else {
        NOTREACHED();
      }

      continue;
    }

    WebFormControlElement* element = &form_element->control_elements[i];

    // It's possible that nameForAutofill() is empty if the form control element
    // has no name or ID.  In that case, iter->nameForAutofill() must also be
    // empty.
    if (form.fields[j].name().empty())
      DCHECK(element->nameForAutofill().isEmpty());
    else
      DCHECK_EQ(form.fields[j].name(), element->nameForAutofill());

    if (!form.fields[j].value().empty() &&
        element->formControlType() != WebString::fromUTF8("submit")) {
      if (element->formControlType() == WebString::fromUTF8("text")) {
        WebInputElement input_element = element->to<WebInputElement>();
        // If the maxlength attribute contains a negative value, maxLength()
        // returns the default maxlength value.
        input_element.setValue(
            form.fields[j].value().substr(0, input_element.maxLength()));
        input_element.setAutofilled(true);
      } else if (element->formControlType() ==
                 WebString::fromUTF8("select-one")) {
        WebSelectElement select_element =
            element->to<WebSelectElement>();
        select_element.setValue(form.fields[j].value());
      }
    }
  }

  return true;
}

void FormManager::FillForms(const std::vector<webkit_glue::FormData>& forms) {
  for (std::vector<webkit_glue::FormData>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    FillForm(*iter);
  }
}

void FormManager::Reset() {
  for (WebFrameFormElementMap::iterator iter = form_elements_map_.begin();
       iter != form_elements_map_.end(); ++iter) {
    STLDeleteElements(&iter->second);
  }
  form_elements_map_.clear();
}

void FormManager::ResetFrame(const WebFrame* frame) {
  WebFrameFormElementMap::iterator iter = form_elements_map_.find(frame);
  if (iter != form_elements_map_.end()) {
    STLDeleteElements(&iter->second);
    form_elements_map_.erase(iter);
  }
}

// static
bool FormManager::FormElementToFormData(const WebFrame* frame,
                                        const FormElement* form_element,
                                        RequirementsMask requirements,
                                        FormData* form) {
  if (requirements & REQUIRE_AUTOCOMPLETE &&
      !form_element->form_element.autoComplete())
    return false;

  form->name = form_element->form_element.name();
  form->method = form_element->form_element.method();
  form->origin = frame->url();
  form->action =
      frame->document().completeURL(form_element->form_element.action());

  // If the completed URL is not valid, just use the action we get from
  // WebKit.
  if (!form->action.is_valid())
    form->action = GURL(form_element->form_element.action());

  // Form elements loop.
  for (std::vector<WebFormControlElement>::const_iterator element_iter =
           form_element->control_elements.begin();
       element_iter != form_element->control_elements.end(); ++element_iter) {
    const WebFormControlElement& control_element = *element_iter;

    if (requirements & REQUIRE_AUTOCOMPLETE &&
        control_element.formControlType() == WebString::fromUTF8("text")) {
      const WebInputElement& input_element =
          control_element.toConst<WebInputElement>();
      if (!input_element.autoComplete())
        continue;
    }

    if (requirements & REQUIRE_ELEMENTS_ENABLED && !control_element.isEnabled())
      continue;

    FormField field;
    WebFormControlElementToFormField(control_element, false, &field);
    form->fields.push_back(field);
  }

  return true;
}

// static
string16 FormManager::InferLabelForElement(
    const WebFormControlElement& element) {
  string16 inferred_label;
  WebNode previous = element.previousSibling();
  if (!previous.isNull()) {
    if (previous.isTextNode()) {
      inferred_label = previous.nodeValue();
      TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
    }

    // If we didn't find text, check for previous paragraph.
    // Eg. <p>Some Text</p><input ...>
    // Note the lack of whitespace between <p> and <input> elements.
    if (inferred_label.empty()) {
      if (previous.isElementNode()) {
        WebElement element = previous.to<WebElement>();
        if (element.hasTagName("p")) {
          inferred_label = FindChildText(element);
        }
      }
    }

    // If we didn't find paragraph, check for previous paragraph to this.
    // Eg. <p>Some Text</p>   <input ...>
    // Note the whitespace between <p> and <input> elements.
    if (inferred_label.empty()) {
      previous = previous.previousSibling();
      if (!previous.isNull() && previous.isElementNode()) {
        WebElement element = previous.to<WebElement>();
        if (element.hasTagName("p")) {
          inferred_label = FindChildText(element);
        }
      }
    }

    // Look for text node prior to <img> tag.
    // Eg. Some Text<img/><input ...>
    if (inferred_label.empty()) {
      while (inferred_label.empty() && !previous.isNull()) {
        if (previous.isTextNode()) {
          inferred_label = previous.nodeValue();
          TrimWhitespace(inferred_label, TRIM_ALL, &inferred_label);
        } else if (previous.isElementNode()) {
          WebElement element = previous.to<WebElement>();
          if (!element.hasTagName("img"))
            break;
        } else {
          break;
        }
        previous = previous.previousSibling();
      }
    }
  }

  // If we didn't find paragraph, check for table cell case.
  // Eg. <tr><td>Some Text</td><td><input ...></td></tr>
  // Eg. <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
  if (inferred_label.empty()) {
    WebNode parent = element.parentNode();
    while (!parent.isNull() && parent.isElementNode() &&
           !parent.to<WebElement>().hasTagName("td"))
      parent = parent.parentNode();

    if (!parent.isNull() && parent.isElementNode()) {
      WebElement element = parent.to<WebElement>();
      if (element.hasTagName("td")) {
        previous = parent.previousSibling();

        // Skip by any intervening text nodes.
        while (!previous.isNull() && previous.isTextNode())
          previous = previous.previousSibling();

        if (!previous.isNull() && previous.isElementNode()) {
          element = previous.to<WebElement>();
          if (element.hasTagName("td")) {
            inferred_label = FindChildText(element);
          }
        }
      }
    }
  }

  return inferred_label;
}
