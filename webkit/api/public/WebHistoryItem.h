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

#ifndef WebHistoryItem_h
#define WebHistoryItem_h

#include "WebCommon.h"

namespace WebKit {
    class WebHistoryItemPrivate;
    class WebString;
    class WebURL;
    struct WebPoint;
    template <typename T> class WebVector;

    // Represents a frame-level navigation entry in session history.  A
    // WebHistoryItem is a node in a tree.
    //
    // Copying a WebHistoryItem is cheap.
    //
    class WebHistoryItem {
    public:
        ~WebHistoryItem() { reset(); }

        WebHistoryItem() : m_private(0) { }
        WebHistoryItem(const WebHistoryItem& h) : m_private(0) { assign(h); }
        WebHistoryItem& operator=(const WebHistoryItem& h) { assign(h); return *this; }

        WEBKIT_API void initialize();
        WEBKIT_API void reset();
        WEBKIT_API void assign(const WebHistoryItem&);

        bool isNull() const { return m_private == 0; }

        WEBKIT_API WebURL url() const;
        WEBKIT_API void setURL(const WebURL&);

        WEBKIT_API WebURL originalURL() const;
        WEBKIT_API void setOriginalURL(const WebURL&);

        WEBKIT_API WebURL referrerURL() const;
        WEBKIT_API void setReferrerURL(const WebURL&);

        WEBKIT_API WebString target() const;
        WEBKIT_API void setTarget(const WebString&) const;

        WEBKIT_API WebString parent() const;
        WEBKIT_API void setParent(const WebString&);

        WEBKIT_API WebString title() const;
        WEBKIT_API void setTitle(const WebString&);

        WEBKIT_API WebString alternateTitle() const;
        WEBKIT_API void setAlternateTitle(const WebString&);

        WEBKIT_API double lastVisitedTime() const;
        WEBKIT_API void setLastVisitedTime(double);

        WEBKIT_API WebPoint scrollOffset() const;
        WEBKIT_API void setScrollOffset(const WebPoint&) const;

        WEBKIT_API bool isTargetItem() const;
        WEBKIT_API void setIsTargetItem(bool) const;

        WEBKIT_API int visitCount() const;
        WEBKIT_API void setVisitCount(int) const;

        WEBKIT_API void documentState(WebVector<WebString>&) const;
        WEBKIT_API void setDocumentState(const WebVector<WebString>&) const;

        WEBKIT_API WebString httpContentType() const;
        WEBKIT_API void setHTTPContentType(const WebString&) const;

        WEBKIT_API bool hasHTTPBody() const;
        WEBKIT_API void httpBody(WebHTTPBody&) const;
        WEBKIT_API void setHTTPBody(const WebHTTPBody&);
        WEBKIT_API void appendToHTTPBody(const WebHTTPBody::Element&);

        WEBKIT_API bool hasChildren() const; 
        WEBKIT_API void children(WebVector<WebHistoryItem>&) const;
        WEBKIT_API void setChildren(const WebVector<WebHistoryItem>&);
        WEBKIT_API void appendToChildren(const WebHistoryItem&);

    private:
        WebHistoryItemPrivate* m_private;
    };

} // namespace WebKit

#endif
