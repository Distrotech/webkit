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

using std::min;

namespace WebCore {

using namespace HTMLNames;

// Default Window animation duration in milliseconds
static const int defaultAnimationDuration = 200;
// Maximum height of a popup window
static const int maxPopupHeight = 320;

const int optionSpacingMiddle = 1;
const int popupWindowBorderWidth = 1;

static LPCTSTR kPopupWindowClassName = _T("PopupWindowClass");
static ATOM registerPopup();
static LRESULT CALLBACK PopupWndProc(HWND, UINT, WPARAM, LPARAM);

PopupMenu::PopupMenu(RenderMenuList* m)
    : m_menuList(m)
    , m_popup(0)
    , m_DC(0)
    , m_bmp(0)
    , m_wasClicked(false)
    , m_windowRect(IntRect())
    , m_itemHeight(0)
    , m_scrollOffset(0)
    , m_wheelDelta(0)
{
}

PopupMenu::~PopupMenu()
{
    if (m_bmp)
        ::DeleteObject(m_bmp);
    if (m_DC)
        ::DeleteObject(m_DC);
    if (m_popup)
        ::DestroyWindow(m_popup);
}

void PopupMenu::show(const IntRect& r, FrameView* v, int index)
{
    calculatePositionAndSize(r, v);
    if (m_windowRect.isEmpty())
        return;

    if (!m_popup) {
        registerPopup();

        DWORD exStyle = 0;
        if (menuList()->style()->direction() == LTR)
            exStyle |= WS_EX_RIGHTSCROLLBAR | WS_EX_LTRREADING;
        else
            exStyle |= WS_EX_LEFTSCROLLBAR | WS_EX_RTLREADING;

        // Even though we already know our size and location at this point, we pass (0,0,0,0) as our size/location here.
        // We need to wait until after the call to ::SetWindowLongPtr to set our size so that in our WM_SIZE handler we can get access to the PopupMenu object
        m_popup = ::CreateWindowEx(exStyle, kPopupWindowClassName, _T("PopupMenu"),
            WS_POPUP | WS_BORDER | WS_VSCROLL,
            0, 0, 0, 0,
            v->containingWindow(), 0, 0, 0);

        if (!m_popup)
            return;

        ::SetWindowLongPtr(m_popup, 0, (LONG_PTR)this);
    }

    ::SetWindowPos(m_popup, HWND_TOP, m_windowRect.x(), m_windowRect.y(), m_windowRect.width(), m_windowRect.height(), 0);

    // Determine whether we should animate our popups
    // Note: Must use 'BOOL' and 'FALSE' instead of 'bool' and 'false' to avoid stack corruption with SystemParametersInfo
    BOOL shouldAnimate = FALSE;
    ::SystemParametersInfo(SPI_GETCOMBOBOXANIMATION, 0, &shouldAnimate, 0);

    if (shouldAnimate) {
        RECT viewRect = {0};
        ::GetWindowRect(v->containingWindow(), &viewRect);

        if (!::IsRectEmpty(&viewRect)) {
            // Popups should slide into view away from the <select> box
            // NOTE: This may have to change for Vista
            DWORD slideDirection = (m_windowRect.y() < viewRect.top + v->contentsToWindow(r.location()).y()) ? AW_VER_NEGATIVE : AW_VER_POSITIVE;

            ::AnimateWindow(m_popup, defaultAnimationDuration, AW_SLIDE | slideDirection | AW_ACTIVATE);
        }
    } else
        ::ShowWindow(m_popup, SW_SHOWNORMAL);
}

void PopupMenu::hide()
{
    ::ShowWindow(m_popup, SW_HIDE);
}

const int endOfLinePadding = 2;
void PopupMenu::calculatePositionAndSize(const IntRect& r, FrameView* v)
{
    // r is in absolute document coordinates, but we want to be in screen coordinates

    RECT viewRect = {0};
    ::GetWindowRect(v->containingWindow(), &viewRect);
    if (::IsRectEmpty(&viewRect))
        return;

    // First, move to WebView coordinates
    IntRect rScreenCoords(v->contentsToWindow(r.location()), r.size());

    // Then, translate to screen coordinates
    rScreenCoords.move(viewRect.left, viewRect.top);

    HTMLSelectElement *select = static_cast<HTMLSelectElement*>(menuList()->node());
    const Vector<HTMLElement*>& items = select->listItems();

    // First, determine the popup's height
    size_t itemCount = items.size();
    m_itemHeight = menuList()->font(false).height() + optionSpacingMiddle;
    int naturalHeight = m_itemHeight * itemCount;
    int popupHeight = min(maxPopupHeight, naturalHeight);
    // The popup should show an integral number of items (i.e. no partial items should be visible)
    popupHeight -= popupHeight % m_itemHeight;
    
    // Next determine its width
    int popupWidth = 0;
    for (int i = 0; i < itemCount; ++i) {
        String text;
        if (items[i]->hasTagName(optionTag))
            text = static_cast<HTMLOptionElement*>(items[i])->optionText();
        else if (items[i]->hasTagName(optgroupTag))
            text = static_cast<HTMLOptGroupElement*>(items[i])->groupLabelText();

        if (text.isEmpty())
            continue;

        popupWidth = max(popupWidth, menuList()->font(false).width(TextRun(text.characters(), text.length())));
    }

    if (naturalHeight > maxPopupHeight)
        // We need room for a scrollbar
        popupWidth += ::GetSystemMetrics(SM_CXVSCROLL);

    // Add padding to align the popup text with the <select> text
    // Note: We can't add paddingRight() in LTR or paddingLeft() in RTL because those values include the width
    // of the dropdown button, so we must use our own endOfLinePadding constant.
    popupWidth += endOfLinePadding + (menuList()->style()->direction() == LTR ? menuList()->paddingLeft() : menuList()->paddingRight());

    // Leave room for the border
    popupWidth += 2 * popupWindowBorderWidth;
    popupHeight += 2 * popupWindowBorderWidth;

    // The popup should be at least as wide as the control on the page
    popupWidth = max(rScreenCoords.width(), popupWidth);

    // LTR <select>s get a left-aligned popup, RTL <select>s get a right-aligned popup
    // This will cause the popup's text to always be aligned with the <select>'s text
    int popupX = menuList()->style()->direction() == LTR ? rScreenCoords.x() : rScreenCoords.right() - popupWidth;

    IntRect popupRect(popupX, rScreenCoords.bottom(), popupWidth, popupHeight);

    // The popup needs to stay within the bounds of the screen and not overlap any toolbars
    FloatRect screen = screenAvailableRect(v);

    // Check that we don't go off the screen vertically
    if (popupRect.bottom() > screen.height()) {
        // The popup will go off the screen, so try placing it above the menulist
        if (rScreenCoords.y() - popupRect.height() < 0) {
            // The popup won't fit above, either, so place it whereever's bigger and resize it to fit
            if ((rScreenCoords.y() + rScreenCoords.height() / 2) < (screen.height() / 2)) {
                // Below is bigger
                popupRect.setHeight(screen.height() - popupRect.y());
            } else {
                // Above is bigger
                popupRect.setY(0);
                popupRect.setHeight(rScreenCoords.y());
            }
        } else {
            // The popup fits above, so reposition it
            popupRect.setY(rScreenCoords.y() - popupRect.height());
        }
    }

    // Check that we don't go off the screen horizontally
    if (menuList()->style()->direction() == LTR && popupRect.x() < screen.x()) {
        popupRect.setWidth(popupRect.width() - (screen.x() - popupRect.x()));
        popupRect.setX(screen.x());
    } else if (menuList()->style()->direction() == RTL && popupRect.right() > screen.right())
        popupRect.setWidth(popupRect.width() - (popupRect.right() - screen.right()));

    m_windowRect = popupRect;
    return;
}

bool PopupMenu::setFocusedIndex(int i, bool setControlText, bool fireOnChange)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(menuList()->node());
    const Vector<HTMLElement*>& listItems = select->listItems();

    if (i < 0 || i >= select->listItems().size() || i == focusedIndex())
        return false;

    HTMLElement* element = listItems[i];

    if (!(element->hasTagName(optionTag) && !static_cast<HTMLOptionElement*>(element)->disabled()))
        return false;

    invalidateItem(focusedIndex());
    invalidateItem(i);

    if (setControlText)
        menuList()->setTextFromOption(select->listToOptionIndex(i));
    menuList()->valueChanged(i, fireOnChange);

    if (!scrollToRevealSelection())
        ::UpdateWindow(m_popup);

    return true;
}

int PopupMenu::focusedIndex() const
{
    if (!menuList())
        return 0;

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(menuList()->node());
    return select->optionToListIndex(select->selectedIndex());
}

void PopupMenu::focusFirst()
{
    if (!menuList())
        return;

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(menuList()->node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    size_t size = listItems.size();

    for (int i = 0; i < size; ++i)
        if (listItems[i]->hasTagName(optionTag) && !listItems[i]->disabled()) {
            setFocusedIndex(i);
            break;
        }
}

void PopupMenu::focusLast()
{
    if (!menuList())
        return;

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(menuList()->node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    size_t size = listItems.size();

    for (int i = size - 1; i > 0; --i)
        if (listItems[i]->hasTagName(optionTag) && !listItems[i]->disabled()) {
            setFocusedIndex(i);
            break;
        }
}

bool PopupMenu::down(unsigned lines)
{
    if (!menuList())
        return false;

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(menuList()->node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    size_t size = listItems.size();

    int lastSelectableIndex, selectedListIndex;
    lastSelectableIndex = selectedListIndex = select->optionToListIndex(select->selectedIndex());
    for (int i = selectedListIndex + 1; i >= 0 && i < size; ++i)
        if (listItems[i]->hasTagName(optionTag) && !listItems[i]->disabled()) {
            lastSelectableIndex = i;
            if (i >= selectedListIndex + (int)lines)
                break;
        }

    return setFocusedIndex(lastSelectableIndex);
}

bool PopupMenu::up(unsigned lines)
{
    if (!menuList())
        return false;

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(menuList()->node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    size_t size = listItems.size();

    int lastSelectableIndex, selectedListIndex;
    lastSelectableIndex = selectedListIndex = select->optionToListIndex(select->selectedIndex());
    for (int i = selectedListIndex - 1; i >= 0 && i < size; --i)
        if (listItems[i]->hasTagName(optionTag) && !listItems[i]->disabled()) {
            lastSelectableIndex = i;
            if (i <= selectedListIndex - (int)lines)
                break;
        }

    return setFocusedIndex(lastSelectableIndex);
}

void PopupMenu::invalidateItem(int index)
{
    if (!m_popup)
        return;

    IntRect damageRect(clientRect());
    damageRect.setY(m_itemHeight * index - m_scrollOffset);
    damageRect.setHeight(m_itemHeight);

    ::InvalidateRect(m_popup, &RECT(damageRect), true);
}

IntRect PopupMenu::clientRect() const
{
    IntRect clientRect = m_windowRect;
    clientRect.inflate(-popupWindowBorderWidth);
    clientRect.setLocation(IntPoint(0, 0));
    return clientRect;
}

void PopupMenu::incrementWheelDelta(int delta)
{
    m_wheelDelta += delta;
}

void PopupMenu::reduceWheelDelta(int delta)
{
    ASSERT(delta >= 0);
    ASSERT(delta <= abs(m_wheelDelta));

    if (m_wheelDelta > 0)
        m_wheelDelta -= delta;
    else if (m_wheelDelta < 0)
        m_wheelDelta += delta;
    else
        return;
}

bool PopupMenu::scrollTo(int line)
{
    if (!m_popup)
        return false;

    SCROLLINFO scrollInfo;
    scrollInfo.cbSize = sizeof(scrollInfo);
    scrollInfo.fMask = SIF_POS;
    if (!::GetScrollInfo(m_popup, SB_VERT, &scrollInfo))
        return false;

    int oldPosition = scrollInfo.nPos;
    scrollInfo.nPos = line;

    ::SetScrollInfo(m_popup, SB_VERT, &scrollInfo, TRUE);
    ::GetScrollInfo(m_popup, SB_VERT, &scrollInfo);

    if (oldPosition == scrollInfo.nPos)
        // No scrolling was performed (we were already at the top or bottom)
        return false;

    m_scrollOffset = scrollInfo.nPos * m_itemHeight;

    UINT flags = SW_INVALIDATE;

#ifdef CAN_SET_SMOOTH_SCROLLING_DURATION
    BOOL shouldSmoothScroll = FALSE;
    ::SystemParametersInfo(SPI_GETLISTBOXSMOOTHSCROLLING, 0, &shouldSmoothScroll, 0);
    if (shouldSmoothScroll)
        flags |= MAKEWORD(SW_SMOOTHSCROLL, smoothScrollAnimationDuration);
#endif

    ::ScrollWindowEx(m_popup, 0, (oldPosition - scrollInfo.nPos) * m_itemHeight, 0, 0, 0, 0, flags);
    ::UpdateWindow(m_popup);

    return true;
}

bool PopupMenu::scrollToRevealSelection()
{
    IntRect client = clientRect();
    IntRect focusedRect(client.x(), focusedIndex() * m_itemHeight, client.width(), m_itemHeight);

    if (focusedRect.y() < m_scrollOffset)
        return scrollTo(focusedIndex());
    else if (focusedRect.bottom() > m_scrollOffset + client.height())
        return scrollTo(focusedIndex() - client.height() / m_itemHeight + 1);

    return false;
}

void PopupMenu::updateFromElement()
{
    if (!m_popup)
        return;

    ::InvalidateRect(m_popup, 0, true);
    if (!scrollToRevealSelection())
        ::UpdateWindow(m_popup);
}

const int separatorPadding = 4;
const int separatorHeight = 1;
void PopupMenu::paint(const IntRect& damageRect, HDC hdc)
{
    if (!m_popup)
        return;

    if (!m_DC) {
        m_DC = ::CreateCompatibleDC(::GetDC(m_popup));
        if (!m_DC)
            return;
    }

    if (!m_bmp) {
        BITMAPINFO bitmapInfo;
        bitmapInfo.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth         = clientRect().width(); 
        bitmapInfo.bmiHeader.biHeight        = -clientRect().height();
        bitmapInfo.bmiHeader.biPlanes        = 1;
        bitmapInfo.bmiHeader.biBitCount      = 32;
        bitmapInfo.bmiHeader.biCompression   = BI_RGB;
        bitmapInfo.bmiHeader.biSizeImage     = 0;
        bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
        bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
        bitmapInfo.bmiHeader.biClrUsed       = 0;
        bitmapInfo.bmiHeader.biClrImportant  = 0;

        void* pixels = 0;
        m_bmp = ::CreateDIBSection(m_DC, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0);
        if (!m_bmp)
            return;

        ::SelectObject(m_DC, m_bmp);
    }

    GraphicsContext context(m_DC);

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(menuList()->node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    size_t itemCount = listItems.size();

    // listRect is the damageRect translated into the coordinates of the entire menu list (which is itemCount * m_itemHeight pixels tall)
    IntRect listRect = damageRect;
    listRect.move(IntSize(0, m_scrollOffset));

    for (int y = listRect.y(); y < listRect.bottom(); y += m_itemHeight) {
        int index = y / m_itemHeight;

        HTMLElement* element = listItems[index];

        Color optionBackgroundColor, optionTextColor;
        if (element->hasTagName(optionTag) && static_cast<HTMLOptionElement*>(element)->selected()) {
            optionBackgroundColor = theme()->platformActiveSelectionBackgroundColor();
            optionTextColor = theme()->platformActiveSelectionForegroundColor();
        } else {
            optionBackgroundColor = element->renderStyle() ? element->renderStyle()->backgroundColor() : menuList()->style()->backgroundColor();
            optionTextColor = element->renderStyle() ? element->renderStyle()->color() : menuList()->style()->color();
        }

        // itemRect is in client coordinates
        IntRect itemRect(0, index * m_itemHeight - m_scrollOffset, damageRect.width(), m_itemHeight);

        // Draw the background for this menu item
        if (!element->renderStyle() || element->renderStyle()->visibility() != HIDDEN) {
            if (optionBackgroundColor.hasAlpha() && optionBackgroundColor != menuList()->style()->backgroundColor())
                context.fillRect(itemRect, menuList()->style()->backgroundColor());
            context.fillRect(itemRect, optionBackgroundColor);
        }

        if (element->hasTagName(hrTag)) {
            IntRect separatorRect(itemRect.x() + separatorPadding, itemRect.y() + (itemRect.height() - separatorHeight) / 2, itemRect.width() - 2 * separatorPadding, separatorHeight);
            context.fillRect(separatorRect, optionTextColor);
            continue;
        }

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
            int textX = itemStyle->direction() == LTR ? menuList()->paddingLeft() : itemRect.right() - itemFont.width(textRun) - menuList()->paddingRight();
            int textY = itemRect.y() + itemFont.ascent() + (itemRect.height() - itemFont.height()) / 2;
            context.drawText(textRun, IntPoint(textX, textY), TextStyle(0, 0, 0, itemStyle->direction() == RTL));
        }
    }

    if (!hdc)
        hdc = ::GetDC(m_popup);

    ::BitBlt(hdc, damageRect.x(), damageRect.y(), damageRect.width(), damageRect.height(), m_DC, damageRect.x(), damageRect.y(), SRCCOPY);
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
    wcex.cbWndExtra     = sizeof(PopupMenu*); // For the PopupMenu pointer
    wcex.hInstance      = Page::instanceHandle();
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kPopupWindowClassName;
    wcex.hIconSm        = 0;

    haveRegisteredWindowClass = true;

    return ::RegisterClassEx(&wcex);
}

const int smoothScrollAnimationDuration = 5000;
static LRESULT CALLBACK PopupWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, 0);
    PopupMenu* popup = reinterpret_cast<PopupMenu*>(longPtr);

    switch (message) {
        case WM_SIZE:
            if (popup) {
                int pageSize = popup->windowRect().height() / popup->itemHeight();
                SCROLLINFO scrollInfo;
                scrollInfo.cbSize = sizeof(scrollInfo);
                scrollInfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
                scrollInfo.nMin = 0;
                scrollInfo.nMax = static_cast<HTMLSelectElement*>(popup->menuList()->node())->listItems().size() - 1;
                scrollInfo.nPage = pageSize;
                scrollInfo.nPos = popup->scrollOffset() / popup->itemHeight();

                ::SetScrollInfo(popup->popupHandle(), SB_VERT, &scrollInfo, TRUE);

                popup->setScrollOffset(scrollInfo.nPos * popup->itemHeight());
            }
            break;
        case WM_ACTIVATE:
            if (popup && popup->menuList() && wParam == WA_INACTIVE)
                popup->menuList()->hidePopup();
            break;
        case WM_KILLFOCUS:
            if (popup && popup->menuList() && (HWND)wParam != popup->popupHandle())
                // Focus is going elsewhere, so hide
                popup->menuList()->hidePopup();
            break;
        case WM_KEYDOWN:
            if (popup && popup->menuList()) {
                lResult = 0;
                switch (LOWORD(wParam)) {
                    case VK_DOWN:
                    case VK_RIGHT:
                        popup->down();
                        break;
                    case VK_UP:
                    case VK_LEFT:
                        popup->up();
                        break;
                    case VK_HOME:
                        popup->focusFirst();
                        break;
                    case VK_END:
                        popup->focusLast();
                        break;
                    case VK_PRIOR:
                        if (popup->focusedIndex() * popup->itemHeight() != popup->scrollOffset()) {
                            // Set the selection to the first visible item
                            int firstVisibleItem = popup->scrollOffset() / popup->itemHeight();
                            popup->up(popup->focusedIndex() - firstVisibleItem);
                        } else
                            // The first visible item is selected, so move the selection back one page
                            popup->up(popup->clientRect().height() / popup->itemHeight());
                        break;
                    case VK_NEXT:
                        if ((popup->focusedIndex() + 1) * popup->itemHeight() != popup->scrollOffset() + popup->clientRect().height()) {
                            // Set the selection to the last visible item
                            int lastVisibleItem = (popup->scrollOffset() + popup->clientRect().height()) / popup->itemHeight() - 1;
                            popup->down(lastVisibleItem - popup->focusedIndex());
                        } else
                            // The last visible item is selected, so move the selection forward one page
                            popup->down(popup->clientRect().height() / popup->itemHeight());
                        break;
                    case VK_TAB:
                        ::SendMessage(popup->menuList()->document()->view()->containingWindow(), message, wParam, lParam);
                        popup->menuList()->hidePopup();
                        break;
                    default:
                        if (isprint(::MapVirtualKey(LOWORD(wParam), 2)))
                            // Send the keydown to the WebView so it can be used for type-ahead find
                            ::SendMessage(popup->menuList()->document()->view()->containingWindow(), message, wParam, lParam);
                        else
                            lResult = 1;
                        break;
                }
            }
            break;
        case WM_CHAR:
            if (popup && popup->menuList()) {
                lResult = 0;
                switch (wParam) {
                    case 0x0D:   // Enter/Return
                        popup->menuList()->hidePopup();
                        popup->menuList()->valueChanged(popup->focusedIndex());
                        break;
                    case 0x1B:   // Escape
                        popup->menuList()->hidePopup();
                        break;
                    case 0x09:   // TAB
                    case 0x08:   // Backspace
                    case 0x0A:   // Linefeed
                    default:     // Character
                        lResult = 1;
                        break;
                }
            }
            break;
        case WM_MOUSEMOVE:
            if (popup) {
                BOOL shouldHotTrack = FALSE;
                ::SystemParametersInfo(SPI_GETHOTTRACKING, 0, &shouldHotTrack, 0);

                if (shouldHotTrack || wParam & MK_LBUTTON)
                    popup->setFocusedIndex(popup->listIndexAtPoint(MAKEPOINTS(lParam)), false);
            }
            break;
        case WM_LBUTTONDOWN:
            if (popup)
                popup->setFocusedIndex(popup->listIndexAtPoint(MAKEPOINTS(lParam)), false);
            break;
        case WM_LBUTTONUP:
            if (popup && popup->menuList()) {
                popup->menuList()->hidePopup();
                popup->menuList()->valueChanged(popup->focusedIndex());
            }
            break;
        case WM_MOUSEWHEEL:
            if (popup) {
                for (popup->incrementWheelDelta(GET_WHEEL_DELTA_WPARAM(wParam)); abs(popup->wheelDelta()) >= WHEEL_DELTA; popup->reduceWheelDelta(WHEEL_DELTA))
                    ::SendMessage(popup->popupHandle(), WM_VSCROLL, popup->wheelDelta() > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            }
            break;
        case WM_PAINT:
            if (popup) {
                PAINTSTRUCT paintInfo;
                ::BeginPaint(popup->popupHandle(), &paintInfo);
                popup->paint(paintInfo.rcPaint, paintInfo.hdc);
                ::EndPaint(popup->popupHandle(), &paintInfo);
                lResult = 0;
            }
            break;
        case WM_PRINTCLIENT:
            if (popup)
                popup->paint(popup->clientRect(), (HDC)wParam);
            break;
        case WM_VSCROLL:
            if (popup) {
                SCROLLINFO scrollInfo;
                scrollInfo.cbSize = sizeof(scrollInfo);
                scrollInfo.fMask = SIF_POS | SIF_TRACKPOS;
                if (!::GetScrollInfo(popup->popupHandle(), SB_VERT, &scrollInfo))
                    break;

                switch (LOWORD(wParam)) {
                    case SB_LINEUP:
                        --scrollInfo.nPos;
                        break;
                    case SB_LINEDOWN:
                        ++scrollInfo.nPos;
                        break;
                    case SB_PAGEUP:
                        scrollInfo.nPos -= popup->windowRect().height() / popup->itemHeight();
                        break;
                    case SB_PAGEDOWN:
                        scrollInfo.nPos += popup->windowRect().height() / popup->itemHeight();
                        break;
                    case SB_THUMBTRACK:
                        scrollInfo.nPos = scrollInfo.nTrackPos;
                        break;
                    default:
                        break;
                }

                popup->scrollTo(scrollInfo.nPos);
            }
            break;
        default:
            lResult = DefWindowProc(hWnd, message, wParam, lParam);
    }

    return lResult;
}

}
