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
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLSelectElement.h"
#include "Page.h"
#include "RenderMenuList.h"
#include "RenderThemeWin.h"
#include "RenderView.h"
#include "Screen.h"
#include "TextStyle.h"
#include <tchar.h>
#include <windows.h>

namespace WebCore {

using namespace HTMLNames;

// Default Window animation duration in milliseconds
static const int defaultAnimationDuration = 200;
// Maximum height of a popup window
static const int maxPopupHeight = 320;

static LPCTSTR kPopupWindowClassName = _T("PopupWindowContainerClass");
static ATOM registerPopup();
static LRESULT CALLBACK PopupWndProc(HWND, UINT, WPARAM, LPARAM);

PopupMenu::PopupMenu(RenderMenuList* m)
    : m_menuList(m)
    , m_popup(0)
    , m_container(0)
    , m_wasClicked(false)
{
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
    if (!m_popup) {
        registerPopup();

        m_container = CreateWindowEx(0, kPopupWindowClassName, _T("PopupMenuContainer"),
            WS_POPUP | WS_CLIPCHILDREN,
            0, 0, 0, 0,
            menuList()->document()->frame()->view()->containingWindow(), 0, 0, 0);

        if (!m_container)
            return;

        SetWindowLongPtr(m_container, 0, (LONG_PTR)this);

        DWORD exStyle = 0;
        if (menuList()->style()->direction() == LTR)
            exStyle |= WS_EX_RIGHTSCROLLBAR | WS_EX_LTRREADING;
        else
            exStyle |= WS_EX_LEFTSCROLLBAR | WS_EX_RTLREADING;

        // FIXME: Should we also use LBS_COMBOBOX here? What does that do?
        m_popup = CreateWindowEx(exStyle, _T("ListBox"), _T("PopupMenu"),
            WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS | LBS_WANTKEYBOARDINPUT | LBS_OWNERDRAWFIXED,
            0, 0, 0, 0,
            m_container, 0, 0, 0);
    }

    if (!m_popup)
        return;

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

static void addString(HWND hWnd, const String& string)
{
    if (!hWnd)
        return;

    UChar* text = (UChar*)malloc((string.length() + 1) * sizeof(UChar));
    if (!text)
        return;

    if (_tcsncpy_s(text, string.length() + 1, string.characters(), string.length())) {
        free(text);
        return;
    }

    SendMessage(hWnd, LB_ADDSTRING, 0, (LPARAM)text);

    free(text);

}

void PopupMenu::addOption(HTMLOptionElement* element)
{
    addString(m_popup, element->optionText());
}

void PopupMenu::addGroupLabel(HTMLOptGroupElement* element)
{
    addString(m_popup, element->groupLabelText());
}

void PopupMenu::addSeparator()
{
    addString(m_popup, "");
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

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(menuList()->node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    size_t size = listItems.size();

    int i;
    for (i = focusedIndex() + 1;
         i >= 0 && i < size && (listItems[i]->disabled() || !listItems[i]->hasTagName(optionTag));
         ++i);

    if (i < 0 || i >= size)
        return false;

    ::SendMessage(m_popup, LB_SETCURSEL, (WPARAM)i, 0);

    menuList()->setTextFromOption(select->listToOptionIndex(i));
    menuList()->valueChanged(i, false);

    return true;
}

bool PopupMenu::up()
{
    if (!m_popup)
        return false;

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(menuList()->node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    size_t size = listItems.size();

    int i;
    for (i = focusedIndex() - 1;
         i >= 0 && i < size && (listItems[i]->disabled() || !listItems[i]->hasTagName(optionTag));
         --i);

    if (i < 0 || i >= size)
        return false;

    ::SendMessage(m_popup, LB_SETCURSEL, (WPARAM)i, 0);

    menuList()->setTextFromOption(select->listToOptionIndex(i));
    menuList()->valueChanged(i, false);

    return true;
}

const int separatorPadding = 4;
const int separatorHeight = 1;
void PopupMenu::drawItem(LPDRAWITEMSTRUCT drawInfo)
{
    IntRect menuRect = drawInfo->rcItem;

    IntRect bitmapRect = drawInfo->rcItem;
    bitmapRect.setLocation(IntPoint(0, 0));

    HDC bitmapDC = ::CreateCompatibleDC(drawInfo->hDC);

    // Create a bitmap DC in which to draw.
    BITMAPINFO bitmapInfo;
    bitmapInfo.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth         = bitmapRect.width(); 
    bitmapInfo.bmiHeader.biHeight        = -bitmapRect.height();
    bitmapInfo.bmiHeader.biPlanes        = 1;
    bitmapInfo.bmiHeader.biBitCount      = 32;
    bitmapInfo.bmiHeader.biCompression   = BI_RGB;
    bitmapInfo.bmiHeader.biSizeImage     = 0;
    bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biClrUsed       = 0;
    bitmapInfo.bmiHeader.biClrImportant  = 0;

    void* pixels = 0;
    HBITMAP bitmap = ::CreateDIBSection(bitmapDC, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0);
    if (!bitmap)
        return;

    HBITMAP oldBMP = (HBITMAP)::SelectObject(bitmapDC, bitmap);

    GraphicsContext context(bitmapDC);

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(menuList()->node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    HTMLElement* element = listItems[drawInfo->itemID];

    Color optionBackgroundColor, optionTextColor;
    if (drawInfo->itemState & ODS_SELECTED) {
        optionBackgroundColor = theme()->platformActiveSelectionBackgroundColor();
        optionTextColor = theme()->platformActiveSelectionForegroundColor();
    } else {
        optionBackgroundColor = element->renderStyle() ? element->renderStyle()->backgroundColor() : menuList()->style()->backgroundColor();
        optionTextColor = element->renderStyle() ? element->renderStyle()->color() : menuList()->style()->color();
    }

    // Draw the background for this menu item
    if (!element->renderStyle() || element->renderStyle()->visibility() != HIDDEN) {
        if (optionBackgroundColor.hasAlpha() && optionBackgroundColor != menuList()->style()->backgroundColor())
            context.fillRect(bitmapRect, menuList()->style()->backgroundColor());
        context.fillRect(bitmapRect, optionBackgroundColor);
    }

    if (element->hasTagName(hrTag)) {
        IntRect separatorRect(bitmapRect.x() + separatorPadding, (bitmapRect.height() - separatorHeight) / 2, bitmapRect.width() - 2 * separatorPadding, separatorHeight);
        context.fillRect(separatorRect, optionTextColor);
    } else {
        String itemText;
        if (element->hasTagName(optionTag))
            itemText = static_cast<HTMLOptionElement*>(element)->optionText();
        else if (element->hasTagName(optgroupTag))
            itemText = static_cast<HTMLOptGroupElement*>(element)->groupLabelText();
       
        TextRun textRun(itemText.characters(), itemText.length());

        RenderStyle* itemStyle = element->renderStyle();
        if (!itemStyle)
            itemStyle = menuList()->style();
            
        context.setPen(optionTextColor);
        
        Font itemFont = menuList()->style()->font();
        if (element->hasTagName(optgroupTag)) {
            FontDescription d = itemFont.fontDescription();
            d.setBold(true);
            itemFont = Font(d, itemFont.letterSpacing(), itemFont.wordSpacing());
            itemFont.update();
        }
        context.setFont(itemFont);
        
        // Draw the item text
        if (itemStyle->visibility() != HIDDEN) {
            int textX = itemStyle->direction() == LTR ? 0 : bitmapRect.right() - itemFont.width(textRun);
            int textY = itemFont.ascent() + (bitmapRect.height() - itemFont.height()) / 2;
            context.drawText(textRun, IntPoint(textX, textY), TextStyle(0, 0, 0, itemStyle->direction() == RTL));
        }
    }

    ::BitBlt(drawInfo->hDC, menuRect.x(), menuRect.y(), menuRect.width(), menuRect.height(), bitmapDC, bitmapRect.x(), bitmapRect.y(), SRCCOPY);

    ::SelectObject(bitmapDC, oldBMP);
    ::DeleteObject(bitmap);
    ::DeleteDC(bitmapDC);
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

const int optionSpacingMiddle = 1;
static LRESULT CALLBACK PopupWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // FIXME: The popup selection should follow the mouse pointer without a click.
    // Currently, we only get selection on keypress or mousedown.

    LRESULT lResult = 0;
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, 0);
    PopupMenu* popup = reinterpret_cast<PopupMenu*>(longPtr);

    LPMEASUREITEMSTRUCT measureInfo;
    LPDRAWITEMSTRUCT drawInfo;

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
            if (popup && HIWORD(wParam) == LBN_SELCHANGE) {
                if (popup->wasClicked()) {
                    popup->menuList()->valueChanged(popup->focusedIndex());
                    popup->menuList()->hidePopup();
                    popup->setWasClicked(false);
                }
            }
            break;
        case WM_VKEYTOITEM:
            if (popup) {
                switch (LOWORD(wParam)) {
                    case VK_DOWN:
                    case VK_RIGHT:
                        popup->down();
                        lResult = -2;
                        break;
                    case VK_UP:
                    case VK_LEFT:
                        popup->up();
                        lResult = -2;
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
                    popup->setWasClicked();
            }
            break;
        case WM_MEASUREITEM:
            measureInfo = (LPMEASUREITEMSTRUCT)lParam;
            ASSERT(measureInfo->CtlType == ODT_LISTBOX);
            // FIXME: This will have to change when <rdar://4772506> is fixed.
            measureInfo->itemWidth = popup->menuList()->width();
            measureInfo->itemHeight = popup->menuList()->font(false).height() + optionSpacingMiddle;
            lResult = TRUE;
            break;
        case WM_DRAWITEM:
            drawInfo = (LPDRAWITEMSTRUCT)lParam;
            ASSERT(drawInfo->CtlType == ODT_LISTBOX);
            popup->drawItem(drawInfo);
        default:
            lResult = DefWindowProc(hWnd, message, wParam, lParam);
    }

    return lResult;
}

}
