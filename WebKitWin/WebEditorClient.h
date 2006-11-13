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

#ifndef WebEditorClient_H
#define WebEditorClient_H

#include "IWebEditingDelegate.h"
#pragma warning(push, 0)
#include <WebCore/EditorClient.h>
#include <WebCore/Shared.h>
#pragma warning(pop)

class WebView;
class WebNotification;

namespace WebCore {

class CSSStyleDeclaration;
class HTMLElement;
class Node;

}

class WebEditorClient : public WebCore::EditorClient, public WebCore::Shared<WebEditorClient> {
public:
    WebEditorClient(WebView*);

    virtual void ref();
    virtual void deref();

    virtual bool isContinuousSpellCheckingEnabled();
    virtual bool isGrammarCheckingEnabled();
    virtual int spellCheckerDocumentTag();


    virtual bool shouldBeginEditing(WebCore::Range*);
    virtual bool shouldEndEditing(WebCore::Range*);

    virtual void didBeginEditing();
    virtual void didEndEditing();

    virtual void respondToChangedContents();

    bool shouldShowDeleteInterface(WebCore::HTMLElement*);
    bool shouldDeleteRange(WebCore::Range*);

    bool shouldInsertNode(WebCore::Node*, WebCore::Range* replacingRange, WebViewInsertAction);
    bool shouldApplyStyle(WebCore::CSSStyleDeclaration*, WebCore::Range*);
    bool shouldChangeTypingStyle(WebCore::CSSStyleDeclaration* currentStyle, WebCore::CSSStyleDeclaration* toProposedStyle);

    void webViewDidChangeTypingStyle(WebNotification*);
    void webViewDidChangeSelection(WebNotification*);

private:
    WebView* m_webView;
};

#endif // WebEditorClient_H
