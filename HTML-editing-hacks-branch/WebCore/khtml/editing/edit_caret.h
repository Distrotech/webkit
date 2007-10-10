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

#ifndef __edit_caret_h__
#define __edit_caret_h__

#include <qobject.h>

class KHTMLPart;
class KHTMLPartPrivate;
class KHTMLView;
class QPainter;
class QRect;
class QTimerEvent;

namespace DOM {
    class NodeImpl;
};

namespace khtml {

class Caret : public QObject
{
  Q_OBJECT

public:
    Caret();
    Caret(DOM::NodeImpl *node, long offset);
    Caret(const Caret &);
    ~Caret();

    void moveTo(DOM::NodeImpl *node, long offset, bool clearSelection=true);
    DOM::NodeImpl *node() const { return m_node; }
    long offset() const { return m_offset; }

    int xPos() const { return m_x; }
    int yPos() const { return m_y; }
    int size() const { return m_size; }

    void setVisible(bool flag=true);
    bool visible() const { return m_visible; }
    void invalidate();
    
    void ensureNodeHasFocus(DOM::NodeImpl *node);

#ifdef APPLE_CHANGES
    void paint(QPainter *p, const QRect &rect) const;
#endif
    
    void moveForwardByCharacter();
    void moveBackwardByCharacter();

    // EDIT FIXME: This should not be public
    void adjustPosition();
        
    friend class KHTMLPart;
    
private:
    KHTMLPart *part() const;
    KHTMLView *view() const;

    void timerEvent(QTimerEvent *e);
    void repaint(bool immediate=false) const;

    DOM::NodeImpl *m_node;  // node containing the caret
    long m_offset;          // offset into node where caret is positioned

    int m_x;                // caret x position in viewport coordinates
    int m_y;                // caret y position in viewport coordinates (specifies the top, not the baseline)
    int m_size;             // size of caret in pixels

    int m_timerId;          // caret blink frequency timer id

    bool m_blinks : 1;      // flag set if caret blinks
    bool m_paint : 1;       // flag used to deal with blinking the caret.
    bool m_visible : 1;     // true if caret is to be displayed at all.
};

}; // end namespace khtml

#endif