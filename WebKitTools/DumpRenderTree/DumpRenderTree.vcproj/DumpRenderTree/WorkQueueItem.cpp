/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "WorkQueueItem.h"

#include <WebCore/COMPtr.h>
#include <WebKit/IWebFrame.h>
#include <WebKit/IWebURLRequest.h>
#include <WebKit/IWebView.h>
#include <WebKit/WebKit.h>

void LoadItem::invoke() const
{
    COMPtr<IWebFrame> targetFrame;
    if (m_target.IsEmpty())
        targetFrame = frame;
    else if (FAILED(frame->findFrameNamed(CComBSTR(m_target), &targetFrame)))
        return;

    COMPtr<IWebURLRequest> request;
    if (FAILED(CoCreateInstance(CLSID_WebURLRequest, 0, CLSCTX_ALL, IID_IWebURLRequest, (void**)&request)))
        return;

    if (FAILED(request->initWithURL(CComBSTR(m_url), WebURLRequestUseProtocolCachePolicy, 60)))
        return;

    targetFrame->loadRequest(request.get());
}

void ReloadItem::invoke() const
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebIBActions> webActions;
    if (SUCCEEDED(webView->QueryInterface(&webActions)))
        webActions->reload(0);
}

void ScriptItem::invoke() const
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    CComBSTR result;
    webView->stringByEvaluatingJavaScriptFromString(CComBSTR(m_script), &result);
}

void BackForwardItem::invoke() const
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    BOOL result;
    if (m_howFar == 1) {
        webView->goForward(&result);
        return;
    }

    if (m_howFar == -1) {
        webView->goBack(&result);
        return;
    }
    
    COMPtr<IWebBackForwardList> bfList;
    if (FAILED(webView->backForwardList(&bfList)))
        return;

    COMPtr<IWebHistoryItem> item;
    if (FAILED(bfList->itemAtIndex(m_howFar, &item)))
        return;

    webView->goToBackForwardItem(item.get(), &result);
}
