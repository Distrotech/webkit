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

#include "WebKitDLL.h"
#include "WebURLResponse.h"
#include "MarshallingHelpers.h"

#include <wtf/platform.h>

#pragma warning( push, 0 )
#include <WebCore/platform/DeprecatedString.h>
#include <WebCore/platform/KURL.h>
#include <WebCore/platform/network/ResourceLoader.h>
#include <WebCore/platform/network/win/ResourceLoaderWin.h>
#pragma warning( pop )

#include <shlobj.h>
#include <shlwapi.h>
#include <tchar.h>

using namespace WebCore;

// IWebURLResponse ----------------------------------------------------------------

WebURLResponse::WebURLResponse()
: m_refCount(0)
, m_url(0)
, m_mimeType(0)
, m_textEncodingName(0)
{
    gClassCount++;
}

WebURLResponse::~WebURLResponse()
{
    SysFreeString(m_url);
    SysFreeString(m_mimeType);
    SysFreeString(m_textEncodingName);
    gClassCount--;
}

WebURLResponse* WebURLResponse::createInstance(ResourceLoader* loader, PlatformResponse platformResponse)
{
    WebURLResponse* instance = new WebURLResponse();
    instance->AddRef();

    if (loader && platformResponse) {
        // REVIEW-FIXME : likely we need to add mime type sniffing here (unless that is done within CFURLResponseGetMIMEType)
#if USE(CFNETWORK)
        CFStringRef mimeType = CFURLResponseGetMIMEType(platformResponse);
        CFStringRef textEncoding = CFURLResponseGetTextEncodingName(platformResponse);
#endif
        DeprecatedString urlStr = loader->url().url();
        instance->m_url = SysAllocStringLen((LPCOLESTR)urlStr.unicode(), urlStr.length());
#if USE(CFNETWORK)
        instance->m_mimeType = MarshallingHelpers::CFStringRefToBSTR(mimeType);
        if (textEncoding)
            instance->m_textEncodingName = MarshallingHelpers::CFStringRefToBSTR(textEncoding);
        else {
            String encoding = loader->responseEncoding();
            instance->m_textEncodingName = SysAllocStringLen((LPCOLESTR)encoding.characters(), encoding.length());
        }
#else
        LPCTSTR separator = _tcschr(platformResponse->contentType, TEXT(';'));        
        instance->m_mimeType = (separator) ? SysAllocStringLen(platformResponse->contentType, (UINT)(separator-platformResponse->contentType)) : SysAllocString(platformResponse->contentType);
        if (separator) {
            // FIXME (if we care about WinInet).  Parse charset out of Content-Type
        }
        String encoding = loader->responseEncoding();
        instance->m_textEncodingName = SysAllocStringLen((LPCOLESTR)encoding.characters(), encoding.length());
#endif
    }

    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebURLResponse::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebURLResponse*>(this);
    else if (IsEqualGUID(riid, IID_IWebURLResponse))
        *ppvObject = static_cast<IWebURLResponse*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebURLResponse::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebURLResponse::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebURLResponse --------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebURLResponse::expectedContentLength( 
    /* [retval][out] */ long long* result)
{
    *result = m_expectedContentLength;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLResponse::initWithURL( 
    /* [in] */ BSTR url,
    /* [in] */ BSTR mimeType,
    /* [in] */ int expectedContentLength,
    /* [in] */ BSTR textEncodingName)
{
    if (m_url || m_mimeType || m_textEncodingName)
        return E_FAIL;

    m_url = SysAllocString(url);
    if (url && !m_url)
        return E_OUTOFMEMORY;
    m_mimeType = SysAllocString(mimeType);
    if (mimeType && !m_mimeType)
        return E_OUTOFMEMORY;
    m_expectedContentLength = expectedContentLength;
    m_textEncodingName = textEncodingName;
    if (textEncodingName && !m_textEncodingName)
        return E_OUTOFMEMORY;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLResponse::MIMEType( 
    /* [retval][out] */ BSTR* result)
{
    *result = SysAllocString(m_mimeType);
    if (m_mimeType && !*result)
        return E_OUTOFMEMORY;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLResponse::suggestedFilename( 
    /* [retval][out] */ BSTR* result)
{
    if (!m_url)
        return E_FAIL;

    TCHAR path[MAX_PATH];
    *path = 0;
    DWORD pathLength = ARRAYSIZE(path);
    if (FAILED(PathCreateFromUrl(m_url, path, &pathLength, 0))) {
        TCHAR* lastSlash = 0;
        TCHAR* walkURL;
        for (walkURL = m_url; *walkURL; walkURL++) {
            if (*walkURL == TEXT('/'))
                lastSlash = walkURL;
            else if (*walkURL == TEXT('#') || *walkURL == TEXT('?'))
                break;
        }
        TCHAR* fileNameStart = lastSlash;
        if (fileNameStart)
            fileNameStart++;
        else
            fileNameStart = m_url;

        if (*lastSlash)
            lastSlash++;
        if (_tcsncpy_s(path, sizeof(path)/sizeof(path[0]), fileNameStart, (size_t)(walkURL-fileNameStart)))
            return E_OUTOFMEMORY;
    }

    if (PathCleanupSpec(0, path) < 0)
        return E_FAIL;

    if (!*path)
        _tcscpy(path, TEXT("*")); // caller will substitute page title in this case

    BSTR extension;
    if (SUCCEEDED(suggestedFileExtension(&extension))) {
        PathRenameExtension(path, extension);
        SysFreeString(extension);
    }

    *result = SysAllocString(path);
    if (!result)
        return E_OUTOFMEMORY;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLResponse::textEncodingName( 
    /* [retval][out] */ BSTR* result)
{
    if (!result)
        return E_INVALIDARG;

    *result = SysAllocString(m_textEncodingName);
    if (m_textEncodingName && !*result)
        return E_OUTOFMEMORY;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLResponse::URL( 
    /* [retval][out] */ BSTR* result)
{
    *result = SysAllocString(m_url);
    if (m_url && !*result)
        return E_OUTOFMEMORY;

    return S_OK;
}

// WebURLResponse -------------------------------------------------------------

HRESULT WebURLResponse::suggestedFileExtension(BSTR *result)
{
    if (!result)
        return E_POINTER;

    *result = 0;

    if (!m_mimeType || !*m_mimeType)
        return E_FAIL;

    HKEY key;
    LONG err = RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("MIME\\Database\\Content Type"), 0, KEY_QUERY_VALUE, &key);
    if (!err) {
        HKEY subKey;
        err = RegOpenKeyEx(key, m_mimeType, 0, KEY_QUERY_VALUE, &subKey);
        if (!err) {
            DWORD keyType = REG_SZ;
            TCHAR extension[MAX_PATH];
            DWORD keySize = sizeof(extension)/sizeof(extension[0]);
            err = RegQueryValueEx(subKey, TEXT("Extension"), 0, &keyType, (LPBYTE)extension, &keySize);
            if (!err && keyType != REG_SZ)
                err = ERROR_INVALID_DATA;
            if (err) {
                // fallback handlers
                if (!_tcscmp(m_mimeType, TEXT("text/html"))) {
                    _tcscpy(extension, TEXT(".html"));
                    err = 0;
                } else if (!_tcscmp(m_mimeType, TEXT("application/xhtml+xml"))) {
                    _tcscpy(extension, TEXT(".xhtml"));
                    err = 0;
                } else if (!_tcscmp(m_mimeType, TEXT("image/svg+xml"))) {
                    _tcscpy(extension, TEXT(".svg"));
                    err = 0;
                }
            }
            if (!err) {
                *result = SysAllocString(extension);
                if (!*result)
                    err = ERROR_OUTOFMEMORY;
            }
            RegCloseKey(subKey);
        }
        RegCloseKey(key);
    }

    return HRESULT_FROM_WIN32(err);
}
