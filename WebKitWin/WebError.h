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

#ifndef WebError_h
#define WebError_h

#include "IWebError.h"

#pragma warning(push, 0)
#include <WebCore/ResourceError.h>
#pragma warning(pop)

// {CC671729-C184-4149-8CFA-4C1A2ABC4399}
DEFINE_GUID(IID_WebError, 0xcc671729, 0xc184, 0x4149, 0x8c, 0xfa, 0x4c, 0x1a, 0x2a, 0xbc, 0x43, 0x99);

class WebError : public IWebError {
public:
    static WebError* createInstance(const WebCore::ResourceError&);
protected:
    WebError(const WebCore::ResourceError&);
    ~WebError();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebError
    virtual HRESULT STDMETHODCALLTYPE errorWithDomain( 
        /* [in] */ BSTR domain,
        /* [in] */ int code,
        /* [in] */ IPropertyBag *dict);
        
    virtual HRESULT STDMETHODCALLTYPE code( 
        /* [retval][out] */ int *result);
        
    virtual HRESULT STDMETHODCALLTYPE domain( 
        /* [retval][out] */ BSTR *result);
        
    virtual HRESULT STDMETHODCALLTYPE initWithDomain( 
        /* [in] */ BSTR domain,
        /* [in] */ int code,
        /* [in] */ IPropertyBag *dict);
        
    virtual HRESULT STDMETHODCALLTYPE localizedDescription( 
        /* [retval][out] */ BSTR *result);
        
    virtual HRESULT STDMETHODCALLTYPE localizedFailureReason( 
        /* [retval][out] */ BSTR *result);
        
    virtual HRESULT STDMETHODCALLTYPE localizedRecoveryOptions( 
        /* [out] */ int *cOptions,
        /* [retval][out] */ BSTR **options);
        
    virtual HRESULT STDMETHODCALLTYPE localizedRecoverySuggestion( 
        /* [retval][out] */ BSTR *result);
       
    virtual HRESULT STDMETHODCALLTYPE recoverAttempter( 
        /* [retval][out] */ IUnknown **result);
        
    virtual HRESULT STDMETHODCALLTYPE userInfo( 
        /* [retval][out] */ IPropertyBag **result);
        
    const WebCore::ResourceError& resourceError() const;

private:
    ULONG m_refCount;
    WebCore::ResourceError m_error;
};

#endif
