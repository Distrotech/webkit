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

#pragma warning(push)
#pragma warning(disable: 4127) // conditional expression is constant
#pragma warning(disable: 4996) // deprecated function

#include "config.h"
#include "WebChromeClient.h"

#include "FloatRect.h"
#include "WebView.h"
#include <WebCore/platform/win/NotImplemented.h>

using namespace WebCore;

PassRefPtr<WebChromeClient> WebChromeClient::create(WebView* webView)
{
    return new WebChromeClient(webView);
}

WebChromeClient::WebChromeClient(WebView* webView)
    : m_webView(webView)
{
}

// FIXME: These should be API calls

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
    LOG_NOIMPL();
    return 0.0;
}

void WebChromeClient::focus()
{
    LOG_NOIMPL();
}

void WebChromeClient::unfocus()
{
    LOG_NOIMPL();
}

Page* WebChromeClient::createWindow(const FrameLoadRequest&)
{
    LOG_NOIMPL();
    return 0;
}

Page* WebChromeClient::createModalDialog(const FrameLoadRequest&)
{
    LOG_NOIMPL();
    return 0;
}

void WebChromeClient::show()
{
    LOG_NOIMPL();
}

bool WebChromeClient::canRunModal()
{
    LOG_NOIMPL();
    return false;
}

void WebChromeClient::runModal()
{
    LOG_NOIMPL();
}

void WebChromeClient::setToolbarsVisible(bool)
{
    LOG_NOIMPL();
}

bool WebChromeClient::toolbarsVisible()
{
    LOG_NOIMPL();
    return false;
}

void WebChromeClient::setStatusbarVisible(bool)
{
    LOG_NOIMPL();
}

bool WebChromeClient::statusbarVisible()
{
    LOG_NOIMPL();
    return false;
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
    LOG_NOIMPL();
}

bool WebChromeClient::menubarVisible()
{
    LOG_NOIMPL();
    return false;
}

void WebChromeClient::setResizable(bool)
{
    LOG_NOIMPL();
}

#pragma warning(pop)
