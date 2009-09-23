// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_OS_EXCHANGE_DATA_PROVIDER_GTK_H_
#define APP_OS_EXCHANGE_DATA_PROVIDER_GTK_H_

#include <gtk/gtk.h>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "app/os_exchange_data.h"
#include "base/pickle.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"

// OSExchangeData::Provider implementation for Gtk. OSExchangeDataProviderGtk
// is created with a set of known data types. In addition specific data
// types can be set on OSExchangeDataProviderGtk by way of the various setters.
// The various has methods return true if the format was supplied to the
// constructor, or explicitly set.
class OSExchangeDataProviderGtk : public OSExchangeData::Provider {
 public:
  OSExchangeDataProviderGtk(int known_formats,
                            const std::set<GdkAtom>& known_custom_formats_);
  OSExchangeDataProviderGtk();

  virtual ~OSExchangeDataProviderGtk();

  int known_formats() const { return known_formats_; }
  const std::set<GdkAtom>& known_custom_formats() const {
    return known_custom_formats_;
  }

  // Returns true if all the formats and custom formats identified by |formats|
  // and |custom_formats| have been set in this provider.
  //
  // NOTE: this is NOT the same as whether a format may be provided (as is
  // returned by the various HasXXX methods), but rather if the data for the
  // formats has been set on this provider by way of the various Setter
  // methods.
  bool HasDataForAllFormats(int formats,
                            const std::set<GdkAtom>& custom_formats) const;

  // Returns the set of formats available as a GtkTargetList. It is up to the
  // caller to free (gtk_target_list_unref) the returned list.
  GtkTargetList* GetTargetList() const;

  // Writes the data to |selection|. |format| is any combination of
  // OSExchangeData::Formats.
  void WriteFormatToSelection(int format,
                              GtkSelectionData* selection) const;

  // Provider methods.
  virtual void SetString(const std::wstring& data);
  virtual void SetURL(const GURL& url, const std::wstring& title);
  virtual void SetFilename(const std::wstring& full_path);
  virtual void SetPickledData(OSExchangeData::CustomFormat format,
                              const Pickle& data);
  virtual bool GetString(std::wstring* data) const;
  virtual bool GetURLAndTitle(GURL* url, std::wstring* title) const;
  virtual bool GetFilename(std::wstring* full_path) const;
  virtual bool GetPickledData(OSExchangeData::CustomFormat format,
                              Pickle* data) const;
  virtual bool HasString() const;
  virtual bool HasURL() const;
  virtual bool HasFile() const;
  virtual bool HasCustomFormat(OSExchangeData::CustomFormat format) const;

 private:
  typedef std::map<OSExchangeData::CustomFormat, Pickle>  PickleData;

  // Returns true if |formats_| contains a string format and the string can be
  // parsed as a URL.
  bool GetPlainTextURL(GURL* url) const;

  // These are the possible formats the OSExchangeData may contain. Don't
  // confuse this with the actual formats that have been set, which are
  // |formats_| and |custom_formats_|.
  const int known_formats_;
  const std::set<GdkAtom> known_custom_formats_;

  // Actual formats that have been set. See comment above |known_formats_|
  // for details.
  int formats_;

  // String contents.
  string16 string_;

  // URL contents.
  GURL url_;
  string16 title_;

  // File name.
  std::string filename_;

  // PICKLED_DATA contents.
  PickleData pickle_data_;

  DISALLOW_COPY_AND_ASSIGN(OSExchangeDataProviderGtk);
};

#endif  // APP_OS_EXCHANGE_DATA_PROVIDER_GTK_H_
