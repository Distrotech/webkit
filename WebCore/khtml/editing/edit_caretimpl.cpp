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

#include <edit_caretimpl.h>

#include <dom_docimpl.h>
#include <dom_nodeimpl.h>
#include <dom_textimpl.h>
#include <khtmlview.h>
#include <render_object.h>

#if APPLE_CHANGES
#include <KWQAssertions.h>
#endif

using DOM::DocumentImpl;
using DOM::EditNodeFilter;
using DOM::Node;
using DOM::NodeImpl;
using DOM::NodeFilter;
using DOM::TextImpl;
using khtml::Caret;
using khtml::CaretImpl;

CaretImpl::CaretImpl(NodeImpl *node, long offset) 
    : m_node(0), m_offset(offset), m_startOfLine(false) 
{
    if (node) {
        m_node = node;
        m_node->ref();
    }
}

CaretImpl::~CaretImpl()
{
    if (m_node)
        m_node->deref();
}

void CaretImpl::setNode(DOM::NodeImpl *node)
{
    if (m_node == node)
        return;

    if (m_node)
        m_node->deref();

    m_node = node;

    if (m_node)
        m_node->ref();
}

void CaretImpl::setPosition(NodeImpl *newNode, long newOffset)
{
    if (newNode == node() && newOffset == offset())
        return;

    // Perform various checks on the new position and fix up as needed
    // 1. Ensure the node set is a leaf node
    if (newNode && newNode->hasChildNodes())
        newNode = newNode->nextLeafNode();

    setNode(newNode);
    setOffset(newOffset);

    adjustPosition();
    
    // this has the effect of keeping the cursor from blinking when the caret moves
    if (node()) {
        KHTMLView *view = node()->getDocument()->view();
        if (view)
            view->caretOn();
    }
}

void CaretImpl::moveForwardByCharacter()
{
    bool moved = false;
    
    if (offset() < node()->caretMaxOffset()) {
        setPosition(node(), offset() + 1);
        moved = true;
    }
    else {
        NodeImpl *n = node()->nextLeafNode();
        while (n) {
            if (n->caretMinOffset() < n->caretMaxOffset()) {
                setPosition(n, n->caretMinOffset() + 1);
                moved = true;
                break;
            }
            else {
                n = n->nextLeafNode();
            }
        }
    }
    
    if (moved) {
        adjustPosition();
        node()->setChanged(true);
        if (node()->renderer())
            node()->renderer()->setNeedsLayoutAndMinMaxRecalc();
    }
}

void CaretImpl::moveBackwardByCharacter()
{
    bool moved = false;

    if (offset() - 1 > 0) {
        setPosition(node(), offset() - 1);
        moved = true;
    }
    else {
        NodeImpl *n = node()->previousLeafNode();
        while (n) {
            if (n->caretMinOffset() < n->caretMaxOffset()) {
                setPosition(n, n->caretMaxOffset() - 1);
                moved = true;
                break;
            }
            else {
                n = n->nextLeafNode();
            }
        }
    }

    if (moved) {
        adjustPosition();
        node()->setChanged(true);
        if (node()->renderer())
            node()->renderer()->setNeedsLayoutAndMinMaxRecalc();
    }
}

void CaretImpl::setStartOfLine(bool startOfLine)
{
    m_startOfLine = startOfLine;
}

void CaretImpl::adjustPosition()
{
    if (!node()) 
        return;

    if (offset() != 0) 
        return;

#if 0

    // some sketchy ideas.
    // not ready for prime time

    if (!node()) 
        return;
    
    if (offset() != 0) 
        return;

    // prune empty text nodes preceding the caret
    int exceptionCode;
    NodeImpl *n = node()->previousSibling();
    while (1) {
        if (!n || !n->isTextNode() || static_cast<TextImpl *>(n)->length() > 0)
            break;
        node()->parentNode()->removeChild(n, exceptionCode);
        n = n->previousSibling();
    }
    
    // determine whether adjustment is needed
    n = node();
    while (n) {
        fprintf(stderr, "needsAdjustmentForEditing: %p : (%s)\n", n, n->nodeName().string().latin1());
        n = n->traversePreviousNode();
        if (!n)
            return;
        if (n->isHTMLBRElement())
            return;
        if (n->isTextNode())
            break;
    }

    EditNodeFilter filter;
    NodeFilter nodeFilter = NodeFilter::createCustom(&filter);
    TreeWalker tw = treeWalker();
    if (tw.isNull())
        return;
    tw.handle()->setFilter(nodeFilter.handle());
    NodeImpl *node = m_node;
    node = tw.previousNode().handle();
    if (node) {
        setNode(node);
        m_offset = node->caretMaxOffset();
        fprintf(stderr, "adjustedForEditing: %p : %d (%s)\n", node, node->caretMaxOffset(), node->nodeName().string().latin1());
    }
    else {
        fprintf(stderr, "not adjustedForEditing\n");
    }
#endif
}

short EditNodeFilter::acceptNode(const DOM::Node &n)
{
    NodeImpl *node = n.handle();
    if (!node) {
        return NodeFilter::FILTER_REJECT;
    }

    if (!node->isHTMLElement() && !node->isTextNode()) {
        return NodeFilter::FILTER_REJECT;
    }

    if (node->isContentEditable()) {
        return NodeFilter::FILTER_ACCEPT;
    }

    return NodeFilter::FILTER_SKIP;
}
