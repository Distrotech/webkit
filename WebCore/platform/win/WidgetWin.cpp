/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "Widget.h"

#include "Cursor.h"
#include "Document.h"
#include "Element.h"
#include "GraphicsContext.h"
#include "FrameWin.h"
#include "IntRect.h"
#include "Font.h"
#include "ScrollView.h"
#include "WidgetClient.h"
#include <winsock2.h>
#include <windows.h>

namespace WebCore {

class WidgetPrivate
{
public:
    Font font;
    WidgetClient* client;
    ScrollView* parent;
    HWND containingWindow;
    IntRect frameRect;
    bool enabled;
    Widget* capturingChild;
    bool suppressInvalidation;
};

Widget::Widget()
    : data(new WidgetPrivate)
{
    data->client = 0;
    data->parent = 0;
    data->containingWindow = 0;
    data->enabled = true;
    data->capturingChild = 0;
    data->suppressInvalidation = false;
}

Widget::~Widget() 
{
    ASSERT(!parent());
    delete data;
}

void Widget::setContainingWindow(HWND containingWindow)
{
    data->containingWindow = containingWindow;
}

HWND Widget::containingWindow() const
{
    return data->containingWindow;
}

void Widget::setClient(WidgetClient* c)
{
    data->client = c;
}

WidgetClient* Widget::client() const
{
    return data->client;
}

IntRect Widget::frameGeometry() const
{
    return data->frameRect;
}

void Widget::setFrameGeometry(const IntRect &rect)
{
    data->frameRect = rect;
}

void Widget::setParent(ScrollView* v)
{
    data->parent = v;
}

ScrollView* Widget::parent() const
{
    return data->parent;
}

void Widget::removeFromParent()
{
    if (parent())
        parent()->removeChild(this);
}

void Widget::show()
{
}

void Widget::hide()
{
}

const Font& Widget::font() const
{
    return data->font;
}

void Widget::setFont(const Font& font)
{
    data->font = font;
}

void Widget::setCursor(const Cursor& cursor)
{
    // SetCursor only works until the next event is received.
    // However, we call this method on every mouse-moved,
    // so this should work well enough for our purposes.
    if (HCURSOR c = cursor.impl())
        SetCursor(c);
}

IntPoint Widget::convertToContainingWindow(const IntPoint& point) const
{
    IntPoint windowPoint = point;
    for (const Widget *parentWidget = parent(), *childWidget = this;
         parentWidget;
         childWidget = parentWidget, parentWidget = parentWidget->parent())
        windowPoint = parentWidget->convertChildToSelf(childWidget, windowPoint);
    return windowPoint;
}

IntPoint Widget::convertFromContainingWindow(const IntPoint& point) const
{
    IntPoint widgetPoint = point;
    for (const Widget *parentWidget = parent(), *childWidget = this;
         parentWidget;
         childWidget = parentWidget, parentWidget = parentWidget->parent())
        widgetPoint = parentWidget->convertSelfToChild(childWidget, widgetPoint);
    return widgetPoint;
}

IntRect Widget::convertToContainingWindow(const IntRect& rect) const
{
    IntRect convertedRect = rect;
    convertedRect.setLocation(convertToContainingWindow(convertedRect.location()));
    return convertedRect;
}

IntPoint Widget::convertChildToSelf(const Widget* child, const IntPoint& point) const
{
    return IntPoint(point.x() + child->x(), point.y() + child->y());
}

IntPoint Widget::convertSelfToChild(const Widget* child, const IntPoint& point) const
{
    return IntPoint(point.x() - child->x(), point.y() - child->y());
}

void Widget::paint(GraphicsContext*, const IntRect&)
{
}

bool Widget::isEnabled() const
{
    return data->enabled;
}

void Widget::setEnabled(bool e)
{
    if (e != data->enabled) {
        data->enabled = e;
        invalidate();
    }
}

bool Widget::suppressInvalidation() const
{
    return data->suppressInvalidation;
}

void Widget::setSuppressInvalidation(bool suppress)
{
    data->suppressInvalidation = suppress;
}

void Widget::invalidate()
{
    invalidateRect(IntRect(0, 0, width(), height()));
}

void Widget::invalidateRect(const IntRect& r)
{
    if (data->suppressInvalidation)
        return;

    // Get the root widget.
    ScrollView* outermostView = parent();
    while (outermostView && outermostView->parent())
        outermostView = outermostView->parent();
    if (!outermostView)
        return;

    IntRect windowRect = convertToContainingWindow(r);

    // Get our clip rect and intersect with it to ensure we don't invalidate too much.
    IntRect clipRect = windowClipRect();
    windowRect.intersect(clipRect);

    ::InvalidateRect(containingWindow(), &RECT(windowRect), false);
    outermostView->addToDirtyRegion(windowRect);
}

bool Widget::capturingMouse() const
{
    // This is true only if we are at the end of the capturing chain, i.e., if we are the capturingTarget.
    return data->capturingChild == this;
}

void Widget::setCapturingMouse(bool capturingMouse)
{
    data->capturingChild = capturingMouse ? this : 0;
    if (parent())
        parent()->setCapturingChild(capturingMouse ? this : 0);
}

Widget* Widget::capturingTarget()
{
    if (!capturingChild() || capturingChild() == this)
        return this;
    return capturingChild()->capturingTarget(); 
}

Widget* Widget::capturingChild()
{
    return data->capturingChild;
}

void Widget::setCapturingChild(Widget* w)
{
    data->capturingChild = w;
    if (parent())
        parent()->setCapturingChild(w ? this : 0);
}

void Widget::setFocus()
{
}

void Widget::clearFocus()
{
}

bool Widget::hasFocus() const
{
    return false;
}

} // namespace WebCore
