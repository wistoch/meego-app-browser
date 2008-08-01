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

#include "chrome/browser/bookmark_codec.h"

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/bookmark_bar_model.h"
#include "googleurl/src/gurl.h"

// Key names.
static const wchar_t* kRootsKey = L"roots";
static const wchar_t* kRootFolderNameKey = L"root";
static const wchar_t* kOtherBookmarFolderNameKey = L"other";
static const wchar_t* kVersionKey = L"version";
static const wchar_t* kTypeKey = L"type";
static const wchar_t* kNameKey = L"name";
static const wchar_t* kDateAddedKey = L"date_added";
static const wchar_t* kURLKey = L"url";
static const wchar_t* kDateModifiedKey = L"date_modified";
static const wchar_t* kChildrenKey = L"children";

// Possible values for kTypeKey.
static const wchar_t* kTypeURL = L"url";
static const wchar_t* kTypeFolder = L"folder";

// Current version of the file.
static const int kCurrentVersion = 1;

Value* BookmarkCodec::Encode(BookmarkBarModel* model) {
  DictionaryValue* roots = new DictionaryValue();
  roots->Set(kRootFolderNameKey, EncodeNode(model->GetBookmarkBarNode()));
  roots->Set(kOtherBookmarFolderNameKey, EncodeNode(model->other_node()));

  DictionaryValue* main = new DictionaryValue();
  main->SetInteger(kVersionKey, kCurrentVersion);
  main->Set(kRootsKey, roots);
  return main;
}

bool BookmarkCodec::Decode(BookmarkBarModel* model, const Value& value) {
  if (value.GetType() != Value::TYPE_DICTIONARY)
    return false;  // Unexpected type.

  const DictionaryValue& d_value = static_cast<const DictionaryValue&>(value);

  int version;
  if (!d_value.GetInteger(kVersionKey, &version) || version != kCurrentVersion)
    return false;  // Unknown version.

  Value* roots;
  if (!d_value.Get(kRootsKey, &roots))
    return false;  // No roots.

  if (roots->GetType() != Value::TYPE_DICTIONARY)
    return false;  // Invalid type for roots.

  DictionaryValue* roots_d_value = static_cast<DictionaryValue*>(roots);
  Value* root_folder_value;
  Value* other_folder_value;
  if (!roots_d_value->Get(kRootFolderNameKey, &root_folder_value) ||
      root_folder_value->GetType() != Value::TYPE_DICTIONARY ||
      !roots_d_value->Get(kOtherBookmarFolderNameKey, &other_folder_value) ||
      other_folder_value->GetType() != Value::TYPE_DICTIONARY)
    return false;  // Invalid type for root folder and/or other folder.

  DecodeNode(model, *static_cast<DictionaryValue*>(root_folder_value),
             model->GetBookmarkBarNode());
  DecodeNode(model, *static_cast<DictionaryValue*>(other_folder_value),
             model->other_node());
  // Need to reset these as Decode sets the type to FOLDER.
  model->GetBookmarkBarNode()->type_ = history::StarredEntry::BOOKMARK_BAR;
  model->other_node()->type_ = history::StarredEntry::OTHER;
  return true;
}

Value* BookmarkCodec::EncodeNode(BookmarkBarNode* node) {
  DictionaryValue* value = new DictionaryValue();
  value->SetString(kNameKey, node->GetTitle());
  value->SetString(kDateAddedKey,
                   Int64ToWString(node->date_added().ToInternalValue()));
  if (node->GetType() == history::StarredEntry::URL) {
    value->SetString(kTypeKey, kTypeURL);
    value->SetString(kURLKey,
                     UTF8ToWide(node->GetURL().possibly_invalid_spec()));
  } else {
    value->SetString(kTypeKey, kTypeFolder);
    value->SetString(kDateModifiedKey,
                     Int64ToWString(node->date_group_modified().
                                    ToInternalValue()));

    ListValue* child_values = new ListValue();
    value->Set(kChildrenKey, child_values);
    for (int i = 0; i < node->GetChildCount(); ++i)
      child_values->Append(EncodeNode(node->GetChild(i)));
  }
  return value;
}

bool BookmarkCodec::DecodeChildren(BookmarkBarModel* model,
                                   const ListValue& child_value_list,
                                   BookmarkBarNode* parent) {
  for (size_t i = 0; i < child_value_list.GetSize(); ++i) {
    Value* child_value;
    if (!child_value_list.Get(i, &child_value))
      return false;

    if (child_value->GetType() != Value::TYPE_DICTIONARY)
      return false;

    BookmarkBarNode* child = new BookmarkBarNode(model);
    parent->Add(static_cast<int>(i), child);
    if (!DecodeNode(model, *static_cast<DictionaryValue*>(child_value), child))
      return false;
  }
  return true;
}

bool BookmarkCodec::DecodeNode(BookmarkBarModel* model,
                               const DictionaryValue& value,
                               BookmarkBarNode* node) {
  std::wstring title;
  if (!value.GetString(kNameKey, &title))
    return false;
  node->SetTitle(title);

  std::wstring date_added_string;
  if (!value.GetString(kDateAddedKey, &date_added_string))
    return false;
  node->date_added_ =
      Time::FromInternalValue(StringToInt64(date_added_string));

  std::wstring type_string;
  if (!value.GetString(kTypeKey, &type_string))
    return false;

  if (type_string != kTypeURL && type_string != kTypeFolder)
    return false;  // Unknown type.

  if (type_string == kTypeURL) {
    std::wstring url_string;
    if (!value.GetString(kURLKey, &url_string))
      return false;
    node->SetURL(GURL(url_string));
    node->type_ = history::StarredEntry::URL;
  } else {
    node->type_ = history::StarredEntry::USER_GROUP;

    std::wstring last_modified_date;
    if (!value.GetString(kDateModifiedKey, &last_modified_date))
      return false;
    node->date_group_modified_ =
        Time::FromInternalValue(StringToInt64(last_modified_date));

    Value* child_values;
    if (!value.Get(kChildrenKey, &child_values))
      return false;

    if (child_values->GetType() != Value::TYPE_LIST)
      return false;

    if (!DecodeChildren(model, *static_cast<ListValue*>(child_values), node)) {
      // There was an error in building the children. Delete all the children.
      while (node->GetChildCount())
        delete node->Remove(node->GetChildCount() - 1);
      return false;
    }
  }
  
  return true;
}
