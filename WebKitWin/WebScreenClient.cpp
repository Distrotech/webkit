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

#include "WebScreenClient.h"

#include "FloatRect.h"
#include "WebChromeClient.h"
#include "WebView.h"
#include <WebCore/platform/win/NotImplemented.h>
#include <WebCore/page/Screen.h>
#include <wtf/PassRefPtr.h>

using namespace WebCore;

PassRefPtr<WebScreenClient> WebScreenClient::create(WebView* webView)
{
    return new WebScreenClient(webView);
}

WebScreenClient::WebScreenClient(WebView* webView)
    : m_webView(webView)
{
}

int WebScreenClient::depth()
{
    LOG_NOIMPL();
    return 0;
}

int WebScreenClient::depthPerComponent()
{
    LOG_NOIMPL();
    return 0;
}

bool WebScreenClient::isMonochrome()
{
    LOG_NOIMPL();
    return false;
}

FloatRect WebScreenClient::rect()
{
    LOG_NOIMPL();
    return FloatRect();
}

FloatRect WebScreenClient::usableRect()
{
    LOG_NOIMPL();
    return FloatRect();
}

#pragma warning(pop)
