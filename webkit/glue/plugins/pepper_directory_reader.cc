// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_directory_reader.h"

#include "base/logging.h"
#include "third_party/ppapi/c/pp_completion_callback.h"
#include "third_party/ppapi/c/pp_errors.h"
#include "webkit/glue/plugins/pepper_file_ref.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"

namespace pepper {

namespace {

PP_Resource Create(PP_Resource directory_ref_id) {
  scoped_refptr<FileRef> directory_ref(
      Resource::GetAs<FileRef>(directory_ref_id));
  if (!directory_ref.get())
    return 0;

  DirectoryReader* reader = new DirectoryReader(directory_ref);
  reader->AddRef();  // AddRef for the caller;
  return reader->GetResource();
}

bool IsDirectoryReader(PP_Resource resource) {
  return !!Resource::GetAs<DirectoryReader>(resource).get();
}

int32_t GetNextEntry(PP_Resource reader_id,
                     PP_DirectoryEntry* entry,
                     PP_CompletionCallback callback) {
  scoped_refptr<DirectoryReader> reader(
      Resource::GetAs<DirectoryReader>(reader_id));
  if (!reader.get())
    return PP_Error_BadResource;

  return reader->GetNextEntry(entry, callback);
}

const PPB_DirectoryReader ppb_directoryreader = {
  &Create,
  &IsDirectoryReader,
  &GetNextEntry
};

}  // namespace

DirectoryReader::DirectoryReader(FileRef* directory_ref)
    : Resource(directory_ref->module()),
      directory_ref_(directory_ref) {
}

DirectoryReader::~DirectoryReader() {
}

const PPB_DirectoryReader* DirectoryReader::GetInterface() {
  return &ppb_directoryreader;
}

int32_t DirectoryReader::GetNextEntry(PP_DirectoryEntry* entry,
                                      PP_CompletionCallback callback) {
  NOTIMPLEMENTED();  // TODO(darin): Implement me!
  return PP_Error_Failed;
}

}  // namespace pepper
