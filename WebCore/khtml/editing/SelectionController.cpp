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
  
#include "khtml_selection.h"

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

enum { CARET_BLINK_FREQUENCY = 500 };

KHTMLSelection::KHTMLSelection() 
	: QObject(),
	  m_part(0),
	  m_startNode(0), m_startOffset(0), m_endNode(0), m_endOffset(0),
	  m_state(NONE), m_caretBlinkTimer(0), m_extendAtEnd(true),
	  m_caretBlinks(true), m_caretPaint(false), m_visible(false)
{
}

KHTMLSelection::KHTMLSelection(const KHTMLSelection &o)
	: QObject(),
	  m_part(o.m_part),
	  m_startNode(0), m_startOffset(0), m_endNode(0), m_endOffset(0)
{
    if (o.m_startNode) {
        m_startNode = o.m_startNode;
        m_startNode->ref();
    }
    if (o.m_endNode) {
        m_endNode = o.m_endNode;
        m_endNode->ref();
    }

	m_state = o.m_state;
	m_caretBlinkTimer = o.m_caretBlinkTimer;
	m_extendAtEnd = o.m_extendAtEnd;
	m_caretBlinks = o.m_caretBlinks;
	m_caretPaint = o.m_caretPaint;
	m_visible = o.m_visible;
	invalidate(); // invalidate the new copy
}

KHTMLSelection::~KHTMLSelection()
{
    if (m_startNode)
        m_startNode->deref();
    if (m_endNode)
        m_endNode->deref();
}

void KHTMLSelection::setSelection(DOM::NodeImpl *node, long offset)
{
	setStartNode(node);
	setEndNode(node);
	setStartOffset(offset);
	setEndOffset(offset);
	invalidate();
}

void KHTMLSelection::setSelection(const DOM::Range &r)
{
	setSelection(r.startContainer().handle(), r.startOffset(), 
		r.endContainer().handle(), r.endOffset());
}

void KHTMLSelection::setSelection(DOM::NodeImpl *startNode, long startOffset, DOM::NodeImpl *endNode, long endOffset)
{
	setStartNode(startNode);
	setEndNode(endNode);
	setStartOffset(startOffset);
	setEndOffset(endOffset);
	invalidate();
}

void KHTMLSelection::clearSelection()
{
	setStartNode(0);
	setEndNode(0);
	setStartOffset(0);
	setEndOffset(0);
	invalidate();
}

void KHTMLSelection::setStartNode(DOM::NodeImpl *node)
{
	if (m_startNode == node)
		return;

	if (m_startNode)
		m_startNode->deref();
	
	m_startNode = node;
	
	if (m_startNode)
		m_startNode->ref();
}

void KHTMLSelection::setStartOffset(long offset)
{
	m_startOffset = offset;
}

void KHTMLSelection::setEndNode(DOM::NodeImpl *node)
{
	if (m_endNode == node)
		return;

	if (m_endNode)
		m_endNode->deref();
	
	m_endNode = node;
	
	if (m_endNode)
		m_endNode->ref();
}
	
void KHTMLSelection::setEndOffset(long offset)
{
	m_endOffset = offset;
}

void KHTMLSelection::setVisible(bool flag)
{
    m_visible = flag;
    invalidate();
}

void KHTMLSelection::invalidate()
{
    // make sure we do not have a dangling start or end
	if (!m_startNode) {
		setStartNode(m_endNode);
		setStartOffset(m_endOffset);
	}
	if (!m_endNode) {
		setEndNode(m_startNode);
		setEndOffset(m_startOffset);
	}

	// update the state
	if (!m_startNode && !m_endNode)
		m_state = NONE;
	if (m_startNode == m_endNode && m_startOffset == m_endOffset)
		m_state = CARET;
	else
		m_state = RANGE;

    // update the blink timer
    killTimer(m_caretBlinkTimer);
    if (m_visible && m_state == CARET && m_caretBlinks)
        m_caretBlinkTimer = startTimer(CARET_BLINK_FREQUENCY);
    else
        m_caretBlinkTimer = -1;

    // short-circuit if not visible
    if (!m_visible) {
        if (m_caretPaint) {
            m_caretPaint = false;
            repaint(true);
        }
        return;
    }

    // short-circuit if not CARET state
	if (m_state != CARET)
		return;

    // calculate the new caret rendering position
    int oldX = m_caretX;   
    int oldY = m_caretY;   
    int oldSize = m_caretSize;
    
    int newX = 0;
    int newY = 0;
    int newSize = 0;
    
    NodeImpl *node = caretNode();
    if (node && node->renderer()) {
        int w;
        node->renderer()->caretPos(caretOffset(), true, newX, newY, w, newSize);
    }

    // repaint the old position if necessary
    // prevents two carets from ever being drawn
    if (m_caretPaint && (oldX != newX || oldY != newY || oldSize != newSize)) {
        repaint(true);
    }

    // update caret rendering position
    m_caretX = newX;
    m_caretY = newY;
    m_caretSize = newSize;

    // paint the caret if it is visible
    if (m_visible && m_caretSize != 0) {
        m_caretPaint = true;
        repaint();
    }
}

#ifdef APPLE_CHANGES
void KHTMLSelection::paint(QPainter *p, const QRect &rect) const
{
    if (!m_caretPaint)
        return;

    QRect pos(m_caretX, m_caretY, 1, m_caretSize);
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

void KHTMLSelection::setPart(KHTMLPart *part)
{
    m_part = part;
}

void KHTMLSelection::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == m_caretBlinkTimer && m_visible) {
        m_caretPaint = !m_caretPaint;
        repaint();
    }
}

void KHTMLSelection::repaint(bool immediate) const
{
    KHTMLView *v = m_part->view();
    if (!v)
        return;
        
    if (immediate)
        v->repaintContents(m_caretX, m_caretY, 1, m_caretSize);
    else
        v->updateContents(m_caretX, m_caretY, 1, m_caretSize);
}
