/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "DragData.h"

#include "ClipboardWin.h"
#include "ClipboardAccessPolicy.h"
#include "DocumentFragment.h"
#include "PlatformString.h"
#include "Markup.h"
#include <objidl.h>
#include <shlwapi.h>
#include <wininet.h>

namespace WebCore {

Clipboard* DragData::createClipboard(ClipboardAccessPolicy policy) const
{
    return new ClipboardWin(true, m_platformDragData, policy);
}

static FORMATETC* urlWFormat()
{
    static UINT cf = RegisterClipboardFormat(L"UniformResourceLocatorW");
    static FORMATETC urlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

static FORMATETC* urlFormat()
{
    static UINT cf = RegisterClipboardFormat(L"UniformResourceLocator");
    static FORMATETC urlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

static FORMATETC* filenameWFormat()
{
    static UINT cf = RegisterClipboardFormat(L"FileNameW");
    static FORMATETC urlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

static FORMATETC* filenameFormat()
{
    static UINT cf = RegisterClipboardFormat(L"FileName");
    static FORMATETC urlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &urlFormat;
}

static FORMATETC* plainTextFormat()
{
    static FORMATETC textFormat = {CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &textFormat;
}

static FORMATETC* plainTextWFormat()
{
    static FORMATETC textFormat = {CF_UNICODETEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &textFormat;
}

//Firefox text/html
static FORMATETC* texthtmlFormat() 
{
    static UINT cf = RegisterClipboardFormat(L"text/html");
    static FORMATETC texthtmlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &texthtmlFormat;
}

//MSIE HTML Format
static FORMATETC* htmlFormat() 
{
    static UINT cf = RegisterClipboardFormat(L"HTML Format");
    static FORMATETC htmlFormat = {cf, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return &htmlFormat;
}

bool DragData::containsURL() const
{
    return SUCCEEDED(m_platformDragData->QueryGetData(urlWFormat())) 
        || SUCCEEDED(m_platformDragData->QueryGetData(urlFormat()))
        || SUCCEEDED(m_platformDragData->QueryGetData(filenameWFormat())) 
        || SUCCEEDED(m_platformDragData->QueryGetData(filenameFormat()));
}

static String extractURL(const String &inURL, String* title)
{
    String url = inURL;
    int splitLoc = url.find('\n');
    if (splitLoc > 0) {
        if (title)
            *title = url.substring(splitLoc+1);
        url.truncate(splitLoc);
    } else if (title)
        *title = url;
    return url;
}

String DragData::asURL(String* title) const
{
    STGMEDIUM store;
    String url;
    if (SUCCEEDED(m_platformDragData->GetData(urlWFormat(), &store))) {
        //URL using unicode
        UChar* data = (UChar*)GlobalLock(store.hGlobal);
        url = extractURL(String(data), title);
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
    } else if (SUCCEEDED(m_platformDragData->GetData(urlFormat(), &store))) {
        //URL using ascii
        char* data = (char*)GlobalLock(store.hGlobal);
        url = extractURL(String(data), title);
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
    } else if (SUCCEEDED(m_platformDragData->GetData(filenameWFormat(), &store))) {
        //file using unicode
        wchar_t* data = (wchar_t*)GlobalLock(store.hGlobal);        
        if (data && data[0] && (PathFileExists(data) || PathIsUNC(data))) {
            wchar_t fileURL[INTERNET_MAX_URL_LENGTH];
            DWORD fileURLLength = sizeof(fileURL) / sizeof(fileURL[0]);
            if (SUCCEEDED(::UrlCreateFromPathW(data, fileURL, &fileURLLength, 0))) {
                url = String((UChar*)fileURL);
                if (title)
                    *title = url;
            }
        }
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
    } else if (SUCCEEDED(m_platformDragData->GetData(filenameFormat(), &store))) {
        //filename using ascii
        char* data = (char*)GlobalLock(store.hGlobal);       
        if (data && data[0] && (PathFileExistsA(data) || PathIsUNCA(data))) {
            char fileURL[INTERNET_MAX_URL_LENGTH];
            DWORD fileURLLength = sizeof(fileURL) / sizeof(fileURL[0]);
            if (SUCCEEDED(::UrlCreateFromPathA(data, fileURL, &fileURLLength, 0))) {
                url = fileURL;
                if (title)
                    *title = url;
            }
        }
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
    }
    return url;
}

bool DragData::containsPlainText() const
{
    return SUCCEEDED(m_platformDragData->QueryGetData(plainTextWFormat()))
        || SUCCEEDED(m_platformDragData->QueryGetData(plainTextFormat()));
}

String DragData::asPlainText() const
{
    STGMEDIUM store;
    String text;
    if (SUCCEEDED(m_platformDragData->GetData(plainTextWFormat(), &store))) {
        //unicode text
        UChar* data = (UChar*)GlobalLock(store.hGlobal);
        text = String(data);
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
    } else if (SUCCEEDED(m_platformDragData->GetData(plainTextFormat(), &store))) {
        //ascii text
        char* data = (char*)GlobalLock(store.hGlobal);
        text = String(data);
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
    }
    return text;
}

static bool containsHTML(IDataObject* data)
{
    return SUCCEEDED(data->QueryGetData(texthtmlFormat())) || SUCCEEDED(data->QueryGetData(htmlFormat()));
}

//fragmentFromCF_HTML is in PasteboardWin.cpp
DocumentFragment* fragmentFromCF_HTML(Document* doc, const String& cf_html);

static PassRefPtr<DocumentFragment> fragmentFromHTML(Document* doc, IDataObject* data) 
{
    if (!doc || !data)
        return 0;

    STGMEDIUM store;
    String html;
    String srcURL;
    if (SUCCEEDED(data->GetData(htmlFormat(), &store))) {
        //MS HTML Format parsing
        char* data = (char*)GlobalLock(store.hGlobal);
        String cf_html(data);         
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store); 
        if (DocumentFragment* fragment = fragmentFromCF_HTML(doc, cf_html))
            return fragment;
    } 
    if (SUCCEEDED(data->GetData(texthtmlFormat(), &store))) {
        //raw html
        UChar* data = (UChar*)GlobalLock(store.hGlobal);
        html = String(data);
        GlobalUnlock(store.hGlobal);      
        ReleaseStgMedium(&store);
        return createFragmentFromMarkup(doc, html, srcURL);
    } 

    return 0;
}

static bool containsFilenames(const IDataObject*)
{
    //FIXME: We'll want to update this once we can produce fragments from files
    return false;
}

static PassRefPtr<DocumentFragment> fragmentFromFilenames(Document*, const IDataObject*)
{
    //FIXME: We should be able to create fragments from files
    return 0;
}

bool DragData::containsColor() const
{
    return false;
}

bool DragData::canSmartReplace() const
{
    return false; 
}

bool DragData::containsCompatibleContent() const
{
    return containsPlainText() || containsURL() 
        || containsHTML(m_platformDragData) 
        || containsFilenames(m_platformDragData) 
        || containsColor();
}

PassRefPtr<DocumentFragment> DragData::asFragment(Document* doc) const
{     
    /*
     * Order is richest format first. On OSX this is:
     * * Web Archive
     * * Filenames
     * * HTML
     * * RTF
     * * TIFF
     * * PICT
     */
        
     if (containsFilenames(m_platformDragData))
         if (DocumentFragment* fragment = fragmentFromFilenames(doc, m_platformDragData).get())
             return fragment;

     if (containsHTML(m_platformDragData))
         if (DocumentFragment* fragment = fragmentFromHTML(doc, m_platformDragData).get())
             return fragment;

     return 0;
}

Color DragData::asColor() const
{
    return Color();
}

}

