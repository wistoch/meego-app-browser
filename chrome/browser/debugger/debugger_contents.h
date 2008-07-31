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
//
// This file defines utility functions for working with strings.

#ifndef CHROME_BROWSER_SHELL_DEBUGGER_CONTENTS_H__
#define CHROME_BROWSER_SHELL_DEBUGGER_CONTENTS_H__

#include "chrome/browser/dom_ui/dom_ui_host.h"

class DebuggerContents : public DOMUIHost {
 public:
  DebuggerContents(Profile* profile, SiteInstance* instance);

  static bool IsDebuggerUrl(const GURL& url);

 protected:
  // WebContents overrides:
  // We override updating history with a no-op so these pages
  // are not saved to history.
  virtual void UpdateHistoryForNavigation(const GURL& url,
      const ViewHostMsg_FrameNavigate_Params& params) { }

  // DOMUIHost implementation.
  virtual void AttachMessageHandlers();

  DISALLOW_EVIL_CONSTRUCTORS(DebuggerContents);
};

#endif  // CHROME_BROWSER_DEBUGGER_CONTENTS_H__
