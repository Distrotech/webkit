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
#include "WidgetClient.h"
#include <winsock2.h>
#include <windows.h>

namespace WebCore {

class WidgetPrivate
{
public:
    Font font;
    WidgetClient* client;
    Widget* parent;
    HWND containingWindow;
    IntRect frameRect;
    bool enabled;
};

Widget::Widget()
    : data(new WidgetPrivate)
{
    data->client = 0;
    data->parent = 0;
    data->containingWindow = 0;
    data->enabled = true;
}

Widget::~Widget() 
{
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

void Widget::setParent(Widget* w)
{
    data->parent = w;
}

Widget* Widget::parent() const
{
    return data->parent;
}

bool Widget::hasFocus() const
{
    if (WidgetClient* client = data->client)
        if (Element* element = client->element(const_cast<Widget*>(this)))
            if (element == element->document()->focusNode())
                return true;

    return false;
}

void Widget::setFocus()
{
}

void Widget::clearFocus()
{
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

IntRect Widget::convertToContainingWindow(const IntRect& rect) const
{
    IntRect convertedRect = rect;
    convertedRect.setLocation(convertToContainingWindow(convertedRect.location()));
    return convertedRect;
}

IntPoint Widget::convertToContainingWindow(const IntPoint& point) const
{
    IntPoint convertedPoint = point;

    const Widget* w = this;
    do {
        convertedPoint.move(w->x(), w->y());
        w = w->parent();
    } while (w);

    return convertedPoint;
}

IntPoint Widget::convertFromContainingWindow(const IntPoint& point) const
{
    IntPoint convertedPoint = point;

    const Widget* w = this;
    do {
        convertedPoint.move(-w->x(), -w->y());
        w = w->parent();
    } while (w);

    return convertedPoint;
}

void Widget::paint(GraphicsContext*, const IntRect&)
{
}

void Widget::setEnabled(bool e)
{
    if (e != data->enabled) {
        data->enabled = e;
        invalidate();
    }
}

void Widget::invalidate()
{
    invalidateRect(frameGeometry());
}

void Widget::invalidateRect(const IntRect& r)
{
    IntRect windowRect = convertToContainingWindow(r);
    ::InvalidateRect(containingWindow(), &RECT(windowRect), false);
}

} // namespace WebCore
