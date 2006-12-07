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
#include "WebContextMenuClient.h"

#include "WebElementPropertyBag.h"
#include "WebView.h"

#pragma warning(push, 0)
#include <WebCore/ContextMenu.h>
#include <WebCore/NotImplemented.h>
#pragma warning(pop)

using namespace WebCore;

WebContextMenuClient::WebContextMenuClient(WebView* webView)
    : m_webView(webView)
{
}

void WebContextMenuClient::contextMenuDestroyed()
{
    delete this;
}

void WebContextMenuClient::addCustomContextMenuItems(ContextMenu* menu)
{
    IWebUIDelegate* uiDelegate = 0;
    if (FAILED(m_webView->uiDelegate(&uiDelegate)))
        return;

    ASSERT(uiDelegate);

    HMENU newMenu = 0;
    WebElementPropertyBag propertyBag(menu->hitTestResult());
    uiDelegate->contextMenuItemsForElement(m_webView, &propertyBag, menu->platformDescription(), &newMenu);
    uiDelegate->Release();

    menu->setPlatformDescription(newMenu);
}

void WebContextMenuClient::contextMenuItemSelected(const ContextMenuItem* item)
{
    ASSERT(item->parentMenu());
    ASSERT(item->type() == ActionType);
    
    if (!item->platformDescription())
        return;

    IWebUIDelegate* uiDelegate = 0;
    if (FAILED(m_webView->uiDelegate(&uiDelegate)))
        return;

    ASSERT(uiDelegate);

    WebElementPropertyBag propertyBag(item->parentMenu()->hitTestResult());
            
    uiDelegate->contextMenuItemSelected(m_webView, item->platformDescription(), &propertyBag);
    uiDelegate->Release();
}

void WebContextMenuClient::copyLinkToClipboard(const HitTestResult&)
{
    LOG_NOIMPL();
}

void WebContextMenuClient::downloadURL(const KURL&)
{
    LOG_NOIMPL();
}

void WebContextMenuClient::copyImageToClipboard(const HitTestResult&)
{
    LOG_NOIMPL();
}

void WebContextMenuClient::lookUpInDictionary(Frame*)
{
    LOG_NOIMPL();
}
