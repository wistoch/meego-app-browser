// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

#include "config.h"
#include "ScriptCallContext.h"
#include "ScriptValue.h"

#include "PlatformString.h"
#include "KURL.h"
#include "v8.h"
#include "v8_binding.h"
#include "v8_proxy.h"

namespace WebCore {

ScriptCallContext::ScriptCallContext(const v8::Arguments& args)
    : m_args(args)
{
  // Line numbers in V8 are starting from zero.
  m_lineNumber = V8Proxy::GetSourceLineNumber() + 1;
  m_sourceURL = KURL(V8Proxy::GetSourceName());
}

ScriptValue ScriptCallContext::argumentAt(unsigned index)
{
    if (index >= argumentCount())
        return ScriptValue(v8::Handle<v8::Value>());

    return ScriptValue(m_args[index]);
}

String ScriptCallContext::argumentStringAt(unsigned index,
                                           bool checkForNullOrUndefined)
{
    if (index >= argumentCount())
        return String();

    return ToWebCoreString(m_args[index]);
}

unsigned ScriptCallContext::argumentCount() const
{
    return m_args.Length();
}

unsigned ScriptCallContext::lineNumber() const
{
    return m_lineNumber;
}

KURL ScriptCallContext::sourceURL() const
{
    return m_sourceURL;
}

}  // namespace WebCore
