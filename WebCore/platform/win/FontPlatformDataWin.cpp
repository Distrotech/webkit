/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "FontPlatformData.h"
#if PLATFORM(CAIRO)
#include <cairo-win32.h>
#elif PLATFORM(CG)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace WebCore {

#if PLATFORM(CAIRO)

FontPlatformData::FontPlatformData(HFONT font, int size, bool bold, bool oblique)
:m_font(font), m_size(size)
{  
    m_fontFace = cairo_win32_font_face_create_for_hfont(font);
    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, size, size);

    static cairo_font_options_t* fontOptions;
    if (!fontOptions)
        // Force ClearType-level quality.
        fontOptions = cairo_font_options_create();
    cairo_font_options_set_antialias(fontOptions, CAIRO_ANTIALIAS_SUBPIXEL);

    m_scaledFont = cairo_scaled_font_create(m_fontFace, &sizeMatrix, &ctm, fontOptions);
}

#elif PLATFORM(CG)

FontPlatformData::FontPlatformData(HFONT font, int size, bool bold, bool oblique)
    : m_font(font)
    , m_size(size)
    , m_cgFont(0)
{
    HDC hdc = GetDC(0);
    SaveDC(hdc);
    
    SelectObject(hdc, font);
    UINT bufferSize = GetOutlineTextMetrics(hdc, 0, NULL);

    // Bitmap fonts are turned off for CG in FontCacheWin::createFontPlatformData
    ASSERT_WITH_MESSAGE(bufferSize != 0, "Bitmap fonts not supported with CoreGraphics.");

    if (bufferSize != 0)
    {
        OUTLINETEXTMETRICW* metrics = (OUTLINETEXTMETRICW*)malloc(bufferSize);

        if (metrics != NULL)
        {
            GetOutlineTextMetricsW(hdc, bufferSize, metrics);
            WCHAR* faceName = (WCHAR*)((uintptr_t)metrics + (uintptr_t)metrics->otmpFaceName);
            bool fontIsBold    = (metrics->otmfsSelection & (1<<5)) != 0;
            bool fontIsOblique = (metrics->otmfsSelection & (1<<0)) != 0;

            m_syntheticBold = bold && !fontIsBold;
            m_syntheticOblique = oblique && !fontIsOblique;

            CFStringRef cfName = CFStringCreateWithCharacters(NULL, (const UniChar*)faceName, wcslen(faceName));
            m_cgFont = CGFontCreateWithFontName(cfName);
            CFRelease(cfName);
        }
    }

    RestoreDC(hdc, -1);
    ReleaseDC(0, hdc);
}

#endif

FontPlatformData::~FontPlatformData()
{
}

}
