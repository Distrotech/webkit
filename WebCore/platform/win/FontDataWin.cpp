/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <winsock2.h>
#include "Font.h"
#include "FontCache.h"
#include "FontData.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include <wtf/MathExtras.h>
#include <unicode/uchar.h>
#include <unicode/unorm.h>
#if PLATFORM(CAIRO)
#include <cairo.h>
#include <cairo-win32.h>
#elif PLATFORM(CG)
#include <ApplicationServices/ApplicationServices.h>
#endif
#include <mlang.h>

namespace WebCore
{

void FontData::platformInit()
{    
    m_isMLangFont = false;

    HDC dc = GetDC(0);
    SaveDC(dc);

    SelectObject(dc, m_font.hfont());

    // FIXME: Need a CG code path for fetching metrics.  This is done using WebKitSystemInterface on Mac,
    // so CG does have a private method for this.
    TEXTMETRIC tm;
    GetTextMetrics(dc, &tm);
    m_ascent = lroundf(tm.tmAscent);
    m_descent = lroundf(tm.tmDescent);
    m_xHeight = m_ascent * 0.56f;  // Best guess for xHeight for non-Truetype fonts.
    m_lineGap = lroundf(tm.tmExternalLeading);
    m_lineSpacing = m_ascent + m_descent + m_lineGap;

    OUTLINETEXTMETRIC otm;
    if (GetOutlineTextMetrics(dc, sizeof(otm), &otm) > 0) {
        // This is a TrueType font.  We might be able to get an accurate xHeight.
        GLYPHMETRICS gm;
        MAT2 mat = { 1, 0, 0, 1 }; // The identity matrix.
        DWORD len = GetGlyphOutlineW(dc, 'x', GGO_METRICS, &gm, 0, 0, &mat);
        if (len != GDI_ERROR && gm.gmptGlyphOrigin.y > 0)
            m_xHeight = gm.gmptGlyphOrigin.y;
    }
    
    RestoreDC(dc, -1);
    ReleaseDC(0, dc);
}

void FontData::platformDestroy()
{
#if PLATFORM(CG)
    CGFontRelease(m_font.cgFont());
#elif PLATFORM(CAIRO)
    cairo_font_face_destroy(m_font.fontFace());
    cairo_scaled_font_destroy(m_font.scaledFont());
#endif

    if (m_isMLangFont) {
        // We have to release the font instead of just deleting it, since we didn't make it.
        IMLangFontLink2* langFontLink = FontCache::getFontLinkInterface();
        if (langFontLink)
            langFontLink->ReleaseFont(m_font.hfont());
    } else
        DeleteObject(m_font.hfont());

    // We don't hash this on Win32, so it's effectively owned by us.
    delete m_smallCapsFontData;
}

FontData* FontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        LOGFONT winfont;
        GetObject(m_font.hfont(), sizeof(LOGFONT), &winfont);
        int smallCapsHeight = lroundf(0.70f * fontDescription.computedSize());
        winfont.lfHeight = -smallCapsHeight;
        HFONT hfont = CreateFontIndirect(&winfont);
        m_smallCapsFontData = new FontData(FontPlatformData(hfont, smallCapsHeight));
    }
    return m_smallCapsFontData;
}

bool FontData::containsCharacters(const UChar* characters, int length) const
{
    HDC dc = GetDC(0);
    SaveDC(dc);
    SelectObject(dc, m_font.hfont());

    WORD* glyphBuffer = new WORD[length];
    GetGlyphIndices(dc, characters, length, glyphBuffer, GGI_MARK_NONEXISTING_GLYPHS);
    
    RestoreDC(dc, -1);
    ReleaseDC(0, dc);

    for (unsigned i = 0; i < length; i++) {
        if (glyphBuffer[i] == 0xFFFFFFFF) {
            delete []glyphBuffer;
            return false;
        }
    }

    delete []glyphBuffer;
    return true;
}

void FontData::determinePitch()
{
    // TEXTMETRICS have this.  Set m_treatAsFixedPitch based off that.
    HDC dc = GetDC((HWND)0);
    SaveDC(dc);
    SelectObject(dc, m_font.hfont());

    // Yes, this looks backwards, but the fixed pitch bit is actually set if the font
    // is *not* fixed pitch.  Unbelievable but true.
    TEXTMETRIC tm;
    GetTextMetrics(dc, &tm);
    m_treatAsFixedPitch = ((tm.tmPitchAndFamily & TMPF_FIXED_PITCH) == 0);

    RestoreDC(dc, -1);
    ReleaseDC(0, dc);
}

float FontData::platformWidthForGlyph(Glyph glyph) const
{
    // FIXME: Need a CG code path for this.
    HDC dc = GetDC(0);
    SaveDC(dc);

    SelectObject(dc, m_font.hfont());

    int width;
    GetCharWidthI(dc, glyph, 1, 0, &width);

    RestoreDC(dc, -1);
    ReleaseDC(0, dc);

    return width;
}

}
