/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "WebCoreTextRenderer.h"

#include "Font.h"
#include "FontDescription.h"
#include "GraphicsContext.h"
#include "TextStyle.h"

#include <CoreGraphics/CoreGraphics.h>

namespace WebCore {

void WebCoreDrawTextAtPoint(const String& text, HDC dc, RECT clipRect, const String& fontFamily, int size, bool bold, bool italic, CGColorRef color)
{
    GraphicsContext graphicsContext(dc);

    FontDescription f;
    FontFamily family;
    family.setFamily(fontFamily);
    f.setFamily(family);
    f.setSpecifiedSize(size);
    f.setComputedSize(size);
    f.setItalic(italic);
    f.setBold(bold);
    f.setIsAbsoluteSize(true);

    Font renderer(f, 0, 0);
    renderer.update();

    TextRun run(text.characters(), text.length());
    TextStyle style;
    ASSERT(CGColorGetNumberOfComponents(color) == 4);
    const CGFloat* components = CGColorGetComponents(color);
    graphicsContext.save();
    graphicsContext.setFillColor(makeRGBA((int)(components[0] * 255), (int)(components[1] * 255), (int)(components[2] * 255), (int)(components[3] * 255)));
    graphicsContext.clip(IntRect(clipRect.left, clipRect.top, clipRect.right-clipRect.left, clipRect.bottom-clipRect.top));
    renderer.drawText(&graphicsContext, run, style, FloatPoint(clipRect.left, clipRect.top+renderer.ascent()));
    graphicsContext.restore();
}

}
