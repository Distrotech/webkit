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
#include "ClipboardWin.h"

#include "CachedImage.h"
#include "csshelper.h"
#include "CString.h"
#include "DeprecatedString.h"
#include "Document.h"
#include "DragData.h"
#include "Element.h"
#include "EventHandler.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "Page.h"
#include "Pasteboard.h"
#include "PlatformMouseEvent.h"
#include "PlatformString.h"
#include "Range.h"
#include "StringHash.h"
#include "WCDataObject.h"
#include <wtf/RefPtr.h>

namespace WebCore {

using namespace HTMLNames;

FORMATETC* urlFormat();
FORMATETC* urlWFormat();
FORMATETC* plainTextFormat();
FORMATETC* plainTextWFormat();
FORMATETC* texthtmlFormat();
FORMATETC* htmlFormat();

HGLOBAL createGlobalData(String str);
HGLOBAL createGlobalData(CString str);
HGLOBAL createGlobalData(const KURL& url, const String& title);
DeprecatedCString markupToCF_HTML(const String& markup, const String& srcURL);
String urlToMarkup(const KURL& url, const String& title);
void replaceNBSP(String& str);
String getURL(IDataObject* dataObject, bool& success, String* title = 0);
String getPlainText(IDataObject* dataObject, bool& success);

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

ClipboardWin::ClipboardWin(bool isForDragging, IDataObject* dataObject, ClipboardAccessPolicy policy)
    : Clipboard(policy, isForDragging)
    , m_dataObject(dataObject)
    , m_writableDataObject(0)
{
}

ClipboardWin::ClipboardWin(bool isForDragging, WCDataObject* dataObject, ClipboardAccessPolicy policy)
    : Clipboard(policy, isForDragging)
    , m_dataObject(dataObject)
    , m_writableDataObject(dataObject)
{
}

ClipboardWin::~ClipboardWin()
{
}

static bool writeURL(WCDataObject *data, const KURL& url, String title, bool withPlainText, bool withHTML)
{
    ASSERT(data);
    ASSERT(!url.isEmpty());
    
    if (!url.isValid())
        return false;

    if (title.isEmpty()) {
        title = url.lastPathComponent();
        if (title.isEmpty())
            title = url.host();
    }

    STGMEDIUM medium = {0};
    medium.tymed = TYMED_HGLOBAL;

    medium.hGlobal = createGlobalData(url, title);
    bool success = false;
    if (medium.hGlobal && FAILED(data->SetData(urlWFormat(), &medium, TRUE)))
        ::GlobalFree(medium.hGlobal);
    else
        success = true;

    if (withHTML) {
        medium.hGlobal = createGlobalData(markupToCF_HTML(urlToMarkup(url, title), ""));
        if (medium.hGlobal && FAILED(data->SetData(htmlFormat(), &medium, TRUE)))
            ::GlobalFree(medium.hGlobal);
        else
            success = true;
    }

    if (withPlainText) {
        medium.hGlobal = createGlobalData(url.url());
        if (medium.hGlobal && FAILED(data->SetData(plainTextWFormat(), &medium, TRUE)))
            ::GlobalFree(medium.hGlobal);
        else
            success = true;
    }

    return success;
}

void ClipboardWin::clearData(const String& type)
{
    //FIXME: Need to be able to write to the system clipboard <rdar://problem/5015941>
    ASSERT(isForDragging());
    if (policy() != ClipboardWritable || !m_writableDataObject)
        return;

    ClipboardDataType dataType = clipboardTypeFromMIMEType(type);

    if (dataType == ClipboardDataTypeURL) {
        m_writableDataObject->clearData(urlWFormat()->cfFormat);
        m_writableDataObject->clearData(urlFormat()->cfFormat);
    }
    if (dataType == ClipboardDataTypeText) {
        m_writableDataObject->clearData(plainTextFormat()->cfFormat);
        m_writableDataObject->clearData(plainTextWFormat()->cfFormat);
    }

}

void ClipboardWin::clearAllData()
{
    //FIXME: Need to be able to write to the system clipboard <rdar://problem/5015941>
    ASSERT(isForDragging());
    if (policy() != ClipboardWritable)
        return;
    
    m_writableDataObject = 0;
    WCDataObject::createInstance(&m_writableDataObject);
    m_dataObject = m_writableDataObject;
}

String ClipboardWin::getData(const String& type, bool& success) const
{     
    success = false;
    if (policy() != ClipboardReadable || !m_dataObject) {
        return "";
    }

    ClipboardDataType dataType = clipboardTypeFromMIMEType(type);
    if (dataType == ClipboardDataTypeText)
        return getPlainText(m_dataObject.get(), success);
    else if (dataType == ClipboardDataTypeURL) 
        return getURL(m_dataObject.get(), success);
    
    return "";
}

bool ClipboardWin::setData(const String &type, const String &data)
{
    //FIXME: Need to be able to write to the system clipboard <rdar://problem/5015941>
    ASSERT(isForDragging());
    if (policy() != ClipboardWritable || !m_writableDataObject)
        return false;

    ClipboardDataType winType = clipboardTypeFromMIMEType(type);

    if (winType == ClipboardDataTypeURL) {
        KURL url = data.deprecatedString();
        if (!url.isValid())
            return false;
        return WebCore::writeURL(m_writableDataObject.get(), url, String(), false, true);
    } else if ( winType == ClipboardDataTypeText) {
        STGMEDIUM medium = {0};
        medium.tymed = TYMED_HGLOBAL;
        medium.hGlobal = createGlobalData(data);
        if (!medium.hGlobal)
            return false;

        if (FAILED(m_writableDataObject->SetData(plainTextWFormat(), &medium, TRUE))) {
            ::GlobalFree(medium.hGlobal);
            return false;
        }
        return true;
    }
    return false;
}

static void addMimeTypesForFormat(HashSet<String>& results, FORMATETC& format)
{
    // URL and Text are provided for compatibility with IE's model
    if (format.cfFormat == urlFormat()->cfFormat || format.cfFormat == urlWFormat()->cfFormat) {
        results.add("URL");
        results.add("text/uri-list");
    }

    if (format.cfFormat == plainTextWFormat()->cfFormat || format.cfFormat == plainTextFormat()->cfFormat) {
        results.add("Text");
        results.add("text/plain");
    }
}

// extensions beyond IE's API
HashSet<String> ClipboardWin::types() const
{ 
    HashSet<String> results; 
    if (policy() != ClipboardReadable && policy() != ClipboardTypesReadable)
        return results;

    if (!m_dataObject)
        return results;

    COMPtr<IEnumFORMATETC> itr;

    if (FAILED(m_dataObject->EnumFormatEtc(0, &itr)))
        return results;

    if (!itr)
        return results;

    FORMATETC data;

    while (SUCCEEDED(itr->Next(1, &data, 0))) {
        addMimeTypesForFormat(results, data);
    }

    return results;
}

void ClipboardWin::setDragImage(CachedImage* image, Node *node, const IntPoint &loc)
{
    if (policy() != ClipboardImageWritable && policy() != ClipboardWritable) 
        return;
        
    if (m_dragImage)
        m_dragImage->deref(this);
    m_dragImage = image;
    if (m_dragImage)
        m_dragImage->ref(this);

    m_dragLoc = loc;
    m_dragImageElement = node;
}

void ClipboardWin::setDragImage(CachedImage* img, const IntPoint &loc)
{
    setDragImage(img, 0, loc);
}

void ClipboardWin::setDragImageElement(Node *node, const IntPoint &loc)
{
    setDragImage(0, node, loc);
}

DragImageRef ClipboardWin::createDragImage(IntPoint& loc) const
{
    HBITMAP result = 0;
    //FIXME: Need to be able to draw element <rdar://problem/5015942>
    if (m_dragImage) {
        result = createDragImageFromImage(m_dragImage->image());        
        loc = m_dragLoc;
    }
    return result;
}

static String imageToMarkup(const String& url)
{
    String markup("<img src=\"");
    markup.append(url);
    markup.append("\"/>");
    return markup;
}

void ClipboardWin::declareAndWriteDragImage(Element* element, const KURL& url, const String& title, Frame* frame)
{
    if (!m_writableDataObject)
         return;
    WebCore::writeURL(m_writableDataObject.get(), url, title, true, false);
    AtomicString imageURL = element->getAttribute(srcAttr);
    if (imageURL.isEmpty()) 
        return;
    String fullURL = frame->document()->completeURL(parseURL(imageURL));
    if (fullURL.isEmpty()) 
        return;
    STGMEDIUM medium = {0};
    medium.tymed = TYMED_HGLOBAL;
    ExceptionCode ec = 0;

    // Put img tag on the clipboard referencing the image
    medium.hGlobal = createGlobalData(markupToCF_HTML(imageToMarkup(fullURL), ""));
    if (medium.hGlobal && FAILED(m_writableDataObject->SetData(htmlFormat(), &medium, TRUE)))
        ::GlobalFree(medium.hGlobal);
}

void ClipboardWin::writeURL(const KURL& url, const String& titleStr, Frame*)
{
    if (!m_writableDataObject)
         return;
    WebCore::writeURL(m_writableDataObject.get(), url, titleStr, true, true);
}

void ClipboardWin::writeRange(Range* selectedRange, Frame* frame)
{
    ASSERT(selectedRange);
    if (!m_writableDataObject)
         return;

    STGMEDIUM medium = {0};
    medium.tymed = TYMED_HGLOBAL;
    ExceptionCode ec = 0;

    medium.hGlobal = createGlobalData(markupToCF_HTML(selectedRange->toHTML(), selectedRange->startContainer(ec)->document()->URL()));
    if (medium.hGlobal && FAILED(m_writableDataObject->SetData(htmlFormat(), &medium, TRUE)))
        ::GlobalFree(medium.hGlobal);

    String str = frame->selectedText();
    replaceNBSP(str);
    medium.hGlobal = createGlobalData(str);
    if (medium.hGlobal && FAILED(m_writableDataObject->SetData(plainTextWFormat(), &medium, TRUE)))
        ::GlobalFree(medium.hGlobal);

}

bool ClipboardWin::hasData()
{
    if (!m_dataObject)
        return false;

    COMPtr<IEnumFORMATETC> itr;
    if (FAILED(m_dataObject->EnumFormatEtc(0, &itr)))
        return false;

    if (!itr)
        return false;

    FORMATETC data;

    if (SUCCEEDED(itr->Next(1, &data, 0))) {
        // There is at least one item in the IDataObject
        return true;
    }

    return false;
}

} // namespace WebCore
