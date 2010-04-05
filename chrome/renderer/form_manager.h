// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_FORM_MANAGER_H_
#define CHROME_RENDERER_FORM_MANAGER_H_

#include <map>
#include <vector>

#include "base/string16.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormElement.h"

namespace webkit_glue {
struct FormData;
class FormField;
}  // namespace webkit_glue

namespace WebKit {
class WebFormControlElement;
class WebFrame;
}  // namespace WebKit

// Manages the forms in a RenderView.
class FormManager {
 public:
  // A bit field mask for form requirements.
  enum RequirementsMask {
    REQUIRE_NONE = 0x0,             // No requirements.
    REQUIRE_AUTOCOMPLETE = 0x1,     // Require that autocomplete != off.
    REQUIRE_ELEMENTS_ENABLED = 0x2  // Require that disabled attribute is off.
  };

  FormManager();
  virtual ~FormManager();

  // Fills out a FormField object from a given WebFormControlElement.
  static void WebFormControlElementToFormField(
      const WebKit::WebFormControlElement& element,
      webkit_glue::FormField* field);

  // Fills out a FormData object from a given WebFormElement.  Returns true if
  // |form| is filled out; it's possible that |element| won't meet the
  // requirements in |requirements|.  This also returns false if there are no
  // fields in |form|.
  // TODO(jhawkins): Remove the user of this in RenderView and move this to
  // private.
  static bool WebFormElementToFormData(const WebKit::WebFormElement& element,
                                       RequirementsMask requirements,
                                       webkit_glue::FormData* form);

  // Scans the DOM in |frame| extracting and storing forms.
  void ExtractForms(const WebKit::WebFrame* frame);

  // Returns a vector of forms that match |requirements|.
  void GetForms(RequirementsMask requirements,
                std::vector<webkit_glue::FormData>* forms);

  // Returns a vector of forms in |frame| that match |requirements|.
  void GetFormsInFrame(const WebKit::WebFrame* frame,
                       RequirementsMask requirements,
                       std::vector<webkit_glue::FormData>* forms);

  // Returns the cached FormData for |element|.  Returns true if the form was
  // found in the cache.
  bool FindForm(const WebKit::WebFormElement& element,
                RequirementsMask requirements,
                webkit_glue::FormData* form);

  // Finds the form that contains |element| and returns it in |form|. Returns
  // false if the form is not found.
  bool FindFormWithFormControlElement(
      const WebKit::WebFormControlElement& element,
      RequirementsMask requirements,
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
  // A map of WebFormControlElements keyed by each element's name.
  typedef std::map<string16, WebKit::WebFormControlElement>
      FormControlElementMap;

  // Stores the WebFormElement and the map of form control elements for each
  // form.
  struct FormElement {
    WebKit::WebFormElement form_element;
    FormControlElementMap control_elements;
  };

  // A map of vectors of FormElements keyed by the WebFrame containing each
  // form.
  typedef std::map<const WebKit::WebFrame*, std::vector<FormElement*> >
      WebFrameFormElementMap;

  // Converts a FormElement to FormData storage.  Returns false if the form does
  // not meet all the requirements in the requirements mask.
  // TODO(jhawkins): Modify FormElement so we don't need |frame|.
  static bool FormElementToFormData(const WebKit::WebFrame* frame,
                                    const FormElement* form_element,
                                    RequirementsMask requirements,
                                    webkit_glue::FormData* form);

  // Resets the forms for the specified |frame|.
  void ResetFrame(const WebKit::WebFrame* frame);

  // Returns the corresponding label for |element|.
  static string16 LabelForElement(const WebKit::WebFormControlElement& element);

  // Infers corresponding label for |element| from surrounding context in the
  // DOM.  Contents of preceeding <p> tag or preceeding text element found in
  // the form.
  static string16 InferLabelForElement(
      const WebKit::WebFormControlElement& element);

  // The map of form elements.
  WebFrameFormElementMap form_elements_map_;

  DISALLOW_COPY_AND_ASSIGN(FormManager);
};

#endif  // CHROME_RENDERER_FORM_MANAGER_H_
