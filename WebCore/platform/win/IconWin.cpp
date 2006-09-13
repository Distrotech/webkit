/*
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
#include "Icon.h"

#include "GraphicsContext.h"
#include "PlatformString.h"
#include <windows.h>

#if PLATFORM(CAIRO)
#include <cairo-win32.h>
#endif

namespace WebCore {

Icon::Icon()
    : m_hIcon(0)
{
}

Icon::~Icon()
{
    if (m_hIcon)
        DestroyIcon(m_hIcon);
}

PassRefPtr<Icon> Icon::newIconForFile(const String& filename)
{
    SHFILEINFO sfi;
    memset(&sfi, 0, sizeof(sfi));

    String tmpFilename = filename;
    if (SHGetFileInfo(tmpFilename.charactersWithNullTermination(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SHELLICONSIZE | SHGFI_SMALLICON)) {
        PassRefPtr<Icon> icon(new Icon());  
        icon->m_hIcon = sfi.hIcon;
        return icon;
    }

    return PassRefPtr<Icon>(0);
}

void Icon::paint(GraphicsContext* context, const IntRect& r)
{
    if (context->paintingDisabled())
        return;

#if PLATFORM(CAIRO)
    cairo_surface_t* surface = cairo_get_target(context->platformContext());
    HDC hdc = cairo_win32_surface_get_dc(surface);
    SaveDC(hdc);

    DrawIconEx(hdc, r.x(), r.y(), m_hIcon, r.width(), r.height(), 0, 0, DI_NORMAL);

    RestoreDC(hdc, -1);
#endif
}

}
