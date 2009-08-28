// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_temp_dir.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"

namespace {

const char* GetDesktopName() {
#if defined(GOOGLE_CHROME_BUILD)
  return "google-chrome.desktop";
#else  // CHROMIUM_BUILD
  static const char* name = NULL;
  if (!name) {
    // Allow $CHROME_DESKTOP to override the built-in value, so that development
    // versions can set themselves as the default without interfering with
    // non-official, packaged versions using the built-in value.
    name = getenv("CHROME_DESKTOP");
    if (!name)
      name = "chromium-browser.desktop";
  }
  return name;
#endif
}

// Helper to launch xdg scripts. We don't want them to ask any questions on the
// terminal etc.
bool LaunchXdgUtility(const std::vector<std::string>& argv) {
  // xdg-settings internally runs xdg-mime, which uses mv to move newly-created
  // files on top of originals after making changes to them. In the event that
  // the original files are owned by another user (e.g. root, which can happen
  // if they are updated within sudo), mv will prompt the user to confirm if
  // standard input is a terminal (otherwise it just does it). So make sure it's
  // not, to avoid locking everything up waiting for mv.
  int devnull = open("/dev/null", O_RDONLY);
  if (devnull < 0)
    return false;
  base::file_handle_mapping_vector no_stdin;
  no_stdin.push_back(std::make_pair(devnull, STDIN_FILENO));

  base::ProcessHandle handle;
  if (!base::LaunchApp(argv, no_stdin, false, &handle)) {
    close(devnull);
    return false;
  }
  close(devnull);

  int success_code;
  base::WaitForExitCode(handle, &success_code);
  return success_code == EXIT_SUCCESS;
}

bool GetDesktopShortcutTemplate(std::string* output) {
  std::vector<std::string> search_paths;

  const char* xdg_data_home = getenv("XDG_DATA_HOME");
  if (xdg_data_home)
    search_paths.push_back(xdg_data_home);

  const char* xdg_data_dirs = getenv("XDG_DATA_DIRS");
  if (xdg_data_dirs) {
    CStringTokenizer tokenizer(xdg_data_dirs,
                               xdg_data_dirs + strlen(xdg_data_dirs), ":");
    while (tokenizer.GetNext()) {
      search_paths.push_back(tokenizer.token());
    }
  }

  std::string template_filename(GetDesktopName());
  for (std::vector<std::string>::const_iterator i = search_paths.begin();
       i != search_paths.end(); ++i) {
    FilePath path = FilePath(*i).Append(template_filename);
    if (file_util::PathExists(path))
      return file_util::ReadFileToString(path, output);
  }

  return false;
}

class CreateDesktopShortcutTask : public Task {
 public:
  CreateDesktopShortcutTask(const ShellIntegration::ShortcutInfo& shortcut_info)
      : shortcut_info_(shortcut_info) {
  }

  virtual void Run() {
    // TODO(phajdan.jr): Report errors from this function, possibly as infobars.
    std::string template_contents;
    if (!GetDesktopShortcutTemplate(&template_contents))
      return;

    std::string contents = ShellIntegration::GetDesktopFileContents(
        template_contents, shortcut_info_.url, shortcut_info_.title);

    ScopedTempDir temp_dir;
    if (!temp_dir.CreateUniqueTempDir())
      return;

    FilePath shortcut_filename =
        ShellIntegration::GetDesktopShortcutFilename(shortcut_info_.url);

    FilePath temp_file_path = temp_dir.path().Append(shortcut_filename);

    int bytes_written = file_util::WriteFile(temp_file_path, contents.data(),
                                             contents.length());

    if (bytes_written != static_cast<int>(contents.length()))
        return;

    if (shortcut_info_.create_on_desktop) {
      FilePath desktop_path;
      if (!PathService::Get(chrome::DIR_USER_DESKTOP, &desktop_path))
        return;
      desktop_path = desktop_path.Append(shortcut_filename);

      if (!file_util::PathExists(desktop_path))
        file_util::CopyFile(temp_file_path, desktop_path);
    }

    if (shortcut_info_.create_in_applications_menu) {
      std::vector<std::string> argv;
      argv.push_back("xdg-desktop-menu");
      argv.push_back("install");

      // Always install in user mode, even if someone runs the browser as root
      // (people do that).
      argv.push_back("--mode");
      argv.push_back("user");

      argv.push_back(temp_file_path.value());
      LaunchXdgUtility(argv);
    }
  }

 private:
  const ShellIntegration::ShortcutInfo shortcut_info_;

  DISALLOW_COPY_AND_ASSIGN(CreateDesktopShortcutTask);
};

}  // namespace

// We delegate the difficult of setting the default browser in Linux desktop
// environments to a new xdg utility, xdg-settings. We'll have to include a copy
// of it for this to work, obviously, but that's actually the suggested approach
// for xdg utilities anyway.

bool ShellIntegration::SetAsDefaultBrowser() {
  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("set");
  argv.push_back("default-web-browser");
  argv.push_back(GetDesktopName());
  return LaunchXdgUtility(argv);
}

bool ShellIntegration::IsDefaultBrowser() {
  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("check");
  argv.push_back("default-web-browser");
  argv.push_back(GetDesktopName());

  std::string reply;
  if (!base::GetAppOutput(CommandLine(argv), &reply)) {
    // If xdg-settings fails, we assume that we should pretend we're the default
    // browser to avoid giving repeated prompts to set ourselves as the default.
    // TODO(mdm): Really, being the default browser should be a ternary query:
    // yes, no, and "don't know" so the UI can reflect this more accurately.
    return true;
  }

  // Allow any reply that starts with "yes".
  return reply.find("yes") == 0;
}

bool ShellIntegration::IsFirefoxDefaultBrowser() {
  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("get");
  argv.push_back("default-web-browser");

  std::string browser;
  // We don't care about the return value here.
  base::GetAppOutput(CommandLine(argv), &browser);
  return browser.find("irefox") != std::string::npos;
}

FilePath ShellIntegration::GetDesktopShortcutFilename(const GURL& url) {
  // Use a prefix, because xdg-desktop-menu requires it.
  std::wstring filename = std::wstring(chrome::kBrowserProcessExecutableName) +
      L"-" + UTF8ToWide(url.spec()) + L".desktop";
  file_util::ReplaceIllegalCharacters(&filename, '_');

  // Return BaseName to be absolutely sure we're not vulnerable to a directory
  // traversal attack.
  return FilePath::FromWStringHack(filename).BaseName();
}

std::string ShellIntegration::GetDesktopFileContents(
    const std::string& template_contents, const GURL& url,
    const string16& title) {
  // See http://standards.freedesktop.org/desktop-entry-spec/latest/
  std::string output_buffer;
  StringTokenizer tokenizer(template_contents, "\n");
  while (tokenizer.GetNext()) {
    // TODO(phajdan.jr): Add the icon.

    if (tokenizer.token().substr(0, 5) == "Exec=") {
      std::string exec_path = tokenizer.token().substr(5);
      StringTokenizer exec_tokenizer(exec_path, " ");
      std::string final_path;
      while (exec_tokenizer.GetNext()) {
        if (exec_tokenizer.token() != "%U")
          final_path += exec_tokenizer.token() + " ";
      }
      std::wstring app_switch_wide(switches::kApp);
      std::string app_switch(StringPrintf("\"--%s=%s\"",
                                          WideToUTF8(app_switch_wide).c_str(),
                                          url.spec().c_str()));
      ReplaceSubstringsAfterOffset(&app_switch, 0, "%", "%%");
      output_buffer += std::string("Exec=") + final_path + app_switch + "\n";
    } else if (tokenizer.token().substr(0, 5) == "Name=") {
      std::string final_title = UTF16ToUTF8(title);
      // Make sure no endline characters can slip in and possibly introduce
      // additional lines (like Exec, which makes it a security risk). Also
      // use the URL as a default when the title is empty.
      if (final_title.empty() ||
          final_title.find("\n") != std::string::npos ||
          final_title.find("\r") != std::string::npos)
        final_title = url.spec();
      output_buffer += StringPrintf("Name=%s\n", final_title.c_str());
    } else if (tokenizer.token().substr(0, 8) == "Comment=") {
      // Skip the line.
    } else {
      output_buffer += tokenizer.token() + "\n";
    }
  }
  return output_buffer;
}

void ShellIntegration::CreateDesktopShortcut(
    const ShortcutInfo& shortcut_info) {
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      new CreateDesktopShortcutTask(shortcut_info));
}
