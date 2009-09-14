/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_BASE_EVENT_H__
#define TALK_BASE_EVENT_H__

namespace talk_base {

#ifdef WIN32
class Event {
public:
  Event() {
    event_ = CreateEvent(NULL, FALSE, FALSE, NULL);
  }
  ~Event() {
    CloseHandle(event_);
  }
  void Set() {
    SetEvent(event_);
  }
  void Reset() {
    ResetEvent(event_);
  }
  void Wait() {
    WaitForSingleObject(event_, INFINITE);
  }
private:
  HANDLE event_;
};
#endif

#ifdef POSIX
#include <cassert>
class Event {
  Event() {
    assert(false);
  }
  ~Event() {
    assert(false);
  }
  void Set() {
    assert(false);
  }
  void Reset() {
    assert(false);
  }
  void Wait() {
    assert(false);
  }
};
#endif

} // namespace talk_base

#endif // TALK_BASE_EVENT_H__
