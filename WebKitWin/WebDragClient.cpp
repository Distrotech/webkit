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
#include "WebDragClient.h"

#include "WebView.h"
#include <WebCore/DragData.h>

WebDragClient::WebDragClient(WebView* webView)
    : m_webView(webView) 
{
    ASSERT(webView);
}

WebCore::DragDestinationAction WebDragClient::actionMaskForDrag(WebCore::DragData* dragData)
{
    COMPtr<IWebUIDelegate> delegateRef = 0;
    //Default behaviour (eg. no delegate, or callback not implemented) is to allow
    //any action
    WebDragDestinationAction mask = WebDragDestinationActionAny;
    if (SUCCEEDED(m_webView->uiDelegate(&delegateRef)))
        delegateRef->dragDestinationActionMaskForDraggingInfo(m_webView, dragData->platformData(), &mask);

    return (WebCore::DragDestinationAction)mask;
}

void WebDragClient::willPerformDragDestinationAction(WebCore::DragDestinationAction action, WebCore::DragData* dragData)
{
    //Default delegate for willPerformDragDestinationAction has no side effects
    //so we just call the delegate, and don't worry about whether it's implemented
    COMPtr<IWebUIDelegate> delegateRef = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&delegateRef)))
        delegateRef->willPerformDragDestinationAction(m_webView, (WebDragDestinationAction)action, dragData->platformData());
}

void WebDragClient::dragControllerDestroyed()
{
    delete this;
}
