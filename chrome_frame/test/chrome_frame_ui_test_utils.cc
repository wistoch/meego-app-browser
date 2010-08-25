// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/chrome_frame_ui_test_utils.h"

#include <windows.h>

#include <sstream>
#include <stack>

#include "base/message_loop.h"
#include "base/scoped_bstr_win.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "chrome_frame/test/win_event_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_frame_test {

// Timeout for waiting on Chrome to create the accessibility tree for the DOM.
const int kChromeDOMAccessibilityTreeTimeoutMs = 10 * 1000;

// Timeout for waiting on a menu to popup.
const int kMenuPopupTimeoutMs = 10 * 1000;

// AccObject methods
AccObject::AccObject(IAccessible* accessible, int child_id)
    : accessible_(accessible), child_id_(child_id) {
  DCHECK(accessible);
  if (child_id != CHILDID_SELF) {
    ScopedComPtr<IDispatch> dispatch;
    // This class does not support referring to a full MSAA object using the
    // parent object and the child id.
    HRESULT result = accessible_->get_accChild(child_id_, dispatch.Receive());
    if (result != S_FALSE && result != E_NOINTERFACE) {
      LOG(ERROR) << "AccObject created which refers to full MSAA object using "
                    "parent object and child id. This should NOT be done.";
    }
    DCHECK(result == S_FALSE || result == E_NOINTERFACE);
  }
}

// static
AccObject* AccObject::CreateFromWindow(HWND hwnd) {
  ScopedComPtr<IAccessible> accessible;
  ::AccessibleObjectFromWindow(hwnd, OBJID_CLIENT,
      IID_IAccessible, reinterpret_cast<void**>(accessible.Receive()));
  if (accessible)
    return new AccObject(accessible);
  return NULL;
}

// static
AccObject* AccObject::CreateFromEvent(HWND hwnd, LONG object_id,
                                      LONG child_id) {
  ScopedComPtr<IAccessible> accessible;
  ScopedVariant acc_child_id;
  ::AccessibleObjectFromEvent(hwnd, object_id, child_id, accessible.Receive(),
                              acc_child_id.Receive());
  if (accessible && acc_child_id.type() == VT_I4)
    return new AccObject(accessible, V_I4(&acc_child_id));
  return NULL;
}

// static
AccObject* AccObject::CreateFromDispatch(IDispatch* dispatch) {
  if (dispatch) {
    ScopedComPtr<IAccessible> accessible;
    accessible.QueryFrom(dispatch);
    if (accessible)
      return new AccObject(accessible);
  }
  return NULL;
}

// static
AccObject* AccObject::CreateFromPoint(int x, int y) {
  ScopedComPtr<IAccessible> accessible;
  ScopedVariant child_id;
  POINT point = {x, y};
  ::AccessibleObjectFromPoint(point, accessible.Receive(), child_id.Receive());
  if (accessible && child_id.type() == VT_I4)
    return new AccObject(accessible, V_I4(&child_id));
  return NULL;
}

bool AccObject::DoDefaultAction() {
  HWND parent_window = NULL;
  ::WindowFromAccessibleObject(accessible_, &parent_window);
  if (!parent_window) {
    DLOG(ERROR) << "Could not get the window containing the given "
                << "accessibility object: " << GetDescription();
    return false;
  }
  wchar_t class_name[MAX_PATH];
  if (!::GetClassName(parent_window, class_name, arraysize(class_name))) {
    DLOG(ERROR) << "Could not get class name from accessibily object's window";
    return false;
  }
  if (wcscmp(class_name, L"#32768") == 0) {
    // Hack: if the desktop is locked, menu items cannot be selected using
    // accDoDefaultAction for some unknown reason. Get around this by
    // sending mouse button messages at the menu item location. Do this
    // even if the desktop is unlocked for consistency. We do not do this
    // for all objects because DoDefaultAction is not always equivalent to
    // a mouse click.
    gfx::Rect menu_item_rect;
    if (GetLocation(&menu_item_rect)) {
      DLOG(WARNING) << "Attempting to click menu item via mouse messages. "
                    << "May not work for all menus";
      // WM_LBUTTON* messages supposedly expect relative coordinates to the
      // client area, which for popup menus seems to be the entire desktop.
      gfx::Point center = menu_item_rect.CenterPoint();
      LPARAM coordinates = (center.y() << 16) | center.x();
      ::PostMessage(parent_window, WM_LBUTTONDOWN, (WPARAM)0, coordinates);
      ::PostMessage(parent_window, WM_LBUTTONUP, (WPARAM)0, coordinates);
      return true;
    }
    // TODO(kkania): Try sending the access key for the menu item to the
    // menu if the mouse button messages are found to be insufficient
    // at times.
    DLOG(ERROR) << "Could not get location of menu item via MSAA. "
        << "Accessibility object: " << WideToUTF8(this->GetDescription())
        << std::endl << "This is necessary to select a menu item since the "
        << "desktop is locked.";
    return false;
  }

  HRESULT result = accessible_->accDoDefaultAction(child_id_);
  EXPECT_HRESULT_SUCCEEDED(result)
      << "Could not do default action for AccObject: " << GetDescription();
  return SUCCEEDED(result);
}

bool AccObject::Focus() {
  EXPECT_HRESULT_SUCCEEDED(
      accessible_->accSelect(SELFLAG_TAKEFOCUS, child_id_));

  // Double check that the object actually received focus. In some cases
  // the parent object must have the focus first.
  bool did_focus = false;
  ScopedVariant focused;
  if (SUCCEEDED(accessible_->get_accFocus(focused.Receive()))) {
    if (focused.type() != VT_EMPTY)
      did_focus = true;
  }
  EXPECT_TRUE(did_focus) << "Could not focus AccObject: " << GetDescription();
  return did_focus;
}

bool AccObject::Select() {
  // SELFLAG_TAKESELECTION needs to be combined with the focus in order to
  // take effect.
  int selection_flag = SELFLAG_TAKEFOCUS | SELFLAG_TAKESELECTION;
  EXPECT_HRESULT_SUCCEEDED(accessible_->accSelect(selection_flag, child_id_));

  // Double check that the object actually received selection.
  bool did_select = false;
  ScopedVariant selected;
  if (SUCCEEDED(accessible_->get_accSelection(selected.Receive()))) {
    if (selected.type() != VT_EMPTY)
      did_select = true;
  }
  EXPECT_TRUE(did_select) << "Could not select AccObject: " << GetDescription();
  return did_select;
}

bool AccObject::SetValue(const std::wstring& value) {
  ScopedBstr value_bstr(value.c_str());
  EXPECT_HRESULT_SUCCEEDED(accessible_->put_accValue(child_id_, value_bstr));

  // Double check that the object's value has actually changed. Some objects'
  // values can not be changed.
  bool did_set_value = false;
  std::wstring actual_value = L"-";
  if (GetValue(&actual_value) && value == actual_value) {
    did_set_value = true;
  }
  EXPECT_TRUE(did_set_value) << "Could not set value for AccObject: "
                             << GetDescription();
  return did_set_value;
}

bool AccObject::GetName(std::wstring* name) {
  DCHECK(name);
  ScopedBstr name_bstr;
  HRESULT result = accessible_->get_accName(child_id_, name_bstr.Receive());
  if (SUCCEEDED(result))
    name->assign(name_bstr, name_bstr.Length());
  return SUCCEEDED(result);
}

bool AccObject::GetRoleText(std::wstring* role_text) {
  DCHECK(role_text);
  ScopedVariant role_variant;
  if (SUCCEEDED(accessible_->get_accRole(child_id_, role_variant.Receive()))) {
    if (role_variant.type() == VT_I4) {
      wchar_t role_text_array[50];
      UINT characters = ::GetRoleText(V_I4(&role_variant), role_text_array,
                                      arraysize(role_text_array));
      if (characters) {
        *role_text = role_text_array;
        return true;
      } else {
        DLOG(ERROR) << "GetRoleText failed for role: "
                    << V_I4(&role_variant);
      }
    } else if (role_variant.type() == VT_BSTR) {
      *role_text = V_BSTR(&role_variant);
      return true;
    } else {
      DLOG(ERROR) << "Role was unexpected variant type: "
                  << role_variant.type();
    }
  }
  return false;
}

bool AccObject::GetValue(std::wstring* value) {
  DCHECK(value);
  ScopedBstr value_bstr;
  HRESULT result = accessible_->get_accValue(child_id_, value_bstr.Receive());
  if (SUCCEEDED(result))
    value->assign(value_bstr, value_bstr.Length());
  return SUCCEEDED(result);
}

bool AccObject::GetState(int* state) {
  DCHECK(state);
  ScopedVariant state_variant;
  if (SUCCEEDED(accessible_->get_accState(child_id_,
                                          state_variant.Receive()))) {
    if (state_variant.type() == VT_I4) {
      *state = V_I4(&state_variant);
      return true;
    }
  }
  return false;
}

bool AccObject::GetLocation(gfx::Rect* location) {
  DCHECK(location);
  long left, top, width, height;  // NOLINT
  HRESULT result = accessible_->accLocation(&left, &top, &width, &height,
                                            child_id_);
  if (SUCCEEDED(result))
    *location = gfx::Rect(left, top, width, height);
  return SUCCEEDED(result);
}

AccObject* AccObject::GetParent() {
  if (IsSimpleElement())
    return new AccObject(accessible_);
  ScopedComPtr<IDispatch> dispatch;
  if (FAILED(accessible_->get_accParent(dispatch.Receive())))
    return NULL;
  return AccObject::CreateFromDispatch(dispatch.get());
}

bool AccObject::GetChildren(RefCountedAccObjectVector* client_objects) {
  DCHECK(client_objects);
  int child_count;
  if (!GetChildCount(&child_count)) {
    LOG(ERROR) << "Failed to get child count of AccObject";
    return false;
  }
  if (child_count == 0)
    return true;
  scoped_array<VARIANT> unscoped_children(new VARIANT[child_count]);
  long actual_child_count;  // NOLINT
  if (FAILED(AccessibleChildren(accessible_, 0L, child_count,
                                unscoped_children.get(),
                                &actual_child_count))) {
    LOG(ERROR) << "Failed to get children of accessible object";
    return false;
  }
  if (actual_child_count == 0)
    return true;
  // Convert the retrieved children array into an array of scoped children.
  scoped_array<ScopedVariant> children(new ScopedVariant[child_count]);
  for (int i = 0; i < child_count; ++i) {
    children[i].Reset(unscoped_children[i]);
  }

  RefCountedAccObjectVector objects;
  for (int i = 0; i < actual_child_count; i++) {
    ScopedComPtr<IDispatch> dispatch;
    if (children[i].type() == VT_I4) {
      // According to MSDN, a server is allowed to return a full Accessibility
      // object using the parent object and the child id. If get_accChild is
      // called with the id, the server must return the actual IAccessible
      // interface. Do that here to get an actual IAccessible interface if
      // possible, since this class operates under the assumption that if the
      // child id is not CHILDID_SELF, the object is a simple element. See the
      // DCHECK in the constructor.
      HRESULT result = accessible_->get_accChild(children[i],
                                                 dispatch.Receive());
      if (result == S_FALSE || result == E_NOINTERFACE) {
        // The object in question really is a simple element. Add it.
        objects.push_back(new AccObject(accessible_, V_I4(&children[i])));
        continue;
      } else if (FAILED(result)) {
        DLOG(WARNING) << "Failed to determine if child id refers to a full "
                      << "object. Error: " << result << std::endl
                      << "Parent object: " << WideToUTF8(GetDescription())
                      << std::endl << "Child ID: " << V_I4(&children[i]);
        // Disregard this object.
        continue;
      }
      // The object in question was actually a full object. It is saved in the
      // |dispatch| arg and will be added down below.
    } else if (children[i].type() == VT_DISPATCH) {
      dispatch.Attach(V_DISPATCH(&children[i].Release()));
    } else {
      DLOG(WARNING) << "Unrecognizable child type, omitting from children";
      continue;
    }

    scoped_refptr<AccObject> child = CreateFromDispatch(dispatch.get());
    if (child) {
      objects.push_back(child);
    } else {
      LOG(ERROR) << "Failed to create AccObject from IDispatch";
      return false;
    }
  }

  client_objects->insert(client_objects->end(), objects.begin(), objects.end());
  return true;
}

bool AccObject::GetChildCount(int* child_count) {
  DCHECK(child_count);
  *child_count = 0;
  if (!IsSimpleElement()) {
    long long_child_count;  // NOLINT
    if (FAILED(accessible_->get_accChildCount(&long_child_count)))
      return false;
    *child_count = static_cast<int>(long_child_count);
  }
  return true;
}

bool AccObject::IsSimpleElement() {
  return V_I4(&child_id_) != CHILDID_SELF;
}

bool AccObject::Equals(AccObject* other) {
  if (other) {
    DCHECK(child_id_.type() == VT_I4 && other->child_id_.type() == VT_I4);
    return accessible_.get() == other->accessible_.get() &&
        V_I4(&child_id_) == V_I4(&other->child_id_);
  }
  return false;
}

std::wstring AccObject::GetDescription() {
  std::wstring name = L"-", role_text = L"-", value = L"-";
  if (GetName(&name))
    name = L"'" + name + L"'";
  if (GetRoleText(&role_text))
    role_text = L"'" + role_text + L"'";
  if (GetValue(&value))
    value = L"'" + value + L"'";
  int state = 0;
  GetState(&state);
  return StringPrintf(L"[%ls, %ls, %ls, 0x%x]", name.c_str(), role_text.c_str(),
                      value.c_str(), state);
}

std::wstring AccObject::GetTree() {
  std::wostringstream string_stream;
  string_stream << L"Accessibility object tree:" << std::endl;
  string_stream << L"[name, role_text, value, state]" << std::endl;

  std::stack<std::pair<scoped_refptr<AccObject>, int> > pairs;
  pairs.push(std::make_pair(this, 0));
  while (!pairs.empty()) {
    scoped_refptr<AccObject> object = pairs.top().first;
    int depth = pairs.top().second;
    pairs.pop();

    for (int i = 0; i < depth; ++i)
      string_stream << L"    ";
    string_stream << object->GetDescription() << std::endl;

    RefCountedAccObjectVector children;
    if (object->GetChildren(&children)) {
      for (int i = static_cast<int>(children.size()) - 1; i >= 0; --i)
        pairs.push(std::make_pair(children[i], depth + 1));
    }
  }
  return string_stream.str();
}

// AccObjectMatcher methods
AccObjectMatcher::AccObjectMatcher(const std::wstring& name,
                                   const std::wstring& role_text,
                                   const std::wstring& value)
    : name_(name), role_text_(role_text), value_(value) {
}

bool AccObjectMatcher::FindHelper(AccObject* object,
                                  scoped_refptr<AccObject>* match) const {
  // Determine if |object| is a match.
  bool does_match = true;
  std::wstring name, role_text, value;
  if (name_.length()) {
    object->GetName(&name);
    does_match = MatchPatternWide(name, name_);
  }
  if (does_match && role_text_.length()) {
    object->GetRoleText(&role_text);
    does_match = MatchPatternWide(role_text, role_text_);
  }
  if (does_match && value_.length()) {
    object->GetValue(&value);
    does_match = MatchPatternWide(value, value_);
  }
  if (does_match) {
    *match = object;
  } else {
    // Try to match the children of |object|.
    AccObject::RefCountedAccObjectVector children;
    if (!object->GetChildren(&children)) {
      LOG(ERROR) << "Could not get children of AccObject";
      return false;
    }
    for (size_t i = 0; i < children.size(); ++i) {
      if (!FindHelper(children[i], match)) {
        return false;
      }
      if (*match)
        break;
    }
  }
  return true;
}

bool AccObjectMatcher::Find(AccObject* object,
                            scoped_refptr<AccObject>* match) const {
  DCHECK(object);
  DCHECK(match);
  *match = NULL;
  return FindHelper(object, match);
}

bool AccObjectMatcher::FindInWindow(HWND hwnd,
                                    scoped_refptr<AccObject>* match) const {
  scoped_refptr<AccObject> object(AccObject::CreateFromWindow(hwnd));
  if (!object) {
    LOG(INFO) << "Failed to get accessible object from window";
    return false;
  }
  return Find(object.get(), match);
}

std::wstring AccObjectMatcher::GetDescription() const {
  std::wostringstream ss;
  ss << L"[";
  if (name_.length())
    ss << L"Name: '" << name_ << L"', ";
  if (role_text_.length())
    ss << L"Role: '" << role_text_ << L"', ";
  if (value_.length())
    ss << L"Value: '" << value_ << L"'";
  ss << L"]";
  return ss.str();
}

// Other methods
bool FindAccObjectInWindow(HWND hwnd, const AccObjectMatcher& matcher,
                           scoped_refptr<AccObject>* object) {
  DCHECK(object);
  EXPECT_TRUE(matcher.FindInWindow(hwnd, object));
  EXPECT_TRUE(*object) << "Element not found for matcher: "
        << matcher.GetDescription();
  if (!*object)
    DumpAccessibilityTreeForWindow(hwnd);
  return *object;
}

void DumpAccessibilityTreeForWindow(HWND hwnd) {
  scoped_refptr<AccObject> object(AccObject::CreateFromWindow(hwnd));
  if (object)
    std::wcout << object->GetTree();
  else
    std::cout << "Could not get IAccessible for window" << std::endl;
}

bool IsDesktopUnlocked() {
  HDESK desk = ::OpenInputDesktop(0, FALSE, DESKTOP_SWITCHDESKTOP);
  if (desk)
    ::CloseDesktop(desk);
  return desk;
}

}  // namespace chrome_frame_test
