// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/stats_table.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCache.h"
#include "webkit/extensions/v8/benchmarking_extension.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebCache;

namespace extensions_v8 {

const char* kBenchmarkingExtensionName = "v8/Benchmarking";

class BenchmarkingWrapper : public v8::Extension {
 public:
  BenchmarkingWrapper() :
      v8::Extension(kBenchmarkingExtensionName,
        "if (typeof(chrome) == 'undefined') {"
        "  chrome = {};"
        "};"
        "if (typeof(chrome.benchmarking) == 'undefined') {"
        "  chrome.benchmarking = {};"
        "};"
        "chrome.benchmarking.clearCache = function() {"
        "  native function ClearCache();"
        "  ClearCache();"
        "};"
        "chrome.benchmarking.closeConnections = function() {"
        "  native function CloseConnections();"
        "  CloseConnections();"
        "};"
        "chrome.benchmarking.counter = function(name) {"
        "  native function GetCounter();"
        "  return GetCounter(name);"
        "};"
        "chrome.benchmarking.isSingleProcess = function() {"
        "  native function IsSingleProcess();"
        "  return IsSingleProcess();"
        "};"
        ) {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("CloseConnections"))) {
      return v8::FunctionTemplate::New(CloseConnections);
    } else if (name->Equals(v8::String::New("ClearCache"))) {
      return v8::FunctionTemplate::New(ClearCache);
    } else if (name->Equals(v8::String::New("GetCounter"))) {
      return v8::FunctionTemplate::New(GetCounter);
    } else if (name->Equals(v8::String::New("IsSingleProcess"))) {
      return v8::FunctionTemplate::New(IsSingleProcess);
    }
    return v8::Handle<v8::FunctionTemplate>();
  }

  static v8::Handle<v8::Value> CloseConnections(const v8::Arguments& args) {
    webkit_glue::CloseCurrentConnections();
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> ClearCache(const v8::Arguments& args) {
    webkit_glue::ClearCache();
    WebCache::clear();
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> GetCounter(const v8::Arguments& args) {
    if (!args.Length() || !args[0]->IsString() || !StatsTable::current())
      return v8::Undefined();

    // Extract the name argument
    char name[256];
    name[0] = 'c';
    name[1] = ':';
    args[0]->ToString()->WriteAscii(&name[2], 0, sizeof(name) - 3);

    int counter = StatsTable::current()->GetCounterValue(name);
    return v8::Integer::New(counter);
  }

  static v8::Handle<v8::Value> IsSingleProcess(const v8::Arguments& args) {
    return v8::Boolean::New(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess));
  }
};

v8::Extension* BenchmarkingExtension::Get() {
  return new BenchmarkingWrapper();
}

}  // namespace extensions_v8
