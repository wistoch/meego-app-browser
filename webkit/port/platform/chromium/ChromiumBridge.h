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

#ifndef ChromiumBridge_h
#define ChromiumBridge_h

#include "config.h"

#include "PasteboardPrivate.h"
#include "PassRefPtr.h"
#include "PlatformString.h"

class NativeImageSkia;

typedef struct NPObject NPObject;

#if PLATFORM(WIN_OS)
typedef struct HFONT__* HFONT;
#endif

namespace WebCore {
    class Cursor;
    class Document;
    class Frame;
    class Image;
    class IntRect;
    class KURL;
    class String;
    class Widget;

    struct PluginInfo;

    // An interface to the embedding layer, which has the ability to answer
    // questions about the system and so on...

    class ChromiumBridge {
    public:
        // Clipboard ----------------------------------------------------------
        static bool clipboardIsFormatAvailable(PasteboardPrivate::ClipboardFormat);

        static String clipboardReadPlainText();
        static void clipboardReadHTML(String*, KURL*);

        static void clipboardWriteSelection(const String&, const KURL&, const String&, bool);
        static void clipboardWriteURL(const KURL&, const String&);
        static void clipboardWriteImage(const NativeImageSkia* bitmap, const KURL&, const String&);

        // Cookies ------------------------------------------------------------
        static void setCookies(const KURL& url, const KURL& policyURL, const String& value);
        static String cookies(const KURL& url, const KURL& policyURL);

        // DNS ----------------------------------------------------------------
        static void prefetchDNS(const String& hostname);

        // Font ---------------------------------------------------------------
#if PLATFORM(WIN_OS)
        static bool ensureFontLoaded(HFONT font);
#endif
        
        // Forms --------------------------------------------------------------
        static void notifyFormStateChanged(const Document* doc);

        // JavaScript ---------------------------------------------------------
        static void notifyJSOutOfMemory(Frame* frame);

        // Language -----------------------------------------------------------
        static String computedDefaultLanguage();

        // LayoutTestMode -----------------------------------------------------
        static bool layoutTestMode();

        // MimeType -----------------------------------------------------------
        static bool isSupportedImageMIMEType(const char* mime_type);
        static bool isSupportedJavascriptMIMEType(const char* mime_type);
        static bool isSupportedNonImageMIMEType(const char* mime_type);
        static bool matchesMIMEType(const String& pattern, const String& type);
        static String mimeTypeForExtension(const String& ext);
        static String mimeTypeFromFile(const String& file_path);
        static String preferredExtensionForMIMEType(const String& mime_type);

        // Plugin -------------------------------------------------------------
        static bool plugins(bool refresh, Vector<PluginInfo*>* plugins);
        static NPObject* pluginScriptableObject(Widget* widget);

        // Protocol -----------------------------------------------------------
        static String uiResourceProtocol();

        // Resources ----------------------------------------------------------
        static PassRefPtr<Image> loadPlatformImageResource(const char* name);

        // Screen -------------------------------------------------------------
        static int screenDepth(Widget*);
        static int screenDepthPerComponent(Widget*);
        static bool screenIsMonochrome(Widget*);
        static IntRect screenRect(Widget*);
        static IntRect screenAvailableRect(Widget*);

        // SharedTimers --------------------------------------------------------
        static void setSharedTimerFiredFunction(void (*func)());
        static void setSharedTimerFireTime(double fire_time);
        static void stopSharedTimer();

        // StatsCounters ------------------------------------------------------
        static void decrementStatsCounter(const char* name);
        static void incrementStatsCounter(const char* name);
        static void initV8CounterFunction();

        // SystemTime ----------------------------------------------------------
        static double currentTime();

        // Trace Event --------------------------------------------------------
        static void traceEventBegin(const char* name, void* id, const char* extra);
        static void traceEventEnd(const char* name, void* id, const char* extra);

        // URL ----------------------------------------------------------------
        static KURL inspectorURL();

        // Widget -------------------------------------------------------------
        static void widgetSetCursor(Widget*, const Cursor&);
        static void widgetSetFocus(Widget*);
    };
}

#endif
