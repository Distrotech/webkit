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
    Caret(DOM::NodeImpl *node, long offset);
    Caret(const Caret &);
    ~Caret();

    DOM::NodeImpl *node() const { return m_node; }
    long offset() const { return m_offset; }

    int xPos() const { return m_x; }
    int yPos() const { return m_y; }
    int height() const { return m_height; }

    bool visible() const { return m_visible; }
    bool displayed() const { return m_displayed; }

    void invalidate();
    void recalcAndStoreCaretPos();

    void initCaret();

    /** displays the caret and reinitializes the blink frequency timer. */
    void caretOn();

    /** hides the caret and kills the blink frequency timer. */
    void caretOff();

    /** makes the caret visible, but does not influence the frequency timer.
     * That means it probably won't get visible immediately.
     */
    void showCaret();
    /** makes the caret invisible, but does not influence the frequency timer.
     * The caret is immediately hidden.
     */
    void hideCaret();
    
    bool placeCaret();

    // -- caret-related member functions (for caret mode as well as design mode)
    /** ensures that the given element is properly focused.
     *
     * If not in caret mode or design mode, keyboard events are only regarded for
     * focused nodes. Therefore, the function ensured that the focus will be
     * properly set on unfocused nodes (or on a suitable ancestor).
     * @param node node to focus
     */
    void ensureNodeHasFocus(DOM::NodeImpl *node);

#ifdef APPLE_CHANGES
    void paintCaret(QPainter *p, const QRect &rect) const;
#endif
    
    void moveForwardByCharacter();
    void moveBackwardByCharacter();

    // EDIT FIXME: This should not be public
    void adjustPosition();
        
    friend class KHTMLPart;
    friend class KHTMLPartPrivate;
    
private:
    Caret();

    KHTMLPart *part() const;
    KHTMLView *view() const;

    void setPosition(DOM::NodeImpl *, long);
    void notifyChanged(DOM::NodeImpl *) const;
    void updateView(bool immediate=false) const;
    void timerEvent(QTimerEvent *e);

    DOM::NodeImpl *m_node;  // node containing the caret
    long m_offset;          // offset into node where caret is positioned

    int m_x;                // caret x position in viewport coordinates
    int m_y;                // caret y position in viewport coordinates (specifies the top, not the baseline)
                    
    int m_height;           // height of caret in pixels
    int m_width;            // width of caret in pixels

    int m_freqTimerId;      // caret blink frequency timer id

    bool m_visible : 1;     // true if currently visible.
    bool m_displayed : 1;   // true if caret is to be displayed at all.
};

}; // end namespace khtml

#endif