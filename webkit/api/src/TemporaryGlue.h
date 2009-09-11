/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TemporaryGlue_h
#define TemporaryGlue_h

// This is a temporary file declaring some functions that the WebKit layer can
// use to call to the Glue layer.  Once the Glue layer moves entirely into the
// WebKit layer, this file will be deleted.

struct _NPP;
typedef struct _NPP NPP_t;
typedef NPP_t* NPP;

namespace WebCore {
    class Cursor;
    class Frame;
    class IntRect;
    class String;
    class Widget;
    class Worker;
    class WorkerContextProxy;
} // namespace WebCore

namespace WebKit {
    class WebMediaPlayer;
    class WebMediaPlayerClient;
    struct WebCursorInfo;

    class TemporaryGlue {
    public:
        virtual WebMediaPlayer* createWebMediaPlayer(WebMediaPlayerClient*, WebCore::Frame*) = 0;
        virtual void setCursorForPlugin(const WebCursorInfo&, WebCore::Frame*) = 0;
        virtual WebCore::String uiResourceProtocol() = 0;
        virtual void notifyJSOutOfMemory(WebCore::Frame*) = 0;
        virtual int screenDepth(WebCore::Widget*)  = 0;
        virtual int screenDepthPerComponent(WebCore::Widget*)  = 0;
        virtual bool screenIsMonochrome(WebCore::Widget*)  = 0;
        virtual WebCore::IntRect screenRect(WebCore::Widget*)  = 0;
        virtual WebCore::IntRect screenAvailableRect(WebCore::Widget*) = 0;
        virtual bool popupsAllowed(NPP) = 0;
        virtual void widgetSetCursor(WebCore::Widget*, const WebCore::Cursor&) = 0;
        virtual void widgetSetFocus(WebCore::Widget*) = 0;
        virtual WebCore::WorkerContextProxy* createWorkerContextProxy(WebCore::Worker* worker) = 0;
    };

} // namespace WebKit

#endif
