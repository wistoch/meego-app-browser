// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H__
#define CHROME_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H__

#include <atlbase.h>
#include <atlcom.h>

#include <oleacc.h>

#include "chrome/views/view.h"

////////////////////////////////////////////////////////////////////////////////
//
// ViewAccessibility
//
// Class implementing the MSAA IAccessible COM interface for a generic View,
// providing accessibility to be used by screen readers and other assistive
// technology (AT).
//
////////////////////////////////////////////////////////////////////////////////
class ATL_NO_VTABLE ViewAccessibility
  : public CComObjectRootEx<CComMultiThreadModel>,
    public IDispatchImpl<IAccessible, &IID_IAccessible, &LIBID_Accessibility> {
 public:
  BEGIN_COM_MAP(ViewAccessibility)
    COM_INTERFACE_ENTRY2(IDispatch, IAccessible)
    COM_INTERFACE_ENTRY(IAccessible)
  END_COM_MAP()

  ViewAccessibility() {}
  ~ViewAccessibility() {}

  HRESULT Initialize(ChromeViews::View* view);

  // Supported IAccessible methods.

  // Retrieves the number of accessible children.
  STDMETHODIMP get_accChildCount(LONG* child_count);

  // Retrieves an IDispatch interface pointer for the specified child.
  STDMETHODIMP get_accChild(VARIANT var_child, IDispatch** disp_child);

  // Retrieves the IDispatch interface of the object's parent.
  STDMETHODIMP get_accParent(IDispatch** disp_parent);

  // Traverses to another UI element and retrieves the object.
  STDMETHODIMP accNavigate(LONG nav_dir, VARIANT start, VARIANT* end);

  // Retrieves the object that has the keyboard focus.
  STDMETHODIMP get_accFocus(VARIANT* focus_child);

  // Retrieves the name of the specified object.
  STDMETHODIMP get_accName(VARIANT var_id, BSTR* name);

  // Retrieves the tooltip description.
  STDMETHODIMP get_accDescription(VARIANT var_id, BSTR* desc);

  // Retrieves the current state of the specified object.
  STDMETHODIMP get_accState(VARIANT var_id, VARIANT* state);

  // Retrieves information describing the role of the specified object.
  STDMETHODIMP get_accRole(VARIANT var_id, VARIANT* role);

  // Retrieves a string that describes the object's default action.
  STDMETHODIMP get_accDefaultAction(VARIANT var_id, BSTR* default_action);

  // Retrieves the specified object's current screen location.
  STDMETHODIMP accLocation(LONG* x_left, LONG* y_top, LONG* width, LONG* height,
                           VARIANT var_id);

  // Retrieves the child element or child object at a given point on the screen.
  STDMETHODIMP accHitTest(LONG x_left, LONG y_top, VARIANT* child);

  // Retrieves the specified object's shortcut.
  STDMETHODIMP get_accKeyboardShortcut(VARIANT var_id, BSTR* access_key);

  // Non-supported IAccessible methods.

  // Out-dated and can be safely said to be very rarely used.
  STDMETHODIMP accDoDefaultAction(VARIANT var_id);

  // No value associated with views.
  STDMETHODIMP get_accValue(VARIANT var_id, BSTR* value);

  // Selections not applicable to views.
  STDMETHODIMP get_accSelection(VARIANT* selected);
  STDMETHODIMP accSelect(LONG flags_sel, VARIANT var_id);

  // Help functions not supported.
  STDMETHODIMP get_accHelp(VARIANT var_id, BSTR* help);
  STDMETHODIMP get_accHelpTopic(BSTR* help_file, VARIANT var_id,
                                LONG* topic_id);

  // Deprecated functions, not implemented here.
  STDMETHODIMP put_accName(VARIANT var_id, BSTR put_name);
  STDMETHODIMP put_accValue(VARIANT var_id, BSTR put_val);

 private:
  // Checks to see if child_id is within the child bounds of view. Returns true
  // if the child is within the bounds, false otherwise.
  bool IsValidChild(int child_id, ChromeViews::View* view) const;

  // Determines navigation direction for accNavigate, based on left, up and
  // previous being mapped all to previous and right, down, next being mapped
  // to next. Returns true if navigation direction is next, false otherwise.
  bool IsNavDirNext(int nav_dir) const;

  // Determines if the navigation target is within the allowed bounds. Returns
  // true if it is, false otherwise.
  bool IsValidNav(int nav_dir, int start_id, int lower_bound,
                  int upper_bound) const;

  // Wrapper to retrieve the view's instance of IAccessible.
  AccessibleWrapper* GetAccessibleWrapper(ChromeViews::View* view) const {
    return view->GetAccessibleWrapper();
  }

  // Helper function which sets applicable states of view.
  void SetState(VARIANT* state, ChromeViews::View* view);

  // Member View needed for view-specific calls.
  ChromeViews::View* view_;

  DISALLOW_EVIL_CONSTRUCTORS(ViewAccessibility);
};
#endif  // CHROME_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H__

