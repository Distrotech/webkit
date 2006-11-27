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
#include "WebChromeClient.h"

#include "WebMutableURLRequest.h"
#include "WebView.h"
#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/ContextMenu.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/NotImplemented.h>
#pragma warning(pop)

using namespace WebCore;

PassRefPtr<WebChromeClient> WebChromeClient::create(WebView* webView)
{
    return new WebChromeClient(webView);
}

WebChromeClient::WebChromeClient(WebView* webView)
    : m_webView(webView)
{
}

void WebChromeClient::ref()
{
    Shared<WebChromeClient>::ref();
}

void WebChromeClient::deref()
{
    Shared<WebChromeClient>::deref();
}

// FIXME: These functions should make delegate calls instead of sizing by themselves.

void WebChromeClient::setWindowRect(const FloatRect& r)
{
    HWND window;
    if (SUCCEEDED(m_webView->hostWindow(&window)))
        MoveWindow(window, static_cast<int>(r.x()), static_cast<int>(r.y()), static_cast<int>(r.width()), static_cast<int>(r.height()), true);
}

FloatRect WebChromeClient::windowRect()
{
    HWND window;
    if (SUCCEEDED(m_webView->hostWindow(&window))) {
        RECT rect;
        GetWindowRect(window, &rect);
        return FloatRect(rect);
    }
    return FloatRect();
}

FloatRect WebChromeClient::pageRect()
{
    LOG_NOIMPL();
    return FloatRect();
}

float WebChromeClient::scaleFactor()
{
    // Windows doesn't support UI scaling.
    return 1.0;
}

void WebChromeClient::focus()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        uiDelegate->webViewFocus(m_webView);
        uiDelegate->Release();
    }
}

void WebChromeClient::unfocus()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        uiDelegate->webViewUnfocus(m_webView);
        uiDelegate->Release();
    }
}

Page* WebChromeClient::createWindow(const FrameLoadRequest& frameLoadRequest)
{
    Page* page = 0;
    IWebMutableURLRequest* request = WebMutableURLRequest::createInstance(frameLoadRequest.resourceRequest());

    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        IWebView* webView = 0;
        if (SUCCEEDED(uiDelegate->createWebViewWithRequest(m_webView, request, &webView)) && webView) {
            page = core(webView);
            webView->Release();
        }
    
        uiDelegate->Release();
    }

    request->Release();
    return page;
}

Page* WebChromeClient::createModalDialog(const FrameLoadRequest&)
{
    Page* page = 0;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        LOG_NOIMPL();
        uiDelegate->Release();
    }
    return page;
}

void WebChromeClient::show()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        uiDelegate->webViewShow(m_webView);
        uiDelegate->Release();
    }
}

bool WebChromeClient::canRunModal()
{
    bool result = false;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        LOG_NOIMPL();
        uiDelegate->Release();
    }
    return result;
}

void WebChromeClient::runModal()
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        LOG_NOIMPL();
        uiDelegate->Release();
    }
}

void WebChromeClient::setToolbarsVisible(bool visible)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        uiDelegate->setToolbarsVisible(m_webView, visible);
        uiDelegate->Release();
    }
}

bool WebChromeClient::toolbarsVisible()
{
    BOOL result = false;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        uiDelegate->webViewAreToolbarsVisible(m_webView, &result);
        uiDelegate->Release();
    }
    return result != false;
}

void WebChromeClient::setStatusbarVisible(bool visible)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        uiDelegate->setStatusBarVisible(m_webView, visible);
        uiDelegate->Release();
    }
}

bool WebChromeClient::statusbarVisible()
{
    BOOL result = false;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        uiDelegate->webViewIsStatusBarVisible(m_webView, &result);
        uiDelegate->Release();
    }
    return result != false;
}

void WebChromeClient::setScrollbarsVisible(bool)
{
    LOG_NOIMPL();
}

bool WebChromeClient::scrollbarsVisible()
{
    LOG_NOIMPL();
    return false;
}

void WebChromeClient::setMenubarVisible(bool)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        LOG_NOIMPL();
        uiDelegate->Release();
    }
}

bool WebChromeClient::menubarVisible()
{
    bool result = false;
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        LOG_NOIMPL();
        uiDelegate->Release();
    }
    return result;
}

void WebChromeClient::setResizable(bool resizable)
{
    IWebUIDelegate* uiDelegate = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        uiDelegate->setResizable(m_webView, resizable);
        uiDelegate->Release();
    }
}

void WebChromeClient::addMessageToConsole(const String& message, unsigned line, const String& url)
{
    IWebUIDelegate* ui;
    if (SUCCEEDED(m_webView->uiDelegate(&ui)) && ui) {
        IWebUIDelegatePrivate* uiPrivate;
        if (SUCCEEDED(ui->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&uiPrivate))) {
            uiPrivate->webViewAddMessageToConsole(m_webView, BString(message), line, BString(url));
            uiPrivate->Release();
        }
        ui->Release();
    }
}

