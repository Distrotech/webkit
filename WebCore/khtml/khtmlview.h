/* This file is part of the KDE project

   Copyright (C) 1997 Martin Jones (mjones@kde.org)
             (C) 1998 Waldo Bastian (bastian@kde.org)
             (C) 1998, 1999 Torben Weis (weis@kde.org)
             (C) 1999 Lars Knoll (knoll@kde.org)
             (C) 1999 Antti Koivisto (koivisto@kde.org)
   Copyright (C) 2003 Apple Computer, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef KHTML_H
#define KHTML_H

// qt includes and classes
#include <qscrollview.h>

class QPainter;
class QRect;

// Uncomment to enable INCREMENTAL_REPAINTING
#define INCREMENTAL_REPAINTING

namespace DOM {
    class HTMLDocumentImpl;
    class DocumentImpl;
    class ElementImpl;
    class HTMLElementImpl;
    class HTMLTitleElementImpl;
    class HTMLGenericFormElementImpl;
    class HTMLFormElementImpl;
    class HTMLAnchorElementImpl;
    class Range;
    class NodeImpl;
    class CSSProperty;
};

namespace khtml {
    class RenderObject;
    class RenderBox;
    class RenderCanvas;
    class RenderStyle;
    class RenderLineEdit;
    class RenderPart;
    class RenderPartObject;
    class RenderWidget;
    class CSSStyleSelector;
    class InlineBox;
    void applyRule(DOM::CSSProperty *prop);
};

class KHTMLPart;
class KHTMLViewPrivate;

/**
 * Renders and displays HTML in a @ref QScrollView.
 *
 * Suitable for use as an application's main view.
 **/
class KHTMLView : public QScrollView
{
    Q_OBJECT

    friend class DOM::HTMLDocumentImpl;
    friend class DOM::HTMLTitleElementImpl;
    friend class DOM::HTMLGenericFormElementImpl;
    friend class DOM::HTMLFormElementImpl;
    friend class DOM::HTMLAnchorElementImpl;
    friend class DOM::DocumentImpl;
    friend class KHTMLPart;
    friend class khtml::RenderCanvas;
    friend class khtml::RenderObject;
    friend class khtml::RenderBox;
    friend class khtml::RenderLineEdit;
    friend class khtml::RenderPart;
    friend class khtml::RenderPartObject;
    friend class khtml::RenderWidget;
    friend class khtml::CSSStyleSelector;
    friend void khtml::applyRule(DOM::CSSProperty *prop);
#if APPLE_CHANGES
    friend class KWQKHTMLPart;
#endif

public:
    /**
     * Constructs a KHTMLView.
     */
    KHTMLView( KHTMLPart *part, QWidget *parent, const char *name=0 );
    virtual ~KHTMLView();

    /**
     * Returns a pointer to the KHTMLPart that is
     * rendering the page.
     **/
    KHTMLPart *part() const { return m_part; }

    int frameWidth() const { return _width; }

    /**
     * Sets a margin in x direction.
     */
    void setMarginWidth(int x);

    /**
     * Returns the margin width.
     *
     * A return value of -1 means the default value will be used.
     */
    int marginWidth() const { return _marginWidth; }

    /*
     * Sets a margin in y direction.
     */
    void setMarginHeight(int y);

    /**
     * Returns the margin height.
     *
     * A return value of -1 means the default value will be used.
     */
    int marginHeight() { return _marginHeight; }

    /**
     * Sets verticals scrollbar mode. Reimplemented for internal reasons.
     */
    virtual void setVScrollBarMode ( ScrollBarMode mode );

    /**
     * Sets horizontal scrollbar mode. Reimplemented for internal reasons.
     */
    virtual void setHScrollBarMode ( ScrollBarMode mode );

    // Sets both horizontal and vertical modes.
    virtual void setScrollBarsMode(ScrollBarMode mode);
    
    /**
     * Prints the HTML document.
     */
    void print();

    /**
     * ensure the display is up to date
     */
    void layout();

    bool inLayout() const;

#ifdef INCREMENTAL_REPAINTING
    bool needsFullRepaint() const;
#endif
    
#if APPLE_CHANGES
    void resetScrollBars();
#endif

    // -- caret-related member functions (for caret mode as well as design mode)
    /** initializes the caret if it hasn't been initialized yet.
     *
     * This method determines a suitable starting position, initializes
     * the internal structures, and calculates the caret's coordinates ready
     * for display.
     *
     * To "deinitialize" the caret, call @ref caretOff
     */
    void initCaret();
    /** returns whether the text under the caret will be overridden.
     */
    bool caretOverrides();
    /** ensures that the given element is properly focused.
     *
     * If not in caret mode or design mode, keyboard events are only regarded for
     * focused nodes. Therefore, the function ensured that the focus will be
     * properly set on unfocused nodes (or on a suitable ancestor).
     * @param node node to focus
     */
    void ensureNodeHasFocus(DOM::NodeImpl *node);
    /** inquires the current caret position and stores it in the caret view
     * context. Also resets the blink frequency timer. It will not display
     * the caret on the canvas.
     */
    void recalcAndStoreCaretPos();
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

#ifdef APPLE_CHANGES
    void paintCaret(QPainter *p, const QRect &rect) const;
#endif

    /** folds the selection to the current caret position.
     *
     * Whatever selection has existed before will be removed by the invocation
     * of this method. Updates are only done if an actual selection has
     * been folded. After the call of this method, no selection will exist
     * any more.
     *
     * No validity checking is done on the parameters. Note that the parameters
     * refer to the old selection, the current caret may be way off.
     * @param startNode starting node of selection
     * @param startOffset offset within the start node.
     * @param endNode ending node of selection
     * @param endOffset offset within the end node.
     * @return @p true if there had been a selection, and it was folded.
     */
    bool foldSelectionToCaret(DOM::NodeImpl *startNode, long startOffset,
                              DOM::NodeImpl *endNode, long endOffset);
    /** places the caret on the current position.
     *
     * The caret is switched off, the position recalculated with respect to
     * the new position. The caret will only be redisplayed if it is on an
     * editable node, in design mode, or in caret mode.
     * @return @p true if the caret has been displayed.
     */
    bool placeCaret();
    // -- caret event handler
    /**
     * Evaluates key presses on editable nodes.
     */
    void caretKeyPressEvent(QKeyEvent *);
    // -- caret navigation member functions
    /** moves the caret to the given position and displays it.
     *
     * If the node is an invalid place, the function sets the caret to an
     * nearby node that is valid.
     *
     * @param node node to be set to
     * @param offset zero-based offset within this node
     * @param clearSelection @p true if any the selection should be cleared
     *    as well. It is ignored if @p thoroughly is false.
     * @return @p true if a previously existing selection has been cleared.
     */
    bool moveCaretTo(DOM::NodeImpl *node, long offset, bool clearSelection);

signals:
        void cleared();
    
protected:
        void clear();
    
#if APPLE_CHANGES
public:
        void clearPart();
#endif
    virtual void resizeEvent ( QResizeEvent * event );
    virtual void showEvent ( QShowEvent * );
    virtual void hideEvent ( QHideEvent *);
    virtual bool focusNextPrevChild( bool next );
#if !APPLE_CHANGES
    virtual void drawContents ( QPainter * p, int clipx, int clipy, int clipw, int cliph );
    virtual void drawContents( QPainter* );
#endif
    
    virtual void viewportMousePressEvent( QMouseEvent * );
    virtual void focusInEvent( QFocusEvent * );
    virtual void focusOutEvent( QFocusEvent * );
    virtual void viewportMouseDoubleClickEvent( QMouseEvent * );
    virtual void viewportMouseMoveEvent(QMouseEvent *);
    virtual void viewportMouseReleaseEvent(QMouseEvent *);
#ifndef QT_NO_WHEELEVENT
    virtual void viewportWheelEvent(QWheelEvent*);
#endif
#if !APPLE_CHANGES
    virtual void dragEnterEvent( QDragEnterEvent* );
    virtual void dropEvent( QDropEvent* );
#endif

    void keyPressEvent( QKeyEvent *_ke );
    void keyReleaseEvent ( QKeyEvent *_ke );
    void contentsContextMenuEvent ( QContextMenuEvent *_ce );
    void doAutoScroll();

    void timerEvent ( QTimerEvent * );

    void repaintRectangle(const QRect& r, bool immediate);
    
#if APPLE_CHANGES
    QWidget *topLevelWidget() const;
    QPoint mapToGlobal(const QPoint &) const;
    void adjustViewSize();
    void initScrollBars();
#endif

    void ref() { ++_refCount; }
    void deref() { if (!--_refCount) delete this; }
    
protected slots:
    void slotPaletteChanged();
    void slotScrollBarMoved();

private:

    void resetCursor();

#ifdef INCREMENTAL_REPAINTING
    void scheduleRelayout();
#else
    void scheduleRelayout(khtml::RenderObject* clippedObj=0);
#endif
    void unscheduleRelayout();

    /**
     * Paints the HTML document to a QPainter.
     * The document will be scaled to match the width of
     * rc and clipped to fit in the height.
     * yOff determines the vertical offset in the document to start with.
     * more, if nonzero will be set to true if the documents extends
     * beyond the rc or false if everything below yOff was painted.
     **/
    void paint(QPainter *p, const QRect &rc, int yOff = 0, bool *more = 0);

    /**
     * Get/set the CSS Media Type.
     *
     * Media type is set to "screen" for on-screen rendering and "print"
     * during printing. Other media types lack the proper support in the
     * renderer and are not activated. The DOM tree and the parser itself,
     * however, properly handle other media types. To make them actually work
     * you only need to enable the media type in the view and if necessary
     * add the media type dependent changes to the renderer.
     */
    void setMediaType( const QString &medium );
    QString mediaType() const;

    bool scrollTo(const QRect &);

    void focusNextPrevNode(bool next);

    void useSlowRepaints();

    void setIgnoreWheelEvents(bool e);

    void init();

    DOM::NodeImpl *nodeUnderMouse() const;

    void restoreScrollBar();

    QStringList formCompletionItems(const QString &name) const;
    void addFormCompletionItem(const QString &name, const QString &value);

    bool dispatchMouseEvent(int eventId, DOM::NodeImpl *targetNode, bool cancelable,
			    int detail,QMouseEvent *_mouse, bool setUnder,
			    int mouseEventType);

    void complete();

    void applyBodyScrollQuirk(khtml::RenderObject* o, ScrollBarMode& hMode, ScrollBarMode& vMode);
    
#ifndef INCREMENTAL_REPAINTING
    // Returns the clipped object we will repaint when we perform our scheduled layout.
    khtml::RenderObject* layoutObject() { return m_layoutObject; }
#endif
    
    // ------------------------------------- member variables ------------------------------------
 private:
    unsigned _refCount;

    int _width;
    int _height;

    int _marginWidth;
    int _marginHeight;

    KHTMLPart *m_part;
    KHTMLViewPrivate *d;

    QString m_medium;   // media type

#ifndef INCREMENTAL_REPAINTING
    // An overflow: hidden clipped object.  If this is set, a scheduled layout will only repaint
    // the object's clipped area, and it will not do a full repaint.
    khtml::RenderObject* m_layoutObject;
#endif
};

#endif

