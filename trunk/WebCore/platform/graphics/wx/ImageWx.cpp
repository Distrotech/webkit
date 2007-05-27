/*
 * Copyright (C) 2007 Apple Computer, Kevin Ollivier.  All rights reserved.
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
#include "Image.h"
#include "BitmapImage.h"

#include "FloatRect.h"
#include "GraphicsContext.h"
#include "AffineTransform.h"
#include <math.h>
#include <stdio.h>

#include <wx/defs.h>
#include <wx/image.h>
#include <wx/dc.h>
#include <wx/dcmemory.h>
#include <wx/bitmap.h>
#include <wx/graphics.h>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d (%s)\n", __FILE__, __LINE__, __FUNCTION__); } while(0)

// This function loads resources from WebKit
Vector<char> loadResourceIntoArray(const char*);

namespace WebCore {

// this is in wxGraphicsContext.cpp
int getWxCompositingOperation(CompositeOperator op, bool hasAlpha);

void FrameData::clear()
{
    if (m_frame) {
        m_frame = 0;
        m_duration = 0.;
        m_hasAlpha = true;
    }
}

// ================================================
// Image Class
// ================================================

Image* Image::loadPlatformResource(const char *name)
{
    Vector<char> arr = loadResourceIntoArray(name);
    Image* img = new BitmapImage();
    RefPtr<SharedBuffer> buffer = new SharedBuffer(arr.data(), arr.size());
    img->setData(buffer, true);
    return img;
}

void BitmapImage::initPlatformData()
{
    // FIXME: NYI
}

// Drawing Routines

void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& dst, const FloatRect& src, CompositeOperator op)
{
    if (!m_source.initialized())
        return;

#if USE(WXGC)
    wxGraphicsContext* context = ctxt->platformContext();
#else
    wxDC* context = ctxt->platformContext();
#endif

    wxBitmap* bitmap = frameAtIndex(m_currentFrame);
    if (!bitmap) // If it's too early we won't have an image yet.
        return;

    IntSize selfSize = size();                       
    FloatRect srcRect(src);
    FloatRect dstRect(dst);
    
    // If we're drawing a sub portion of the image or scaling then create
    // a pattern transformation on the image and draw the transformed pattern.
    // Test using example site at http://www.meyerweb.com/eric/css/edge/complexspiral/demo.html
    float scaleX = srcRect.width() / dstRect.width();
    float scaleY = srcRect.height() / dstRect.height();
   
// FIXME: Transparency doesn't seem to be respected when we do this...
#if USE(WXGC)
    context->DrawBitmap(*bitmap, dst.x(), dst.y(), dst.width(), dst.height()); 
#else
    context->DrawBitmap(*bitmap, dst.x(), dst.y(), true);
#endif
    startAnimation();
}


void BitmapImage::drawPattern(GraphicsContext* ctxt, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, CompositeOperator, const FloatRect& dstRect)
{
    if (!m_source.initialized())
        return;

#if USE(WXGC)
    wxGraphicsContext* context = ctxt->platformContext();
#else
    wxDC* context = ctxt->platformContext();
#endif

    wxBitmap* bitmap = frameAtIndex(m_currentFrame);
    if (!bitmap) // If it's too early we won't have an image yet.
        return;
   
    float currentW = 0;
    float currentH = 0;
    
#if USE(WXGC)
    context->ConcatTransform(patternTransform);
#else
    wxMemoryDC* mydc = new wxMemoryDC();
    mydc->SelectObject(*bitmap);
#endif

    while ( currentW < dstRect.width() )
    {
        while ( currentH < dstRect.height() )
        {
// FIXME: This doesn't seem to work for transparent images.
#if USE(WXGC)
            context->DrawBitmap(*bitmap, dstRect.x() + currentW, dstRect.y() + currentH, srcRect.width(), srcRect.height()); 
#else
            context->DrawBitmap(*bitmap, dstRect.x() + currentW, dstRect.y() + currentH, true);
#endif
            currentH += srcRect.height();
        }
        currentW += srcRect.width();
        currentH = 0;
    }

    startAnimation();

}

void BitmapImage::checkForSolidColor()
{

}

void BitmapImage::invalidatePlatformData()
{

}

wxBitmap* BitmapImage::getWxBitmap() const
{
    return const_cast<BitmapImage*>(this)->frameAtIndex(0);
}


}
