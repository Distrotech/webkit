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

#pragma warning(push, 0)
#include <WebCore/bridge/EditorClient.h>
#pragma warning(pop)

#include "IWebEditingDelegate.h"

class WebView;
class WebNotification;

namespace WebCore {

class CSSStyleDeclaration;
class HTMLElement;
class Node;

class WebEditorClient : public EditorClient
{
public:
    WebEditorClient(WebView*);
    ~WebEditorClient();

    // EditorClient
    virtual bool isContinuousSpellCheckingEnabled();
    virtual bool isGrammarCheckingEnabled();
    virtual int spellCheckerDocumentTag();


    virtual bool shouldBeginEditing(Range*);
    virtual bool shouldEndEditing(Range*);
    virtual void didBeginEditing();
    virtual void respondToChangedContents();
    virtual void didEndEditing();

    bool shouldDeleteRange(Range*);
    bool shouldInsertNode(Node*, Range* replacingRange, WebViewInsertAction givenAction);
//    bool shouldInsertText(BSTR text, Range* replacingRange, WebViewInsertAction givenAction);
//    bool shouldChangeSelectedRange(Range* currentRange, Range* toProposedRange, NSSelectionAffinity selectionAffinity, bool stillSelecting);
    bool shouldApplyStyle(CSSStyleDeclaration*, Range* toElementsInDOMRange);
    bool shouldChangeTypingStyle(CSSStyleDeclaration* currentStyle, CSSStyleDeclaration* toProposedStyle);
//    bool doCommandBySelector(SEL selector);

    void webViewDidChangeTypingStyle(WebNotification*);
    void webViewDidChangeSelection(WebNotification*);
//    NSUndoManager* undoManagerForWebView(WebView*);

    bool shouldShowDeleteInterface(HTMLElement*);

private:
    WebView* m_webView;
};

}

#endif // WebEditorClient_H
