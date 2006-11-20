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
#include "ContextMenu.h"

#include "CString.h"
#include "NotImplemented.h"
#include <windows.h>

namespace WebCore {

unsigned ContextMenu::itemCount()
{
    if (!m_menu)
        return 0;

    return ::GetMenuItemCount(m_menu);
}

void ContextMenu::insertItem(unsigned int position, ContextMenuItem item)
{
    if (!m_menu) {
        HMENU menu = ::CreatePopupMenu();
        if (!menu)
            return;
        setPlatformMenuDescription(menu);
    }

    MENUITEMINFO info;
    info.cbSize = sizeof(info);
    info.fMask = MIIM_FTYPE;
    
    LPWSTR titleString;

    switch (item.type) {
        case ActionType:
            info.fMask |= MIIM_STRING | MIIM_ID;
            info.fType = MFT_STRING;
            info.wID = item.action;
            info.cch = item.title.length();
            titleString = _wcsdup(item.title.charactersWithNullTermination());
            info.dwTypeData = titleString;
            break;
        case SeparatorType:
            info.fType = MFT_SEPARATOR;
            break;
        case SubmenuType:
        default:
            ASSERT_NOT_REACHED();
            break;
    }

    ::InsertMenuItem(m_menu, position, true, &info);

    if (item.type == ActionType)
        free(titleString);
}

void ContextMenu::appendItem(ContextMenuItem item)
{
    insertItem(itemCount(), item);
}

void ContextMenu::setPlatformMenuDescription(HMENU menu)
{
    if (!menu || menu == m_menu)
        return;
    
    if (m_menu)
        ::DestroyMenu(m_menu);

    m_menu = menu;

    MENUINFO info;
    info.cbSize = sizeof(info);
    info.fMask = MIM_STYLE;
    info.dwStyle = MNS_NOTIFYBYPOS;
    ::SetMenuInfo(m_menu, &info);
}

void ContextMenu::show()
{
}

void ContextMenu::hide()
{
}

}
