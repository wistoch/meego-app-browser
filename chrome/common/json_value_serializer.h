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

#ifndef CHROME_COMMON_JSON_VALUE_SERIALIZER_H__
#define CHROME_COMMON_JSON_VALUE_SERIALIZER_H__

#include <string>

#include "base/basictypes.h"
#include "base/values.h"

class JSONStringValueSerializer : public ValueSerializer {
 public:
  // json_string is the string that will be source of the deserialization
  // or the destination of the serialization.  The caller of the constructor
  // retains ownership of the string.
  JSONStringValueSerializer(std::string* json_string)
      : json_string_(json_string),
        initialized_with_const_string_(false),
        pretty_print_(false),
        allow_trailing_comma_(false) {
  }

  // This version allows initialization with a const string reference for
  // deserialization only.
  JSONStringValueSerializer(const std::string& json_string)
      : json_string_(&const_cast<std::string&>(json_string)),
        initialized_with_const_string_(true),
        allow_trailing_comma_(false) {
  }

  ~JSONStringValueSerializer();

  // Attempt to serialize the data structure represented by Value into
  // JSON.  If the return value is true, the result will have been written
  // into the string passed into the constructor.
  bool Serialize(const Value& root);

  // Attempt to deserialize the data structure encoded in the string passed
  // in to the constructor into a structure of Value objects.  If the return
  // value is true, the |root| parameter will be set to point to a new Value
  // object that corresponds to the values represented in the string.  The
  // caller takes ownership of the returned Value objects.
  bool Deserialize(Value** root);

  void set_pretty_print(bool new_value) { pretty_print_ = new_value; }
  bool pretty_print() { return pretty_print_; }

  bool set_allow_trailing_comma(bool new_value) {
    allow_trailing_comma_ = new_value;
  }

 private:
  std::string* json_string_;
  bool initialized_with_const_string_;
  bool pretty_print_;  // If true, serialization will span multiple lines.
  // If true, deserialization will allow trailing commas.
  bool allow_trailing_comma_;  

  DISALLOW_EVIL_CONSTRUCTORS(JSONStringValueSerializer);
};

class JSONFileValueSerializer : public ValueSerializer {
 public:
  // json_file_patch is the path of a file that will be source of the
  // deserialization or the destination of the serialization.
  // When deserializing, the file should exist, but when serializing, the
  // serializer will attempt to create the file at the specified location.
  JSONFileValueSerializer(const std::wstring& json_file_path)
    : json_file_path_(json_file_path) {}

  ~JSONFileValueSerializer() {}

  // DO NOT USE except in unit tests to verify the file was written properly.
  // We should never serialize directly to a file since this will block the
  // thread. Instead, serialize to a string and write to the file you want on
  // the file thread.
  //
  // Attempt to serialize the data structure represented by Value into
  // JSON.  If the return value is true, the result will have been written
  // into the file whose name was passed into the constructor.
  bool Serialize(const Value& root);

  // Attempt to deserialize the data structure encoded in the file passed
  // in to the constructor into a structure of Value objects.  If the return
  // value is true, the |root| parameter will be set to point to a new Value
  // object that corresponds to the values represented in the file.  The
  // caller takes ownership of the returned Value objects.
  bool Deserialize(Value** root);

 private:
  std::wstring json_file_path_;

  DISALLOW_EVIL_CONSTRUCTORS(JSONFileValueSerializer);
};

#endif  // CHROME_COMMON_JSON_VALUE_SERIALIZER_H__
