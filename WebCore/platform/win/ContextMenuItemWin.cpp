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
#include "ContextMenuItem.h"

#include "ContextMenu.h"

#include "CString.h"
#include <windows.h>

namespace WebCore {

ContextMenuItem::ContextMenuItem(LPMENUITEMINFO item, ContextMenu* parentMenu)
    : m_parentMenu(parentMenu)
    , m_platformDescription(item)
{
    switch (item->fType) {
        case MFT_STRING:
            if (item->hSubMenu)
                m_type = SubmenuType;
            else
                m_type = ActionType;
            break;
        case MFT_SEPARATOR:
            m_type = SeparatorType;
            break; 
        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

ContextMenuItem::ContextMenuItem(ContextMenu* parentMenu, ContextMenu* subMenu)
    : m_parentMenu(parentMenu)
    , m_platformDescription(0)
    , m_type(SeparatorType)
{
    m_platformDescription = (LPMENUITEMINFO)malloc(sizeof(MENUITEMINFO));
    if (!m_platformDescription)
        return;

    memset(m_platformDescription, 0, sizeof(MENUITEMINFO));
    m_platformDescription->cbSize = sizeof(MENUITEMINFO);

    m_platformDescription->wID = ContextMenuItemTagNoAction;
    if (subMenu) {
        m_platformDescription->fMask |= MIIM_SUBMENU;
        m_platformDescription->hSubMenu = subMenu->platformDescription();
    }
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* parentMenu, 
    ContextMenu* subMenu)
    : m_parentMenu(parentMenu)
    , m_type(type)
{
    m_platformDescription = (LPMENUITEMINFO)malloc(sizeof(MENUITEMINFO));
    if (!m_platformDescription)
        return;

    memset(m_platformDescription, 0, sizeof(MENUITEMINFO));
    m_platformDescription->cbSize = sizeof(MENUITEMINFO);
    m_platformDescription->fMask = MIIM_FTYPE;

    if (m_type == SeparatorType) {
        m_platformDescription->fType = MFT_SEPARATOR;
        return;
    }
    
    if (subMenu) {
        m_platformDescription->fMask |= MIIM_STRING | MIIM_SUBMENU;
        m_platformDescription->hSubMenu = subMenu->platformDescription();
    } else
        m_platformDescription->fMask |= MIIM_STRING | MIIM_ID;

    m_platformDescription->fType = MFT_STRING;
    
    if (type == ActionType)
        m_platformDescription->wID = action;
    
    String t = title;
    m_platformDescription->cch = t.length();
    m_platformDescription->dwTypeData = wcsdup(t.charactersWithNullTermination());
}

ContextMenuItem::~ContextMenuItem()
{
    if (m_platformDescription) {
        if (m_platformDescription->fType == MFT_STRING)
            free(m_platformDescription->dwTypeData);
        free(m_platformDescription);
    }
}

PlatformMenuItemDescription ContextMenuItem::platformDescription() const
{
    return m_platformDescription;
}

ContextMenuAction ContextMenuItem::action() const
{ 
    return static_cast<ContextMenuAction>(m_platformDescription->wID);
}

String ContextMenuItem::title() const 
{
    return String(m_platformDescription->dwTypeData, wcslen(m_platformDescription->dwTypeData));
}

PlatformMenuDescription ContextMenuItem::platformSubMenu() const
{
    return m_platformDescription->hSubMenu;
}

void ContextMenuItem::setAction(ContextMenuAction action)
{
    m_platformDescription->wID = action; 
}

void ContextMenuItem::setTitle(String title)
{
    if (m_platformDescription->dwTypeData)
        free(m_platformDescription->dwTypeData);
    
    m_platformDescription->cch = title.length();
    m_platformDescription->dwTypeData = wcsdup(title.charactersWithNullTermination());
}

void ContextMenuItem::setSubMenu(ContextMenu* subMenu)
{
    if (subMenu->platformDescription() == m_platformDescription->hSubMenu)
        return;

    if (m_platformDescription->hSubMenu)
        ::DestroyMenu(m_platformDescription->hSubMenu);
    
    m_platformDescription->fMask |= MIIM_SUBMENU;
    m_platformDescription->hSubMenu = subMenu->platformDescription();
}

}
