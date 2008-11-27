// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/greasemonkey_slave.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "base/shared_memory.h"
#include "googleurl/src/gurl.h"


// GreasemonkeyScript

void GreasemonkeyScript::Parse(const StringPiece& script_text) {
  ParseMetadata(script_text);

  // TODO(aa): Set body to just the part after the metadata block? This would
  // significantly cut down on the size of the injected script in some cases.
  // Would require remembering the line number the body begins at, for correct
  // error line number reporting.
  body_ = script_text;
}

bool GreasemonkeyScript::MatchesUrl(const GURL& url) {
  for (std::vector<std::string>::iterator pattern = include_patterns_.begin();
       pattern != include_patterns_.end(); ++pattern) {
    if (MatchPattern(url.spec(), *pattern)) {
      return true;
    }
  }

  return false;
}

void GreasemonkeyScript::ParseMetadata(const StringPiece& script_text) {
  // http://wiki.greasespot.net/Metadata_block
  StringPiece line;
  size_t line_start = 0;
  size_t line_end = 0;
  bool in_metadata = false;

  static const StringPiece kUserScriptBegin("// ==UserScript==");
  static const StringPiece kUserScriptEng("// ==/UserScript==");
  static const StringPiece kIncludeDeclaration("// @include ");

  while (line_start < script_text.length()) {
    line_end = script_text.find('\n', line_start);

    // Handle the case where there is no trailing newline in the file.
    if (line_end == std::string::npos) {
      line_end = script_text.length() - 1;
    }

    line.set(script_text.data() + line_start, line_end - line_start);

    if (!in_metadata) {
      if (line.starts_with(kUserScriptBegin)) {
        in_metadata = true;
      }
    } else {
      if (line.starts_with(kUserScriptEng)) {
        break;
      }

      if (line.starts_with(kIncludeDeclaration)) {
        std::string pattern(line.data() + kIncludeDeclaration.length(),
                            line.length() - kIncludeDeclaration.length());
        std::string pattern_trimmed;
        TrimWhitespace(pattern, TRIM_ALL, &pattern_trimmed);
        AddInclude(pattern_trimmed);
      }

      // TODO(aa): Handle more types of metadata.
    }

    line_start = line_end + 1;
  }

  // If no @include patterns were specified, default to @include *.
  // This is what Greasemonkey for Firefox does.
  if (include_patterns_.size() == 0) {
    AddInclude("*");
  }
}

void GreasemonkeyScript::AddInclude(const std::string &glob_pattern) {
  include_patterns_.push_back(EscapeGlob(glob_pattern));
}

std::string GreasemonkeyScript::EscapeGlob(const std::string& input_pattern) {
  std::string output_pattern;

  for (size_t i = 0; i < input_pattern.length(); ++i) {
    switch (input_pattern[i]) {
      // These characters have special meaning to the MatchPattern() function,
      // so we escape them.
      case '\\':
      case '?':
        output_pattern += '\\';
        // fall through

      default:
        output_pattern += input_pattern[i];
    }
  }

  return output_pattern;
}


// GreasemonkeySlave
GreasemonkeySlave::GreasemonkeySlave() : shared_memory_(NULL) {
}

bool GreasemonkeySlave::UpdateScripts(base::SharedMemoryHandle shared_memory) {
  scripts_.clear();

  // Create the shared memory object (read only).
  shared_memory_.reset(new base::SharedMemory(shared_memory, true));
  if (!shared_memory_.get())
    return false;

  // First get the size of the memory block.
  if (!shared_memory_->Map(sizeof(Pickle::Header)))
    return false;
  Pickle::Header* pickle_header =
      reinterpret_cast<Pickle::Header*>(shared_memory_->memory());

  // Now map in the rest of the block.
  int pickle_size = sizeof(Pickle::Header) + pickle_header->payload_size;
  shared_memory_->Unmap();
  if (!shared_memory_->Map(pickle_size))
    return false;

  // Unpickle scripts.
  void* iter = NULL;
  int num_scripts = 0;
  Pickle pickle(reinterpret_cast<char*>(shared_memory_->memory()),
                pickle_size);
  pickle.ReadInt(&iter, &num_scripts);

  for (int i = 0; i < num_scripts; ++i) {
    const char* url = NULL;
    int url_length = 0;
    const char* body = NULL;
    int body_length = 0;

    pickle.ReadData(&iter, &url, &url_length);
    pickle.ReadData(&iter, &body, &body_length);

    scripts_.push_back(GreasemonkeyScript(StringPiece(url, url_length)));
    GreasemonkeyScript& script = scripts_.back();
    script.Parse(StringPiece(body, body_length));
  }

  return true;
}

bool GreasemonkeySlave::InjectScripts(WebFrame* frame) {
  for (std::vector<GreasemonkeyScript>::iterator script = scripts_.begin();
       script != scripts_.end(); ++script) {
    if (script->MatchesUrl(frame->GetURL())) {
      frame->ExecuteJavaScript(script->GetBody().as_string(),
                               GURL(script->GetURL().as_string()));
    }
  }

  return true;
}
