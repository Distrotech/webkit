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
  
#include <edit_caret.h>

#include <dom_docimpl.h>
#include <dom_nodeimpl.h>
#include <dom_textimpl.h>
#include <khtml_part.h>
#include <render_object.h>

#if APPLE_CHANGES
#include <KWQAssertions.h>
#endif

using DOM::DocumentImpl;
using DOM::NodeImpl;
using DOM::TextImpl;
using khtml::Caret;

Caret::Caret(NodeImpl *node, long offset) : m_node(0), m_offset(offset)
{
    if (node) {
        m_node = node;
        m_node->ref();
    }
}

Caret::Caret(const Caret &other)
{
    m_node = other.m_node;
    if (m_node)
        m_node->ref();
    m_offset = other.m_offset;
}

Caret::~Caret()
{
    if (m_node)
        m_node->deref();
}

NodeImpl *Caret::node() const
{
    return m_node;
}

long Caret::offset() const
{
	return m_offset;
}

void Caret::setPosition(NodeImpl *node, long offset)
{
    if (node == m_node && offset == m_offset)
        return;

    if (node && node->hasChildNodes())
        node = node->nextLeafNode();

    if (m_node != node) {
		if (m_node)
			m_node->deref();

    	m_node = node;

		if (m_node)
			m_node->ref();
	}

    m_offset = offset;

    adjustPosition();
    notifyChanged(node);
}

void Caret::moveForwardByCharacter()
{
    if (!node())
        return;

    KHTMLPart *part = node()->getDocument()->part();
    if (!part)
        return;

    bool moved = false;
    
    long caretOffset = offset();
    
    if (caretOffset < node()->caretMaxOffset()) {
        part->moveCaretTo(node(), caretOffset + 1);
        moved = true;
    }
    else {
        NodeImpl *n = node()->nextLeafNode();
        while (n) {
            if (n->caretMinOffset() < n->caretMaxOffset()) {
                part->moveCaretTo(n, n->caretMinOffset() + 1);
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
        notifyChanged(node());
    }
}

void Caret::moveBackwardByCharacter()
{
    if (!node())
        return;

    KHTMLPart *part = node()->getDocument()->part();
    if (!part)
        return;

    bool moved = false;

    if (offset() - 1 >= 0) {
        part->moveCaretTo(node(), offset() - 1);
        moved = true;
    }
    else {
        NodeImpl *n = node()->previousLeafNode();
        while (n) {
            if (n->caretMinOffset() < n->caretMaxOffset()) {
                part->moveCaretTo(n, n->caretMaxOffset() - 1);
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
        notifyChanged(node());
    }
}

void Caret::adjustPosition()
{
    if (!node()) 
        return;

    if (offset() != 0) 
        return;

    KHTMLPart *part = node()->getDocument()->part();
    if (!part)
        return;

    // prune empty text nodes preceding the caret
    int exceptionCode;
    NodeImpl *n = node()->previousLeafNode();
    while (1) {
        if (!n || !n->isTextNode() || static_cast<TextImpl *>(n)->length() > 0)
            break;
        node()->parentNode()->removeChild(n, exceptionCode);
        notifyChanged(node()->parentNode());
        n = n->previousLeafNode();
    }

    // if we are at offset zero of a text node, and a text node precedes us
    // move to the end of the preceding text node
    n = node()->previousLeafNode();
    if (n && n->isTextNode()) {
        part->moveCaretTo(n, n->caretMaxOffset());
    }
}

void Caret::notifyChanged(NodeImpl *node) const
{
    if (!node)
        return;

    node->setChanged(true);
    if (node->renderer())
        node->renderer()->setNeedsLayoutAndMinMaxRecalc();
}
