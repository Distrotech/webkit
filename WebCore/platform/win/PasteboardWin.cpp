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
#include "KURL.h"
#include "NotImplemented.h"
#include "Page.h"
#include "Range.h"
#include "TextEncoding.h"
#include "markup.h"

namespace WebCore {

static UINT HTMLClipboardFormat = 0;
static UINT BookmarkClipboardFormat = 0;

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

static HGLOBAL createGlobalData(const KURL& url, const String& title)
{
    String mutableURL(url.url());
    String mutableTitle(title);
    SIZE_T size = mutableURL.length() + mutableTitle.length() + 2;  // +1 for "\n" and +1 for null terminator
    HGLOBAL cbData = ::GlobalAlloc(GPTR, size * sizeof(UChar));

    if (cbData) {
        PWSTR buffer = (PWSTR)::GlobalLock(cbData);
        swprintf_s(buffer, size, L"%s\n%s", mutableURL.charactersWithNullTermination(), mutableTitle.charactersWithNullTermination());
        ::GlobalUnlock(cbData);
    }
    return cbData;
}

static HGLOBAL createGlobalData(String str)
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

static HGLOBAL createGlobalData(CString str)
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
static DeprecatedCString markupToCF_HTML(const String& markup, const String& srcURL)
{
    if (!markup.length())
        return DeprecatedCString();

    DeprecatedCString cf_html        ("Version:0.9");
    DeprecatedCString startHTML      ("\nStartHTML:");
    DeprecatedCString endHTML        ("\nEndHTML:");
    DeprecatedCString startFragment  ("\nStartFragment:");
    DeprecatedCString endFragment    ("\nEndFragment:");
    DeprecatedCString sourceURL      ("\nSourceURL:");

    bool shouldFillSourceURL = !srcURL.isEmpty() && (srcURL != "about:blank");
    if (shouldFillSourceURL)
        sourceURL.append(srcURL.utf8());

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

static String urlToMarkup(const KURL& url, const String& title)
{
    String markup("<a href=\"");
    markup.append(url.url());
    markup.append("\">");
    markup.append(title);
    markup.append("</a>");
    return markup;
}

static void replaceNBSP(String& str)
{
    const unsigned short NonBreakingSpaceCharacter = 0xA0;
    str.replace(NonBreakingSpaceCharacter, " ");
}

Pasteboard* Pasteboard::generalPasteboard() 
{
    static Pasteboard* pasteboard = new Pasteboard;
    return pasteboard;
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

    HTMLClipboardFormat = ::RegisterClipboardFormat(L"HTML Format");
    BookmarkClipboardFormat = ::RegisterClipboardFormat(L"UniformResourceLocatorW");
}

void Pasteboard::clear()
{
    if (::OpenClipboard(m_owner)) {
        ::EmptyClipboard();
        ::CloseClipboard();
    }
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    clear();
    
    // Put CF_HTML format on the pasteboard 
    if (::OpenClipboard(m_owner)) {
        ExceptionCode ec = 0;
        HGLOBAL cbData = createGlobalData(markupToCF_HTML(selectedRange->toHTML(), selectedRange->startContainer(ec)->document()->URL()));
        if (!::SetClipboardData(HTMLClipboardFormat, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }
    
    // Put plain string on the pasteboard. CF_UNICODETEXT covers CF_TEXT as well
    String str = frame->selectedText();
    replaceNBSP(str);
    if (::OpenClipboard(m_owner)) {
        HGLOBAL cbData = createGlobalData(str);
        if (!::SetClipboardData(CF_UNICODETEXT, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }
}

void Pasteboard::writeURL(const KURL& url, const String& titleStr, Frame* frame)
{
    ASSERT(!url.isEmpty());

    clear();

    String title(titleStr);
    if (title.isEmpty()) {
        title = url.lastPathComponent();
        if (title.isEmpty())
            title = url.host();
    }

    // write to clipboard in format com.apple.safari.bookmarkdata to be able to paste into the bookmarks view with appropriate title
    if (::OpenClipboard(m_owner)) {
        HGLOBAL cbData = createGlobalData(url, title);
        if (!::SetClipboardData(BookmarkClipboardFormat, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }

    // write to clipboard in format CF_HTML to be able to paste into contenteditable areas as a link
    if (::OpenClipboard(m_owner)) {
        HGLOBAL cbData = createGlobalData(markupToCF_HTML(urlToMarkup(url, title), ""));
        if (!::SetClipboardData(HTMLClipboardFormat, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }

    // bare-bones CF_UNICODETEXT support
    if (::OpenClipboard(m_owner)) {
        HGLOBAL cbData = createGlobalData(url.url());
        if (!::SetClipboardData(CF_UNICODETEXT, cbData))
            ::GlobalFree(cbData);
        ::CloseClipboard();
    }
}

bool Pasteboard::canSmartReplace()
{ 
    // WebSmartPastePboardType is unavailable
    return false; 
}

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

    return String();
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText)
{
    chosePlainText = false;
    
    if (::IsClipboardFormatAvailable(HTMLClipboardFormat) && ::OpenClipboard(m_owner)) {
        // get data off of clipboard
        HANDLE cbData = ::GetClipboardData(HTMLClipboardFormat);
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

} // namespace WebCore
