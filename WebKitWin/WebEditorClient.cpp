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
#include "WebKitDLL.h"
#include "WebEditorClient.h"

#include "IWebEditingDelegate.h"
#include "IWebURLResponse.h"
#include "WebView.h"
#pragma warning(push, 0)
#include <WebCore/EditCommand.h>
#include <WebCore/HTMLElement.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Range.h>
#pragma warning(pop)

using namespace WebCore;

class WebUndoCommand {
public:
    virtual void execute() = 0;
};

class WebUndoManager {
public:
    WebUndoManager() 
        : m_inUndo(false)
        , m_inRedo(false) {}

    ~WebUndoManager() 
    {
        clearCommands();
    }

    void clearCommands()
    {
        deleteAllValues(m_undoStack);
        m_undoStack.resize(0);

        deleteAllValues(m_redoStack);
        m_redoStack.resize(0);
    }

    void registerCommand(WebUndoCommand* command) 
    {
        if (!m_inUndo && !m_inRedo)
            m_redoStack.clear();

        if (m_inUndo) {
            m_redoStack.append(command);
        } else {
            m_undoStack.append(command);
        }
    }

    void undo() 
    {
        m_inUndo = true;
        undoOrRedo(m_undoStack);
        m_inUndo = false;
    }

    void redo()
    {
        m_inRedo = true;
        undoOrRedo(m_redoStack);
        m_inRedo = false;
    }

    bool canUndo() { return !m_undoStack.isEmpty(); }
    bool canRedo() { return !m_redoStack.isEmpty(); }

private:
    void undoOrRedo(Vector<WebUndoCommand*>& stack)
    {
        ASSERT(!stack.isEmpty());

        OwnPtr<WebUndoCommand> command(stack[static_cast<int>(stack.size()) - 1]);
        stack.removeLast();

        command->execute();
    }

    bool m_inUndo;
    bool m_inRedo;

    Vector<WebUndoCommand*> m_undoStack;
    Vector<WebUndoCommand*> m_redoStack;

};

WebEditorClient::WebEditorClient(WebView* webView)
    : m_webView(webView)
    , m_undoManager(new WebUndoManager)
{
}

void WebEditorClient::ref()
{
    Shared<WebEditorClient>::ref();
}

void WebEditorClient::deref()
{
    Shared<WebEditorClient>::deref();
}

bool WebEditorClient::isContinuousSpellCheckingEnabled()
{
    LOG_NOIMPL();
    return false;
}

bool WebEditorClient::isGrammarCheckingEnabled()
{
    LOG_NOIMPL();
    return false;
}

int WebEditorClient::spellCheckerDocumentTag()
{
    LOG_NOIMPL();
    return 0;
}

bool WebEditorClient::shouldBeginEditing(Range*)
{
    LOG_NOIMPL();
    return true;
}

bool WebEditorClient::shouldEndEditing(Range*)
{
    LOG_NOIMPL();
    return true;
}

void WebEditorClient::didBeginEditing()
{
    LOG_NOIMPL();
}

void WebEditorClient::respondToChangedContents()
{
    LOG_NOIMPL();
}

void WebEditorClient::didEndEditing()
{
    LOG_NOIMPL();
}

bool WebEditorClient::shouldDeleteRange(Range* /*range*/)
{
    BOOL result = false;
    IWebViewEditingDelegate* editingDelegate;
    // FIXME: DOMRange needs to be implemented before anything meaningful can be done here
    IDOMRange* domRange(0);
    if (SUCCEEDED(m_webView->editingDelegate(&editingDelegate))) {
        editingDelegate->shouldDeleteDOMRange(m_webView, domRange, &result);
        editingDelegate->Release();
    }
    return !!result;
}

bool WebEditorClient::shouldInsertNode(Node* /*node*/, Range* /*replacingRange*/, WebViewInsertAction /*givenAction*/)
{ LOG_NOIMPL(); return false; }

//bool WebEditorClient::shouldInsertText(BSTR /*text*/, Range* /*replacingRange*/, WebViewInsertAction /*givenAction*/)
//{ LOG_NOIMPL(); return false; }

//bool WebEditorClient::shouldChangeSelectedRange(Range *currentRange, Range *toProposedRange, SelectionAffinity selectionAffinity, bool stillSelecting)
//{ LOG_NOIMPL(); return false; }

bool WebEditorClient::shouldApplyStyle(CSSStyleDeclaration* /*style*/, Range* /*toElementsInDOMRange*/)
{ LOG_NOIMPL(); return true; }

bool WebEditorClient::shouldChangeTypingStyle(CSSStyleDeclaration* /*currentStyle*/, CSSStyleDeclaration* /*toProposedStyle*/)
{ LOG_NOIMPL(); return false; }

//bool WebEditorClient::doCommandBySelector(SEL selector)
//{ LOG_NOIMPL(); return false; }


void WebEditorClient::webViewDidChangeTypingStyle(WebNotification* /*notification*/)
{  LOG_NOIMPL(); }

void WebEditorClient::webViewDidChangeSelection(WebNotification* /*notification*/)
{  LOG_NOIMPL(); }

//NSUndoManager* WebEditorClient::undoManagerForWebView(WebView *webView)
//{  LOG_NOIMPL(); return NULL; }

bool WebEditorClient::shouldShowDeleteInterface(HTMLElement* /*element*/)
{ LOG_NOIMPL(); return false; }

class WebEditorUndoCommand : public WebUndoCommand 
{
public:
    WebEditorUndoCommand(PassRefPtr<EditCommand> editCommand, bool isUndo)
        : m_editCommand(editCommand)
        , m_isUndo(isUndo) { }

    void execute()
    {
        if (m_isUndo)
            m_editCommand->unapply();
        else
            m_editCommand->reapply();
    }

private:
    RefPtr<EditCommand> m_editCommand;
    bool m_isUndo;
};

void WebEditorClient::registerCommandForUndo(PassRefPtr<EditCommand> command)
{
    m_undoManager->registerCommand(new WebEditorUndoCommand(command, true));
}

void WebEditorClient::registerCommandForRedo(PassRefPtr<EditCommand> command)
{
    m_undoManager->registerCommand(new WebEditorUndoCommand(command, false));
}

void WebEditorClient::clearUndoRedoOperations()
{
    m_undoManager->clearCommands();
}

bool WebEditorClient::canUndo() const
{
    return m_undoManager->canUndo();
}

bool WebEditorClient::canRedo() const
{
    return m_undoManager->canRedo();
}

void WebEditorClient::undo()
{
    return m_undoManager->undo();
}

void WebEditorClient::redo()
{
    return m_undoManager->redo();
}

bool WebEditorClient::selectWordBeforeMenuEvent()
{
    return false;
}

bool WebEditorClient::isEditable()
{
    return false;
}
