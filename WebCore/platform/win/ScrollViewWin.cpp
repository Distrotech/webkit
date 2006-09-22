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
#include "GraphicsContext.h"
#include "IntRect.h"
#include "PlatformWheelEvent.h"
#include "ScrollBar.h"
#include <algorithm>
#include <winsock2.h>
#include <windows.h>
#include <wtf/Assertions.h>

using namespace std;

namespace WebCore {

class ScrollView::ScrollViewPrivate {
public:
    ScrollViewPrivate()
        : hasStaticBackground(false)
        , suppressScrollBars(false)
        , vScrollBarMode(ScrollBarAuto)
        , hScrollBarMode(ScrollBarAuto)
    {
    }
    IntSize scrollOffset;
    IntSize contentsSize;
    bool hasStaticBackground;
    bool suppressScrollBars;
    ScrollBarMode vScrollBarMode;
    ScrollBarMode hScrollBarMode;
};

ScrollView::ScrollView()
    : m_data(new ScrollViewPrivate())
{
}

ScrollView::~ScrollView()
{
    delete m_data;
}

void ScrollView::updateContents(const IntRect& rect, bool now)
{
    IntRect viewRect = rect; // rect is in document coordinates
    viewRect.move(-m_data->scrollOffset);
    viewRect.intersect(frameGeometry());
    IntRect containingWindowRect = convertToContainingWindow(viewRect);

    RECT containingWindowRectWin = containingWindowRect;
    HWND containingWindowHandle = containingWindow();

#if PAINT_FLASHING_DEBUG
    HDC dc = GetDC(containingWindowHandle);
    FillRect(dc, &containingWindowRectWin, (HBRUSH)GetStockObject(BLACK_BRUSH));
    ReleaseDC(containingWindowHandle, dc);
#endif

    InvalidateRect(containingWindowHandle, &containingWindowRectWin, true);
    if (now)
        UpdateWindow(containingWindowHandle);
}

int ScrollView::visibleWidth() const
{
    return visibleContentRect().width();
}

int ScrollView::visibleHeight() const
{
    return visibleContentRect().height();
}

FloatRect ScrollView::visibleContentRect() const
{
    // FIXME: Subframes need to subtract scrollbar width & height when scrollbars are showing
    FloatRect contentRect = frameGeometry();
    contentRect.move(m_data->scrollOffset);
    return contentRect;
}

void ScrollView::setContentsPos(int newX, int newY)
{
    int dx = newX - contentsX();
    int dy = newY - contentsY();
    scrollBy(dx, dy);
}

void ScrollView::resizeContents(int w, int h)
{
    IntSize newSize(w, h);
    if (m_data->contentsSize != newSize) {
        m_data->contentsSize = newSize;
        updateScrollBars();
    } else
        fprintf(stdout, "resizeContents\n");
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
    return m_data->contentsSize.width();
}

int ScrollView::contentsHeight() const
{
    return m_data->contentsSize.height();
}

IntPoint ScrollView::viewportToContents(const IntPoint& viewportPoint)
{
    POINT contentsPointWin = viewportPoint;
    MapWindowPoints(GetAncestor(containingWindow(), GA_ROOT), containingWindow(), &contentsPointWin, 1);
    IntPoint contentsPoint = convertFromContainingWindow(contentsPointWin);
    contentsPoint += scrollOffset();
    return contentsPoint;
}

IntPoint ScrollView::contentsToViewport(const IntPoint& contentsPoint)
{
    IntPoint viewportPoint = contentsPoint;
    viewportPoint -= scrollOffset();
    viewportPoint = convertToContainingWindow(viewportPoint);
    POINT viewportPointWin = viewportPoint;
    MapWindowPoints(containingWindow(), GetAncestor(containingWindow(), GA_ROOT), &viewportPointWin, 1);
    return viewportPointWin;
}

IntSize ScrollView::scrollOffset() const
{
    return m_data->scrollOffset;
}

IntSize ScrollView::maximumScroll() const
{
    IntSize delta = (m_data->contentsSize - IntSize(visibleWidth(), visibleHeight())) - m_data->scrollOffset;
    delta.clampNegativeToZero();
    return delta;
}

void ScrollView::scrollBy(int dx, int dy)
{
    IntSize scrollOffset = m_data->scrollOffset;
    IntSize newScrollOffset = scrollOffset + IntSize(dx, dy).shrunkTo(maximumScroll());
    newScrollOffset.clampNegativeToZero();

    if (newScrollOffset == scrollOffset)
        return;

    m_data->scrollOffset = newScrollOffset;
    updateScrollBars();

    // ScrollBar updates can fail, so we check the final delta before scrolling
    IntSize scrollDelta = m_data->scrollOffset - scrollOffset;
    if (scrollDelta == IntSize())
        return;

    if (!m_data->hasStaticBackground) { // The main frame can just blit the WebView window
        // FIXME: Could make this more efficient by passing a valid clip rect for only the document content.
        ScrollWindowEx(containingWindow(), -scrollDelta.width(), -scrollDelta.height(), 0, 0, 0, 0, SW_INVALIDATE);
    } else {
        // FIXME: Find a way to blit subframes without blitting overlapping content
        RECT dirtyRect = convertToContainingWindow(frameGeometry());
        InvalidateRect(containingWindow(), &dirtyRect, true);
    }
}

WebCore::ScrollBarMode ScrollView::hScrollBarMode() const
{
    return m_data->hScrollBarMode;
}

WebCore::ScrollBarMode ScrollView::vScrollBarMode() const
{
    return m_data->vScrollBarMode;
}

void ScrollView::suppressScrollBars(bool suppressed, bool repaintOnSuppress)
{
    m_data->suppressScrollBars = suppressed;
    if (repaintOnSuppress)
        updateScrollBars();
}

void ScrollView::setHScrollBarMode(ScrollBarMode newMode)
{
    if (m_data->hScrollBarMode != newMode) {
        m_data->hScrollBarMode = newMode;
        updateScrollBars();
    }
}

void ScrollView::setVScrollBarMode(ScrollBarMode newMode)
{
    if (m_data->vScrollBarMode != newMode) {
        m_data->vScrollBarMode = newMode;
        updateScrollBars();
    }
}

void ScrollView::setScrollBarsMode(ScrollBarMode newMode)
{
    m_data->hScrollBarMode = m_data->vScrollBarMode = newMode;
    updateScrollBars();
}

void ScrollView::setStaticBackground(bool flag)
{
    m_data->hasStaticBackground = flag;
}

int ScrollView::updateScrollInfo(short type, int current, int max, int pageSize)
{
    if (!parent()) {
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin   = 0;
        si.nMax   = max;
        si.nPage  = pageSize;
        si.nPos   = current;
        SetScrollInfo(containingWindow(), type, &si, TRUE);
        GetScrollInfo(containingWindow(), type, &si);
        return si.nPos;
    }

    // FIXME: Handle scrolling for subframes
    return current;
}

void ScrollView::updateScrollBars()
{ 
    IntSize maxScrollPosition(contentsWidth() - visibleWidth(), contentsHeight() - visibleHeight());
    IntSize scroll = scrollOffset().shrunkTo(maxScrollPosition);
    scroll.clampNegativeToZero();

    m_data->scrollOffset = 
        IntSize(updateScrollInfo(SB_HORZ, scroll.width(), contentsWidth() - 1, width()),
                updateScrollInfo(SB_VERT, scroll.height(), contentsHeight() - 1, height()));

    // FIXME: Use custom scrollbars, which will allow us to show/hide scrollbars for subframes, too
    if (!parent()) {
        if (m_data->hScrollBarMode != ScrollBarAuto || m_data->suppressScrollBars)
            ShowScrollBar(containingWindow(), SB_HORZ, (m_data->hScrollBarMode != ScrollBarAlwaysOff) && !m_data->suppressScrollBars);
        if (m_data->vScrollBarMode != ScrollBarAuto || m_data->suppressScrollBars)
            ShowScrollBar(containingWindow(), SB_VERT, (m_data->vScrollBarMode != ScrollBarAlwaysOff) && !m_data->suppressScrollBars);
    }
}

void ScrollView::addChild(Widget* child, int x, int y) 
{ 
    // we don't need to do the offscreen position initialization that KDE needs
    if (x != -500000)
        child->move(x, y);

    child->setParent(this);
    child->setContainingWindow(containingWindow());
}

void ScrollView::removeChild(Widget* child)
{ 
    child->setParent(0);
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

    context->addClip(frameGeometry());

    context->translate(x(), y());
    documentDirtyRect.move(-x(), -y());

    context->translate(-contentsX(), -contentsY());
    documentDirtyRect.move(contentsX(), contentsY());

    static_cast<const FrameView*>(this)->frame()->paint(context, documentDirtyRect);

    context->restore();
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

} // namespace WebCore
