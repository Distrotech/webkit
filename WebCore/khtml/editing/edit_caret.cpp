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
using DOM::Node;
using DOM::NodeImpl;
using DOM::Range;
using DOM::TextImpl;
using khtml::Caret;

enum { BLINK_FREQUENCY = 500 };

Caret::Caret(NodeImpl *node, long offset) 
    : m_node(0), m_offset(offset), 
      m_x(0), m_y(0), m_size(0), 
      m_timerId(0), m_blinks(true), m_paint(false), m_visible(false)
{
    if (node) {
        m_node = node;
        m_node->ref();
    }
}

Caret::Caret() 
    : m_node(0), m_offset(0), 
      m_x(0), m_y(0), m_size(0),
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
    m_size = other.m_size;
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

void Caret::moveTo(NodeImpl *node, long offset, bool clearSelection) 
{
    // Fix up caret node
    // EDIT FIXME: this finds the next node with a renderer,
    // setting the node to 0 if there are no more renderers.
    // It should search backwards then.
    // This still won't solve the problem what to do if *no* element has a renderer.
    if (node) { 
        RenderObject *r = node->renderer();
        while (!r) {
            node = node->nextLeafNode();
            if (!node) 
                break;
            r = node->renderer();
        }

        // Fix up caret offset
        long max = node->caretMaxOffset();
        long min = node->caretMinOffset();
        if (offset < min) 
            offset = min;
        else if (offset > max) 
            offset = max;
    }

    bool positionChanged = m_node != node || m_offset != offset;

    // update caret values
    m_offset = offset;
    if (m_node != node) {
		if (m_node)
			m_node->deref();
    	m_node = node;
		if (m_node)
			m_node->ref();
	}

    // test for clearing the selection
    KHTMLPart *p = part();
    if (p) {
        if (clearSelection) {
            ensureNodeHasFocus(m_node);
            p->slotClearSelection();
        }
        else {
#if 0
            // save selection. setting focus might clear it.
            Range selection = p->selection();
            ensureNodeHasFocus(m_node);
            // restore selection, but don't place caret
            p->setSelection(selection, false);
#endif
        }
    }
    
    if (positionChanged) {
        invalidate();
        if (p)
            p->emitCaretPositionChanged();
    }
}

void Caret::moveForwardByCharacter()
{
    if (!node())
        return;

    KHTMLPart *part = node()->getDocument()->part();
    if (!part)
        return;

    long caretOffset = offset();
    
    if (caretOffset < node()->caretMaxOffset()) {
        moveTo(node(), caretOffset + 1);
    }
    else {
        NodeImpl *n = node()->nextLeafNode();
        while (n) {
            if (n->caretMinOffset() < n->caretMaxOffset()) {
                moveTo(n, n->caretMinOffset() + 1);
                break;
            }
            else {
                n = n->nextLeafNode();
            }
        }
    }
}

void Caret::moveBackwardByCharacter()
{
    if (!node())
        return;

    KHTMLPart *part = node()->getDocument()->part();
    if (!part)
        return;

    if (offset() - 1 >= 0) {
        moveTo(node(), offset() - 1);
    }
    else {
        NodeImpl *n = node()->previousLeafNode();
        while (n) {
            if (n->caretMinOffset() < n->caretMaxOffset()) {
                moveTo(n, n->caretMaxOffset() - 1);
                break;
            }
            else {
                n = n->nextLeafNode();
            }
        }
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
        moveTo(n, n->caretMaxOffset());
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
        if (m_paint) {
            m_paint = false;
            repaint(true);
        }
        return;
    }

    // update caret rendering position
    int oldX = m_x;   
    int oldY = m_y;   
    int oldSize = m_size;   
    if (m_node && m_node->renderer()) {
        int w;
        m_node->renderer()->caretPos(m_offset, true, m_x, m_y, w, m_size);
    }
    else
        m_x = m_y = m_size = 0; // default values

    // repaint the old position if necessary
    // prevents two carets from ever being drawn
    if (m_paint && (oldX != m_x || oldY != m_y || oldSize != m_size)) {
        KHTMLView *v = view();
        if (v)
            v->updateContents(oldX, oldY, 1, oldSize);
    }

    // paint the caret if it is visible
    if (m_visible && m_size != 0) {
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
        v->repaintContents(m_x, m_y, 1, m_size);
    else
        v->updateContents(m_x, m_y, 1, m_size);
}

#ifdef APPLE_CHANGES
void Caret::paint(QPainter *p, const QRect &rect) const
{
    if (!m_paint)
        return;

    QRect pos(m_x, m_y, 1, m_size);
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

    if (node && node->focused()) 
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


