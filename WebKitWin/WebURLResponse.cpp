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
#include <WebCore/BString.h>
#include <WebCore/DeprecatedString.h>
#include <WebCore/KURL.h>
#include <WebCore/ResourceHandle.h>
#pragma warning( pop )

#include <shlobj.h>
#include <shlwapi.h>
#include <tchar.h>

using namespace WebCore;

// IWebURLResponse ----------------------------------------------------------------

WebURLResponse::WebURLResponse()
{
    gClassCount++;
}

WebURLResponse::~WebURLResponse()
{
    gClassCount--;
}

WebURLResponse* WebURLResponse::createInstance(const ResourceResponse& response)
{
    WebURLResponse* instance = new WebURLResponse();
    instance->AddRef();
    instance->m_response = response;

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
    *result = m_response.expectedContentLength();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLResponse::initWithURL( 
    /* [in] */ BSTR url,
    /* [in] */ BSTR mimeType,
    /* [in] */ int expectedContentLength,
    /* [in] */ BSTR textEncodingName)
{
    m_response = ResourceResponse(String(url).deprecatedString(), String(mimeType), expectedContentLength, String(textEncodingName), String());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLResponse::MIMEType( 
    /* [retval][out] */ BSTR* result)
{
    BString mimeType(m_response.mimeType());
    *result = mimeType.release();
    if (!m_response.mimeType().isNull() && !*result)
        return E_OUTOFMEMORY;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLResponse::suggestedFilename( 
    /* [retval][out] */ BSTR* result)
{
    if (m_response.url().isEmpty())
        return E_FAIL;

    // FIXME: this logic should be at the network layer
    BString url(m_response.url().url());
    TCHAR path[MAX_PATH];
    *path = 0;
    DWORD pathLength = ARRAYSIZE(path);
    if (FAILED(PathCreateFromUrl(url, path, &pathLength, 0))) {
        TCHAR* lastSlash = 0;
        TCHAR* walkURL;
        for (walkURL = url; *walkURL; walkURL++) {
            if (*walkURL == TEXT('/'))
                lastSlash = walkURL;
            else if (*walkURL == TEXT('#') || *walkURL == TEXT('?'))
                break;
        }
        TCHAR* fileNameStart = lastSlash;
        if (fileNameStart)
            fileNameStart++;
        else
            fileNameStart = url;

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

    BString textEncodingName(m_response.textEncodingName());
    *result = textEncodingName.release();
    if (!m_response.textEncodingName().isNull() && !*result)
        return E_OUTOFMEMORY;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebURLResponse::URL( 
    /* [retval][out] */ BSTR* result)
{
    if (!result)
        return E_INVALIDARG;

    BString url(m_response.url().url());
    *result = url.release();
    if (!m_response.url().isEmpty() && !*result)
        return E_OUTOFMEMORY;

    return S_OK;
}

// WebURLResponse -------------------------------------------------------------

HRESULT WebURLResponse::suggestedFileExtension(BSTR *result)
{
    if (!result)
        return E_POINTER;

    *result = 0;

    if (m_response.mimeType().isEmpty())
        return E_FAIL;

    BString mimeType(m_response.mimeType());
    HKEY key;
    LONG err = RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("MIME\\Database\\Content Type"), 0, KEY_QUERY_VALUE, &key);
    if (!err) {
        HKEY subKey;
        err = RegOpenKeyEx(key, mimeType, 0, KEY_QUERY_VALUE, &subKey);
        if (!err) {
            DWORD keyType = REG_SZ;
            TCHAR extension[MAX_PATH];
            DWORD keySize = sizeof(extension)/sizeof(extension[0]);
            err = RegQueryValueEx(subKey, TEXT("Extension"), 0, &keyType, (LPBYTE)extension, &keySize);
            if (!err && keyType != REG_SZ)
                err = ERROR_INVALID_DATA;
            if (err) {
                // fallback handlers
                if (!_tcscmp(mimeType, TEXT("text/html"))) {
                    _tcscpy(extension, TEXT(".html"));
                    err = 0;
                } else if (!_tcscmp(mimeType, TEXT("application/xhtml+xml"))) {
                    _tcscpy(extension, TEXT(".xhtml"));
                    err = 0;
                } else if (!_tcscmp(mimeType, TEXT("image/svg+xml"))) {
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
