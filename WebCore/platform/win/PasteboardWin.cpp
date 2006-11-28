/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "Pasteboard.h"

#include "CString.h"
#include "DeprecatedString.h"
#include "DocumentFragment.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "Page.h"
#include "TextEncoding.h"
#include "markup.h"
#include "NotImplemented.h"

namespace WebCore {

UINT WebArchivePboardTypeID         = ::RegisterClipboardFormat(L"Apple Web Archive pasteboard type");
UINT WebRTFPboardTypeID             = ::RegisterClipboardFormat(L"Rich Text Format");
UINT CF_HTML                        = ::RegisterClipboardFormat(L"HTML Format");

static LRESULT CALLBACK PasteboardOwnerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

Pasteboard* Pasteboard::s_generalPasteboard = 0;

Pasteboard* Pasteboard::generalPasteboard() 
{
    if (!s_generalPasteboard)
        s_generalPasteboard = new Pasteboard();
    return s_generalPasteboard;
}

Pasteboard::Pasteboard()
{ 
    // make a dummy HWND to be the Windows clipboard's owner
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc    = PasteboardOwnerWndProc;
    wcex.hInstance      = Page::instanceHandle();
    wcex.lpszClassName  = L"PasteboardOwnerWindowClass";
    ::RegisterClassEx(&wcex);

    m_owner = ::CreateWindow(L"PasteboardOwnerWindowClass", L"PasteboardOwnerWindow", 0, 0, 0, 0, 0,
        HWND_MESSAGE, 0, 0, 0);
}

Pasteboard::~Pasteboard()
{
    delete s_generalPasteboard;
    s_generalPasteboard = 0;
    if (m_owner)
        ::DestroyWindow(m_owner);
}

HashSet<int> Pasteboard::registerSelectionPasteboardTypes()
{
    HashSet<int> types;
    types.add(WebArchivePboardTypeID);
    types.add(CF_HTML);
    types.add(WebRTFPboardTypeID);
    types.add(CF_UNICODETEXT);

    return types;
}

void Pasteboard::clearTypes()
{
    if (::OpenClipboard(m_owner)) {
        ::EmptyClipboard();
        ::CloseClipboard();
    }
}

void Pasteboard::writeSelection(PassRefPtr<Range> selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    clearTypes();
    HashSet<int> types = registerSelectionPasteboardTypes();
    bool result = false;
    HANDLE handle;
    
    // FIXME: WebArchivePboardType unsupported for now

    // Put CF_HTML format on the pasteboard 
    if (types.contains(CF_HTML)) {
        if (::OpenClipboard(m_owner)) {
            HGLOBAL cbData = createHandle(createCF_HTMLFromRange(selectedRange));
            if (!::SetClipboardData(CF_HTML, cbData))
                ::GlobalFree(cbData);
            ::CloseClipboard();
        }
    }

    // FIXME: WebRTFPboardType unsupported for now
    
    // Put plain string on the pasteboard. CF_UNICODETEXT covers CF_TEXT as well
    if (types.contains(CF_UNICODETEXT)) {
        String str = frame->selectedText();
        replaceNBSP(str);
        if (::OpenClipboard(m_owner)) {
            HGLOBAL cbData = createHandle(str);
            if (!::SetClipboardData(CF_UNICODETEXT, cbData))
                ::GlobalFree(cbData);
            ::CloseClipboard();
        }
    }
    
    // not going to support WebSmartPastePboardType like we do on the Mac, or at least we appear to but not really
}

HGLOBAL Pasteboard::createHandle(String str)
{   
    SIZE_T size = (str.length() + 1) * sizeof(UChar);
    HGLOBAL cbData = ::GlobalAlloc(GPTR, size);
    if (cbData) {
        ::GlobalLock(cbData);
        memcpy(cbData, str.charactersWithNullTermination(), size);
        ::GlobalUnlock(cbData);
    }
    return cbData;
}

HGLOBAL Pasteboard::createHandle(CString str)
{
    SIZE_T size = (str.length() + 1) * sizeof(char);
    HGLOBAL cbData = ::GlobalAlloc(GPTR, size);
    if (cbData) {
        ::GlobalLock(cbData);
        memcpy(cbData, str.data(), size);
        ::GlobalUnlock(cbData);
    }
    return cbData;
}

 
// Documentation for the CF_HTML format is available at http://msdn.microsoft.com/workshop/networking/clipboard/htmlclipboard.asp
DeprecatedCString Pasteboard::createCF_HTMLFromRange(PassRefPtr<Range> selectedRange)
{
    String markup = selectedRange->toHTML();
    if (!markup.length())
        return DeprecatedCString();
    DeprecatedCString cf_html        ("Version:0.9");
    DeprecatedCString startHTML      ("\nStartHTML:");
    DeprecatedCString endHTML        ("\nEndHTML:");
    DeprecatedCString startFragment  ("\nStartFragment:");
    DeprecatedCString endFragment    ("\nEndFragment:");
    DeprecatedCString sourceURL      ("\nSourceURL:");
    ExceptionCode ec = 0;
    DeprecatedCString URL = selectedRange->startContainer(ec)->document()->URL().utf8();
    bool shouldFillSourceURL = (URL != "about:blank");
    if (shouldFillSourceURL)
        sourceURL.append(URL);
    DeprecatedCString startMarkup    ("\n<HTML>\n<BODY>\n<!--StartFragment-->\n");
    DeprecatedCString endMarkup      ("\n<!--EndFragment-->\n</BODY>\n</HTML>");

    // calculate offsets
    const unsigned UINT_MAXdigits = 10; // number of digits in UINT_MAX in base 10
    unsigned startHTMLOffset = cf_html.length() + startHTML.length() + endHTML.length() + startFragment.length() + endFragment.length() + (shouldFillSourceURL ? sourceURL.length() : 0) + (4*UINT_MAXdigits);
    unsigned startFragmentOffset = startHTMLOffset + startMarkup.length();
    CString markupUTF8 = markup.utf8();
    unsigned endFragmentOffset = startFragmentOffset + markupUTF8.length();
    unsigned endHTMLOffset = endFragmentOffset + endMarkup.length();

    // fill in needed data
    startHTML.append(String::format("%010u", startHTMLOffset).deprecatedString().utf8());
    endHTML.append(String::format("%010u", endHTMLOffset).deprecatedString().utf8());
    startFragment.append(String::format("%010u", startFragmentOffset).deprecatedString().utf8());
    endFragment.append(String::format("%010u", endFragmentOffset).deprecatedString().utf8());
    startMarkup.append(markupUTF8.data());

    // create full cf_html string from the fragments
    cf_html.append(startHTML);
    cf_html.append(endHTML);
    cf_html.append(startFragment);
    cf_html.append(endFragment);
    if (shouldFillSourceURL)
        cf_html.append(sourceURL);
    cf_html.append(startMarkup);
    cf_html.append(endMarkup);

    return cf_html;
}

void Pasteboard::replaceNBSP(String& str)
{
    const unsigned short NonBreakingSpaceCharacter = 0xA0;
    str.replace(NonBreakingSpaceCharacter, " ");
}

bool Pasteboard::canSmartReplace()
{ LOG_NOIMPL(); return false; }

String Pasteboard::plainText(Frame* frame)
{
    if (::IsClipboardFormatAvailable(CF_UNICODETEXT) && ::OpenClipboard(m_owner)) {
        HANDLE cbData = ::GetClipboardData(CF_UNICODETEXT);
        if (cbData) {
            UChar* buffer = (UChar*)::GlobalLock(cbData);
            String fromClipboard(buffer);
            ::GlobalUnlock(cbData);
            ::CloseClipboard();
            return fromClipboard;
        } else
            ::CloseClipboard();
    }

    if (::IsClipboardFormatAvailable(CF_TEXT) && ::OpenClipboard(m_owner)) {
        HANDLE cbData = ::GetClipboardData(CF_TEXT);
        if (cbData) {
            char* buffer = (char*)::GlobalLock(cbData);
            String fromClipboard(buffer);
            ::GlobalUnlock(cbData);
            ::CloseClipboard();
            return fromClipboard;
        } else
            ::CloseClipboard();
    }

    // FIXME: WebRTFPboardType unsupported for now 

    return String();
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;

    // FIXME: Unsupported at this time
    //if (::IsClipboardFormatAvailable(WebArchivePboardTypeID))
    //if (::IsClipboardFormatAvailable(CF_HDROP)) // is this the proper analog for NSFilenamesPboardType?
    
    if (::IsClipboardFormatAvailable(CF_HTML) && ::OpenClipboard(m_owner)) {
        // get data off of clipboard
        HANDLE cbData = ::GetClipboardData(CF_HTML);
        if (cbData) {
            SIZE_T dataSize = ::GlobalSize(cbData);
            String cf_html((char*)::GlobalLock(cbData), dataSize);
            ::GlobalUnlock(cbData);
            ::CloseClipboard();

            // obtain baseURL if present
            String srcURLStr("sourceURL:");
            String srcURL;
            unsigned lineStart = cf_html.find(srcURLStr, 0, false);
            if (lineStart != -1) {
                unsigned srcEnd = cf_html.find("\n", lineStart, false);
                unsigned srcStart = lineStart+srcURLStr.length();
                srcURL = cf_html.substring(srcStart, srcEnd-srcStart).stripWhiteSpace();
            }

            // obtain markup portion
            unsigned markupStart = cf_html.find("<html", 0, false);
            String markup = cf_html.substring(markupStart);

            RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(frame->document(), markup, srcURL);
            if (fragment)
                return fragment.release();
        } else 
            ::CloseClipboard();
    }
    
    // FIXME: Unsupported at this time
    //if (::IsClipboardFormatAvailable(WebRTFPboardType))
    
    // FIXME: Unsupported at this time
    if (::IsClipboardFormatAvailable(CF_TIFF)) {
        //ExceptionCode ec;
        //RefPtr<Element> image = frame->document()->createElement("img", ec);
        //HANDLE cbDataHandle = ::GetClipboardData(CF_TIFF);
        //image->setAttribute("src", ???, ec);
        //RefPtr<DocumentFragment> fragment = frame->document()->createDocumentFragment();
        //if (fragment) {
        //    fragment->appendChild(image, ec);
        //    return fragment.release();
        //}
    }
 
    if (allowPlainText && ::IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        chosePlainText = true;
        if (::OpenClipboard(m_owner)) {
            HANDLE cbData = ::GetClipboardData(CF_UNICODETEXT);
            if (cbData) {
                UChar* buffer = (UChar*)GlobalLock(cbData);
                String str(buffer);
                ::GlobalUnlock( cbData );
                ::CloseClipboard();
                RefPtr<DocumentFragment> fragment = createFragmentFromText(context.get(), str);
                if (fragment)
                    return fragment.release();
            } else 
                ::CloseClipboard();
        }
    }

    if (allowPlainText && ::IsClipboardFormatAvailable(CF_TEXT)) {
        chosePlainText = true;
        if (::OpenClipboard(m_owner)) {
            HANDLE cbData = ::GetClipboardData(CF_TEXT);
            if (cbData) {
                char* buffer = (char*)GlobalLock(cbData);
                String str(buffer);
                ::GlobalUnlock( cbData );
                ::CloseClipboard();
                RefPtr<DocumentFragment> fragment = createFragmentFromText(context.get(), str);
                if (fragment)
                    return fragment.release();
            } else
                ::CloseClipboard();
        }
    }
    
    return 0;
}

static LRESULT CALLBACK PasteboardOwnerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lresult = 0;
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, 0);

    switch(message) {
    case WM_RENDERFORMAT:
        // This message comes when SetClipboardData was sent a null data handle 
        // and now it's come time to put the data on the clipboard.
        break;
    case WM_RENDERALLFORMATS:
        // This message comes when SetClipboardData was sent a null data handle
        // and now this application is about to quit, so it must put data on 
        // the clipboard before it exits.
        break;
    case WM_DRAWCLIPBOARD:
        break;
    case WM_DESTROY:
        break;
    case WM_CHANGECBCHAIN:
        break;
    default:
        lresult = DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }
    return lresult;
}

} // namespace WebCore
