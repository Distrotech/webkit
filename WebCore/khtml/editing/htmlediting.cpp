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
#include "dom2_rangeimpl.h"
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
using DOM::RangeImpl;
using DOM::TextImpl;

using khtml::DeleteTextCommand;
using khtml::EditCommand;
using khtml::EditCommandID;
using khtml::InputTextCommand;

//------------------------------------------------------------------------------------------
// EditCommand

EditCommand::EditCommand(DOM::DocumentImpl *document) : m_document(0)
{
    m_document = document;
    if (m_document) {
        m_document->ref();
        if (m_document->view() && m_document->view()->part())
            m_selection = m_document->view()->part()->selection();
    }
}

EditCommand::~EditCommand()
{
    if (m_document)
        m_document->deref();
}

void EditCommand::notifyChanged(NodeImpl *node) const
{
    if (!node)
        return;

    node->setChanged(true);
    if (node->renderer())
        node->renderer()->setNeedsLayoutAndMinMaxRecalc();
}

void EditCommand::notifyNodesChanged() const
{
    // notify all the nodes in the selection
    NodeImpl *startNode = selection().startContainer().handle();
    NodeImpl *endNode = selection().endContainer().handle();
    if (startNode == endNode) {
        notifyChanged(startNode);
    }
    else {
        for (NodeImpl *node = startNode; node != endNode; node = node->traverseNextNode()) {
            notifyChanged(node);
        }
        notifyChanged(endNode);
    }

    // notify the caret node
    KHTMLView *view = document()->view();
    if (!view)
        return;

    KHTMLPart *part = view->part();
    if (!part)
        return;

    notifyChanged(part->caret().node().handle());
}

void EditCommand::deleteSelection()
{
    // EDIT FIXME: need to save the contents for redo
    notifyChanged(selection().startContainer().handle());
    notifyChanged(selection().endContainer().handle());
    if (document()->view() && document()->view()->part())
        document()->view()->part()->setSelection(Range(selection().startContainer(), selection().startOffset(), selection().startContainer(), selection().startOffset()), true);

    selection().deleteContents();
}

void EditCommand::pruneEmptyNodes() const
{
    KHTMLView *view = document()->view();
    if (!view)
        return;

    KHTMLPart *part = view->part();
    if (!part)
        return;

    Caret caret = part->caret();
    
    bool prunedNodes = false;
    NodeImpl *node = caret.node().handle();
    while (1) {
        if (node->isTextNode()) {
            TextImpl *textNode = static_cast<TextImpl *>(node);
            if (textNode->length() == 0) {
                fprintf(stderr, "prune text: %p\n", textNode);
                node = textNode->traversePreviousNode();
                removeNode(textNode);
                prunedNodes = true;
            }
            else {
                break;
            }
        }
        else if (!node->hasChildNodes()) {
            NodeImpl *n = node;
            node = node->traversePreviousNode();
            fprintf(stderr, "prune elem: %p\n", n);
            removeNode(n);
            prunedNodes = true;
        }
        else {
            fprintf(stderr, "has children: %p : %d\n", node, node->childNodes()->length());
            break;
        }
    }
    
    if (prunedNodes) {
        fprintf(stderr, "prune move to: %p : %s\n", node, node->nodeName().string().latin1());
        part->moveCaretTo(node, node->caretMaxOffset());
    }
}

void EditCommand::removeNode(DOM::NodeImpl *node) const
{
    if (!node)
        return;
    
    notifyChanged(node->parentNode());
    int exceptionCode;
    node->remove(exceptionCode);
}

//------------------------------------------------------------------------------------------
// InputTextCommand

EditCommandID InputTextCommand::commandID() const { return InputTextCommandID; }

InputTextCommand::InputTextCommand(DocumentImpl *document, const DOMString &text) 
    : EditCommand(document)
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
    return m_text.length() == 1 && (m_text[0] == '\n' || m_text[0] == '\r');
}

bool InputTextCommand::isSpace() const
{
    return m_text.length() == 1 && (m_text[0] == ' ');
}

bool InputTextCommand::apply()
{
    KHTMLView *view = document()->view();
    if (!view)
        return false;

    KHTMLPart *part = view->part();
    if (!part)
        return false;

    Caret caret = part->caret();
    if (!caret.node().handle()->isTextNode())
        return false;

    // notify nodes that will change
    notifyNodesChanged();

    // Delete the current selection
    if (view->caretOverrides()) {
        deleteSelection();
        caret.adjustPosition();
    }
    
    TextImpl *textNode = static_cast<TextImpl *>(caret.node().handle());
    int exceptionCode;
    
    if (isLineBreak()) {
        TextImpl *textBeforeNode = document()->createTextNode(textNode->substringData(0, caret.offset(), exceptionCode));
        textNode->deleteData(0, caret.offset(), exceptionCode);
        ElementImpl *breakNode = document()->createHTMLElement("BR", exceptionCode);
        textNode->parentNode()->insertBefore(textBeforeNode, textNode, exceptionCode);
        textNode->parentNode()->insertBefore(breakNode, textNode, exceptionCode);
        textBeforeNode->deref();
        breakNode->deref();
        
        // Set the cursor at the beginning of the node after the split.
        part->moveCaretTo(textNode, 0);
    }
    else {
        textNode->insertData(caret.offset(), text(), exceptionCode);
        // EDIT FIXME: this is a hack for now
        // advance the cursor
        int textLength = text().length();
        part->moveCaretTo(caret.node().handle(), caret.offset() + textLength);
    }

    return true;
}

bool InputTextCommand::canUndo() const
{
    return true;
}

//------------------------------------------------------------------------------------------
// DeleteTextCommand

EditCommandID DeleteTextCommand::commandID() const { return DeleteTextCommandID; }

DeleteTextCommand::DeleteTextCommand(DocumentImpl *document) 
    : EditCommand(document)
{
}

bool DeleteTextCommand::apply()
{
    KHTMLView *view = document()->view();
    if (!view)
        return false;

    KHTMLPart *part = view->part();
    if (!part)
        return false;

    Caret caret = part->caret();

    // notify nodes that will change
    notifyNodesChanged();

    // Delete the current selection
    if (!selection().collapsed()) {
        deleteSelection();
        caret.adjustPosition();
        return true;
    }

    if (caret.node().handle()->isTextNode()) {
        int exceptionCode;
        int offset = caret.offset() - 1;
        if (offset >= 0) {
            // Delete character at cursor
            TextImpl *textNode = static_cast<TextImpl *>(caret.node().handle());
            textNode->deleteData(offset, 1, exceptionCode);
            part->moveCaretTo(textNode, offset);
            pruneEmptyNodes();
            return true;
        }
        else {
            NodeImpl *node = caret.node().handle()->previousLeafNode();
            if (!node)
                return false;
            if (node->isTextNode()) {
                TextImpl *textNode = static_cast<TextImpl *>(node);
                offset = node->caretMaxOffset() - 1;
                textNode->deleteData(offset, 1, exceptionCode);
                part->moveCaretTo(textNode, offset);
                pruneEmptyNodes();
            }
        }
    }

    return true;
}

bool DeleteTextCommand::canUndo() const
{
    return true;
}

