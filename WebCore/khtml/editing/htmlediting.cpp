/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include "htmlediting.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#endif

#include "dom_elementimpl.h"
#include "dom_nodeimpl.h"
#include "dom_textimpl.h"
#include "khtmlview.h"
#include "khtml_part.h"
#include "render_object.h"

using DOM::DocumentImpl;
using DOM::DOMString;
using DOM::ElementImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::Range;
using DOM::TextImpl;

using khtml::DeleteTextCommand;
using khtml::EditCommand;
using khtml::EditCommandID;
using khtml::InputTextCommand;

//------------------------------------------------------------------------------------------
// EditCommand

void EditCommand::notifyChanged(NodeImpl *node) const
{
    node->setChanged(true);
    if (node->renderer())
        node->renderer()->setNeedsLayoutAndMinMaxRecalc();
}

//------------------------------------------------------------------------------------------
// InputTextCommand

EditCommandID InputTextCommand::commandID() const { return InputTextCommandID; }

InputTextCommand::InputTextCommand(const Range &selection, const DOMString &text) 
    : EditCommand(selection)
{
    if (text.isEmpty()) {
#if APPLE_CHANGES
        ERROR("InputTextCommand constructed with zero-length string");
#endif
        m_text = "";
    }
    else {
        m_text = text; 
    }
}

bool InputTextCommand::isLineBreak() const
{
    return !m_text.isEmpty() && (m_text[0] == '\n' || m_text[0] == '\r');
}

bool InputTextCommand::isSpace() const
{
    return !m_text.isEmpty() && (m_text[0] == ' ');
}

bool InputTextCommand::applyToDocument(DocumentImpl *doc)
{
    KHTMLView *view = doc->view();
    if (!view)
        return false;
    KHTMLPart *part = view->part();
    if (!part)
        return false;

    NodeImpl *caretNode = selection().startContainer().handle();
    if (!caretNode->isTextNode())
        return false;
    
    // Delete the current selection
    if (view->caretOverrides()) {
        // EDIT FIXME: need to save the contents for redo
        selection().deleteContents();
        part->setSelection(Range(caretNode, selection().startOffset(), caretNode, selection().startOffset()));
    }
    
    TextImpl *textNode = static_cast<TextImpl *>(caretNode);
    int exceptionCode;
    
    if (isLineBreak()) {
        TextImpl *textBeforeNode = doc->createTextNode(textNode->substringData(0, selection().startOffset(), exceptionCode));
        textNode->deleteData(0, selection().startOffset(), exceptionCode);
        ElementImpl *breakNode = doc->createHTMLElement("BR", exceptionCode);
        textNode->parentNode()->insertBefore(textBeforeNode, textNode, exceptionCode);
        textNode->parentNode()->insertBefore(breakNode, textNode, exceptionCode);
        textBeforeNode->deref();
        breakNode->deref();
        
        // Set the cursor at the beginning of the node after the split.
        part->setSelection(Range(caretNode, 0, caretNode, 0));
    }
    else {
        textNode->insertData(selection().startOffset(), text(), exceptionCode);
        // EDIT FIXME: this is a hack for now
        // advance the cursor
        int textLength = text().length();
        part->setSelection(Range(selection().startContainer(), selection().startOffset() + textLength, 
                                    selection().startContainer(), selection().startOffset() + textLength));
    }
    
    notifyChanged(textNode);
    return true;
}

bool InputTextCommand::canUndo() const
{
    return true;
}

//------------------------------------------------------------------------------------------
// DeleteTextCommand

EditCommandID DeleteTextCommand::commandID() const { return DeleteTextCommandID; }

DeleteTextCommand::DeleteTextCommand(const Range &selection) 
    : EditCommand(selection)
{
}

bool DeleteTextCommand::applyToDocument(DocumentImpl *doc)
{
    KHTMLView *view = doc->view();
    if (!view)
        return false;
    KHTMLPart *part = view->part();
    if (!part)
        return false;
    
    NodeImpl *caretNode = selection().startContainer().handle();
    if (!caretNode->isTextNode())
        return false;
    
    // Delete the current selection
    if (!selection().collapsed()) {
        // EDIT FIXME: need to save the contents for redo
        selection().deleteContents();
        part->setSelection(Range(caretNode, selection().startOffset(), caretNode, selection().startOffset()));
    }
    else {
        // EDIT FIXME: this is a hack for now
        TextImpl *textNode = static_cast<TextImpl *>(caretNode);
        int exceptionCode;
        int offset = selection().startOffset() - 1;
        fprintf(stderr, "delete offset: %d\n", offset);
        if (offset >= 0) {
            // Delete character at cursor
            textNode->deleteData(offset, 1, exceptionCode);
            part->setSelection(Range(selection().startContainer(), selection().startOffset() - 1, 
                                     selection().startContainer(), selection().startOffset() - 1));
        }
        else if (textNode->previousSibling()) {
            // look at previous sibling
            NodeImpl *previousSibling = textNode->previousSibling();
            if (previousSibling->isTextNode()) {
                fprintf(stderr, "previousSibling is text node\n");
                // delete last character in text node
                TextImpl *previousTextNode = static_cast<TextImpl *>(previousSibling);
                previousTextNode->deleteData(previousTextNode->length() - 1, 1, exceptionCode);
                // leave caret right where it is
            }
            else if (previousSibling->isHTMLBRElement()) {
                // remove BR
                fprintf(stderr, "remove BR\n");
                textNode->parentNode()->removeChild(previousSibling, exceptionCode);
            }
            else {
                fprintf(stderr, "delete fall through\n");
            }
        }
        else {
            fprintf(stderr, "node is first child\n");
        }
    }
    
    notifyChanged(caretNode);
    return true;
}

bool DeleteTextCommand::canUndo() const
{
    return true;
}

