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
	  m_baseNode(0), m_baseOffset(0), m_extentNode(0), m_extentOffset(0),
	  m_state(NONE), m_caretBlinkTimer(0), m_baseIsStart(true),
	  m_caretBlinks(true), m_caretPaint(false), m_visible(false)
{
}

KHTMLSelection::KHTMLSelection(const KHTMLSelection &o)
	: QObject(),
	  m_part(o.m_part),
	  m_baseNode(0), m_baseOffset(0), m_extentNode(0), m_extentOffset(0)
{
    if (o.m_baseNode) {
        m_baseNode = o.m_baseNode;
        m_baseNode->ref();
    }
    if (o.m_extentNode) {
        m_extentNode = o.m_extentNode;
        m_extentNode->ref();
    }

	m_state = o.m_state;
	m_caretBlinkTimer = o.m_caretBlinkTimer;
	m_baseIsStart = o.m_baseIsStart;
	m_caretBlinks = o.m_caretBlinks;
	m_caretPaint = o.m_caretPaint;
	m_visible = o.m_visible;
	invalidate(); // invalidate the new copy
}

KHTMLSelection::~KHTMLSelection()
{
    if (m_baseNode)
        m_baseNode->deref();
    if (m_extentNode)
        m_extentNode->deref();
}

KHTMLSelection &KHTMLSelection::operator=(const KHTMLSelection &o)
{
    m_part = o.m_part;
    
    if (m_baseNode)
        m_baseNode->deref();
    m_baseNode = 0;

    if (o.m_baseNode) {
        m_baseNode = o.m_baseNode;
        m_baseNode->ref();
    }

    if (m_extentNode)
        m_extentNode->deref();
    m_extentNode = 0;

    if (o.m_extentNode) {
        m_extentNode = o.m_extentNode;
        m_extentNode->ref();
    }

	m_state = o.m_state;
	m_caretBlinkTimer = o.m_caretBlinkTimer;
	m_baseIsStart = o.m_baseIsStart;
	m_caretBlinks = o.m_caretBlinks;
	m_caretPaint = o.m_caretPaint;
	m_visible = o.m_visible;
    invalidate();
    return *this;
}

void KHTMLSelection::setSelection(DOM::NodeImpl *node, long offset)
{
	setBaseNode(node);
	setExtentNode(node);
	setBaseOffset(offset);
	setExtentOffset(offset);
	invalidate();
}

void KHTMLSelection::setSelection(const DOM::Range &r)
{
	setSelection(r.startContainer().handle(), r.startOffset(), 
		r.endContainer().handle(), r.endOffset());
}

void KHTMLSelection::setSelection(DOM::NodeImpl *startNode, long startOffset, DOM::NodeImpl *endNode, long endOffset)
{
	setBaseNode(startNode);
	setExtentNode(endNode);
	setBaseOffset(startOffset);
	setExtentOffset(endOffset);
	invalidate();
}

void KHTMLSelection::setBase(DOM::NodeImpl *node, long offset)
{
	setBaseNode(node);
	setBaseOffset(offset);
	invalidate();
}

void KHTMLSelection::setExtent(DOM::NodeImpl *node, long offset)
{
	setExtentNode(node);
	setExtentOffset(offset);
	invalidate();
}

void KHTMLSelection::clearSelection()
{
	setBaseNode(0);
	setExtentNode(0);
	setBaseOffset(0);
	setExtentOffset(0);
	invalidate();
}

void KHTMLSelection::setBaseNode(DOM::NodeImpl *node)
{
	if (m_baseNode == node)
		return;

	if (m_baseNode)
		m_baseNode->deref();
	
	m_baseNode = node;
	
	if (m_baseNode)
		m_baseNode->ref();
}

void KHTMLSelection::setBaseOffset(long offset)
{
	m_baseOffset = offset;
}

void KHTMLSelection::setExtentNode(DOM::NodeImpl *node)
{
	if (m_extentNode == node)
		return;

	if (m_extentNode)
		m_extentNode->deref();
	
	m_extentNode = node;
	
	if (m_extentNode)
		m_extentNode->ref();
}
	
void KHTMLSelection::setExtentOffset(long offset)
{
	m_extentOffset = offset;
}

void KHTMLSelection::setVisible(bool flag)
{
    m_visible = flag;
    invalidate();
}

void KHTMLSelection::invalidate()
{
    // make sure we do not have a dangling start or end
	if (!m_baseNode) {
		setBaseNode(m_extentNode);
		setBaseOffset(m_extentOffset);
        m_baseIsStart = true;
	}
	else if (!m_extentNode) {
		setExtentNode(m_baseNode);
		setExtentOffset(m_baseOffset);
        m_baseIsStart = true;
	}
    else {
        if (m_baseNode == m_extentNode) {
            if (m_baseOffset > m_extentOffset)
                m_baseIsStart = false;
            else 
                m_baseIsStart = true;
        }
        else if (nodeIsBeforeNode(m_baseNode, m_extentNode))
            m_baseIsStart = true;
        else
            m_baseIsStart = false;
    }

	// update the state
	if (!m_baseNode && !m_extentNode)
		m_state = NONE;
	if (m_baseNode == m_extentNode && m_baseOffset == m_extentOffset)
		m_state = CARET;
	else
		m_state = RANGE;

    // update the blink timer
    if (m_caretBlinkTimer >= 0)
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
    
    NodeImpl *node = startNode();
    if (node && node->renderer()) {
        int w;
        node->renderer()->caretPos(startOffset(), true, newX, newY, w, newSize);
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

bool KHTMLSelection::isEmpty() const
{
    return m_baseNode == 0 && m_extentNode == 0;
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

bool KHTMLSelection::nodeIsBeforeNode(NodeImpl *n1, NodeImpl *n2) 
{
	if (!n1 || !n2) 
		return true;
 
 	if (n1 == n2)
 		return true;
 
 	bool result = false;
    int n1Depth = 0;
    int n2Depth = 0;

    // First we find the depths of the two nodes in the tree (n1Depth, n2Depth)
    DOM::NodeImpl *n = n1;
    while (n->parentNode()) {
        n = n->parentNode();
        n1Depth++;
    }
    n = n2;
    while (n->parentNode()) {
        n = n->parentNode();
        n2Depth++;
    }
    // Climb up the tree with the deeper node, until both nodes have equal depth
    while (n2Depth > n1Depth) {
        n2 = n2->parentNode();
        n2Depth--;
    }
    while (n1Depth > n2Depth) {
        n1 = n1->parentNode();
        n1Depth--;
    }
    // Climb the tree with both n1 and n2 until they have the same parent
    while (n1->parentNode() != n2->parentNode()) {
        n1 = n1->parentNode();
        n2 = n2->parentNode();
    }
    // Iterate through the parent's children until n1 or n2 is found
    n = n1->parentNode()->firstChild();
    while (n) {
        if (n == n1) {
            result = true;
            break;
        }
        else if (n == n2) {
            result = false;
            break;
        }
        n = n->nextSibling();
    }
	return result;
}
