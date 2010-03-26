// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_FORM_MANAGER_H_
#define CHROME_RENDERER_FORM_MANAGER_H_

#include <map>
#include <vector>

#include "base/string16.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "webkit/glue/form_data.h"

namespace WebKit {
class WebFrame;
}

// Manages the forms in a RenderView.
class FormManager {
 public:
  // A bitfield mask for form requirements.
  typedef enum {
    REQUIRE_NONE = 0x0,             // No requirements.
    REQUIRE_AUTOCOMPLETE = 0x1,     // Require that autocomplete != off.
    REQUIRE_ELEMENTS_ENABLED = 0x2  // Require that disabled attribute is off.
  } RequirementsMask;

  FormManager();
  virtual ~FormManager();

  // Scans the DOM in |frame| extracting and storing forms.
  void ExtractForms(WebKit::WebFrame* frame);

  // Returns a vector of forms that match |requirements|.
  void GetForms(std::vector<webkit_glue::FormData>* forms,
                RequirementsMask requirements);

  // Finds the form that contains |input_element| and returns it in |form|.
  // Returns false if the form is not found.
  bool FindForm(const WebKit::WebInputElement& input_element,
                webkit_glue::FormData* form);

  // Fills the form represented by |form|.  |form| should have the name set to
  // the name of the form to fill out, and the number of elements and values
  // must match the number of stored elements in the form.
  // TODO(jhawkins): Is matching on name alone good enough?  It's possible to
  // store multiple forms with the same names from different frames.
  bool FillForm(const webkit_glue::FormData& form);

  // Resets the stored set of forms.
  void Reset();

 private:
  // A map of WebInputElements keyed by each element's name.
  typedef std::map<string16, WebKit::WebInputElement> FormInputElementMap;

  // Stores the WebFormElement and the map of input elements for each form.
  struct FormElement {
    WebKit::WebFormElement form_element;
    FormInputElementMap input_elements;
  };

  // A map of vectors of FormElements keyed by the WebFrame containing each
  // form.
  typedef std::map<WebKit::WebFrame*, std::vector<FormElement*> >
      WebFrameFormElementMap;

  // Resets the forms for the specified |frame|.
  void ResetFrame(WebKit::WebFrame* frame);

  // Converts a FormElement to FormData storage.
  // TODO(jhawkins): Modify FormElement so we don't need |frame|.
  void FormElementToFormData(WebKit::WebFrame* frame,
                             const FormElement* form_element,
                             RequirementsMask requirements,
                             webkit_glue::FormData* form);

  // Returns the corresponding label for |element|.
  static string16 LabelForElement(const WebKit::WebInputElement& element);

  // Infers corresponding label for |element| from surrounding context in the
  // DOM.  Contents of preceeding <p> tag or preceeding text element found in
  // the form.
  static string16 InferLabelForElement(const WebKit::WebInputElement& element);

  // The map of form elements.
  WebFrameFormElementMap form_elements_map_;

  DISALLOW_COPY_AND_ASSIGN(FormManager);
};

#endif  // CHROME_RENDERER_FORM_MANAGER_H_
