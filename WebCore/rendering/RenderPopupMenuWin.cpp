/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
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
#include "RenderPopupMenuWin.h"

#include "Screen.h"
#include "FloatRect.h"
#include "FrameView.h"
#include "HTMLSelectElement.h"
#include "HTMLOptionElement.h"
#include <tchar.h>

namespace WebCore {
#if 0
LRESULT CALLBACK popupWindowProc(HWND, UINT, WPARAM, LPARAM);

WNDPROC listBoxOrigWindowProc;
#endif
RenderPopupMenuWin::RenderPopupMenuWin(Node* n, RenderMenuList* m)
    : RenderPopupMenu(n, m)
    , hwndCombo(0)
{
}

RenderPopupMenuWin::~RenderPopupMenuWin()
{
    if (hwndCombo)
        DestroyWindow(hwndCombo);
}

void RenderPopupMenuWin::clear()
{
    ASSERT(hwndCombo);
    SendMessage(hwndCombo, LB_RESETCONTENT, 0, 0);
}

void RenderPopupMenuWin::populate()
{
    ASSERT(hwndCombo);
    clear();

    RenderPopupMenu::populate();
}

void RenderPopupMenuWin::showPopup(const IntRect& r, FrameView* v, int index)
{
    // FIXME - The second argument to CreateWindow should be the current title of the Safari window. This argument is shown as the title of the window when Alt-Tabing while the popup has focus
    if (!hwndCombo)
        hwndCombo = CreateWindow(L"LISTBOX", L"PopupMenu",
            WS_POPUP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS | LBS_WANTKEYBOARDINPUT,
            0, 0, 0, 0,
            v->windowHandle(), 0, 0, 0);

    if (!hwndCombo)
        return;

    //setComboWindowProc((WNDPROC)&popupWindowProc);

    SendMessage(hwndCombo, WM_SETFONT, (WPARAM)menuList()->font(false).fontDataAt(0)->platformData().hfont(), MAKELPARAM(false, 0));

    populate();
    if (SendMessage(hwndCombo, LB_GETCOUNT, 0, 0) <= 0)
        return;
    ASSERT(SendMessage(hwndCombo, LB_GETCOUNT, 0, 0) > index);

    // By default the popup should display just below the menulist and be large enough to display all its items
    IntRect popupRect = r;
    popupRect.setY(r.y() + r.height());

    int itemHeight = SendMessage(hwndCombo, LB_GETITEMHEIGHT, 0, 0);
    HTMLSelectElement *select = static_cast<HTMLSelectElement*>(menuList()->node());
    const Vector<HTMLElement*>& items = select->listItems();
    size_t size = items.size();
    popupRect.setHeight(itemHeight * size + itemHeight / 2);

    // Popup windows are positioned in screen coordinates, but popupRect is in FrameView coordinates, so we have to find the screen origin of the FrameView to position correctly
    RECT viewRect;
    if (!GetWindowRect(v->windowHandle(), &viewRect))
        return;
    
    // Translate popupRect into screen coordinates
    popupRect.setX(popupRect.x() + viewRect.left);
    popupRect.setY(popupRect.y() + viewRect.top);

    // The popup needs to stay within the bounds of the screen
    FloatRect screen = usableScreenRect(v);

    if (popupRect.y() + popupRect.height() > screen.height()) {
        // The popup will go off the screen, so try placing it above the menulist
        if (viewRect.top + r.y() - popupRect.height() < 0) {
            // The popup won't fit above, either, so place it whereever's bigger and resize it to fit
            if ((viewRect.top + r.y() + r.height() / 2) < (screen.height() / 2)) {
                // Below is bigger
                popupRect.setHeight(screen.height() - popupRect.y());
            } else {
                // Above is bigger
                popupRect.setY(0);
                popupRect.setHeight(viewRect.top + r.y());
            }
        } else {
            // The popup fits above, so reposition it
            popupRect.setY(viewRect.top + r.y() - popupRect.height());
        }
    }

    // Place the popup
    SetWindowPos(hwndCombo, HWND_TOP, popupRect.x(), popupRect.y(), popupRect.width(), popupRect.height(), 0);

    // Placing the popup also resizes it to an integral multiple of the item height (so that no partial items are shown),
    // so in the case where the popup is above the menulist the bottom of the popup may no longer touch the menulist,
    // so we need to reposition once more.
    
    if (popupRect.y() < viewRect.top + r.y()) {
        RECT newPopupRect;
        GetWindowRect(hwndCombo, &newPopupRect);
        SetWindowPos(hwndCombo, HWND_TOP, newPopupRect.left, viewRect.top + r.y() - (newPopupRect.bottom - newPopupRect.top), newPopupRect.right - newPopupRect.left, newPopupRect.bottom - newPopupRect.top, 0);
    }

    //HWND hWndOldFocus = SetFocus(hwndCombo);
    SendMessage(hwndCombo, LB_SETCURSEL, index, 0);

    // Finally we can show the popup (but don't activate it)
    ShowWindow(hwndCombo, SW_SHOWNOACTIVATE);

    // Now we capture the mouse
    //SetCapture(hwndCombo);

    //modalPopupLoop(v->windowHandle());

    // We're done with the mouse
    //ReleaseCapture();

    //SetFocus(hWndOldFocus);

    //resetComboWindowProc();

#if 0
    int newIndex = SendMessage(hwndCombo, LB_GETCURSEL, 0, 0);
    if (index != newIndex && newIndex >= 0)
        menuList()->valueChanged(newIndex);

    NSFont* font = style()->font().getNSFont();

    NSRect titleFrame = [popup titleRectForBounds:r];
    if (titleFrame.size.width <= 0 || titleFrame.size.height <= 0)
        titleFrame = r;
    float vertOffset = roundf((NSMaxY(r) - NSMaxY(titleFrame)) + NSHeight(titleFrame));
    // Adjust for fonts other than the system font.
    NSFont* defaultFont = [NSFont systemFontOfSize:[font pointSize]];
    vertOffset += [font descender] - [defaultFont descender];
    vertOffset = fmin(NSHeight(r), vertOffset);

    NSMenu* menu = [popup menu];
    // FIXME: Need to document where this magic number 10 comes from.
    NSPoint location = NSMakePoint(NSMinX(r) - 10, NSMaxY(r) - vertOffset);

    // Save the current event that triggered the popup, so we can clean up our event
    // state after the NSMenu goes away.
    RefPtr<FrameMac> frame = Mac(v->frame());
    NSEvent* event = [frame->currentEvent() retain];

    wkPopupMenu(menu, location, roundf(NSWidth(r)), view, index, font);
    int newIndex = [popup indexOfSelectedItem];
    [popup dismissPopUp];

    if (index != newIndex && newIndex >= 0)
        menuList()->valueChanged(newIndex);

    // Give the frame a chance to fix up its event state, since the popup eats all the
    // events during tracking.
    frame->sendFakeEventsAfterWidgetTracking(event);

    [event release];
#endif
}

void RenderPopupMenuWin::addOption(HTMLOptionElement* element)
{
    String optionText = element->optionText();
 
    int bufSize = (optionText.length() + 1) * sizeof(UChar);
    UChar* text = (UChar*)malloc(bufSize);
    if (!text)
        return;

    if (_tcsncpy_s(text, bufSize, optionText.characters(), optionText.length()))
        return;

    SendMessage(hwndCombo, LB_ADDSTRING, 0, (LPARAM)text);

    free(text);
}

void RenderPopupMenuWin::setSelectedIndex(const unsigned int index)
{
    SendMessage(hwndCombo, LB_SETCURSEL, index, 0);
}

#if 0
void RenderPopupMenuWin::modalPopupLoop(HWND hWndParent)
{
    if (hWndParent)
        EnableWindow(hWndParent, false);

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(menuList()->node());

    bool done = false;
    MSG msg;
    while (!done) {
        WaitMessage();
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            bool shouldContinue = false;
            
            if (msg.message == WM_QUIT) {
                PostMessage(0, WM_QUIT, 0, 0);
                done = true;
                break;
            }

            if (msg.message == WM_KEYDOWN) {
                int index = select->optionToListIndex(select->selectedIndex());

                switch (msg.wParam) {
                    case VK_DOWN:
                    case VK_RIGHT:
                        if (index < select->listItems().size() - 1) {
                            setSelected(++index);
                            shouldContinue = true;
                        }
                        break;
                    case VK_UP:
                    case VK_LEFT:
                        if (index > 0) {
                            setSelected(--index);
                            shouldContinue = true;
                        }
                        break;
                    case VK_ESCAPE:
                    case VK_RETURN:
                        done = true;
                        break;
                }
            }

            if (done)
                break;
            if (shouldContinue)
                continue;

if 0
            if (msg.hwnd == hwndCombo && msg.message == WM_CLOSE) {
                // The popup window is closing. Time for us to exit our loop
                done = true;
                break;
            }

            if (msg.message == WM_COMMAND && HIWORD(msg.wParam) == LBN_SELCHANGE) {
                done = true;
                break;
            }
endif
            
            // The message wasn't for us, so we'll just pass it along
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (hWndParent)
        EnableWindow(hWndParent, true);
}
if 0
LRESULT CALLBACK popupWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Are there other cases in which the popup should close?
    switch (uMsg) {
        case WM_LBUTTONDOWN:
            // When a mouse click occurs, we always want to close the popup, UNLESS the click was on the popup's scrollbar
            POINTS pos;
            pos = MAKEPOINTS(lParam);
            
            RECT popupRect, clientRect;
            GetWindowRect(hWnd, &popupRect);
            GetClientRect(hWnd, &clientRect);
            
            // If the click was on the scrollbar, then don't close
            if (pos.x > clientRect.right && pos.x <= popupRect.right
                && pos.y >= 0 && pos.y <= popupRect.bottom)
                    break;

            // The click was either outside the popup entirely, or it was on an item in the popup. Either way, we should close
            PostMessage(hWnd, WM_CLOSE, 0, 0);
            break;
        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                return 0;
            }
        case WM_ACTIVATE:
            return 0;
    }
    
    return CallWindowProc(listBoxOrigWindowProc, hWnd, uMsg, wParam, lParam);
}

void RenderPopupMenuWin::setComboWindowProc(WNDPROC proc)
{
    listBoxOrigWindowProc = (WNDPROC)SetWindowLongPtr(hwndCombo, GWL_WNDPROC, (LONG_PTR)proc);
}

void RenderPopupMenuWin::resetComboWindowProc()
{
    SetWindowLongPtr(hwndCombo, GWL_WNDPROC, (LONG_PTR)listBoxOrigWindowProc);
}
#endif
}