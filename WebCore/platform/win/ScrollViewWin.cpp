/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Justin Haygood <jhaygood@spsu.edu>.
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

#include "config.h"
#include "ScrollView.h"

#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "RenderTheme.h" // FIXME: This sucks, but scrollview should eventually merge into FrameView anyway.
#include "GraphicsContext.h"
#include "IntRect.h"
#include "PlatformScrollBar.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ScrollBar.h"
#include <algorithm>
#include <winsock2.h>
#include <windows.h>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>

using namespace std;

namespace WebCore {

class ScrollView::ScrollViewPrivate : public ScrollbarClient {
public:
    ScrollViewPrivate(ScrollView* view)
        : m_view(view)
        , m_hasStaticBackground(false)
        , m_scrollbarsSuppressed(false)
        , m_inUpdateScrollbars(false)
        , m_resizerOverlapsContent(false)
        , m_vScrollbarMode(ScrollbarAuto)
        , m_hScrollbarMode(ScrollbarAuto)
    {
    }

    ~ScrollViewPrivate()
    {
        setHasHorizontalScrollbar(false);
        setHasVerticalScrollbar(false);
    }

    void setHasHorizontalScrollbar(bool hasBar);
    void setHasVerticalScrollbar(bool hasBar);

    void valueChanged(Scrollbar*);

    ScrollView* m_view;
    IntSize m_scrollOffset;
    IntSize m_contentsSize;
    bool m_hasStaticBackground;
    bool m_scrollbarsSuppressed;
    bool m_inUpdateScrollbars;
    bool m_resizerOverlapsContent;
    ScrollbarMode m_vScrollbarMode;
    ScrollbarMode m_hScrollbarMode;
    RefPtr<PlatformScrollbar> m_vBar;
    RefPtr<PlatformScrollbar> m_hBar;
    HashSet<const Widget*> m_children;
};

void ScrollView::ScrollViewPrivate::setHasHorizontalScrollbar(bool hasBar)
{
    if (Scrollbar::hasPlatformScrollbars()) {
        if (hasBar && !m_hBar) {
            m_hBar = new PlatformScrollbar(this, HorizontalScrollbar, RegularScrollbar);
            m_view->addChild(m_hBar.get());
        } else if (!hasBar && m_hBar) {
            m_view->removeChild(m_hBar.get());
            m_hBar = 0;
        }
    }
}

void ScrollView::ScrollViewPrivate::setHasVerticalScrollbar(bool hasBar)
{
    if (Scrollbar::hasPlatformScrollbars()) {
        if (hasBar && !m_vBar) {
            m_vBar = new PlatformScrollbar(this, VerticalScrollbar, RegularScrollbar);
            m_view->addChild(m_vBar.get());
        } else if (!hasBar && m_vBar) {
            m_view->removeChild(m_vBar.get());
            m_vBar = 0;
        }
    }
}

void ScrollView::ScrollViewPrivate::valueChanged(Scrollbar* bar)
{
    // Figure out if we really moved.
    IntSize newOffset = m_scrollOffset;
    if (bar == m_hBar)
        newOffset.setWidth(bar->value());
    else if (bar == m_vBar)
        newOffset.setHeight(bar->value());
    IntSize scrollDelta = newOffset - m_scrollOffset;
    if (scrollDelta == IntSize())
        return;
    m_scrollOffset = newOffset;

    if (m_scrollbarsSuppressed)
        return;

    RECT dirtyRect = m_view->convertToContainingWindow(IntRect(0, 0, m_view->visibleWidth(), m_view->visibleHeight()));
    if (!m_hasStaticBackground) { // The main frame can just blit the WebView window
        // FIXME: Could make this more efficient by passing a valid clip rect for only the document content.
        // FIXME: Find a way to blit subframes without blitting overlapping content
        ::ScrollWindowEx(m_view->containingWindow(), -scrollDelta.width(), -scrollDelta.height(), &dirtyRect, &RECT(m_view->windowClipRect()), 0, 0, SW_INVALIDATE | SW_SCROLLCHILDREN);
    } else
        ::InvalidateRect(m_view->containingWindow(), &dirtyRect, false);

    m_view->geometryChanged();
}

ScrollView::ScrollView()
    : m_data(new ScrollViewPrivate(this))
{
}

ScrollView::~ScrollView()
{
    delete m_data;
}

void ScrollView::updateContents(const IntRect& rect, bool now)
{
    if (rect.isEmpty())
        return;

    IntPoint windowPoint = contentsToWindow(rect.location());
    IntRect containingWindowRect = rect;
    containingWindowRect.setLocation(windowPoint);

    RECT containingWindowRectWin = containingWindowRect;
    HWND containingWindowHandle = containingWindow();

#if PAINT_FLASHING_DEBUG
    HDC dc = GetDC(containingWindowHandle);
    FillRect(dc, &containingWindowRectWin, (HBRUSH)GetStockObject(BLACK_BRUSH));
    ReleaseDC(containingWindowHandle, dc);
#endif

    ::InvalidateRect(containingWindowHandle, &containingWindowRectWin, false);
    if (now)
        ::UpdateWindow(containingWindowHandle);
}

int ScrollView::visibleWidth() const
{
    return width() - (m_data->m_vBar ? m_data->m_vBar->width() : 0);
}

int ScrollView::visibleHeight() const
{
    return height() - (m_data->m_hBar ? m_data->m_hBar->height() : 0);
}

FloatRect ScrollView::visibleContentRect() const
{
    return FloatRect(contentsX(), contentsY(), visibleWidth(), visibleHeight());
}

void ScrollView::setContentsPos(int newX, int newY)
{
    int dx = newX - contentsX();
    int dy = newY - contentsY();
    scrollBy(dx, dy);
}

void ScrollView::resizeContents(int w, int h)
{
    IntSize newContentsSize(w, h);
    if (m_data->m_contentsSize != newContentsSize) {
        m_data->m_contentsSize = newContentsSize;
        updateScrollbars(m_data->m_scrollOffset);
    }
}

void ScrollView::setFrameGeometry(const IntRect& newGeometry)
{
    IntRect oldGeometry = frameGeometry();
    Widget::setFrameGeometry(newGeometry);

    if (newGeometry == oldGeometry)
        return;

    if (newGeometry.width() != oldGeometry.width() || newGeometry.height() != oldGeometry.height())
        updateScrollbars(m_data->m_scrollOffset);
    
    geometryChanged();
}

int ScrollView::contentsX() const
{
    return scrollOffset().width();
}

int ScrollView::contentsY() const
{
    return scrollOffset().height();
}

int ScrollView::contentsWidth() const
{
    return m_data->m_contentsSize.width();
}

int ScrollView::contentsHeight() const
{
    return m_data->m_contentsSize.height();
}

IntPoint ScrollView::windowToContents(const IntPoint& windowPoint) const
{
    IntPoint viewPoint = convertFromContainingWindow(windowPoint);
    return viewPoint + scrollOffset();
}

IntPoint ScrollView::contentsToWindow(const IntPoint& contentsPoint) const
{
    IntPoint viewPoint = contentsPoint - scrollOffset();
    return convertToContainingWindow(viewPoint);  
}

IntPoint ScrollView::convertChildToSelf(const Widget* child, const IntPoint& point) const
{
    IntPoint newPoint = point;
    if (child != m_data->m_hBar && child != m_data->m_vBar)
        newPoint = point - scrollOffset();
    return Widget::convertChildToSelf(child, newPoint);
}

IntPoint ScrollView::convertSelfToChild(const Widget* child, const IntPoint& point) const
{
    IntPoint newPoint = point;
    if (child != m_data->m_hBar && child != m_data->m_vBar)
        newPoint = point + scrollOffset();
    return Widget::convertSelfToChild(child, newPoint);
}

IntSize ScrollView::scrollOffset() const
{
    return m_data->m_scrollOffset;
}

IntSize ScrollView::maximumScroll() const
{
    IntSize delta = (m_data->m_contentsSize - IntSize(visibleWidth(), visibleHeight())) - scrollOffset();
    delta.clampNegativeToZero();
    return delta;
}

void ScrollView::scrollBy(int dx, int dy)
{
    IntSize scrollOffset = m_data->m_scrollOffset;
    IntSize newScrollOffset = scrollOffset + IntSize(dx, dy).shrunkTo(maximumScroll());
    newScrollOffset.clampNegativeToZero();

    if (newScrollOffset == scrollOffset)
        return;

    updateScrollbars(newScrollOffset);
}

void ScrollView::scrollPointRecursively(int x, int y)
{
    IntPoint p(max(0, x), max(0, y));
    ScrollView* view = this;
    ScrollView* oldView = view;
    while (view) {
        view->setContentsPos(p.x(), p.y());
        p.move(view->x() - view->scrollOffset().width(), view->y() - view->scrollOffset().height());
        view = static_cast<ScrollView*>(parent());
    }
}

WebCore::ScrollbarMode ScrollView::hScrollbarMode() const
{
    return m_data->m_hScrollbarMode;
}

WebCore::ScrollbarMode ScrollView::vScrollbarMode() const
{
    return m_data->m_vScrollbarMode;
}

void ScrollView::suppressScrollbars(bool suppressed, bool repaintOnSuppress)
{
    m_data->m_scrollbarsSuppressed = suppressed;
    if (repaintOnSuppress && !suppressed) {
        if (m_data->m_hBar)
            m_data->m_hBar->invalidate();
        if (m_data->m_vBar)
            m_data->m_vBar->invalidate();
    }
}

void ScrollView::setHScrollbarMode(ScrollbarMode newMode)
{
    if (m_data->m_hScrollbarMode != newMode) {
        m_data->m_hScrollbarMode = newMode;
        updateScrollbars(m_data->m_scrollOffset);
    }
}

void ScrollView::setVScrollbarMode(ScrollbarMode newMode)
{
    if (m_data->m_vScrollbarMode != newMode) {
        m_data->m_vScrollbarMode = newMode;
        updateScrollbars(m_data->m_scrollOffset);
    }
}

void ScrollView::setScrollbarsMode(ScrollbarMode newMode)
{
    if (m_data->m_hScrollbarMode != newMode ||
        m_data->m_vScrollbarMode != newMode) {
        m_data->m_hScrollbarMode = m_data->m_vScrollbarMode = newMode;
        updateScrollbars(m_data->m_scrollOffset);
    }
}

void ScrollView::setStaticBackground(bool flag)
{
    m_data->m_hasStaticBackground = flag;
}

void ScrollView::updateScrollbars(const IntSize& desiredOffset)
{
    // Don't allow re-entrancy into this function.
    if (m_data->m_inUpdateScrollbars)
        return;
    
    m_data->m_inUpdateScrollbars = true;

    bool hasVerticalScrollbar = m_data->m_vBar;
    bool hasHorizontalScrollbar = m_data->m_hBar;
    bool oldHasVertical = hasVerticalScrollbar;
    bool oldHasHorizontal = hasHorizontalScrollbar;
    ScrollbarMode hScroll = m_data->m_hScrollbarMode;
    ScrollbarMode vScroll = m_data->m_vScrollbarMode;
    
    const int cVerticalWidth = PlatformScrollbar::verticalScrollbarWidth();
    const int cHorizontalHeight = PlatformScrollbar::horizontalScrollbarHeight();

    for (int pass = 0; pass < 2; pass++) {
        bool scrollsVertically;
        bool scrollsHorizontally;

        if (!m_data->m_scrollbarsSuppressed && (hScroll == ScrollbarAuto || vScroll == ScrollbarAuto)) {
            // Do a layout if pending before checking if scrollbars are needed.
            if (isFrameView() && (hasVerticalScrollbar != oldHasVertical || hasHorizontalScrollbar != oldHasHorizontal))
                static_cast<FrameView*>(this)->layout();
             
            scrollsVertically = (vScroll == ScrollbarAlwaysOn) || (vScroll == ScrollbarAuto && contentsHeight() > height());
            if (scrollsVertically)
                scrollsHorizontally = (hScroll == ScrollbarAlwaysOn) || (hScroll == ScrollbarAuto && contentsWidth() + cVerticalWidth > width());
            else {
                scrollsHorizontally = (hScroll == ScrollbarAlwaysOn) || (hScroll == ScrollbarAuto && contentsWidth() > width());
                if (scrollsHorizontally)
                    scrollsVertically = (vScroll == ScrollbarAlwaysOn) || (vScroll == ScrollbarAuto && contentsHeight() + cHorizontalHeight > height());
            }
        }
        else {
            scrollsHorizontally = (hScroll == ScrollbarAuto) ? hasHorizontalScrollbar : (hScroll == ScrollbarAlwaysOn);
            scrollsVertically = (vScroll == ScrollbarAuto) ? hasVerticalScrollbar : (vScroll == ScrollbarAlwaysOn);
        }
        
        if (hasVerticalScrollbar != scrollsVertically) {
            m_data->setHasVerticalScrollbar(scrollsVertically);
            hasVerticalScrollbar = scrollsVertically;
        }

        if (hasHorizontalScrollbar != scrollsHorizontally) {
            m_data->setHasHorizontalScrollbar(scrollsHorizontally);
            hasHorizontalScrollbar = scrollsHorizontally;
        }
    }
    
    // Set up the range (and page step/line step).
    IntSize maxScrollPosition(contentsWidth() - visibleWidth(), contentsHeight() - visibleHeight());
    IntSize scroll = desiredOffset.shrunkTo(maxScrollPosition);
    scroll.clampNegativeToZero();

    bool resizerOverlapsContent = !(m_data->m_hBar || m_data->m_vBar);
    bool resizerNeedsInvalidation = (resizerOverlapsContent != m_data->m_resizerOverlapsContent);
    m_data->m_resizerOverlapsContent = resizerOverlapsContent;

    IntRect resizerRect;
    if (!resizerOverlapsContent || resizerNeedsInvalidation) {
        resizerRect = windowResizerRect();
        resizerRect.setLocation(windowToContents(resizerRect.location()));
        if (resizerNeedsInvalidation)
            invalidateRect(resizerRect);
    }

    if (m_data->m_hBar) {
        int clientWidth = visibleWidth();
        m_data->m_hBar->setEnabled(contentsWidth() > clientWidth);
        int pageStep = (clientWidth - PAGE_KEEP);
        if (pageStep < 0) pageStep = clientWidth;
        IntRect oldRect(m_data->m_hBar->frameGeometry());
        IntRect hBarRect = IntRect(0,
                                   height() - m_data->m_hBar->height(),
                                   width() - (m_data->m_vBar ? m_data->m_vBar->width() : 0),
                                   m_data->m_hBar->height());
        if (hBarRect.intersects(resizerRect)) {
            int overlap = hBarRect.right() - resizerRect.x();
            if (overlap > 0 && resizerRect.right() >= hBarRect.right())
                hBarRect.setWidth(hBarRect.width() - overlap);
        }
        m_data->m_hBar->setRect(hBarRect);
        if (!m_data->m_scrollbarsSuppressed && oldRect != m_data->m_hBar->frameGeometry())
            m_data->m_hBar->invalidate();

        if (m_data->m_scrollbarsSuppressed)
            m_data->m_hBar->setSuppressInvalidation(true);
        m_data->m_hBar->setSteps(LINE_STEP, pageStep);
        m_data->m_hBar->setProportion(clientWidth, contentsWidth());
        m_data->m_hBar->setValue(scroll.width());
        if (m_data->m_scrollbarsSuppressed)
            m_data->m_hBar->setSuppressInvalidation(false); 
    }
    if (m_data->m_vBar) {
        int clientHeight = visibleHeight();
        m_data->m_vBar->setEnabled(contentsHeight() > clientHeight);
        int pageStep = (clientHeight - PAGE_KEEP);
        if (pageStep < 0) pageStep = clientHeight;
        IntRect oldRect(m_data->m_vBar->frameGeometry());
        IntRect vBarRect = IntRect(width() - m_data->m_vBar->width(), 
                                   0,
                                   m_data->m_vBar->width(),
                                   height() - (m_data->m_hBar ? m_data->m_hBar->height() : 0));
        if (vBarRect.intersects(resizerRect)) {
            int overlap = vBarRect.bottom() - resizerRect.y();
            if (overlap > 0 && resizerRect.bottom() >= vBarRect.bottom())
                vBarRect.setHeight(vBarRect.height() - overlap);
        }
        m_data->m_vBar->setRect(vBarRect);
        if (!m_data->m_scrollbarsSuppressed && oldRect != m_data->m_vBar->frameGeometry())
            m_data->m_vBar->invalidate();

        if (m_data->m_scrollbarsSuppressed)
            m_data->m_vBar->setSuppressInvalidation(true);
        m_data->m_vBar->setSteps(LINE_STEP, pageStep);
        m_data->m_vBar->setProportion(clientHeight, contentsHeight());
        m_data->m_vBar->setValue(scroll.height());
        if (m_data->m_scrollbarsSuppressed)
            m_data->m_vBar->setSuppressInvalidation(false);
    }

    m_data->m_inUpdateScrollbars = false;
}

PlatformScrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent)
{
    IntPoint viewPoint = convertFromContainingWindow(mouseEvent.pos());
    if (m_data->m_hBar && m_data->m_hBar->frameGeometry().contains(viewPoint))
        return m_data->m_hBar.get();
    if (m_data->m_vBar && m_data->m_vBar->frameGeometry().contains(viewPoint))
        return m_data->m_vBar.get();
    return 0;
}

void ScrollView::addChild(Widget* child) 
{ 
    child->setParent(this);
    child->setContainingWindow(containingWindow());
    m_data->m_children.add(child);
}

void ScrollView::removeChild(Widget* child)
{ 
    if (child == capturingChild()) {
        // If our child (or one of its descendants is capturing mouse events but suddenly goes away,
        // we become the new capturer.
        setCapturingChild(0);
        setCapturingMouse(true);
    }
    if (child == focusedChild()) {
        // If our child is focused and goes away, then we become the new focused element.
        setFocusedChild(0);
        setFocused(true);
    }

    child->setParent(0);
    m_data->m_children.remove(child);
}

void ScrollView::paint(GraphicsContext* context, const IntRect& rect)
{
    // FIXME: This code is here so we don't have to fork FrameView.h/.cpp.
    // In the end, FrameView should just merge with ScrollView.
    ASSERT(isFrameView());

    if (context->paintingDisabled())
        return;

    IntRect documentDirtyRect = rect;
    documentDirtyRect.intersect(frameGeometry());

    context->save();

    context->translate(x(), y());
    documentDirtyRect.move(-x(), -y());

    context->translate(-contentsX(), -contentsY());
    documentDirtyRect.move(contentsX(), contentsY());

    context->clip(enclosingIntRect(visibleContentRect()));

    static_cast<const FrameView*>(this)->frame()->paint(context, documentDirtyRect);

    context->restore();

    // Now paint the scrollbars.
    if (!m_data->m_scrollbarsSuppressed && (m_data->m_hBar || m_data->m_vBar)) {
        context->save();
        IntRect scrollViewDirtyRect = rect;
        scrollViewDirtyRect.intersect(frameGeometry());
        context->translate(x(), y());
        scrollViewDirtyRect.move(-x(), -y());
        if (m_data->m_hBar)
            m_data->m_hBar->paint(context, scrollViewDirtyRect);
        if (m_data->m_vBar)
            m_data->m_vBar->paint(context, scrollViewDirtyRect);
        
        if (m_data->m_hBar && m_data->m_vBar) {
            // Fill the scroll corner box with a light grey color.
            IntRect scrollCornerRect(width() - m_data->m_vBar->width(),
                                     height() - m_data->m_hBar->height(),
                                     m_data->m_vBar->width(),
                                     m_data->m_hBar->height());
            if (scrollCornerRect.intersects(scrollViewDirtyRect))
                context->fillRect(scrollCornerRect, Color(makeRGB(221,221,221)));
        }

        context->restore();
    }
}

void ScrollView::themeChanged()
{
    PlatformScrollbar::themeChanged();
    theme()->themeChanged();
    invalidate();
}

void ScrollView::wheelEvent(PlatformWheelEvent& e)
{
    // Determine how much we want to scroll.  If we can move at all, we will accept the event.
    IntSize maxScrollDelta = maximumScroll();
    if ((e.deltaX() < 0 && maxScrollDelta.width() > 0) ||
        (e.deltaX() > 0 && scrollOffset().width() > 0) ||
        (e.deltaY() < 0 && maxScrollDelta.height() > 0) ||
        (e.deltaY() > 0 && scrollOffset().height() > 0))
        e.accept();

    scrollBy(-e.deltaX() * LINE_STEP, -e.deltaY() * LINE_STEP);
}

void ScrollView::geometryChanged() const
{
    HashSet<const Widget*>::const_iterator end = m_data->m_children.end();
    for (HashSet<const Widget*>::const_iterator current = m_data->m_children.begin(); current != end; ++current)
        (*current)->geometryChanged();
}

void ScrollView::scroll(ScrollDirection direction, ScrollGranularity granularity)
{
    if  ((direction == ScrollUp || direction == ScrollDown) && m_data->m_vBar)
        m_data->m_vBar->scroll(direction, granularity);
    else if (m_data->m_hBar)
        m_data->m_hBar->scroll(direction, granularity);
}

IntRect ScrollView::windowResizerRect()
{
    return static_cast<FrameView*>(this)->frame()->windowResizerRect();
}

bool ScrollView::resizerOverlapsContent() const
{
    return m_data->m_resizerOverlapsContent;
}

} // namespace WebCore
