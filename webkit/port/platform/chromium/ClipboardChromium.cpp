/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#pragma warning(push, 0)
#include "CachedImage.h"
#include "ChromiumDataObject.h"
#include "ClipboardChromium.h"
#include "CSSHelper.h"
#include "CString.h"
#include "Document.h"
#include "DragData.h"
#include "Editor.h"
#include "Element.h"
#include "EventHandler.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "Image.h"
#include "MIMETypeRegistry.h"
#include "markup.h"
#include "Page.h"
#include "Pasteboard.h"
#include "PlatformString.h"
#include "Range.h"
#include "RenderImage.h"
#include "StringBuilder.h"
#include "StringHash.h"
#include <wtf/RefPtr.h>
#pragma warning(pop)

// TODO(tc): Once the clipboard methods get moved to the bridge,
// we can get rid of our dependency on webkit_glue and base.
#undef LOG
#include "base/string_util.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

namespace WebCore {

using namespace HTMLNames;

// We provide the IE clipboard types (URL and Text), and the clipboard types specified in the WHATWG Web Applications 1.0 draft
// see http://www.whatwg.org/specs/web-apps/current-work/ Section 6.3.5.3

enum ClipboardDataType { ClipboardDataTypeNone, ClipboardDataTypeURL, ClipboardDataTypeText };

static ClipboardDataType clipboardTypeFromMIMEType(const String& type)
{
    String qType = type.stripWhiteSpace().lower();

    // two special cases for IE compatibility
    if (qType == "text" || qType == "text/plain" || qType.startsWith("text/plain;"))
        return ClipboardDataTypeText;
    if (qType == "url" || qType == "text/uri-list")
        return ClipboardDataTypeURL;

    return ClipboardDataTypeNone;
}

#if PLATFORM(WIN_OS)
static void replaceNewlinesWithWindowsStyleNewlines(String& str)
{
    static const UChar Newline = '\n';
    static const char* const WindowsNewline("\r\n");
    str.replace(Newline, WindowsNewline);
}
#endif

static void replaceNBSPWithSpace(String& str)
{
    static const UChar NonBreakingSpaceCharacter = 0xA0;
    static const UChar SpaceCharacter = ' ';
    str.replace(NonBreakingSpaceCharacter, SpaceCharacter);
}


ClipboardChromium::ClipboardChromium(bool isForDragging,
                                     ChromiumDataObject* dataObject,
                                     ClipboardAccessPolicy policy)
    : Clipboard(policy, isForDragging)
    , m_dataObject(dataObject)
{
}

PassRefPtr<ClipboardChromium> ClipboardChromium::create(bool isForDragging,
    ChromiumDataObject* dataObject, ClipboardAccessPolicy policy)
{
    return adoptRef(new ClipboardChromium(isForDragging, dataObject, policy));
}

void ClipboardChromium::clearData(const String& type)
{
    if (policy() != ClipboardWritable || !m_dataObject)
        return;

    ClipboardDataType dataType = clipboardTypeFromMIMEType(type);

    if (dataType == ClipboardDataTypeURL) {
        m_dataObject->url = KURL();
        m_dataObject->url_title = "";
    }
    if (dataType == ClipboardDataTypeText) {
        m_dataObject->plain_text = "";
    }
}

void ClipboardChromium::clearAllData()
{
    if (policy() != ClipboardWritable)
        return;

    m_dataObject->clear();
}

String ClipboardChromium::getData(const String& type, bool& success) const
{
    success = false;
    if (policy() != ClipboardReadable || !m_dataObject) {
        return "";
    }

    ClipboardDataType dataType = clipboardTypeFromMIMEType(type);
    if (dataType == ClipboardDataTypeText) {
        String text;
        if (!isForDragging()) {
#if PLATFORM(WIN_OS)
            // If this isn't for a drag, it's for a cut/paste event handler.
            // In this case, we need to use our glue methods to access the
            // clipboard contents.
            std::wstring wideStr;
            // TODO(tc): Once the clipboard methods get moved to the bridge,
            // we can get rid of our dependency on webkit_glue.
            webkit_glue::ClipboardReadText(&wideStr);
            if (wideStr.empty()) {
                std::string asciiText;
                webkit_glue::ClipboardReadAsciiText(&asciiText);
                wideStr = ASCIIToWide(asciiText);
            }
            success = !wideStr.empty();
            text = webkit_glue::StdWStringToString(wideStr);
#endif
        } else if (!m_dataObject->plain_text.isEmpty()) {
            success = true;
            text = m_dataObject->plain_text;
        }
        return text;
    } else if (dataType == ClipboardDataTypeURL) {
        if (!m_dataObject->url.isEmpty()) {
            success = true;
            return m_dataObject->url.string();
        }
    }

    return "";
}

bool ClipboardChromium::setData(const String& type, const String& data)
{
    if (policy() != ClipboardWritable)
        return false;

    ClipboardDataType winType = clipboardTypeFromMIMEType(type);

    if (winType == ClipboardDataTypeURL) {
        m_dataObject->url = KURL(data);
        return m_dataObject->url.isValid();
    }

    if (winType == ClipboardDataTypeText) {
        m_dataObject->plain_text = data;
        return true;
    }
    return false;
}

// extensions beyond IE's API
HashSet<String> ClipboardChromium::types() const
{
    HashSet<String> results;
    if (policy() != ClipboardReadable && policy() != ClipboardTypesReadable)
        return results;

    if (!m_dataObject)
        return results;

    if (m_dataObject->url.isValid()) {
        results.add("URL");
        results.add("text/uri-list");
    }

    if (!m_dataObject->plain_text.isEmpty()) {
        results.add("Text");
        results.add("text/plain");
    }

    return results;
}

void ClipboardChromium::setDragImage(CachedImage* image, Node *node, const IntPoint &loc)
{
    if (policy() != ClipboardImageWritable && policy() != ClipboardWritable)
        return;

    if (m_dragImage)
        m_dragImage->removeClient(this);
    m_dragImage = image;
    if (m_dragImage)
        m_dragImage->addClient(this);

    m_dragLoc = loc;
    m_dragImageElement = node;
}

void ClipboardChromium::setDragImage(CachedImage* img, const IntPoint &loc)
{
    setDragImage(img, 0, loc);
}

void ClipboardChromium::setDragImageElement(Node *node, const IntPoint &loc)
{
    setDragImage(0, node, loc);
}

DragImageRef ClipboardChromium::createDragImage(IntPoint& loc) const
{
    DragImageRef result = 0;
    if (m_dragImage) {
        result = createDragImageFromImage(m_dragImage->image());
        loc = m_dragLoc;
    }
    return result;
}

static String imageToMarkup(const String& url, Element* element)
{
    StringBuilder markup;
    markup.append("<img src=\"");
    markup.append(url);
    markup.append("\"");
    // Copy over attributes.  If we are dragging an image, we expect things like
    // the id to be copied as well.
    NamedAttrMap* attrs = element->attributes();
    unsigned int length = attrs->length();
    for (unsigned int i = 0; i < length; ++i) {
        Attribute* attr = attrs->attributeItem(i);
        if (attr->localName() == "src")
            continue;
        markup.append(" ");
        markup.append(attr->localName());
        markup.append("=\"");
        String escapedAttr = attr->value();
        escapedAttr.replace("\"", "&quot;");
        markup.append(escapedAttr);
        markup.append("\"");
    }

    markup.append("/>");
    return markup.toString();
}

static CachedImage* getCachedImage(Element* element)
{
    // Attempt to pull CachedImage from element
    ASSERT(element);
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isImage())
        return 0;

    RenderImage* image = static_cast<RenderImage*>(renderer);
    if (image->cachedImage() && !image->cachedImage()->errorOccurred())
        return image->cachedImage();

    return 0;
}

static void writeImageToDataObject(ChromiumDataObject* dataObject, Element* element,
                                   const KURL& url)
{
    // Shove image data into a DataObject for use as a file
    CachedImage* cachedImage = getCachedImage(element);
    if (!cachedImage || !cachedImage->image() || !cachedImage->isLoaded())
        return;

    SharedBuffer* imageBuffer = cachedImage->image()->data();
    if (!imageBuffer || !imageBuffer->size())
        return;

    dataObject->file_content = imageBuffer;

    // Determine the filename for the file contents of the image.  We try to
    // use the alt tag if one exists, otherwise we fall back on the suggested
    // filename in the http header, and finally we resort to using the filename
    // in the URL.
    String extension(".");
    extension += WebCore::MIMETypeRegistry::getPreferredExtensionForMIMEType(
        cachedImage->response().mimeType());
    String title = element->getAttribute(altAttr);
    if (title.isEmpty()) {
        title = cachedImage->response().suggestedFilename();
        if (title.isEmpty()) {
            // TODO(tc): Get the filename from the URL.
        }
    }
    dataObject->file_content_filename = title + extension;
}

void ClipboardChromium::declareAndWriteDragImage(Element* element, const KURL& url, const String& title, Frame* frame)
{
    if (!m_dataObject)
        return;

    m_dataObject->url = url;
    m_dataObject->url_title = title;

    // Write the bytes in the image to the file format.
    writeImageToDataObject(m_dataObject.get(), element, url);

    AtomicString imageURL = element->getAttribute(srcAttr);
    if (imageURL.isEmpty())
        return;

    String fullURL = frame->document()->completeURL(parseURL(imageURL));
    if (fullURL.isEmpty())
        return;

    // Put img tag on the clipboard referencing the image
    m_dataObject->text_html = imageToMarkup(fullURL, element);
}

void ClipboardChromium::writeURL(const KURL& url, const String& title, Frame*)
{
    if (!m_dataObject)
        return;
    m_dataObject->url = url;
    m_dataObject->url_title = title;

    // The URL can also be used as plain text.
    m_dataObject->plain_text = url.string();

    // The URL can also be used as an HTML fragment.
    String markup("<a href=\"");
    markup.append(url.string());
    markup.append("\">");
    markup.append(title);
    markup.append("</a>");
    m_dataObject->text_html = markup;
}

void ClipboardChromium::writeRange(Range* selectedRange, Frame* frame)
{
    ASSERT(selectedRange);
    if (!m_dataObject)
         return;

    m_dataObject->text_html = createMarkup(selectedRange, 0,
        AnnotateForInterchange);

    String str = frame->selectedText();
#if PLATFORM(WIN_OS)
    replaceNewlinesWithWindowsStyleNewlines(str);
#endif
    replaceNBSPWithSpace(str);
    m_dataObject->plain_text = str;
}

bool ClipboardChromium::hasData()
{
    if (!m_dataObject)
        return false;

    return m_dataObject->hasData();
}

} // namespace WebCore
