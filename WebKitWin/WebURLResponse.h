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

#ifndef WebURLResponse_H
#define WebURLResponse_H

#include "config.h"

#include "IWebURLResponse.h"

#include <WebCore/platform/network/ResourceLoaderClient.h>

class WebURLResponse : public IWebURLResponse
{
public:
    static WebURLResponse* createInstance(WebCore::ResourceLoader* loader, WebCore::PlatformResponse platformResponse);
protected:
    WebURLResponse();
    ~WebURLResponse();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebURLRequest
    virtual HRESULT STDMETHODCALLTYPE expectedContentLength( 
        /* [retval][out] */ long long *result);
    
    virtual HRESULT STDMETHODCALLTYPE initWithURL( 
        /* [in] */ BSTR url,
        /* [in] */ BSTR mimeType,
        /* [in] */ int expectedContentLength,
        /* [in] */ BSTR textEncodingName);
    
    virtual HRESULT STDMETHODCALLTYPE MIMEType( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE suggestedFilename( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE textEncodingName( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE URL( 
        /* [retval][out] */ BSTR *result);

protected:
    HRESULT suggestedFileExtension(BSTR* result);

protected:
    ULONG                       m_refCount;
    BSTR                        m_url;
    BSTR                        m_mimeType;
    BSTR                        m_textEncodingName;
    DWORD                       m_expectedContentLength;
};

#endif
