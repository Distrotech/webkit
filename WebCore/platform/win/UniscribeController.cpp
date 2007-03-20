/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "UniscribeController.h"
#include "Font.h"
#include "FontData.h"
#include "TextStyle.h"
#include <wtf/MathExtras.h>

namespace WebCore {

UniscribeController::UniscribeController(const Font* font, const TextRun& run, const TextStyle& style, 
                                         GraphicsContext* graphicsContext)
: m_font(*font)
, m_run(run)
, m_style(style)
, m_context(graphicsContext)
, m_width(0)
{
    // Null out our uniscribe structs
    resetControlAndState();

    // FIXME: We really want to be using a newer version of Uniscribe that supports the new OpenType
    // functions.  Those functions would allow us to turn off kerning and ligatures.  Without being able
    // to do that, we will have buggy line breaking and metrics when simple and complex text are close
    // together (the complex code path will narrow the text because of kerning and ligatures and then
    // when bidi processing splits into multiple runs, the simple portions will get wider and cause us to
    // spill off the edge of a line).

    // Now itemize the string.
    m_items.resize(6);
    int numItems = 0;
    while (ScriptItemize(run.characters(), run.length(), m_items.size() - 1, &m_control, &m_state, m_items.data(), &numItems) == E_OUTOFMEMORY) {
        m_items.resize(m_items.size() * 2);
        resetControlAndState();
    }
    m_items.resize(numItems + 1);

    for (unsigned i = 0; i < m_items.size() - 1; i++)
        shapeAndPlaceItem(i);    
}

void UniscribeController::resetControlAndState()
{
    memset(&m_control, 0, sizeof(SCRIPT_CONTROL));
    memset(&m_state, 0, sizeof(SCRIPT_STATE));

    // Set up the correct direction for the run.
    m_state.uBidiLevel = m_style.rtl();
    
    // Lock the correct directional override.  This is much simpler than ATSUI.  We don't
    // need to access style.directionalOverride(), since Uniscribe just supports an absolute
    // override.
    m_state.fOverrideDirection = true;
}

void UniscribeController::shapeAndPlaceItem(unsigned i)
{
    // Determine the string for this item.
    const UChar* str = m_run.characters() + m_items[i].iCharPos;
    unsigned len = m_items[i+1].iCharPos - m_items[i].iCharPos;
    SCRIPT_ITEM item = m_items[i];

    // Get our current FontData that we are using.
    unsigned dataIndex = 0;
    const FontData* fontData = m_font.fontDataAt(dataIndex);

    // Set up buffers to hold the results of shaping the item.
    Vector<WORD> glyphs;
    Vector<WORD> clusters;
    Vector<SCRIPT_VISATTR> visualAttributes;
    clusters.resize(len + 1);
     
    // Shape the item.  This will provide us with glyphs for the item.  We will
    // attempt to shape using the first available FontData.  If the shaping produces a result with missing
    // glyphs, then we will fall back to the next FontData.
    // FIXME: This isn't as good as per-glyph fallback, but in practice it should be pretty good, since
    // items are broken up by "shaping engine", meaning unique scripts will be broken up into
    // separate items.
    bool lastResortFontTried = false;
    while (fontData) {
        // The recommended size for the glyph buffer is 1.5 * the character length + 16 in the uniscribe docs.
        // Apparently this is a good size to avoid having to make repeated calls to ScriptShape.
        glyphs.resize(1.5 * len + 16);
        visualAttributes.resize(glyphs.size());
   
        if (shape(str, len, item, fontData, glyphs, clusters, visualAttributes))
            break;
        else {
            // Try again with the next font in the list.
            if (lastResortFontTried) {
                fontData = 0;
                break;
            } else {
                fontData = m_font.fontDataAt(++dataIndex);
                if (!fontData) {
                    // Out of fonts.  Get a font data based on the actual characters.
                    fontData = m_font.fontDataForCharacters(str, len);
                    lastResortFontTried = true;
                }
            }
        }
    }

    // Just give up.  We were unable to shape.
    if (!fontData)
        return;

    // We now have a collection of glyphs.
    // FIXME: Use the cluster and visual attr information to do letter-spacing, word-spacing
    // and justification.
    // FIXME: Support rounding hacks.
    // FIXME: Support smallcaps (re-itemize and re-shape to do this?).
    Vector<GOFFSET> offsets;
    Vector<int> advances;
    offsets.resize(glyphs.size());
    advances.resize(glyphs.size());
    int glyphCount = 0;
    HRESULT placeResult = ScriptPlace(0, fontData->scriptCache(), glyphs.data(), glyphs.size(), visualAttributes.data(),
                                      &item.a, advances.data(), offsets.data(), 0);
    if (placeResult == E_PENDING) {
        // The script cache isn't primed with enough info yet.  We need to select our HFONT into
        // a DC and pass the DC in to ScriptPlace.
        HDC hdc = GetDC(0);
        HFONT hfont = fontData->platformData().hfont();
        HFONT oldFont = (HFONT)SelectObject(hdc, hfont);
        placeResult = ScriptPlace(hdc, fontData->scriptCache(), glyphs.data(), glyphs.size(), visualAttributes.data(),
                                  &item.a, advances.data(), offsets.data(), 0);
        SelectObject(hdc, oldFont);
        ReleaseDC(0, hdc);
    }
    
    if (FAILED(placeResult))
        return;

    // Populate our glyph buffer with this information.
    for (unsigned i = 0; i < glyphs.size(); i++) {
        Glyph glyph = glyphs[i];
        float advance = advances[i] / 32.0f;
        if (!m_font.isPrinterFont() && !fontData->isSystemFont())
            advance = lroundf(advance);
        GOFFSET offset = offsets[i]; // FIXME: What the heck do i do with this?
        m_width += advance;
        m_glyphBuffer.add(glyph, fontData, advance);
    }

    // Swap the order of the glyphs if right-to-left.
    if (m_style.rtl())
        for (int j = 0, end = m_glyphBuffer.size() - 1; j < m_glyphBuffer.size() / 2; ++j, --end)
            m_glyphBuffer.swap(j, end);
}

bool UniscribeController::shape(const UChar* str, int len, SCRIPT_ITEM item, const FontData* fontData,
                                Vector<WORD>& glyphs, Vector<WORD>& clusters,
                                Vector<SCRIPT_VISATTR>& visualAttributes)
{
    HDC hdc = 0;
    HFONT oldFont = 0;
    HRESULT shapeResult = E_PENDING;
    int glyphCount = 0;
    do {
        item.a.fLogicalOrder = true;
        shapeResult = ScriptShape(hdc, fontData->scriptCache(), str, len, glyphs.size(), &item.a,
                                  glyphs.data(), clusters.data(), visualAttributes.data(), &glyphCount);
        if (shapeResult == E_PENDING) {
            // The script cache isn't primed with enough info yet.  We need to select our HFONT into
            // a DC and pass the DC in to ScriptShape.
            ASSERT(!hdc);
            hdc = GetDC(0);
            HFONT hfont = fontData->platformData().hfont();
            oldFont = (HFONT)SelectObject(hdc, hfont);
        } else if (shapeResult == E_OUTOFMEMORY) {
            // Need to resize our buffers.
            glyphs.resize(glyphs.size() * 2);
            visualAttributes.resize(glyphs.size());
        }
    } while (shapeResult == E_PENDING || shapeResult == E_OUTOFMEMORY);

    if (hdc) {
        SelectObject(hdc, oldFont);
        ReleaseDC(0, hdc);
    }

    if (FAILED(shapeResult))
        return false;
    
    // We may still have missing glyphs even if we succeeded.  We need to treat missing glyphs as
    // a failure so that we will fall back to another font.
    bool containsMissingGlyphs = false;
    SCRIPT_FONTPROPERTIES* fontProperties = fontData->scriptFontProperties();
    for (int i = 0; i < glyphCount; i++) {
        WORD glyph = glyphs[i];
        if (glyph == fontProperties->wgDefault ||
            (glyph == fontProperties->wgInvalid && glyph != fontProperties->wgBlank)) {
            containsMissingGlyphs = true;
            break;
        }
    }

    if (containsMissingGlyphs)
        return false;

    glyphs.resize(glyphCount);
    visualAttributes.resize(glyphCount);

    return true;
}

}
