/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
 *
 * All rights reserved.
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

#include "EditCommand.h"
#include "Editor.h"
#include "EditorClientWx.h"
#include "PlatformString.h"
#include "Frame.h"
#include "FrameView.h"
#include "PlatformKeyboardEvent.h"
#include "KeyboardEvent.h"
#include "KeyboardCodes.h"
#include "Page.h"
#include "SelectionController.h"
#include "FocusController.h"
#include "WebView.h"

#include <stdio.h>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d (%s)\n", __FILE__, __LINE__, __FUNCTION__); } while(0)

namespace WebCore {

EditorClientWx::~EditorClientWx()
{
    m_page = NULL;
}

void EditorClientWx::setPage(Page* page)
{
    m_page = page;
}

void EditorClientWx::pageDestroyed()
{
    notImplemented();
}

bool EditorClientWx::shouldDeleteRange(Range*)
{
    notImplemented();
    return true;
}

bool EditorClientWx::shouldShowDeleteInterface(HTMLElement*)
{
    notImplemented();
    return false;
}

bool EditorClientWx::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientWx::isContinuousSpellCheckingEnabled()
{
    notImplemented();
    return false;
}

void EditorClientWx::toggleContinuousSpellChecking()
{
    notImplemented();
}

bool EditorClientWx::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

void EditorClientWx::toggleGrammarChecking()
{
    notImplemented();
}

int EditorClientWx::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClientWx::selectWordBeforeMenuEvent()
{
    notImplemented();
    return false;
}

bool EditorClientWx::isEditable()
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();

    if (frame)
    {
        wxWebView* webKitWin = dynamic_cast<wxWebView*>(frame->view()->nativeWindow());
        if (webKitWin)
        {
            return webKitWin->IsEditable();
        }
    }
    return false;
}

bool EditorClientWx::shouldBeginEditing(Range*)
{
    //notImplemented();
    return true;
}

bool EditorClientWx::shouldEndEditing(Range*)
{
    //notImplemented();
    return true;
}

bool EditorClientWx::shouldInsertNode(Node*, Range*,
                                       EditorInsertAction)
{
    //notImplemented();
    return true;
}

bool EditorClientWx::shouldInsertText(String, Range*,
                                       EditorInsertAction)
{
    //notImplemented();
    return true;
}

bool EditorClientWx::shouldApplyStyle(CSSStyleDeclaration*,
                                       Range*)
{
    //notImplemented();
    return true;
}

bool EditorClientWx::shouldChangeSelectedRange(Range* fromRange, Range* toRange, 
                                EAffinity, bool stillSelecting)
{
    //notImplemented();
    return true;
}

void EditorClientWx::didBeginEditing()
{
    notImplemented();
}

void EditorClientWx::respondToChangedContents()
{
    notImplemented();
}

void EditorClientWx::didEndEditing()
{
    notImplemented();
}

void EditorClientWx::didWriteSelectionToPasteboard()
{
    notImplemented();
}

void EditorClientWx::didSetSelectionTypesForPasteboard()
{
    notImplemented();
}

void EditorClientWx::registerCommandForUndo(PassRefPtr<EditCommand>)
{
    notImplemented();
}

void EditorClientWx::registerCommandForRedo(PassRefPtr<EditCommand>)
{
    notImplemented();
}

void EditorClientWx::clearUndoRedoOperations()
{
    notImplemented();
}

bool EditorClientWx::canUndo() const
{
    notImplemented();
    return false;
}

bool EditorClientWx::canRedo() const
{
    notImplemented();
    return false;
}

void EditorClientWx::undo()
{
    notImplemented();
}

void EditorClientWx::redo()
{
    notImplemented();
}

void EditorClientWx::handleInputMethodKeypress(KeyboardEvent* event)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    const PlatformKeyboardEvent* kevent = event->keyEvent();
    Node* start = frame->selectionController()->start().node();
    if (start && start->isContentEditable()) 
    {
            frame->editor()->insertText(kevent->text(), event);
    }
    event->setDefaultHandled();
}

void EditorClientWx::handleKeypress(KeyboardEvent* event)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    const PlatformKeyboardEvent* kevent = event->keyEvent();
    if (!kevent->isKeyUp()) {
        Node* start = frame->selectionController()->start().node();
        if (start && start->isContentEditable()) {
            switch(kevent->WindowsKeyCode()) {
            case VK_BACK:
                fprintf(stderr, "backspace hit!\n");
                frame->editor()->deleteWithDirection(SelectionController::BACKWARD,
                                                     CharacterGranularity, false, true);
                break;
            case VK_DELETE:
                fprintf(stderr, "delete hit!\n");
                frame->editor()->deleteWithDirection(SelectionController::FORWARD,
                                                     CharacterGranularity, false, true);
                break;
            case VK_LEFT:
                frame->editor()->execCommand("MoveLeft");
                break;
            case VK_RIGHT:
                frame->editor()->execCommand("MoveRight");
                break;
            case VK_UP:
                frame->editor()->execCommand("MoveUp");
                break;
            case VK_DOWN:
                frame->editor()->execCommand("MoveDown");
                break;
            default:
                break; //frame->editor()->insertText(kevent->text(), event);
            }
            event->setDefaultHandled();
        }
    }
}

void EditorClientWx::textFieldDidBeginEditing(Element*)
{
    notImplemented();
}

void EditorClientWx::textFieldDidEndEditing(Element*)
{
    notImplemented();
}

void EditorClientWx::textDidChangeInTextField(Element*)
{
    notImplemented();
}

bool EditorClientWx::doTextFieldCommandFromEvent(Element*, KeyboardEvent*)
{
    notImplemented();
    return false;
}

void EditorClientWx::textWillBeDeletedInTextField(Element*)
{
    notImplemented();
}

void EditorClientWx::textDidChangeInTextArea(Element*)
{
    notImplemented();
}

void EditorClientWx::respondToChangedSelection()
{
    notImplemented();
}

}
