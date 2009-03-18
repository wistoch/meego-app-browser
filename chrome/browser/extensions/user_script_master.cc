// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_master.h"

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/stl_util-inl.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_util.h"

// static
bool GetDeclarationValue(const StringPiece& line, const StringPiece& prefix,
                         std::string* value) {
  if (!line.starts_with(prefix))
    return false;

  std::string temp(line.data() + prefix.length(),
                   line.length() - prefix.length());
  TrimWhitespace(temp, TRIM_ALL, value);
  return true;
}

// static
bool UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      const StringPiece& script_text, UserScript* script) {
  // http://wiki.greasespot.net/Metadata_block
  StringPiece line;
  size_t line_start = 0;
  size_t line_end = 0;
  bool in_metadata = false;

  static const StringPiece kUserScriptBegin("// ==UserScript==");
  static const StringPiece kUserScriptEng("// ==/UserScript==");
  static const StringPiece kIncludeDeclaration("// @include ");
  static const StringPiece kMatchDeclaration("// @match ");
  static const StringPiece kRunAtDeclaration("// @run-at ");
  static const StringPiece kRunAtDocumentStartValue("document-start");
  static const StringPiece kRunAtDocumentEndValue("document-end");

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

      std::string value;
      if (GetDeclarationValue(line, kIncludeDeclaration, &value)) {
        // We escape some characters that MatchPattern() considers special.
        ReplaceSubstringsAfterOffset(&value, 0, "\\", "\\\\");
        ReplaceSubstringsAfterOffset(&value, 0, "?", "\\?");
        script->add_glob(value);
      } else if (GetDeclarationValue(line, kMatchDeclaration, &value)) {
        URLPattern pattern;
        if (!pattern.Parse(value))
          return false;
        script->add_url_pattern(pattern);
      } else if (GetDeclarationValue(line, kRunAtDeclaration, &value)) {
        if (value == kRunAtDocumentStartValue)
          script->set_run_location(UserScript::DOCUMENT_START);
        else if (value != kRunAtDocumentEndValue)
          return false;
      }

      // TODO(aa): Handle more types of metadata.
    }

    line_start = line_end + 1;
  }

  // It is probably a mistake to declare both @include and @match rules.
  if (script->globs().size() > 0 && script->url_patterns().size() > 0)
    return false;

  // If no patterns were specified, default to @include *. This is what
  // Greasemonkey does.
  if (script->globs().size() == 0 && script->url_patterns().size() == 0)
    script->add_glob("*");

  return true;
}

void UserScriptMaster::ScriptReloader::StartScan(
    MessageLoop* work_loop, const FilePath& script_dir,
    const UserScriptList& lone_scripts) {
  // Add a reference to ourselves to keep ourselves alive while we're running.
  // Balanced by NotifyMaster().
  AddRef();
  work_loop->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &UserScriptMaster::ScriptReloader::RunScan,
                        script_dir, lone_scripts));
}

void UserScriptMaster::ScriptReloader::NotifyMaster(
    base::SharedMemory* memory) {
  if (!master_) {
    // The master went away, so these new scripts aren't useful anymore.
    delete memory;
  } else {
    master_->NewScriptsAvailable(memory);
  }

  // Drop our self-reference.
  // Balances StartScan().
  Release();
}

void UserScriptMaster::ScriptReloader::RunScan(
    const FilePath script_dir, const UserScriptList lone_scripts) {
  base::SharedMemory* shared_memory = GetNewScripts(script_dir, lone_scripts);

  // Post the new scripts back to the master's message loop.
  master_message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &UserScriptMaster::ScriptReloader::NotifyMaster,
                        shared_memory));
}

base::SharedMemory* UserScriptMaster::ScriptReloader::GetNewScripts(
    const FilePath& script_dir, const UserScriptList& lone_scripts) {
  UserScriptList all_scripts;

  // Find all the scripts in |script_dir|.
  if (!script_dir.value().empty()) {
    // Create the "<Profile>/User Scripts" directory if it doesn't exist
    if (!file_util::DirectoryExists(script_dir))
      file_util::CreateDirectory(script_dir);

    file_util::FileEnumerator enumerator(script_dir, false,
                                         file_util::FileEnumerator::FILES,
                                         FILE_PATH_LITERAL("*.user.js"));
    for (FilePath file = enumerator.Next(); !file.value().empty();
         file = enumerator.Next()) {
      all_scripts.push_back(UserScript());
      UserScript& info = all_scripts.back();
      info.set_url(GURL(std::string(chrome::kUserScriptScheme) + ":/" +
          net::FilePathToFileURL(file.ToWStringHack()).ExtractFileName()));
      info.set_path(file);
    }
  }

  if (all_scripts.empty() && lone_scripts.empty())
    return NULL;

  // Add all the lone scripts.
  all_scripts.insert(all_scripts.end(), lone_scripts.begin(),
                     lone_scripts.end());

  // Load and pickle each script. Look for a metadata header if there are no
  // url_patterns specified already.
  Pickle pickle;
  pickle.WriteSize(all_scripts.size());
  for (UserScriptList::iterator iter = all_scripts.begin();
       iter != all_scripts.end(); ++iter) {
    // TODO(aa): Support unicode script files.
    std::string contents;
    file_util::ReadFileToString(iter->path().ToWStringHack(), &contents);

    if (iter->url_patterns().empty()) {
      // TODO(aa): Handle errors parsing header.
      if (!ParseMetadataHeader(contents, &(*iter)))
        return NULL;
    }

    iter->Pickle(&pickle);

    // Write scripts as 'data' so that we can read it out in the slave without
    // allocating a new string.
    pickle.WriteData(contents.c_str(), contents.length());
  }

  // Create the shared memory object.
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());

  if (!shared_memory->Create(std::wstring(),  // anonymous
                             false,  // read-only
                             false,  // open existing
                             pickle.size())) {
    return NULL;
  }

  // Map into our process.
  if (!shared_memory->Map(pickle.size()))
    return NULL;

  // Copy the pickle to shared memory.
  memcpy(shared_memory->memory(), pickle.data(), pickle.size());

  return shared_memory.release();
}

UserScriptMaster::UserScriptMaster(MessageLoop* worker_loop,
                                   const FilePath& script_dir)
    : user_script_dir_(script_dir),
      worker_loop_(worker_loop),
      pending_scan_(false) {
  if (!user_script_dir_.value().empty())
    AddWatchedPath(script_dir);
}

UserScriptMaster::~UserScriptMaster() {
  if (script_reloader_)
    script_reloader_->DisownMaster();

// TODO(aa): Enable this when DirectoryWatcher is implemented for linux and mac.
#if defined(OS_WIN)
  STLDeleteElements(&dir_watchers_);
#endif
}

void UserScriptMaster::AddWatchedPath(const FilePath& path) {
// TODO(aa): Enable this when DirectoryWatcher is implemented for linux and mac.
#if defined(OS_WIN)
  DirectoryWatcher* watcher = new DirectoryWatcher();
  watcher->Watch(path, this, true);
  dir_watchers_.push_back(watcher);
#endif
}

void UserScriptMaster::NewScriptsAvailable(base::SharedMemory* handle) {
  // Ensure handle is deleted or released.
  scoped_ptr<base::SharedMemory> handle_deleter(handle);

  if (pending_scan_) {
    // While we were scanning, there were further changes.  Don't bother
    // notifying about these scripts and instead just immediately rescan.
    pending_scan_ = false;
    StartScan();
  } else {
    // We're no longer scanning.
    script_reloader_ = NULL;
    // We've got scripts ready to go.
    shared_memory_.swap(handle_deleter);

    NotificationService::current()->Notify(
        NotificationType::USER_SCRIPTS_LOADED,
        NotificationService::AllSources(),
        Details<base::SharedMemory>(handle));
  }
}

void UserScriptMaster::OnDirectoryChanged(const FilePath& path) {
  if (script_reloader_.get()) {
    // We're already scanning for scripts.  We note that we should rescan when
    // we get the chance.
    pending_scan_ = true;
    return;
  }

  StartScan();
}

void UserScriptMaster::StartScan() {
  if (!script_reloader_)
    script_reloader_ = new ScriptReloader(this);

  script_reloader_->StartScan(worker_loop_, user_script_dir_, lone_scripts_);
}
