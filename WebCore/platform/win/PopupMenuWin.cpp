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
#include "PopupMenu.h"

#include "FloatRect.h"
#include "FontData.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "Page.h"
#include "RenderMenuList.h"
#include "RenderView.h"
#include "Screen.h"
#include <tchar.h>
#include <windows.h>

// Default Window animation duration in milliseconds
static const int defaultAnimationDuration = 200;
// Maximum height of a popup window
static const int maxPopupHeight = 320;

namespace WebCore {

static LPCTSTR kPopupWindowClassName = _T("PopupWindowContainerClass");
static ATOM registerPopup();
static LRESULT CALLBACK PopupWndProc(HWND, UINT, WPARAM, LPARAM);

PopupMenu::PopupMenu(RenderMenuList* m)
    : m_menuList(m)
    , m_popup(0)
    , m_container(0)
    , m_wasClicked(false)
{
    // FIXME: Might be nice to move all this code (which can possibly fail) into an initialization
    // function so that we don't have a failing constructor (which is hard/impossible to check for)
    
    registerPopup();

    m_container = CreateWindowEx(0, kPopupWindowClassName, _T("PopupMenuContainer"),
        WS_POPUP | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        menuList()->document()->frame()->view()->containingWindow(), 0, 0, 0);

    if (!m_container)
        return;

    SetWindowLongPtr(m_container, 0, (LONG_PTR)this);

    // FIXME: Should we also use LBS_COMBOBOX here? What does that do?
    m_popup = CreateWindowEx(0, _T("ListBox"), _T("PopupMenu"),
        WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS | LBS_WANTKEYBOARDINPUT,
        0, 0, 0, 0,
        m_container, 0, 0, 0);
}

PopupMenu::~PopupMenu()
{
    if (m_popup)
        DestroyWindow(m_popup);
    if (m_container)
        DestroyWindow(m_container);
}

void PopupMenu::clear()
{
    if (!m_popup)
        return;

    SendMessage(m_popup, LB_RESETCONTENT, 0, 0);
}

void PopupMenu::populate()
{
    if (!m_popup)
        return;

    clear();

    PopupMenu::addItems();
}

void PopupMenu::show(const IntRect& r, FrameView* v, int index)
{
    if (!m_popup)
        return;

    // FIXME: Should use RenderThemeWin::systemFont unless explicitly styled
    SendMessage(m_popup, WM_SETFONT, (WPARAM)menuList()->font(false).fontDataAt(0)->platformData().hfont(), MAKELPARAM(false, 0));

    populate();
    if (SendMessage(m_popup, LB_GETCOUNT, 0, 0) <= 0)
        return;
    ASSERT(SendMessage(m_popup, LB_GETCOUNT, 0, 0) > index);

    setPositionAndSize(r, v);

    SendMessage(m_popup, LB_SETCURSEL, index, 0);

    // Finally we can show the popup
    ShowWindow(m_container, SW_SHOW);
    ShowWindow(m_popup, SW_SHOW);

    SetFocus(m_popup);
}

void PopupMenu::hide()
{
    ShowWindow(m_container, SW_HIDE);
}

void PopupMenu::setPositionAndSize(const IntRect& r, FrameView* v)
{
    // r is in absolute document coordinates, but we want to be coordinates relative to the WebView
    IntRect rViewCoords(v->contentsToWindow(r.location()), r.size());

    // First, size the popup
    int itemHeight = SendMessage(m_popup, LB_GETITEMHEIGHT, 0, 0);
    HTMLSelectElement *select = static_cast<HTMLSelectElement*>(menuList()->node());
    const Vector<HTMLElement*>& items = select->listItems();
    size_t size = items.size();
    // Add an extra (itemHeight / 2) here because the popup will shrink itself to not show any partial items when we call SetWindowPos()
    int popupHeight = min(maxPopupHeight, itemHeight * size + itemHeight / 2);

    IntRect popupRect(rViewCoords.x(), rViewCoords.bottom(), rViewCoords.width(), popupHeight);

    // WS_POPUP windows are positioned in screen coordinates, but popupRect is in WebView coordinates,
    // so we have to find the screen origin of the FrameView to position correctly
    RECT viewRect = {0};
    GetWindowRect(v->frame()->view()->containingWindow(), &viewRect);
    if (IsRectEmpty(&viewRect))
        return;

    // Translate popupRect into screen coordinates
    popupRect.move(viewRect.left, viewRect.top);

    // The popup needs to stay within the bounds of the screen and not overlap any toolbars
    FloatRect screen = usableScreenRect(v->frame()->page());

    if (popupRect.bottom() > screen.height()) {
        // The popup will go off the screen, so try placing it above the menulist
        if (viewRect.top + rViewCoords.y() - popupRect.height() < 0) {
            // The popup won't fit above, either, so place it whereever's bigger and resize it to fit
            if ((viewRect.top + rViewCoords.y() + rViewCoords.height() / 2) < (screen.height() / 2)) {
                // Below is bigger
                popupRect.setHeight(screen.height() - popupRect.y());
            } else {
                // Above is bigger
                popupRect.setY(0);
                popupRect.setHeight(viewRect.top + rViewCoords.y());
            }
        } else {
            // The popup fits above, so reposition it
            popupRect.setY(viewRect.top + rViewCoords.y() - popupRect.height());
        }
    }

    // Place the popup container and the popup.
    // The container (with style WS_POPUP) is positioned in screen coordinates, and the popup
    // (with style WS_CHILD) is positioned relative to the container.
    SetWindowPos(m_container, HWND_TOP, popupRect.x(), popupRect.y(), popupRect.width(), popupRect.height(), SWP_NOACTIVATE);
    SetWindowPos(m_popup, 0, 0, 0, popupRect.width(), popupRect.height(), SWP_NOACTIVATE | SWP_NOZORDER);

    // Placing the popup also resizes it to an integral multiple of the item height (so that no partial items are shown)
    // This means we need to resize the container.
    RECT rect = {0};
    GetWindowRect(m_popup, &rect);
    if (IsRectEmpty(&rect))
        return;
    popupRect.setHeight(rect.bottom - rect.top);

    // Also, if the popup is above the <select> box, the popup will be too high after resizing
    // (because it's gotten shorter), so in that case we must reposition it.
    if (popupRect.y() < viewRect.top + rViewCoords.y())
        popupRect.setY(viewRect.top + rViewCoords.y() - popupRect.height());

    SetWindowPos(m_container, HWND_TOP, popupRect.x(), popupRect.y(), popupRect.width(), popupRect.height(), SWP_NOACTIVATE);

    // Determine whether we should animate our popups
    // Note: Must use 'BOOL' and 'FALSE' instead of 'bool' and 'false' to avoid stack corruption with SystemParametersInfo
    BOOL shouldAnimate = FALSE;
    SystemParametersInfo(SPI_GETCOMBOBOXANIMATION, 0, &shouldAnimate, 0);

    if (shouldAnimate) {
        // Set the windows to animate when shown
        // NOTE: This may have to change for Vista

        // Popups should slide into view away from the <select> box
        DWORD slideDirection = (popupRect.y() < viewRect.top + rViewCoords.y()) ? AW_VER_NEGATIVE : AW_VER_POSITIVE;

        AnimateWindow(m_container, defaultAnimationDuration, AW_SLIDE | slideDirection);
        AnimateWindow(m_popup, defaultAnimationDuration, AW_SLIDE | slideDirection | AW_ACTIVATE);
    }
}

void PopupMenu::addOption(HTMLOptionElement* element)
{
    if (!m_popup)
        return;

    String optionText = element->optionText();
 
    UChar* text = (UChar*)malloc((optionText.length() + 1) * sizeof(UChar));
    if (!text)
        return;

    if (_tcsncpy_s(text, optionText.length() + 1, optionText.characters(), optionText.length())) {
        free(text);
        return;
    }

    SendMessage(m_popup, LB_ADDSTRING, 0, (LPARAM)text);

    free(text);
}

int PopupMenu::focusedIndex() const
{
    if (!m_popup)
        return -1;

    return SendMessage(m_popup, LB_GETCURSEL, 0, 0);
}
bool PopupMenu::down()
{
    if (!m_popup)
        return false;

    int sel = focusedIndex() + 1;
    if (sel < 0 || sel >= SendMessage(m_popup, LB_GETCOUNT, 0, 0))
        return false;

    menuList()->setTextFromOption(sel);
    menuList()->valueChanged(sel, false);

    return true;
}

bool PopupMenu::up()
{
    if (!m_popup)
        return false;

    int sel = focusedIndex() - 1;
    if (sel < 0 || sel >= SendMessage(m_popup, LB_GETCOUNT, 0, 0))
        return false;

    menuList()->setTextFromOption(sel);
    menuList()->valueChanged(sel, false);

    return true;
}

static ATOM registerPopup()
{
    static bool haveRegisteredWindowClass = false;

    if (haveRegisteredWindowClass)
        return true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = 0;
    wcex.lpfnWndProc    = PopupWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = sizeof(void*); // For the PopupMenu pointer
    wcex.hInstance      = Page::instanceHandle();
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kPopupWindowClassName;
    wcex.hIconSm        = 0;

    haveRegisteredWindowClass = true;

    return RegisterClassEx(&wcex);
}

static LRESULT CALLBACK PopupWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // FIXME: The popup selection should follow the mouse pointer without a click.
    // Currently, we only get selection on keypress or mousedown.

    LRESULT lResult = 0;
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, 0);
    PopupMenu* popup = reinterpret_cast<PopupMenu*>(longPtr);

    switch (message) {
        case WM_ACTIVATE:
            if (popup && wParam == WA_INACTIVE)
                popup->menuList()->hidePopup();
            break;
        case WM_KILLFOCUS:
            if (popup && (HWND)wParam != popup->popupHandle())
                // Focus is going elsewhere, so hide
                popup->menuList()->hidePopup();
            break;
        case WM_COMMAND:
            if (popup && HIWORD(wParam) == LBN_SELCHANGE && popup->wasClicked()) {
                popup->menuList()->valueChanged(popup->focusedIndex());
                popup->menuList()->hidePopup();
                popup->setWasClicked(false);
            }
            break;
        case WM_VKEYTOITEM:
            if (popup) {
                switch (LOWORD(wParam)) {
                    case VK_DOWN:
                    case VK_RIGHT:
                        popup->down();
                        lResult = -1;
                        break;
                    case VK_UP:
                    case VK_LEFT:
                        popup->up();
                        lResult = -1;
                        break;
                    case VK_RETURN:
                        popup->menuList()->valueChanged(popup->focusedIndex());
                    case VK_ESCAPE:
                        popup->menuList()->hidePopup();
                        lResult = -2;
                        break;
                }
            }
            break;
        case WM_PARENTNOTIFY:
            if (popup && LOWORD(wParam) == WM_LBUTTONDOWN) {
                // We only want to register clicks that were not on the popup's scrollbar
                int x = LOWORD(lParam), y = HIWORD(lParam);
                RECT rect;
                if (!GetClientRect(popup->popupHandle(), &rect))
                    break;
                if (x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom)
                    popup->setWasClicked(true);
            }
            break;
        default:
            lResult = DefWindowProc(hWnd, message, wParam, lParam);
    }

    return lResult;
}

}
