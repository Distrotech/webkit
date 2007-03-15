/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "DumpRenderTree.h"
#include "WaitUntilDoneDelegate.h"

#include <wtf/Platform.h>
#include "LayoutTestController.h"
#include <JavaScriptCore/JavaScriptCore.h>
#include <stdio.h>
#include <WebKit/IWebFramePrivate.h>


HRESULT STDMETHODCALLTYPE WaitUntilDoneDelegate::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegate))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WaitUntilDoneDelegate::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WaitUntilDoneDelegate::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT STDMETHODCALLTYPE WaitUntilDoneDelegate::hasCustomMenuImplementation( 
        /* [retval][out] */ BOOL *hasCustomMenus)
{
    *hasCustomMenus = FALSE;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WaitUntilDoneDelegate::didStartProvisionalLoadForFrame( 
        /* [in] */ IWebView* webView,
        /* [in] */ IWebFrame* frame) 
{
    // Make sure we only set this once per test.  If it gets cleared, and then set again, we might
    // end up doing two dumps for one test.
    if (!topLoadingFrame && !done)
        topLoadingFrame = frame;

    return S_OK; 
}

HRESULT STDMETHODCALLTYPE WaitUntilDoneDelegate::didReceiveTitle( 
        /* [in] */ IWebView *webView,
        /* [in] */ BSTR title,
        /* [in] */ IWebFrame *frame)
{
    return S_OK;
}

void WaitUntilDoneDelegate::locationChangeDone(IWebError*, IWebFrame* frame)
{
    if (frame != topLoadingFrame)
        return;

    topLoadingFrame = 0;

    if (!waitToDump)
        dump();
}

HRESULT STDMETHODCALLTYPE WaitUntilDoneDelegate::didFinishLoadForFrame( 
        /* [in] */ IWebView* webView,
        /* [in] */ IWebFrame* frame)
{
    locationChangeDone(0, frame);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WaitUntilDoneDelegate::didFailLoadWithError( 
    /* [in] */ IWebView* webView,
    /* [in] */ IWebError* error,
    /* [in] */ IWebFrame* forFrame)
{
    locationChangeDone(error, forFrame);
    return S_OK;
}


HRESULT STDMETHODCALLTYPE WaitUntilDoneDelegate::runJavaScriptAlertPanelWithMessage( 
        /* [in] */ IWebView* /*sender*/,
        /* [in] */ BSTR message)
{
    wprintf(L"ALERT: %s\n", message);

    return S_OK;
}


HRESULT STDMETHODCALLTYPE WaitUntilDoneDelegate::windowScriptObjectAvailable( 
        /* [in] */ IWebView *sender,
        /* [in] */ JSContextRef context,
        /* [in] */ JSObjectRef windowObject)
{
    JSStringRef layoutTestControllerStr = JSStringCreateWithUTF8CString("layoutTestController");
    JSValueRef theController = makeLayoutTestController(context);
    JSObjectSetProperty(context, windowObject, layoutTestControllerStr, theController, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, 0);
    JSStringRelease(layoutTestControllerStr);

    return S_OK;
}
