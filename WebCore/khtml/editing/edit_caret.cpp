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
      m_x(0), m_y(0), m_height(0), m_width(0), 
      m_freqTimerId(0), m_visible(false), m_displayed(false)
{
    if (node) {
        m_node = node;
        m_node->ref();
    }
}

Caret::Caret() 
    : m_node(0), m_offset(0), 
      m_x(0), m_y(0), m_height(0), m_width(0), 
      m_freqTimerId(0), m_visible(false), m_displayed(false)
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
    m_width = other.m_width;
    m_freqTimerId = other.m_freqTimerId;
    m_visible = other.m_visible;
    m_displayed = other.m_displayed;
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

void Caret::initCaret() 
{
    KHTMLPart *p = part();
    if (!p)
        return;

    if (p->xmlDocImpl()) {
        if (!m_node) {
            // set to document, position will be sanitized anyway
            p->moveCaretTo(p->document().handle(), 0L);
            // This sanity check is necessary for the not so unlikely case that
            // setEditMode or setCaretMode is called before any render objects have
            // been created.
            if (!m_node->renderer()) 
                return;
        }
        p->moveCaretTo(m_node, m_offset);
    }
}

void Caret::invalidate()
{
    if (!m_node || !m_displayed)
        return;

    hideCaret();
    recalcAndStoreCaretPos();
    showCaret();
}

void Caret::recalcAndStoreCaretPos()
{
    if (!m_node)
        return;

    if (m_node->renderer()) 
        m_node->renderer()->caretPos(m_offset, true, m_x, m_y, m_width, m_height);
    else {
        // return default values
        m_x = m_y = m_height = -1;
        m_width = 1;	// the caret has a default width of one pixel. If you want
                   // to check for validity, only test the x-coordinate for >= 0.
    }
}

void Caret::caretOn() {
    if (!m_node)
        return;

    killTimer(m_freqTimerId);
    m_freqTimerId = startTimer(BLINK_FREQUENCY);
    m_visible = true;
    m_displayed = true;
    updateView();
}

void Caret::caretOff() {
    killTimer(m_freqTimerId);
    m_freqTimerId = -1;
    m_displayed = false;
    if (m_visible) {
        m_visible = false;
        updateView();
    }
}

void Caret::showCaret() {
    m_displayed = true;
    if (m_visible) {
        updateView();
    }
}

void Caret::hideCaret() {
    if (m_visible) {
        m_visible = false;
        updateView(true);
        m_visible = true;
    }
    else {
        m_displayed = false;
    }
}

void Caret::updateView(bool immediate) const
{
    KHTMLView *view = m_node->getDocument()->view();
    if (!view)
        return;
        
    if (immediate)
        view->repaintContents(m_x, m_y, 1, m_height);
    else
        view->updateContents(m_x, m_y, 1, m_height);
}

bool Caret::placeCaret() 
{
    caretOff();

    recalcAndStoreCaretPos();
    
    if (!m_node)
        return false;

    KHTMLPart *p = part();
    if (!p)
        return false;

    if (!p->isEditingAtCaret())
        return false;

    // save selection. setting focus might clear it.
    Range selection = p->selection();
    ensureNodeHasFocus(m_node);
    // restore selection, but don't place caret
    p->setSelection(selection, false);
        
    if (!p->selection().collapsed())
        return false;

    caretOn();
    return true;
}

#ifdef APPLE_CHANGES
void Caret::paintCaret(QPainter *p, const QRect &rect) const
{
    if (!m_visible)
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
    if (e->timerId() == m_freqTimerId) {
        m_visible = !m_visible;
        if (m_displayed)
            updateView();
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


