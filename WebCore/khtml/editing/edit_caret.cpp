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
  
#include "edit_caret.h"

#include "khtml_part.h"
#include "khtmlview.h"
#include "qevent.h"
#include "qpainter.h"
#include "qrect.h"
#include "dom/dom2_range.h"
#include "dom/dom_node.h"
#include "rendering/render_object.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_textimpl.h"

#if APPLE_CHANGES
#include <KWQAssertions.h>
#endif

using DOM::DocumentImpl;
using DOM::NodeImpl;
using DOM::Range;
using DOM::TextImpl;
using khtml::Caret;

enum { BLINK_FREQUENCY = 500 };

Caret::Caret(NodeImpl *node, long offset) 
    : m_node(0), m_offset(offset), 
      m_x(0), m_y(0), m_height(0), 
      m_timerId(0), m_blinks(true), m_paint(false), m_visible(false)
{
    if (node) {
        m_node = node;
        m_node->ref();
    }
}

Caret::Caret() 
    : m_node(0), m_offset(0), 
      m_x(0), m_y(0), m_height(0),
      m_timerId(0), m_blinks(true), m_paint(false), m_visible(false)
{
}

Caret::Caret(const Caret &other) : QObject()
{
    m_node = other.m_node;
    if (m_node)
        m_node->ref();
    m_offset = other.m_offset;
    m_x = other.m_x;
    m_y = other.m_y;
    m_height = other.m_height;
    m_timerId = other.m_timerId;
    m_blinks = other.m_blinks;
    m_paint = other.m_paint;
    m_visible = other.m_visible;
}

Caret::~Caret()
{
    if (m_node)
        m_node->deref();
}

KHTMLPart *Caret::part() const 
{ 
    return m_node ? m_node->getDocument()->part(): 0; 
}

KHTMLView *Caret::view() const 
{ 
    return m_node ? m_node->getDocument()->view(): 0; 
}

void Caret::setPosition(NodeImpl *node, long offset)
{
    if (node == m_node && offset == m_offset)
        return;

    // set the node and offset
    m_offset = offset;

    if (node && node->hasChildNodes())
        node = node->nextLeafNode();

    if (m_node != node) {
		if (m_node)
			m_node->deref();
    	m_node = node;
		if (m_node)
			m_node->ref();
	}

    if (!m_node)
        return;

    // place the caret
    adjustPosition();
    
    KHTMLPart *p = part();
    if (!p || !p->isEditingAtCaret())
        return;

    // save selection. setting focus might clear it.
    Range selection = p->selection();
    ensureNodeHasFocus(m_node);
    // restore selection, but don't place caret
    p->setSelection(selection, false);
    if (!p->selection().collapsed())
        return;

    invalidate();
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
        n = n->previousLeafNode();
    }

    // if we are at offset zero of a text node, and a text node precedes us
    // move to the end of the preceding text node
    n = node()->previousLeafNode();
    if (n && n->isTextNode()) {
        part->moveCaretTo(n, n->caretMaxOffset());
    }
}

void Caret::invalidate()
{
    // update the blink timer
    killTimer(m_timerId);
    if (m_blinks && m_visible)
        m_timerId = startTimer(BLINK_FREQUENCY);
    else
        m_timerId = -1;

    // short-circuit if caret is not visible
    if (!m_visible) {
        m_paint = false;
        repaint(true);
        return;
    }

    // update caret rendering position
    int oldX = m_x;   
    int oldY = m_y;   
    int oldHeight = m_height;   
    if (m_node && m_node->renderer()) {
        int w;
        m_node->renderer()->caretPos(m_offset, true, m_x, m_y, w, m_height);
    }
    else
        m_x = m_y = m_height = 0; // default values

    // repaint the old position if necessary
    // prevents two carets from ever being drawn
    if (oldX != m_x || oldY != m_y || oldHeight != m_height) {
        KHTMLView *v = view();
        if (v)
            v->updateContents(oldX, oldY, 1, oldHeight);
    }

    // paint the caret if it is visible
    if (m_visible && m_height != 0) {
        m_paint = true;
        repaint();
    }
}

void Caret::setVisible(bool flag) 
{
    m_visible = flag;
    invalidate();
}

void Caret::repaint(bool immediate) const
{
    KHTMLView *v = view();
    if (!v)
        return;
        
    if (immediate)
        v->repaintContents(m_x, m_y, 1, m_height);
    else
        v->updateContents(m_x, m_y, 1, m_height);
}

#ifdef APPLE_CHANGES
void Caret::paint(QPainter *p, const QRect &rect) const
{
    if (!m_paint)
        return;

    QRect pos(m_x, m_y, 1, m_height);
    if (pos.intersects(rect)) {
        QPen pen = p->pen();
        pen.setStyle(SolidLine);
        pen.setColor(Qt::black);
        pen.setWidth(1);
        p->setPen(pen);
        p->drawLine(pos.left(), pos.top(), pos.left(), pos.bottom());
    }
}
#endif

void Caret::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == m_timerId && m_visible) {
        m_paint = !m_paint;
        repaint();
    }
}

void Caret::ensureNodeHasFocus(NodeImpl *node) 
{
    KHTMLPart *p = part();
    if (!p)
        return;

    if (p->inEditMode() || node->focused()) 
        return;
    
    // Find first ancestor whose "user-input" is "enabled"
    NodeImpl *firstAncestor = node->parentNode();
    while (node) {
        if (node->renderer())
            break;
        firstAncestor = node;
        node = node->parentNode();
    }
    
    if (!node) 
        firstAncestor = 0;
    
    // Set focus node on the document
    p->xmlDocImpl()->setFocusNode(firstAncestor);
    emit p->nodeActivated(firstAncestor);
}


