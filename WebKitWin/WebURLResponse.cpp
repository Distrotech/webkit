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

#pragma warning( push, 0 )
#include "DeprecatedString.h"
#include "KURL.h"
#include "ResourceLoader.h"
#include "ResourceLoaderWin.h"
#pragma warning( pop )

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

    DeprecatedString urlStr = loader->url().url();
    DeprecatedString charsetStr = loader->queryMetaData("charset").deprecatedString();

    if (loader && platformResponse) {
        instance->m_url = SysAllocStringLen((LPCOLESTR)urlStr.unicode(), urlStr.length());
        instance->m_textEncodingName = SysAllocStringLen((LPCOLESTR)charsetStr.unicode(), charsetStr.length());
        instance->m_mimeType = SysAllocString(platformResponse->contentType);
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
    /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebURLResponse::textEncodingName( 
    /* [retval][out] */ BSTR* result)
{
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
