// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a set of user experience metrics data recorded by
// the MetricsService.  This is the unit of data that is sent to the server.

#ifndef CHROME_BROWSER_METRICS_LOG_H__
#define CHROME_BROWSER_METRICS_LOG_H__

#include <libxml/xmlwriter.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/histogram.h"
#include "base/time.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/glue/webplugin.h"

struct AutocompleteLog;
class DictionaryValue;
class GURL;
class PrefService;

class MetricsLog {
 public:
  // Creates a new metrics log
  // client_id is the identifier for this profile on this installation
  // session_id is an integer that's incremented on each application launch
  MetricsLog(const std::string& client_id, int session_id);
  virtual ~MetricsLog();

  static void RegisterPrefs(PrefService* prefs);

  // Records a user-initiated action.
  void RecordUserAction(const wchar_t* key);

  enum WindowEventType {
    WINDOW_CREATE = 0,
    WINDOW_OPEN,
    WINDOW_CLOSE,
    WINDOW_DESTROY
  };

  static const char* WindowEventTypeToString(WindowEventType type);

  void RecordWindowEvent(WindowEventType type, int window_id, int parent_id);

  // Records a page load.
  // window_id - the index of the tab in which the load took place
  // url - which URL was loaded
  // origin - what kind of action initiated the load
  // load_time - how long it took to load the page
  void MetricsLog::RecordLoadEvent(int window_id,
                                   const GURL& url,
                                   PageTransition::Type origin,
                                   int session_index,
                                   TimeDelta load_time);

  // Records the current operating environment.  Takes the list of installed
  // plugins as a parameter because that can't be obtained synchronously
  // from the UI thread.
  // profile_metrics, if non-null, gives a dictionary of all profile metrics
  // that are to be recorded. Each value in profile_metrics should be a
  // dictionary giving the metrics for the profile.
  void RecordEnvironment(const std::vector<WebPluginInfo>& plugin_list,
                         const DictionaryValue* profile_metrics);

  // Records the input text, available choices, and selected entry when the
  // user uses the Omnibox to open a URL.
  void RecordOmniboxOpenedURL(const AutocompleteLog& log);

  void RecordHistogramDelta(const Histogram& histogram,
                            const Histogram::SampleSet& snapshot);

  // Stop writing to this record and generate the encoded representation.
  // None of the Record* methods can be called after this is called.
  void CloseLog();

  // These methods allow retrieval of the encoded representation of the
  // record.  They can only be called after CloseLog() has been called.
  // GetEncodedLog returns false if buffer_size is less than
  // GetEncodedLogSize();
  int GetEncodedLogSize();
  bool GetEncodedLog(char* buffer, int buffer_size);

  // Returns the amount of time in seconds that this log has been in use.
  int GetElapsedSeconds();

  int num_events() { return num_events_; }

  // Creates an MD5 hash of the given value, and returns hash as a byte
  // buffer encoded as a std::string.
  static std::string CreateHash(const std::string& value);

  // Return a base64-encoded MD5 hash of the given string.
  static std::string CreateBase64Hash(const std::string& string);

 protected:
  // Returns a string containing the current time.
  // Virtual so that it can be overridden for testing.
  virtual std::string GetCurrentTimeString();

 private:
  // Helper class that invokes StartElement from constructor, and EndElement
  // from destructor.
  //
  // Use the macro OPEN_ELEMENT_FOR_SCOPE to help avoid usage problems.
  class ScopedElement {
   public:
    ScopedElement(MetricsLog* log, const std::string& name) : log_(log) {
      DCHECK(log);
      log->StartElement(name.c_str());
    }

    ScopedElement(MetricsLog* log, const char* name) : log_(log) {
      DCHECK(log);
      log->StartElement(name);
    }

    ~ScopedElement() {
      log_->EndElement();
    }

   private:
     MetricsLog* log_;
  };
  friend class ScopedElement;

  // Convenience versions of xmlWriter functions
  void StartElement(const char* name);
  void EndElement();
  void WriteAttribute(const std::string& name, const std::string& value);
  void WriteIntAttribute(const std::string& name, int value);
  void WriteInt64Attribute(const std::string& name, int64 value);

  // Write the attributes that are common to every metrics event type.
  void WriteCommonEventAttributes();

  // Get the current version of the application as a string.
  std::string GetVersionString() const;

  // Returns the date at which the current metrics client ID was created as
  // a string containing milliseconds since the epoch, or "0" if none was found.
  std::string GetInstallDate() const;

  // Writes application stability metrics (as part of the profile log).
  // NOTE: Has the side-effect of clearing those counts.
  void WriteStabilityElement();

  // Writes the list of installed plugins.
  void WritePluginList(const std::vector<WebPluginInfo>& plugin_list);

  // Writes all profile metrics. This invokes WriteProfileMetrics for each key
  // in all_profiles_metrics that starts with kProfilePrefix.
  void WriteAllProfilesMetrics(const DictionaryValue& all_profiles_metrics);

  // Writes metrics for the profile identified by key. This writes all
  // key/value pairs in profile_metrics.
  void WriteProfileMetrics(const std::wstring& key,
                           const DictionaryValue& profile_metrics);

  Time start_time_;
  Time end_time_;

  std::string client_id_;
  std::string session_id_;

  // locked_ is true when record has been packed up for sending, and should
  // no longer be written to.  It is only used for sanity checking and is
  // not a real lock.
  bool locked_;

  xmlBufferPtr buffer_;
  xmlTextWriterPtr writer_;
  int num_events_;  // the number of events recorded in this log

  DISALLOW_EVIL_CONSTRUCTORS(MetricsLog);
};

#endif  // CHROME_BROWSER_METRICS_LOG_H__

