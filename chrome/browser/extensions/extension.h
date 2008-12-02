// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_H__

#include <string>
#include <vector>

#include "base/string16.h"
#include "base/values.h"

// Represents a Chromium extension.
class Extension {
 public:
  Extension(){};

  // The format for extension manifests that this code understands.
  static const int kExpectedFormatVersion = 1;

  // Keys used in JSON representation of extensions.
  static const std::wstring kFormatVersionKey;
  static const std::wstring kIdKey;
  static const std::wstring kNameKey;
  static const std::wstring kDescriptionKey;
  static const std::wstring kContentScriptsKey;

  // Error messages returned from InitFromValue().
  static const std::wstring kInvalidFormatVersionError;
  static const std::wstring kInvalidIdError;
  static const std::wstring kInvalidNameError;
  static const std::wstring kInvalidDescriptionError;
  static const std::wstring kInvalidContentScriptsListError;
  static const std::wstring kInvalidContentScriptError;

  // A human-readable ID for the extension. The convention is to use something
  // like 'com.example.myextension', but this is not currently enforced. An
  // extension's ID is used in things like directory structures and URLs, and
  // is expected to not change across versions. In the case of conflicts,
  // updates will only be allowed if the extension can be validated using the
  // previous version's update key.
  const std::wstring& id() const { return id_; }

  // A human-readable name of the extension.
  const std::wstring& name() const { return name_; }

  // An optional longer description of the extension.
  const std::wstring& description() const { return description_; }

  // Paths to the content scripts that the extension contains.
  const std::vector<std::wstring>& content_scripts() const {
    return content_scripts_;
  }

  // Initialize the extension from a parsed manifest.
  bool InitFromValue(const DictionaryValue& value, std::wstring* error);

  // Serialize the extension to a DictionaryValue.
  void CopyToValue(DictionaryValue* value);

 private:
  std::wstring id_;
  std::wstring name_;
  std::wstring description_;
  std::vector<std::wstring> content_scripts_;

  DISALLOW_COPY_AND_ASSIGN(Extension);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_H__
