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

#ifndef __khtml_selection_h__
#define __khtml_selection_h__

#include <qobject.h>

class KHTMLPart;
class KHTMLPartPrivate;
class KHTMLView;
class QPainter;
class QRect;
class QTimerEvent;

namespace DOM {
    class NodeImpl;
    class Range;
};

class KHTMLSelection : public QObject
{
  Q_OBJECT

public:
    KHTMLSelection();
    KHTMLSelection(const KHTMLSelection &);
    ~KHTMLSelection();

	enum EState { NONE, CARET, RANGE };

	EState state() const { return m_state; }

    void setSelection(DOM::NodeImpl *node, long offset);
    void setSelection(const DOM::Range &);
    void setSelection(DOM::NodeImpl *startNode, long startOffset, DOM::NodeImpl *endNode, long endOffset);
    void clearSelection();

    DOM::NodeImpl *startNode() const { return m_startNode; }
    long startOffset() const { return m_startOffset; }

    DOM::NodeImpl *endNode() const { return m_endNode; }
    long endOffset() const { return m_endOffset; }

    DOM::NodeImpl *caretNode() const { return m_extendAtEnd ? m_endNode : m_startNode; }
    long caretOffset() const { return m_extendAtEnd ? m_endOffset : m_startOffset; }

    void setVisible(bool flag=true);
    bool visible() const { return m_visible; }
    void invalidate();
    
#ifdef APPLE_CHANGES
    void paint(QPainter *p, const QRect &rect) const;
#endif
    
    friend class KHTMLPart;
    
private:
    void setPart(KHTMLPart *part);

    void timerEvent(QTimerEvent *e);
    void repaint(bool immediate=false) const;

	void setStartNode(DOM::NodeImpl *);
	void setStartOffset(long);
	void setEndNode(DOM::NodeImpl *);
	void setEndOffset(long);

    KHTMLPart *m_part;          // part for this selection
    DOM::NodeImpl *m_startNode; // start node for the selection
    long m_startOffset;         // offset into start node where selection starts
    DOM::NodeImpl *m_endNode;   // end node for the selection
    long m_endOffset;           // offset into end node where selection starts

	EState m_state;             // the state of the selection

    int m_caretBlinkTimer;      // caret blink frequency timer id
	
	int m_caretX;
	int m_caretY;
	int m_caretSize;

	bool m_extendAtEnd : 1;     // true if the selection "extends" at the end when arrowing
    bool m_caretBlinks : 1;     // true if caret blinks
    bool m_caretPaint : 1;      // flag used to deal with blinking the caret
    bool m_visible : 1;         // true if selection is to be displayed at all
};

#endif