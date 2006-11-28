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
#include "WebKitDLL.h"
#include "WebView.h"

#include "IWebNotification.h"
#include "WebEditorClient.h"
#include "WebElementPropertyBag.h"
#include "WebFrame.h"
#include "WebBackForwardList.h"
#include "WebChromeClient.h"
#include "WebContextMenuClient.h"
#include "WebKit.h"
#include "WebNotificationCenter.h"
#include "WebPreferences.h"
#pragma warning( push, 0 )
#include <WebCore/BString.h>
#include <WebCore/CommandByName.h>
#include <WebCore/CString.h>
#include <WebCore/Document.h>
#include <WebCore/Editor.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/FrameWin.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/IntRect.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PlatformWheelEvent.h>
#include <WebCore/ResourceHandleClient.h>
#include <WebCore/SelectionController.h>
#include <WebCore/TypingCommand.h>
#pragma warning(pop)
#include <JavaScriptCore/kjs/value.h>
#include <tchar.h>

using namespace WebCore;

const LPCWSTR kWebViewWindowClassName = L"WebViewWindowClass";

const int WM_XP_THEMECHANGED = 0x031A;

static ATOM registerWebView();
static LRESULT CALLBACK WebViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

HRESULT createMatchEnumerator(Vector<IntRect>* rects, IEnumTextMatches** matches);

// WebView ----------------------------------------------------------------

WebView::WebView()
: m_refCount(0)
, m_hostWindow(0)
, m_viewWindow(0)
, m_mainFrame(0)
, m_backingStoreBitmap(0)
, m_backingStoreDirtyRegion(0)
, m_frameLoadDelegate(0)
, m_frameLoadDelegatePrivate(0)
, m_uiDelegate(0)
, m_uiDelegatePrivate(0)
, m_formDelegate(0)
, m_backForwardList(0)
, m_preferences(0)
, m_userAgentOverridden(false)
, m_textSizeMultiplier(1)
{
    m_backForwardList = WebBackForwardList::createInstance();
    m_backingStoreSize.cx = m_backingStoreSize.cy = 0;

    gClassCount++;
}

WebView::~WebView()
{
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->removeObserver(this, WebPreferences::webPreferencesChangedNotification(), 0);

    if (m_backForwardList)
        m_backForwardList->Release();
    if (m_frameLoadDelegate)
        m_frameLoadDelegate->Release();
    if (m_frameLoadDelegatePrivate)
        m_frameLoadDelegatePrivate->Release();
    if (m_uiDelegate)
        m_uiDelegate->Release();
    if (m_uiDelegatePrivate)
        m_uiDelegatePrivate->Release();
    if (m_formDelegate)
        m_formDelegate->Release();
    if (m_preferences)
        m_preferences->Release();

    delete m_page;

    deleteBackingStore();

    gClassCount--;
}

WebView* WebView::createInstance()
{
    WebView* instance = new WebView();
    instance->AddRef();
    return instance;
}

void WebView::deleteBackingStore()
{
    if (m_backingStoreBitmap) {
        ::DeleteObject(m_backingStoreBitmap);
        m_backingStoreBitmap = 0;
    }

    if (m_backingStoreDirtyRegion) {
        ::DeleteObject(m_backingStoreDirtyRegion);
        m_backingStoreDirtyRegion = 0;
    }

    m_backingStoreSize.cx = m_backingStoreSize.cy = 0;
}

bool WebView::ensureBackingStore()
{
    RECT windowRect;
    ::GetClientRect(m_viewWindow, &windowRect);
    LONG width = windowRect.right - windowRect.left;
    LONG height = windowRect.bottom - windowRect.top;
    if (width > 0 && height > 0 && (width != m_backingStoreSize.cx || height != m_backingStoreSize.cy)) {
        deleteBackingStore();

        m_backingStoreSize.cx = width;
        m_backingStoreSize.cy = height;
        BITMAPINFO bitmapInfo;
        bitmapInfo.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth         = width; 
        bitmapInfo.bmiHeader.biHeight        = -height;
        bitmapInfo.bmiHeader.biPlanes        = 1;
        bitmapInfo.bmiHeader.biBitCount      = 32;
        bitmapInfo.bmiHeader.biCompression   = BI_RGB;
        bitmapInfo.bmiHeader.biSizeImage     = 0;
        bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
        bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
        bitmapInfo.bmiHeader.biClrUsed       = 0;
        bitmapInfo.bmiHeader.biClrImportant  = 0;

        void* pixels = NULL;
        m_backingStoreBitmap = ::CreateDIBSection(NULL, &bitmapInfo, DIB_RGB_COLORS, &pixels, NULL, 0);
        return true;
    }

    return false;
}

void WebView::addToDirtyRegion(const IntRect& dirtyRect)
{
    HRGN newRegion = ::CreateRectRgn(dirtyRect.x(), dirtyRect.y(),
                                     dirtyRect.right(), dirtyRect.bottom());
    addToDirtyRegion(newRegion);
}

void WebView::addToDirtyRegion(HRGN newRegion)
{
    if (m_backingStoreDirtyRegion) {
        HRGN combinedRegion = ::CreateRectRgn(0,0,0,0);
        ::CombineRgn(combinedRegion, m_backingStoreDirtyRegion, newRegion, RGN_OR);
        ::DeleteObject(m_backingStoreDirtyRegion);
        ::DeleteObject(newRegion);
        m_backingStoreDirtyRegion = combinedRegion;
    } else
        m_backingStoreDirtyRegion = newRegion;
}

void WebView::scrollBackingStore(FrameView* frameView, int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    // Make a region to hold the invalidated scroll area.
    HRGN updateRegion = ::CreateRectRgn(0, 0, 0, 0);

    // Collect our device context info and select the bitmap to scroll.
    HDC windowDC = ::GetDC(m_viewWindow);
    HDC bitmapDC = ::CreateCompatibleDC(windowDC);
    ::SelectObject(bitmapDC, m_backingStoreBitmap);
    
    // Scroll the bitmap.
    RECT scrollRectWin(scrollViewRect);
    RECT clipRectWin(clipRect);
    ::ScrollDC(bitmapDC, dx, dy, &scrollRectWin, &clipRectWin, updateRegion, 0);
    RECT regionBox;
    ::GetRgnBox(updateRegion, &regionBox);

    // Flush.
    GdiFlush();

    // Add the dirty region to the backing store's dirty region.
    addToDirtyRegion(updateRegion);

    if (m_uiDelegatePrivate)
        m_uiDelegatePrivate->webViewScrolled(this);

    // Update the backing store.
    updateBackingStore(frameView, bitmapDC, false);

    // Clean up.
    ::DeleteDC(bitmapDC);
    ::ReleaseDC(m_viewWindow, windowDC);

}

void WebView::updateBackingStore(FrameView* frameView, HDC dc, bool backingStoreCompletelyDirty)
{
    HDC windowDC = 0;
    HDC bitmapDC = dc;
    if (!dc) {
        windowDC = ::GetDC(m_viewWindow);
        bitmapDC = ::CreateCompatibleDC(windowDC);
        ::SelectObject(bitmapDC, m_backingStoreBitmap);
    }

    if (m_backingStoreDirtyRegion || backingStoreCompletelyDirty) {
        // This emulates the Mac smarts for painting rects intelligently.  This is
        // very important for us, since we double buffer based off dirty rects.
        bool useRegionBox = true;
        const int cRectThreshold = 10;
        const float cWastedSpaceThreshold = 0.75f;
        RECT regionBox;
        if (!backingStoreCompletelyDirty) {
            ::GetRgnBox(m_backingStoreDirtyRegion, &regionBox);
            DWORD regionDataSize = GetRegionData(m_backingStoreDirtyRegion, sizeof(RGNDATA), NULL);
            if (regionDataSize) {
                RGNDATA* regionData = (RGNDATA*)malloc(regionDataSize);
                GetRegionData(m_backingStoreDirtyRegion, regionDataSize, regionData);
                if (regionData->rdh.nCount <= cRectThreshold) {
                    double unionPixels = (regionBox.right - regionBox.left) * (regionBox.bottom - regionBox.top);
                    double singlePixels = 0;
                    
                    unsigned i;
                    RECT* rect;
                    for (i = 0, rect = (RECT*)regionData->Buffer; i < regionData->rdh.nCount; i++, rect++)
                        singlePixels += (rect->right - rect->left) * (rect->bottom - rect->top);
                    double wastedSpace = 1.0 - (singlePixels / unionPixels);
                    if (wastedSpace > cWastedSpaceThreshold) {
                        // Paint individual rects.
                        useRegionBox = false;
                        for (i = 0, rect = (RECT*)regionData->Buffer; i < regionData->rdh.nCount; i++, rect++)
                            paintIntoBackingStore(frameView, bitmapDC, rect);
                    }
                }
                free(regionData);
            }
        } else
            ::GetClientRect(m_viewWindow, &regionBox);

        if (useRegionBox)
            paintIntoBackingStore(frameView, bitmapDC, &regionBox);

        if (m_backingStoreDirtyRegion) {
            ::DeleteObject(m_backingStoreDirtyRegion);
            m_backingStoreDirtyRegion = 0;
        }
    }

    if (!dc) {
        ::DeleteDC(bitmapDC);
        ::ReleaseDC(m_viewWindow, windowDC);
    }

    GdiFlush();
}

void WebView::paint(HDC dc, LPARAM options)
{
    // Do a layout first so that everything we render is always current.
    m_mainFrame->layoutIfNeeded();

    FrameView* frameView = m_mainFrame->impl()->view();

    RECT rcPaint;
    HDC hdc;
    HRGN region = 0;
    int regionType = NULLREGION;
    PAINTSTRUCT ps;
    if (!dc) {
        region = CreateRectRgn(0,0,0,0);
        regionType = GetUpdateRgn(m_viewWindow, region, false);
        hdc = BeginPaint(m_viewWindow, &ps);
        rcPaint = ps.rcPaint;
    } else {
        hdc = dc;
        ::GetClientRect(m_viewWindow, &rcPaint);
        if (options & PRF_ERASEBKGND)
            ::FillRect(hdc, &rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));
    }

    HDC bitmapDC = ::CreateCompatibleDC(hdc);
    bool backingStoreCompletelyDirty = ensureBackingStore();
    ::SelectObject(bitmapDC, m_backingStoreBitmap);

    // Update our backing store if needed.
    updateBackingStore(frameView, bitmapDC, backingStoreCompletelyDirty);

    // Now we blit the updated backing store
    IntRect windowDirtyRect = rcPaint;
    
    // Apply the same heuristic for this update region too.
    bool useWindowDirtyRect = true;
    if (region && regionType == COMPLEXREGION) {
        const int cRectThreshold = 10;
        const float cWastedSpaceThreshold = 0.75f;
        DWORD regionDataSize = GetRegionData(region, sizeof(RGNDATA), NULL);
        if (regionDataSize) {
            RGNDATA* regionData = (RGNDATA*)malloc(regionDataSize);
            GetRegionData(region, regionDataSize, regionData);
            if (regionData->rdh.nCount <= cRectThreshold) {
                double unionPixels = windowDirtyRect.width() * windowDirtyRect.height();
                double singlePixels = 0;
                
                unsigned i;
                RECT* rect;
                for (i = 0, rect = (RECT*)regionData->Buffer; i < regionData->rdh.nCount; i++, rect++)
                    singlePixels += (rect->right - rect->left) * (rect->bottom - rect->top);
                double wastedSpace = 1.0 - (singlePixels / unionPixels);
                if (wastedSpace > cWastedSpaceThreshold) {
                    // Paint individual rects.
                    useWindowDirtyRect = false;
                    for (i = 0, rect = (RECT*)regionData->Buffer; i < regionData->rdh.nCount; i++, rect++)
                        paintIntoWindow(bitmapDC, hdc, rect);
                }
            }
            free(regionData);
        }
    }
    
    ::DeleteObject(region);

    if (useWindowDirtyRect)
        paintIntoWindow(bitmapDC, hdc, &rcPaint);

    ::DeleteDC(bitmapDC);

    // Paint the gripper.
    IWebUIDelegate* ui;
    if (SUCCEEDED(uiDelegate(&ui))) {
        IWebUIDelegatePrivate* uiPrivate;
        if (SUCCEEDED(ui->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&uiPrivate))) {
            RECT r;
            if (SUCCEEDED(uiPrivate->webViewResizerRect(this, &r)))
                uiPrivate->webViewDrawResizer(this, hdc, (frameView->resizerOverlapsContent() ? true : false), &r);
            uiPrivate->Release();
        }
        ui->Release();
    }

    if (!dc)
        EndPaint(m_viewWindow, &ps);
}

void WebView::paintIntoBackingStore(FrameView* frameView, HDC bitmapDC, LPRECT dirtyRect)
{
#if FLASH_BACKING_STORE_REDRAW
    HDC dc = ::GetDC(m_viewWindow);
    HBRUSH yellowBrush = CreateSolidBrush(RGB(255, 255, 0));
    FillRect(dc, dirtyRect, yellowBrush);
    DeleteObject(yellowBrush);
    GdiFlush();
    Sleep(50);
    paintIntoWindow(bitmapDC, dc, dirtyRect);
    ::ReleaseDC(m_viewWindow, dc);
#endif

    FillRect(bitmapDC, dirtyRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
    GraphicsContext gc(bitmapDC);
    gc.save();
    gc.clip(IntRect(*dirtyRect));
    frameView->paint(&gc, IntRect(*dirtyRect));
    gc.restore();
}

void WebView::paintIntoWindow(HDC bitmapDC, HDC windowDC, LPRECT dirtyRect)
{
#if FLASH_WINDOW_REDRAW
    HBRUSH greenBrush = CreateSolidBrush(RGB(0, 255, 0));
    FillRect(windowDC, dirtyRect, greenBrush);
    DeleteObject(greenBrush);
    GdiFlush();
    Sleep(50);
#endif

    // Blit the dirty rect from the backing store into the same position
    // in the destination DC.
    BitBlt(windowDC, dirtyRect->left, dirtyRect->top, dirtyRect->right - dirtyRect->left, dirtyRect->bottom - dirtyRect->top, bitmapDC,
           dirtyRect->left, dirtyRect->top, SRCCOPY);
}

#define MAXIMUM_DPI 300

static void getPrintRects(HDC printDC, IntRect& devicePrintRect, IntRect& rasterizingPrintRect)
{
    devicePrintRect = IntRect(0, 0, 0, 0);
    rasterizingPrintRect = IntRect(0, 0, 0, 0);
    if (!printDC)
        return;

    int dpiX = GetDeviceCaps(printDC, LOGPIXELSX);
    int dpiY = GetDeviceCaps(printDC, LOGPIXELSY);
    double scaleX = (dpiX <= MAXIMUM_DPI) ? 1.0 : (double)MAXIMUM_DPI / (double)dpiX;
    double scaleY = (dpiY <= MAXIMUM_DPI) ? 1.0 : (double)MAXIMUM_DPI / (double)dpiY;

    devicePrintRect = IntRect(0, 0, 
                      GetDeviceCaps(printDC, PHYSICALWIDTH)  - 2 * GetDeviceCaps(printDC, PHYSICALOFFSETX),
                      GetDeviceCaps(printDC, PHYSICALHEIGHT) - 2 * GetDeviceCaps(printDC, PHYSICALOFFSETY) );
    rasterizingPrintRect = devicePrintRect;
    rasterizingPrintRect.setSize(IntSize((int)(scaleX*devicePrintRect.width()), (int)(scaleY*devicePrintRect.height())));
}

HRESULT STDMETHODCALLTYPE WebView::startPrintJob( 
    /* [in] */ HDC printDC)
{
    if (!printDC)
        return E_POINTER;

    FrameWin* frame = static_cast<FrameWin*>(focusedTargetFrame());
    if (!frame || !frame->document() || frame->document()->printing())
        return E_FAIL;

    IntRect devicePrintRect;
    IntRect rasterizingPrintRect;
    getPrintRects(printDC, devicePrintRect, rasterizingPrintRect);
    if (rasterizingPrintRect.isEmpty())
        return E_FAIL;

    frame->document()->setPrinting(true);
    m_pages = frame->computePageRects(rasterizingPrintRect, 1.0f);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::endPrintJob( 
    /* [in] */ HDC /*printDC*/)
{
    FrameWin* frame = static_cast<FrameWin*>(focusedTargetFrame());
    if (!frame || !frame->document() || !frame->document()->printing())
        return E_FAIL;

    frame->document()->setPrinting(false);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::getPrintedPageCount( 
    /* [in] */ HDC printDC,
    /* [retval][out] */ UINT *pageCount)
{
    *pageCount = 0;
    if (!printDC)
        return E_POINTER;

    FrameWin* frame = static_cast<FrameWin*>(focusedTargetFrame());
    if (!frame || !frame->document())
        return E_FAIL;

    if (frame->document()->printing())
        *pageCount = (UINT) m_pages.size();
    else {
        IntRect devicePrintRect;
        IntRect rasterizingPrintRect;
        getPrintRects(printDC, devicePrintRect, rasterizingPrintRect);
        if (rasterizingPrintRect.isEmpty())
            return E_FAIL;
        *pageCount = (UINT) frame->computePageRects(rasterizingPrintRect, 1.0f).size();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::printPage( 
    /* [in] */ HDC printDC,
    UINT pageNumber)
{
    if (!printDC)
        return E_POINTER;

    HRESULT hr = S_OK;
    HDC bitmapDC = 0;
    HBITMAP bitmap = 0;

    FrameWin* frame = static_cast<FrameWin*>(focusedTargetFrame());
    if (!frame || !frame->document())
        return E_FAIL;

    // will be false if we're called from print preview
    bool inPrintJob = frame->document()->printing();
    if (!inPrintJob)
        startPrintJob(printDC);

    IntRect devicePrintRect;
    IntRect rasterizingPrintRect;
    getPrintRects(printDC, devicePrintRect, rasterizingPrintRect);
    if (devicePrintRect.isEmpty() || rasterizingPrintRect.isEmpty())
        return E_FAIL;

    RECT rcPaint = devicePrintRect;
#ifdef PRE_FILL_BLANKS
    HBRUSH yellowBrush = CreateSolidBrush(RGB(255, 255, 0));
    FillRect(dc, &rcPaint, yellowBrush);
#endif

    if (pageNumber <= m_pages.size()) {
        IntRect pageRect = m_pages[(long)pageNumber-1];

        BITMAPINFO bitmapInfo;
        bitmapInfo.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth         = rasterizingPrintRect.width(); 
        bitmapInfo.bmiHeader.biHeight        = -rasterizingPrintRect.height();
        bitmapInfo.bmiHeader.biPlanes        = 1;
        bitmapInfo.bmiHeader.biBitCount      = 32;
        bitmapInfo.bmiHeader.biCompression   = BI_RGB;
        bitmapInfo.bmiHeader.biSizeImage     = 0;
        bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
        bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
        bitmapInfo.bmiHeader.biClrUsed       = 0;
        bitmapInfo.bmiHeader.biClrImportant  = 0;

        void* pixels = 0;
        bitmap = CreateDIBSection(printDC, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0);
        if (!bitmap) {
            hr = E_OUTOFMEMORY;
            goto exit;
        }
        bitmapDC = CreateCompatibleDC(printDC);
        if (!bitmapDC) {
            hr = E_OUTOFMEMORY;
            goto exit;
        }
        HBITMAP oldBitmap = (HBITMAP)SelectObject(bitmapDC, bitmap);
        ASSERT(oldBitmap);

        FillRect(bitmapDC, &rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));
#ifdef PRE_FILL_BLANKS
        HBRUSH redBrush = CreateSolidBrush(RGB(255, 0, 0));
        FillRect(bitmapDC, &rcPaint, redBrush);
#endif

        GraphicsContext gc(bitmapDC);

        CGFloat scale = (float)rasterizingPrintRect.width()/ (float)pageRect.width();
        CGContextScaleCTM(gc.platformContext(), scale, scale);
        CGContextTranslateCTM(gc.platformContext(), CGFloat(-pageRect.x()),
                                                    CGFloat(-pageRect.y()));


        frame->paint(&gc, pageRect);

        StretchDIBits(printDC, 0, 0, devicePrintRect.width(), devicePrintRect.height(), 0, 0, rasterizingPrintRect.width(), rasterizingPrintRect.height(), pixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);

        SelectObject(bitmapDC, oldBitmap);
    }

exit:
    if (bitmapDC)
        DeleteDC(bitmapDC);
    if (bitmap)
        DeleteObject(bitmap);
    if (!inPrintJob)
        endPrintJob(printDC);
    return hr;
}

Page* WebView::page()
{
    return m_page;
}

void WebView::handleMouseEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
    static LONG globalClickCount;
    static IntPoint globalPrevPoint;
    static MouseButton globalPrevButton;
    static LONG globalPrevMouseDownTime;

    // Create our event.
    PlatformMouseEvent mouseEvent(m_viewWindow, message, wParam, lParam);
   
    bool insideThreshold = abs(globalPrevPoint.x() - mouseEvent.pos().x()) < ::GetSystemMetrics(SM_CXDOUBLECLK) &&
                           abs(globalPrevPoint.y() - mouseEvent.pos().y()) < ::GetSystemMetrics(SM_CYDOUBLECLK);
    LONG messageTime = ::GetMessageTime();
    
    if (message == WM_LBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_RBUTTONDOWN) {
        // FIXME: I'm not sure if this is the "right" way to do this
        // but without this call, we never become focused since we don't allow
        // the default handling of mouse events.
        SetFocus(m_viewWindow);

        // Always start capturing events when the mouse goes down in our HWND.
        ::SetCapture(m_viewWindow);
        m_page->mainFrame()->view()->setCapturingMouse(true);

        if (((messageTime - globalPrevMouseDownTime) < (LONG)::GetDoubleClickTime()) && 
            insideThreshold &&
            mouseEvent.button() == globalPrevButton)
            globalClickCount++;
        else
            // Reset the click count.
            globalClickCount = 1;
        globalPrevMouseDownTime = messageTime;
        
        mouseEvent.setClickCount(globalClickCount);
        m_page->mainFrame()->eventHandler()->handleMousePressEvent(mouseEvent);
    } else if (message == WM_LBUTTONDBLCLK || message == WM_MBUTTONDBLCLK || message == WM_RBUTTONDBLCLK) {
        globalClickCount = 2;
        mouseEvent.setClickCount(globalClickCount);
        m_page->mainFrame()->eventHandler()->handleMousePressEvent(mouseEvent);
    } else if (message == WM_LBUTTONUP || message == WM_MBUTTONUP || message == WM_RBUTTONUP) {
        // Record the global position and the button of the up.
        globalPrevButton = mouseEvent.button();
        globalPrevPoint = mouseEvent.pos();
        mouseEvent.setClickCount(globalClickCount);
        Widget* capturingTarget = m_page->mainFrame()->view()->capturingTarget();
        ASSERT(capturingTarget);
        capturingTarget->handleMouseReleaseEvent(mouseEvent);
        capturingTarget->setCapturingMouse(false);
        ::ReleaseCapture();
    } else if (message == WM_MOUSEMOVE) {
        if (!insideThreshold)
            globalClickCount = 0;
        mouseEvent.setClickCount(globalClickCount);
        Widget* capturingTarget = m_page->mainFrame()->view()->capturingTarget();
        capturingTarget->handleMouseMoveEvent(mouseEvent);
    }
}

void WebView::mouseWheel(WPARAM wParam, LPARAM lParam)
{
    PlatformWheelEvent wheelEvent(m_viewWindow, wParam, lParam);
    m_mainFrame->impl()->eventHandler()->handleWheelEvent(wheelEvent);
}

bool WebView::execCommand(WPARAM wParam, LPARAM /*lParam*/)
{
    Frame* frame = focusedTargetFrame();
    bool handled = false;
    switch (LOWORD(wParam)) {
    case Cut:
        handled = frame->editor()->execCommand("Cut");
        break;
    case Copy:
        handled = frame->editor()->execCommand("Copy");
        break;
    case Paste:
        handled = frame->editor()->execCommand("Paste");
        break;
    case ForwardDelete:
        handled = frame->editor()->execCommand("ForwardDelete");
        break;
    case SelectAll:
        handled = frame->editor()->execCommand("SelectAll");
        break;
    case Undo:
        handled = frame->editor()->execCommand("Undo");
        break;
    case Redo:
        handled = frame->editor()->execCommand("Redo");
        break;
    default:
        break;
    }
    return handled;
}

FrameView* WebView::focusedTarget()
{
    // FIXME: I'm not sure this cast is valid if you have a focused plug-in.
    return static_cast<FrameView*>(m_page->mainFrame()->view()->focusedTarget());
}

Frame* WebView::focusedTargetFrame()
{
    return focusedTarget()->frame();
}

bool WebView::keyUp(WPARAM wParam, LPARAM lParam)
{
    PlatformKeyboardEvent keyEvent(m_viewWindow, wParam, lParam);
    FrameWin* frame = static_cast<FrameWin*>(focusedTargetFrame());

    return frame->keyEvent(keyEvent);
}

static const unsigned CtrlKey = 1 << 0;
static const unsigned AltKey = 1 << 1;
static const unsigned ShiftKey = 1 << 2;


struct KeyEntry {
    unsigned virtualKey;
    unsigned modifiers;
    const char* name;
};

static const KeyEntry keyEntries[] = {
    { VK_LEFT,   0,                  "MoveLeft"                                    },
    { VK_LEFT,   ShiftKey,           "MoveLeftAndModifySelection"                  },
    { VK_LEFT,   CtrlKey,            "MoveWordLeft"                                },
    { VK_LEFT,   CtrlKey | ShiftKey, "MoveWordLeftAndModifySelection"              },
    { VK_RIGHT,  0,                  "MoveRight"                                   },
    { VK_RIGHT,  ShiftKey,           "MoveRightAndModifySelection"                 },
    { VK_RIGHT,  CtrlKey,            "MoveWordRight"                               },
    { VK_RIGHT,  CtrlKey | ShiftKey, "MoveWordRightAndModifySelection"             },
    { VK_UP,     0,                  "MoveUp"                                      },
    { VK_UP,     ShiftKey,           "MoveUpAndModifySelection"                    },
    { VK_DOWN,   0,                  "MoveDown"                                    },
    { VK_DOWN,   ShiftKey,           "MoveDownAndModifySelection"                  },
    { VK_HOME,   0,                  "MoveToBeginningOfLine"                       },
    { VK_HOME,   ShiftKey,           "MoveToBeginningOfLineAndModifySelection"     },
    { VK_HOME,   CtrlKey,            "MoveToBeginningOfDocument"                   },
    { VK_HOME,   CtrlKey | ShiftKey, "MoveToBeginningOfDocumentAndModifySelection" },

    { VK_END,    0,                  "MoveToEndOfLine"                             },
    { VK_END,    ShiftKey,           "MoveToEndOfLineAndModifySelection"           },
    { VK_END,    CtrlKey,            "MoveToEndOfDocument"                         },
    { VK_END,    CtrlKey | ShiftKey, "MoveToEndOfDocumentAndModifySelection"       },

    { VK_BACK,   0,                  "Delete"                                      },
    { VK_DELETE, 0,                  "ForwardDelete"                               },
    
    { 'B',       CtrlKey,            "ToggleBold"                                  },
    { 'I',       CtrlKey,            "ToggleItalic"                                },
};

const char* editCommandForKey(const PlatformKeyboardEvent& keyEvent)
{
    static HashMap<int, const char*>* commandsMap = 0;

    if (!commandsMap) {
        commandsMap = new HashMap<int, const char*>;

        for (unsigned i = 0; i < _countof(keyEntries); i++)
            commandsMap->set(keyEntries[i].modifiers << 16 | keyEntries[i].virtualKey, keyEntries[i].name);
    }

    unsigned modifiers = 0;
    if (keyEvent.shiftKey())
        modifiers |= ShiftKey;
    if (keyEvent.altKey())
        modifiers |= AltKey;
    if (keyEvent.ctrlKey())
        modifiers |= CtrlKey;

    return commandsMap->get(modifiers << 16 | keyEvent.WindowsKeyCode());
}


bool WebView::handleEditingKeyboardEvent(FrameWin* frame, const PlatformKeyboardEvent& keyEvent)
{
    const char* editCommand = editCommandForKey(keyEvent);

    if (editCommand) {
        frame->editor()->execCommand(editCommand);
        return true;
    }

    // We don't want unhandled commands sent to the char handler
    if (keyEvent.ctrlKey())
        return true;

    switch (keyEvent.WindowsKeyCode()) {
        // Say we handle this so it won't get passed to the char handler.
        case VK_ESCAPE:
            break;
        case VK_RETURN:
            if (frame->selectionController()->isContentRichlyEditable())
                TypingCommand::insertParagraphSeparator(frame->document());
            else
                TypingCommand::insertLineBreak(frame->document());
            break;
        default:
            return false;
    }
    return true;
}

bool WebView::keyDown(WPARAM wParam, LPARAM lParam)
{
    PlatformKeyboardEvent keyEvent(m_viewWindow, wParam, lParam);
    FrameWin* frame = static_cast<FrameWin*>(focusedTargetFrame());

    if (frame->keyEvent(keyEvent))
        return true;

    if (frame->selectionController()->isContentEditable())
        return handleEditingKeyboardEvent(frame, keyEvent);

    // Need to scroll the page if the arrow keys, space(shift), pgup/dn, or home/end are hit.
    ScrollDirection direction;
    ScrollGranularity granularity;
    switch (keyEvent.WindowsKeyCode()) {
        case VK_LEFT:
            granularity = ScrollByLine;
            direction = ScrollLeft;
            break;
        case VK_RIGHT:
            granularity = ScrollByLine;
            direction = ScrollRight;
            break;
        case VK_UP:
            granularity = ScrollByLine;
            direction = ScrollUp;
            break;
        case VK_DOWN:
            granularity = ScrollByLine;
            direction = ScrollDown;
            break;
        case VK_HOME:
            granularity = ScrollByDocument;
            direction = ScrollUp;
            break;
        case VK_END:
            granularity = ScrollByDocument;
            direction = ScrollDown;
            break;
        case VK_SPACE:
            granularity = ScrollByPage;
            direction = (GetKeyState(VK_SHIFT) & 0x8000) ? ScrollUp : ScrollDown;
            break;
        case VK_PRIOR:
            granularity = ScrollByPage;
            direction = ScrollUp;
            break;
        case VK_NEXT:
            granularity = ScrollByPage;
            direction = ScrollDown;
            break;
        default:
            // We return true here so the WM_CHAR handler won't pick up unhandled messages.
            return true;
    }

    focusedTarget()->scroll(direction, granularity);

    return true;
}

bool WebView::inResizer(LPARAM lParam)
{
    if (!m_uiDelegatePrivate)
        return false;

    RECT r;
    if (FAILED(m_uiDelegatePrivate->webViewResizerRect(this, &r)))
        return false;

    POINT pt;
    pt.x = LOWORD(lParam);
    pt.y = HIWORD(lParam);
    return !!PtInRect(&r, pt);
}

static ATOM registerWebViewWindowClass()
{
    static bool haveRegisteredWindowClass = false;
    if (haveRegisteredWindowClass)
        return true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_DBLCLKS;
    wcex.lpfnWndProc    = WebViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 4; // 4 bytes for the IWebView pointer
    wcex.hInstance      = gInstance;
    wcex.hIcon          = 0;
    wcex.hCursor        = 0;
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebViewWindowClassName;
    wcex.hIconSm        = 0;

    return RegisterClassEx(&wcex);
}

static LRESULT CALLBACK WebViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool handledByKeydown = false;
    LRESULT lResult = 0;
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, 0);
    WebView* webView = reinterpret_cast<WebView*>(longPtr);
    WebFrame* mainFrameImpl = webView ? webView->topLevelFrame() : 0;
    if (!mainFrameImpl)
        return DefWindowProc(hWnd, message, wParam, lParam);

    switch (message) {
        case WM_PAINT: {
            IWebDataSource* dataSource = 0;
            mainFrameImpl->dataSource(&dataSource);
            if (!dataSource || mainFrameImpl->impl()->view()->didFirstLayout())
                webView->paint(0, 0);
            else
                ValidateRect(hWnd, 0);
            if (dataSource)
                dataSource->Release();
            break;
        }
        case WM_PRINTCLIENT:
            webView->paint((HDC)wParam, lParam);
            break;
        case WM_DESTROY:
            // Do nothing?
            break;
        case WM_MOUSEMOVE:
            if (webView->inResizer(lParam))
                SetCursor(LoadCursor(0, IDC_SIZENWSE));
            // fall through
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            if (mainFrameImpl->impl()->view()->didFirstLayout())
                webView->handleMouseEvent(message, wParam, lParam);
            break;
        case WM_MOUSEWHEEL:
            if (mainFrameImpl->impl()->view()->didFirstLayout())
                webView->mouseWheel(wParam, lParam);
            break;
        case WM_KEYDOWN:
            handledByKeydown = webView->keyDown(wParam, lParam);
            break;
        case WM_KEYUP:
            webView->keyUp(wParam, lParam);
            break;
        case WM_CHAR: 
            if (!handledByKeydown) {
                // FIXME: We need to use WM_UNICHAR to support supplementary characters.
                UChar c = (UChar)wParam;
                TypingCommand::insertText(webView->focusedTargetFrame()->document(), String(&c, 1), false);
                webView->focusedTargetFrame()->revealSelection(RenderLayer::gAlignToEdgeIfNeeded); 
            }
            break;
        case WM_SIZE:
            if (lParam != 0) {
                mainFrameImpl->setNeedsLayout();
                mainFrameImpl->impl()->view()->resize(LOWORD(lParam), HIWORD(lParam));

                if (!mainFrameImpl->loading())
                    mainFrameImpl->impl()->sendResizeEvent();
            }
            break;
        case WM_SHOWWINDOW:
            lResult = DefWindowProc(hWnd, message, wParam, lParam);
            if (wParam == 0)
                // The window is being hidden (e.g., because we switched tabs.
                // Null out our backing store.
                webView->deleteBackingStore();
            break;
        case WM_SETFOCUS:
            // It's ok to just always do setWindowHasFocus, since we won't fire the focus event on the DOM
            // window unless the value changes.  It's also ok to do setIsActive inside focus,
            // because Windows has no concept of having to update control tints (e.g., graphite vs. aqua)
            // and therefore only needs to update the selection (which is limited to the focused frame).
            webView->focusedTargetFrame()->setIsActive(true);
            webView->focusedTargetFrame()->setWindowHasFocus(true);
            break;
        case WM_KILLFOCUS: {
            // However here we have to be careful.  If we are losing focus because of a deactivate,
            // then we need to remember our focused target for restoration later.  
            // If we are losing focus to another part of our window, then we are no longer focused for real
            // and we need to clear out the focused target.
            webView->focusedTargetFrame()->setIsActive(false);
            if (GetAncestor(hWnd, GA_ROOT) == GetActiveWindow()) {
                webView->focusedTarget()->setFocused(false);
                webView->focusedTargetFrame()->setWindowHasFocus(false);
            }
            break;
        }
        case WM_CUT:
        case WM_COPY:
        case WM_PASTE:
        case WM_CLEAR:
        case WM_COMMAND:
            webView->execCommand(wParam, lParam);
            break;
        case WM_XP_THEMECHANGED:
            if (mainFrameImpl) {
                webView->deleteBackingStore();
                mainFrameImpl->impl()->view()->themeChanged();
            }
            break;
        default:
            lResult = DefWindowProc(hWnd, message, wParam, lParam);
    }

    return lResult;
}


HRESULT WebView::goToItem(IWebHistoryItem* item, WebFrameLoadType withLoadType)
{
    m_mainFrame->stopLoading();
    return m_mainFrame->goToItem(item, withLoadType);
}

HRESULT WebView::updateWebCoreSettingsFromPreferences(IWebPreferences* preferences)
{
    HRESULT hr;
    BSTR str;
    int size;
    BOOL enabled;
    bool newEnabled;
    bool changed = false;
    
    //[_private->settings setCursiveFontFamily:[preferences cursiveFontFamily]];
    hr = preferences->cursiveFontFamily(&str);
    if (FAILED(hr))
        return hr;
    AtomicString cursiveFont(str, SysStringLen(str));
    if (m_settings.cursiveFontName() != cursiveFont) {
        m_settings.setCursiveFontName(cursiveFont);
        changed = true;
    }
    SysFreeString(str);

    //[_private->settings setDefaultFixedFontSize:[preferences defaultFixedFontSize]];
    hr = preferences->defaultFixedFontSize(&size);
    if (FAILED(hr))
        return hr;
    if (m_settings.mediumFixedFontSize() != size) {
        m_settings.setMediumFixedFontSize(size);
        changed = true;
    }

    //[_private->settings setDefaultFontSize:[preferences defaultFontSize]];
    hr = preferences->defaultFontSize(&size);
    if (FAILED(hr))
        return hr;
    if (m_settings.mediumFontSize() != size) {
        m_settings.setMediumFontSize(size);
        changed = true;
    }
    
    //[_private->settings setDefaultTextEncoding:[preferences defaultTextEncodingName]];
    hr = preferences->defaultTextEncodingName(&str);
    if (FAILED(hr))
        return hr;
    DeprecatedString encoding((DeprecatedChar*)str, SysStringLen(str));
    if (m_settings.encoding() != encoding) {
        m_settings.setEncoding(encoding);
        changed = true;
    }
    SysFreeString(str);

    //[_private->settings setFantasyFontFamily:[preferences fantasyFontFamily]];
    hr = preferences->fantasyFontFamily(&str);
    if (FAILED(hr))
        return hr;
    AtomicString fantasyFont(str, SysStringLen(str));
    if (m_settings.fantasyFontName() != fantasyFont) {
        m_settings.setFantasyFontName(fantasyFont);
        changed = true;
    }
    SysFreeString(str);

    //[_private->settings setFixedFontFamily:[preferences fixedFontFamily]];
    hr = preferences->fixedFontFamily(&str);
    if (FAILED(hr))
        return hr;
    AtomicString fixedFont(str, SysStringLen(str));
    if (m_settings.fixedFontName() != fixedFont) {
        m_settings.setFixedFontName(fixedFont);
        changed = true;
    }
    SysFreeString(str);

    //[_private->settings setJavaEnabled:[preferences isJavaEnabled]];
    hr = preferences->isJavaEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    newEnabled = !!enabled;
    if (m_settings.isJavaEnabled() != newEnabled) {
        m_settings.setIsJavaEnabled(newEnabled);
        changed = true;
    }

    //[_private->settings setJavaScriptEnabled:[preferences isJavaScriptEnabled]];
    hr = preferences->isJavaScriptEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    newEnabled = !!enabled;
    if (m_settings.isJavaScriptEnabled() != newEnabled) {
        m_settings.setIsJavaScriptEnabled(newEnabled);
        changed = true;
    }

    //[_private->settings setJavaScriptCanOpenWindowsAutomatically:[preferences javaScriptCanOpenWindowsAutomatically]];
    hr = preferences->javaScriptCanOpenWindowsAutomatically(&enabled);
    if (FAILED(hr))
        return hr;
    newEnabled = !!enabled;
    if (m_settings.JavaScriptCanOpenWindowsAutomatically() != newEnabled) {
        m_settings.setJavaScriptCanOpenWindowsAutomatically(newEnabled);
        changed = true;
    }

    //[_private->settings setMinimumFontSize:[preferences minimumFontSize]];
    hr = preferences->minimumFontSize(&size);
    if (FAILED(hr))
        return hr;
    if (m_settings.minFontSize() != size) {
        m_settings.setMinFontSize(size);
        changed = true;
    }

    //[_private->settings setMinimumLogicalFontSize:[preferences minimumLogicalFontSize]];
    hr = preferences->minimumLogicalFontSize(&size);
    if (FAILED(hr))
        return hr;
    if (m_settings.minLogicalFontSize() != size) {
        m_settings.setMinLogicalFontSize(size);
        changed = true;
    }

    //[_private->settings setPluginsEnabled:[preferences arePlugInsEnabled]];
    hr = preferences->arePlugInsEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    newEnabled = !!enabled;
    if (m_settings.isPluginsEnabled() != newEnabled) {
        m_settings.setArePluginsEnabled(newEnabled);
        changed = true;
    }

    //[_private->settings setPrivateBrowsingEnabled:[preferences privateBrowsingEnabled]];
    hr = preferences->privateBrowsingEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    newEnabled = !!enabled;
    if (m_settings.privateBrowsingEnabled() != newEnabled) {
        m_settings.setPrivateBrowsingEnabled(newEnabled);
        changed = true;
    }

    //[_private->settings setSansSerifFontFamily:[preferences sansSerifFontFamily]];
    hr = preferences->sansSerifFontFamily(&str);
    if (FAILED(hr))
        return hr;
    AtomicString sansSerifFont(str, SysStringLen(str));
    if (m_settings.sansSerifFontName() != sansSerifFont) {
        m_settings.setSansSerifFontName(sansSerifFont);
        changed = true;
    }
    SysFreeString(str);

    //[_private->settings setSerifFontFamily:[preferences serifFontFamily]];
    hr = preferences->serifFontFamily(&str);
    if (FAILED(hr))
        return hr;
    AtomicString serifFont(str, SysStringLen(str));
    if (m_settings.serifFontName() != serifFont) {
        m_settings.setSerifFontName(serifFont);
        changed = true;
    }
    SysFreeString(str);

    //[_private->settings setStandardFontFamily:[preferences standardFontFamily]];
    hr = preferences->standardFontFamily(&str);
    if (FAILED(hr))
        return hr;
    AtomicString standardFont(str, SysStringLen(str));
    if (m_settings.stdFontName() != standardFont) {
        m_settings.setStdFontName(standardFont);
        changed = true;
    }
    SysFreeString(str);

    //[_private->settings setWillLoadImagesAutomatically:[preferences loadsImagesAutomatically]];
    hr = preferences->loadsImagesAutomatically(&enabled);
    if (FAILED(hr))
        return hr;
    newEnabled = !!enabled;
    if (m_settings.autoLoadImages() != newEnabled) {
        m_settings.setAutoLoadImages(newEnabled);
        changed = true;
    }

    //if ([preferences userStyleSheetEnabled]) {
    hr = preferences->userStyleSheetEnabled(&enabled);
    if (FAILED(hr))
        return hr;
    if (enabled) {
        //[_private->settings setUserStyleSheetLocation:[[preferences userStyleSheetLocation] _web_originalDataAsString]];
        hr = preferences->userStyleSheetLocation(&str);
        if (FAILED(hr))
            return hr;
        DeprecatedString newURL((DeprecatedChar*)str, SysStringLen(str));
        if (m_settings.userStyleSheetLocation().url() != newURL) {
            m_settings.setUserStyleSheetLocation(KURL(newURL));
            changed = true;
        }
        SysFreeString(str);
    } else {
        //[_private->settings setUserStyleSheetLocation:@""];
        DeprecatedString emptyURLStr("");
        if (m_settings.userStyleSheetLocation().url() != emptyURLStr) {
            m_settings.setUserStyleSheetLocation(KURL(emptyURLStr));
            changed = true;
        }
    }

    //[_private->settings setShouldPrintBackgrounds:[preferences shouldPrintBackgrounds]];
    hr = preferences->shouldPrintBackgrounds(&enabled);
    if (FAILED(hr))
        return hr;
    newEnabled = !!enabled;
    if (m_settings.shouldPrintBackgrounds() != newEnabled) {
        m_settings.setShouldPrintBackgrounds(newEnabled);
        changed = true;
    }

    //[_private->settings setTextAreasAreResizable:[preferences textAreasAreResizable]];
    hr = preferences->textAreasAreResizable(&enabled);
    if (FAILED(hr))
        return hr;
    newEnabled = !!enabled;
    if (m_settings.textAreasAreResizable() != newEnabled) {
        m_settings.setTextAreasAreResizable(newEnabled);
        changed = true;
    }

    if (changed) {
        Page::setNeedsReapplyStylesForSettingsChange(&m_settings);
        m_mainFrame->invalidate(); //FIXME
    }

    return S_OK;
}

Settings* WebView::settings()
{
    return &m_settings;
}

static String osVersion()
{
    String osVersion;
    OSVERSIONINFO versionInfo = {0};
    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    GetVersionEx(&versionInfo);

    if (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
        if (versionInfo.dwMajorVersion == 4) {
            if (versionInfo.dwMinorVersion == 0)
                osVersion = "Windows 95";
            else if (versionInfo.dwMinorVersion == 10)
                osVersion = "Windows 98";
            else if (versionInfo.dwMinorVersion == 90)
                osVersion = "Windows 98; Win 9x 4.90";
        }
    } else if (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
        osVersion = String::format("Windows NT %d.%d", versionInfo.dwMajorVersion, versionInfo.dwMinorVersion);

    if (!osVersion.length())
        osVersion = String::format("Windows %d.%d", versionInfo.dwMajorVersion, versionInfo.dwMinorVersion);

    return osVersion;
}

static String language()
{
    TCHAR languageName[256];
    if (!GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, languageName, ARRAYSIZE(languageName)))
        return "en";
    else
        return String(languageName, (unsigned int)_tcslen(languageName));
}

static String webKitVersion()
{
    String versionStr = "420+";
    void* data = 0;

    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate;

    TCHAR path[MAX_PATH];
    GetModuleFileName(gInstance, path, ARRAYSIZE(path));
    DWORD handle;
    DWORD versionSize = GetFileVersionInfoSize(path, &handle);
    if (!versionSize)
        goto exit;
    data = malloc(versionSize);
    if (!data)
        goto exit;
    if (!GetFileVersionInfo(path, 0, versionSize, data))
        goto exit;
    UINT cbTranslate;
    if (!VerQueryValue(data, TEXT("\\VarFileInfo\\Translation"), (LPVOID*)&lpTranslate, &cbTranslate))
        goto exit;
    TCHAR key[256];
    _stprintf_s(key, ARRAYSIZE(key), TEXT("\\StringFileInfo\\%04x%04x\\ProductVersion"), lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);
    LPCTSTR productVersion;
    UINT productVersionLength;
    if (!VerQueryValue(data, (LPTSTR)(LPCTSTR)key, (void**)&productVersion, &productVersionLength))
        goto exit;
    versionStr = String(productVersion, productVersionLength);

exit:
    if (data)
        free(data);
    return versionStr;
}

const String& WebView::userAgentForKURL(const KURL& /*url*/)
{
    if (m_userAgentOverridden)
        return m_userAgentCustom;

    if (!m_userAgentStandard.length())
        m_userAgentStandard = String::format("Mozilla/5.0 (Windows; U; %s; %s) AppleWebKit/%s (KHTML, like Gecko)%s%s", osVersion().latin1().data(), language().latin1().data(), webKitVersion().latin1().data(), (m_applicationName.length() ? " " : ""), m_applicationName.latin1().data());

    return m_userAgentStandard;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, CLSID_WebView))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebView*>(this);
    else if (IsEqualGUID(riid, IID_IWebView))
        *ppvObject = static_cast<IWebView*>(this);
    else if (IsEqualGUID(riid, IID_IWebViewPrivate))
        *ppvObject = static_cast<IWebViewPrivate*>(this);
    else if (IsEqualGUID(riid, IID_IWebIBActions))
        *ppvObject = static_cast<IWebIBActions*>(this);
    else if (IsEqualGUID(riid, IID_IWebViewCSS))
        *ppvObject = static_cast<IWebViewCSS*>(this);
    else if (IsEqualGUID(riid, IID_IWebViewEditing))
        *ppvObject = static_cast<IWebViewEditing*>(this);
    else if (IsEqualGUID(riid, IID_IWebViewUndoableEditing))
        *ppvObject = static_cast<IWebViewUndoableEditing*>(this);
    else if (IsEqualGUID(riid, IID_IWebViewEditingActions))
        *ppvObject = static_cast<IWebViewEditingActions*>(this);
    else if (IsEqualGUID(riid, IID_IWebNotificationObserver))
        *ppvObject = static_cast<IWebNotificationObserver*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebView::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebView::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebView --------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::canShowMIMEType( 
    /* [in] */ BSTR /*mimeType*/,
    /* [retval][out] */ BOOL* /*canShow*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::canShowMIMETypeAsHTML( 
    /* [in] */ BSTR /*mimeType*/,
    /* [retval][out] */ BOOL* canShow)
{
    // FIXME
    *canShow = TRUE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::MIMETypesShownAsHTML( 
    /* [out] */ int* /*count*/,
    /* [retval][out] */ BSTR** /*mimeTypes*/) 
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setMIMETypesShownAsHTML( 
        /* [size_is][in] */ BSTR* /*mimeTypes*/,
        /* [in] */ int /*cMimeTypes*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::URLFromPasteboard( 
    /* [in] */ IDataObject* /*pasteboard*/,
    /* [retval][out] */ BSTR* /*url*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::URLTitleFromPasteboard( 
    /* [in] */ IDataObject* /*pasteboard*/,
    /* [retval][out] */ BSTR* /*urlTitle*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::initWithFrame( 
    /* [in] */ RECT* /* frame */,
    /* [in] */ BSTR /*frameName*/,
    /* [in] */ BSTR groupName)
{
    HRESULT hr = S_OK;

    if (m_viewWindow)
        return E_FAIL;

    registerWebViewWindowClass();
    m_viewWindow = CreateWindowEx(0, kWebViewWindowClassName, 0, WS_CHILD | WS_CLIPCHILDREN,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, m_hostWindow, 0, gInstance, 0);

    m_groupName = String(groupName, SysStringLen(groupName));

    m_page = new Page(new WebChromeClient(this), new WebContextMenuClient(this), new WebEditorClient(this));

    WebFrame* webFrame = WebFrame::createInstance();
    webFrame->initWithWebFrameView(0 /*FIXME*/, this, m_page, 0);
    m_mainFrame = webFrame;
    webFrame->Release(); // The WebFrame is owned by the Frame, so release our reference to it.

    #pragma warning(suppress: 4244)
    SetWindowLongPtr(m_viewWindow, 0, (LONG_PTR)this);
    ShowWindow(m_viewWindow, SW_SHOW);

    // Update WebCore with preferences.  These values will either come from an archived WebPreferences,
    // or from the standard preferences, depending on whether this method was called from initWithCoder:
    // or initWithFrame, respectively.
    //[self _updateWebCoreSettingsFromPreferences: [self preferences]];
    IWebPreferences* prefs;
    if (FAILED(preferences(&prefs)))
        return hr;
    hr = updateWebCoreSettingsFromPreferences(prefs);
    prefs->Release();
    if (FAILED(hr))
        return hr;

    // Register to receive notifications whenever preference values change.
    //[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
    //                                             name:WebPreferencesChangedNotification object:[self preferences]];
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    if (!WebPreferences::webPreferencesChangedNotification())
        return E_OUTOFMEMORY;
    notifyCenter->addObserver(this, WebPreferences::webPreferencesChangedNotification(), 0);

    return hr;
}

HRESULT STDMETHODCALLTYPE WebView::close()
{
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->removeObserver(this, WebPreferences::webPreferencesChangedNotification(), 0);

    setHostWindow(0);
    setFrameLoadDelegate(0);
    setFrameLoadDelegatePrivate(0);
    setUIDelegate(0);
    setFormDelegate(0);

    if (m_backForwardList) {
        m_backForwardList->Release();
        m_backForwardList = 0;
    }

    delete m_page;
    m_page = 0;

    deleteBackingStore();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setUIDelegate( 
    /* [in] */ IWebUIDelegate* d)
{
    if (m_uiDelegate)
        m_uiDelegate->Release();
    m_uiDelegate = d;
    if (d)
        d->AddRef();

    if (m_uiDelegatePrivate) {
        m_uiDelegatePrivate->Release();
        m_uiDelegatePrivate = 0;
    }
    if (d) {
        if (FAILED(d->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&m_uiDelegatePrivate)))
            m_uiDelegatePrivate = 0;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::uiDelegate( 
    /* [out][retval] */ IWebUIDelegate** d)
{
    if (m_uiDelegate)
        m_uiDelegate->AddRef();
    *d = m_uiDelegate;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setResourceLoadDelegate( 
    /* [in] */ IWebResourceLoadDelegate* /*d*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::resourceLoadDelegate( 
    /* [out][retval] */ IWebResourceLoadDelegate** /*d*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setDownloadDelegate( 
    /* [in] */ IWebDownloadDelegate* /*d*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::downloadDelegate( 
    /* [out][retval] */ IWebDownloadDelegate** /*d*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setFrameLoadDelegate( 
    /* [in] */ IWebFrameLoadDelegate* d)
{
    if (m_frameLoadDelegate)
        m_frameLoadDelegate->Release();
    m_frameLoadDelegate = d;
    if (d)
        d->AddRef();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::frameLoadDelegate( 
    /* [out][retval] */ IWebFrameLoadDelegate** d)
{
    if (m_frameLoadDelegate)
        m_frameLoadDelegate->AddRef();
    *d = m_frameLoadDelegate;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setPolicyDelegate( 
    /* [in] */ IWebPolicyDelegate* /*d*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::policyDelegate( 
    /* [out][retval] */ IWebPolicyDelegate** /*d*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::mainFrame( 
    /* [out][retval] */ IWebFrame** frame)
{
    *frame = m_mainFrame;
    if (!m_mainFrame)
        return E_FAIL;

    m_mainFrame->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::backForwardList( 
    /* [out][retval] */ IWebBackForwardList** list)
{
    *list = 0;
    if (!m_backForwardList)
        return E_FAIL;

    m_backForwardList->AddRef();
    *list = m_backForwardList;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setMaintainsBackForwardList( 
    /* [in] */ BOOL flag)
{
    if (flag && !m_backForwardList)
        m_backForwardList = WebBackForwardList::createInstance();
    else if (!flag && m_backForwardList) {
        m_backForwardList->Release();
        m_backForwardList = 0;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::goBack( 
    /* [retval][out] */ BOOL* succeeded)
{
    HRESULT hr = S_OK;

    *succeeded = FALSE;

    if (!m_backForwardList)
        return E_FAIL;

    IWebHistoryItem* item;
    hr = m_backForwardList->backItem(&item);
    if (SUCCEEDED(hr) && item) {
        hr = goToBackForwardItem(item, succeeded);
        item->Release();
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE WebView::goForward( 
    /* [retval][out] */ BOOL* succeeded)
{
    HRESULT hr = S_OK;

    *succeeded = FALSE;

    if (!m_backForwardList)
        return E_FAIL;

    IWebHistoryItem* item;
    hr = m_backForwardList->forwardItem(&item);
    if (SUCCEEDED(hr) && item) {
        hr = goToBackForwardItem(item, succeeded);
        item->Release();
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE WebView::goToBackForwardItem( 
    /* [in] */ IWebHistoryItem* item,
    /* [retval][out] */ BOOL* succeeded)
{
    *succeeded = FALSE;

    HRESULT hr = goToItem(item, WebFrameLoadTypeIndexedBackForward);
    if (SUCCEEDED(hr))
        *succeeded = TRUE;

    return hr;
}

HRESULT STDMETHODCALLTYPE WebView::setTextSizeMultiplier( 
    /* [in] */ float multiplier)
{
    if (m_textSizeMultiplier != multiplier)
        m_textSizeMultiplier = multiplier;
    
    if (!m_mainFrame)
        return E_FAIL;

    m_mainFrame->setTextSizeMultiplier(multiplier);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::textSizeMultiplier( 
    /* [retval][out] */ float* multiplier)
{
    *multiplier = m_textSizeMultiplier;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setApplicationNameForUserAgent( 
    /* [in] */ BSTR applicationName)
{
    m_applicationName = String(applicationName, SysStringLen(applicationName));
    m_userAgentStandard = String();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::applicationNameForUserAgent( 
    /* [retval][out] */ BSTR* applicationName)
{
    *applicationName = SysAllocStringLen(m_applicationName.characters(), m_applicationName.length());
    if (!*applicationName && m_applicationName.length())
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setCustomUserAgent( 
    /* [in] */ BSTR userAgentString)
{
    m_userAgentOverridden = true;
    m_userAgentCustom = String(userAgentString, SysStringLen(userAgentString));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::customUserAgent( 
    /* [retval][out] */ BSTR* userAgentString)
{
    *userAgentString = 0;
    if (!m_userAgentOverridden)
        return S_OK;
    *userAgentString = SysAllocStringLen(m_userAgentCustom.characters(), m_userAgentCustom.length());
    if (!*userAgentString && m_userAgentCustom.length())
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::userAgentForURL( 
    /* [in] */ BSTR url,
    /* [retval][out] */ BSTR* userAgent)
{
    DeprecatedString urlStr((DeprecatedChar*)url, SysStringLen(url));
    String userAgentString = this->userAgentForKURL(KURL(urlStr));
    *userAgent = SysAllocStringLen(userAgentString.characters(), userAgentString.length());
    if (!*userAgent && userAgentString.length())
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::supportsTextEncoding( 
    /* [retval][out] */ BOOL* supports)
{
    *supports = TRUE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setCustomTextEncodingName( 
    /* [in] */ BSTR encodingName)
{
    if (!m_mainFrame)
        return E_FAIL;

    HRESULT hr;
    BSTR oldEncoding;
    hr = customTextEncodingName(&oldEncoding);
    if (SUCCEEDED(hr)) {
        m_overrideEncoding = String(encodingName, SysStringLen(encodingName));
        if (oldEncoding != encodingName && (!oldEncoding || !encodingName || _tcscmp(oldEncoding, encodingName)))
            hr = m_mainFrame->reloadAllowingStaleDataWithOverrideEncoding(encodingName);
        SysFreeString(oldEncoding);
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE WebView::customTextEncodingName( 
    /* [retval][out] */ BSTR* encodingName)
{
    HRESULT hr = S_OK;
    IWebDataSource *dataSource = 0;
    IWebDataSourcePrivate *dataSourcePrivate = 0;
    *encodingName = 0;

    if (!m_mainFrame) {
        hr = E_FAIL;
        goto exit;
    }

    if (FAILED(m_mainFrame->provisionalDataSource(&dataSource)) || !dataSource) {
        hr = m_mainFrame->dataSource(&dataSource);
        if (FAILED(hr) || !dataSource)
            goto exit;
    }

    hr = dataSource->QueryInterface(IID_IWebDataSourcePrivate, (void**)&dataSourcePrivate);
    if (FAILED(hr))
        goto exit;

    hr = dataSourcePrivate->overrideEncoding(encodingName);
    if (FAILED(hr))
        goto exit;

    if (!*encodingName)
        *encodingName = SysAllocStringLen(m_overrideEncoding.characters(), m_overrideEncoding.length());

    if (!*encodingName && m_overrideEncoding.length()) {
        hr = E_OUTOFMEMORY;
        goto exit;
    }

exit:
    if (dataSourcePrivate)
        dataSourcePrivate->Release();
    if (dataSource)
        dataSource->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE WebView::setMediaStyle( 
    /* [in] */ BSTR /*media*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::mediaStyle( 
    /* [retval][out] */ BSTR* /*media*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::stringByEvaluatingJavaScriptFromString( 
    /* [in] */ BSTR script, // assumes input does not have "JavaScript" at the begining.
    /* [retval][out] */ BSTR* result)
{
    m_mainFrame->impl()->loader()->createEmptyDocument();
    KJS::JSValue* scriptExecutionResult = m_mainFrame->impl()->loader()->executeScript(0, WebCore::String(script), true);
    if(!scriptExecutionResult)
        return E_FAIL;
    else if (scriptExecutionResult->isString())
        *result = BString(String(scriptExecutionResult->getString()));

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::windowScriptObject( 
    /* [retval][out] */ IWebScriptObject** /*webScriptObject*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setPreferences( 
    /* [in] */ IWebPreferences* /*prefs*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::preferences( 
    /* [retval][out] */ IWebPreferences** prefs)
{
    HRESULT hr = S_OK;

    if (!m_preferences) {
        WebPreferences* instance = WebPreferences::createInstance();
        if (!instance)
            return E_FAIL;
        hr = instance->standardPreferences(&m_preferences);
        instance->Release();
    }

    m_preferences->AddRef();
    *prefs = m_preferences;
    return hr;
}

HRESULT STDMETHODCALLTYPE WebView::setPreferencesIdentifier( 
    /* [in] */ BSTR /*anIdentifier*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::preferencesIdentifier( 
    /* [retval][out] */ BSTR* /*anIdentifier*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setHostWindow( 
    /* [in] */ HWND window)
{
    if (m_viewWindow)
        SetParent(m_viewWindow, window);

    m_hostWindow = window;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::hostWindow( 
    /* [retval][out] */ HWND* window)
{
    *window = m_hostWindow;
    return S_OK;
}


static Frame *incrementFrame(Frame *curr, bool forward, bool wrapFlag)
{
    return forward
        ? curr->tree()->traverseNextWithWrap(wrapFlag)
        : curr->tree()->traversePreviousWithWrap(wrapFlag);
}

HRESULT STDMETHODCALLTYPE WebView::searchFor( 
    /* [in] */ BSTR str,
    /* [in] */ BOOL forward,
    /* [in] */ BOOL caseFlag,
    /* [in] */ BOOL wrapFlag,
    /* [retval][out] */ BOOL* found)
{
    if (!found)
        return E_INVALIDARG;
    
    if (!m_page || !m_page->mainFrame())
        return E_UNEXPECTED;

    if (!str || !SysStringLen(str))
        return E_INVALIDARG;

    String search(str, SysStringLen(str));


    WebCore::Frame* frame = focusedTargetFrame();
    WebCore::Frame* startFrame = frame;
    do {
        *found = frame->findString(search, !!forward, !!caseFlag, false);
        if (*found) {
            if (frame != startFrame)
                startFrame->selectionController()->clear();
            frame->view()->setFocus();
            return S_OK;
        }
        frame = incrementFrame(frame, !!forward, !!wrapFlag);
    } while (frame && frame != startFrame);

    // Search contents of startFrame, on the other side of the selection that we did earlier.
    // We cheat a bit and just research with wrap on
    if (wrapFlag && !startFrame->selectionController()->isNone()) {
        *found = startFrame->findString(search, !!forward, !!caseFlag, true);
        frame->view()->setFocus();
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::markAllMatchesForText(
    BSTR str, BOOL caseSensitive, BOOL highlight, UINT limit, UINT* matches)
{
    if (!matches)
        return E_INVALIDARG;

    if (!m_page || !m_page->mainFrame())
        return E_UNEXPECTED;

    if (!str || !SysStringLen(str))
        return E_INVALIDARG;

    String search(str, SysStringLen(str));
    *matches = 0;

    WebCore::Frame* frame = m_page->mainFrame();
    do {
        frame->setMarkedTextMatchesAreHighlighted(!!highlight);
        *matches += frame->markAllMatchesForText(search, !!caseSensitive, (limit == 0)? 0 : (limit - *matches));
        frame = incrementFrame(frame, true, false);
    } while (frame);

    return S_OK;

}

HRESULT STDMETHODCALLTYPE WebView::unmarkAllTextMatches()
{
    if (!m_page || !m_page->mainFrame())
        return E_UNEXPECTED;

    WebCore::Frame* frame = m_page->mainFrame();
    do {
        frame->document()->removeMarkers(DocumentMarker::TextMatch);
        frame = incrementFrame(frame, true, false);
    } while (frame);


    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::rectsForTextMatches(
    IEnumTextMatches** pmatches)
{
    Vector<IntRect> allRects;
    WebCore::Frame* frame = m_page->mainFrame();
    do {
        Vector<IntRect> frameRects = frame->document()->renderedRectsForMarkers(DocumentMarker::TextMatch);
        IntPoint frameOffset(-frame->view()->scrollOffset().width(), -frame->view()->scrollOffset().height());
        frameOffset = frame->view()->convertToContainingWindow(frameOffset);

        Vector<IntRect>::iterator end = frameRects.end();
        for (Vector<IntRect>::iterator it = frameRects.begin(); it < end; it++) {
            it->move(frameOffset.x(), frameOffset.y());
            allRects.append(*it);
        }
        frame = incrementFrame(frame, true, false);
    } while (frame);

    return createMatchEnumerator(&allRects, pmatches);
}

HRESULT STDMETHODCALLTYPE WebView::generateSelectionImage(BOOL forceWhiteText, HBITMAP* image)
{
    *image = 0;

    WebCore::Frame* frame = focusedTargetFrame();

    if (frame)
        *image = Win(frame)->imageFromSelection(forceWhiteText?TRUE:FALSE);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::selectionImageRect(RECT* rc)
{
    WebCore::Frame* frame = focusedTargetFrame();

    if (frame) {
        IntRect ir = frame->selectionRect();
        ir = frame->view()->convertToContainingWindow(ir);
        ir.move(-frame->view()->scrollOffset().width(), -frame->view()->scrollOffset().height());
        rc->left = ir.x();
        rc->top = ir.y();
        rc->bottom = rc->top + ir.height();
        rc->right = rc->left + ir.width();
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::registerViewClass( 
    /* [in] */ IWebDocumentView* /*view*/,
    /* [in] */ IWebDocumentRepresentation* /*representation*/,
    /* [in] */ BSTR /*forMIMEType*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setGroupName( 
        /* [in] */ BSTR groupName)
{
    m_groupName = String(groupName, SysStringLen(groupName));
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::groupName( 
        /* [retval][out] */ BSTR* groupName)
{
    *groupName = SysAllocStringLen(m_groupName.characters(), m_groupName.length());
    if (!*groupName && m_groupName.length())
        return E_OUTOFMEMORY;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::estimatedProgress( 
        /* [retval][out] */ double* /*estimatedProgress*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::isLoading( 
        /* [retval][out] */ BOOL* /*isLoading*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::elementAtPoint( 
        /* [in] */ LPPOINT point,
        /* [retval][out] */ IPropertyBag** elementDictionary)
{
    Frame* frame = m_mainFrame->impl();
    IntPoint webCorePoint = IntPoint(point->x, point->y);
    HitTestResult result = HitTestResult(webCorePoint);
    if (frame->renderer())
        result = frame->eventHandler()->hitTestResultAtPoint(webCorePoint, false);
    *elementDictionary = new WebElementPropertyBag(result);
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteboardTypesForSelection( 
        /* [out][in] */ int* /*count*/,
        /* [retval][out] */ BSTR** /*types*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::writeSelectionWithPasteboardTypes( 
        /* [size_is][in] */ BSTR* /*types*/,
        /* [in] */ int /*cTypes*/,
        /* [in] */ IDataObject* /*pasteboard*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteboardTypesForElement( 
        /* [in] */ IPropertyBag* /*elementDictionary*/,
        /* [out][in] */ int* /*count*/,
        /* [retval][out] */ BSTR** /*types*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::writeElement( 
        /* [in] */ IPropertyBag* /*elementDictionary*/,
        /* [size_is][in] */ BSTR* /*withPasteboardTypes*/,
        /* [in] */ int /*cWithPasteboardTypes*/,
        /* [in] */ IDataObject* /*pasteboard*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::moveDragCaretToPoint( 
        /* [in] */ LPPOINT /*point*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::removeDragCaret( void)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setDrawsBackground( 
        /* [in] */ BOOL /*drawsBackground*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::drawsBackground( 
        /* [retval][out] */ BOOL* /*drawsBackground*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setMainFrameURL( 
        /* [in] */ BSTR /*urlString*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::mainFrameURL( 
        /* [retval][out] */ BSTR* /*urlString*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::mainFrameDocument( 
        /* [retval][out] */ IDOMDocument** /*document*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::mainFrameTitle( 
        /* [retval][out] */ BSTR* /*title*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::mainFrameIcon( 
        /* [retval][out] */ HBITMAP* /*icon*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// IWebIBActions ---------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::takeStringURLFrom( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::stopLoading( 
        /* [in] */ IUnknown* /*sender*/)
{
    if (!m_mainFrame)
        return E_FAIL;

    return m_mainFrame->stopLoading();
}
    
HRESULT STDMETHODCALLTYPE WebView::reload( 
        /* [in] */ IUnknown* /*sender*/)
{
    if (!m_mainFrame)
        return E_FAIL;

    return m_mainFrame->reload();
}
    
HRESULT STDMETHODCALLTYPE WebView::canGoBack( 
        /* [in] */ IUnknown* /*sender*/,
        /* [retval][out] */ BOOL* result)
{
    *result = FALSE;

    if (!m_backForwardList)
        return E_FAIL;

    IWebHistoryItem* item;
    HRESULT hr = m_backForwardList->backItem(&item);
    if (FAILED(hr))
        return hr;

    if (item) {
        *result = TRUE;
        item->Release();
    }

    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::goBack( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::canGoForward( 
        /* [in] */ IUnknown* /*sender*/,
        /* [retval][out] */ BOOL* result)
{
    *result = FALSE;

    if (!m_backForwardList)
        return E_FAIL;

    IWebHistoryItem* item;
    HRESULT hr = m_backForwardList->forwardItem(&item);
    if (FAILED(hr))
        return hr;

    if (item) {
        *result = TRUE;
        item->Release();
    }

    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::goForward( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

#define MinimumTextSizeMultiplier   0.5f
#define MaximumTextSizeMultiplier   3.0f
#define TextSizeMultiplierRatio     1.2f

HRESULT STDMETHODCALLTYPE WebView::canMakeTextLarger( 
        /* [in] */ IUnknown* /*sender*/,
        /* [retval][out] */ BOOL* result)
{
    bool canGrowMore = m_textSizeMultiplier*TextSizeMultiplierRatio < MaximumTextSizeMultiplier;
    *result = canGrowMore ? TRUE : FALSE;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::makeTextLarger( 
        /* [in] */ IUnknown* /*sender*/)
{
    float newScale = m_textSizeMultiplier*TextSizeMultiplierRatio;
    bool canGrowMore = newScale < MaximumTextSizeMultiplier;
    if (!canGrowMore)
        return E_FAIL;
    return setTextSizeMultiplier(newScale);

}
    
HRESULT STDMETHODCALLTYPE WebView::canMakeTextSmaller( 
        /* [in] */ IUnknown* /*sender*/,
        /* [retval][out] */ BOOL* result)
{
    bool canShrinkMore = m_textSizeMultiplier/TextSizeMultiplierRatio > MinimumTextSizeMultiplier;
    *result = canShrinkMore ? TRUE : FALSE;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::makeTextSmaller( 
        /* [in] */ IUnknown* /*sender*/)
{
    float newScale = m_textSizeMultiplier/TextSizeMultiplierRatio;
    bool canShrinkMore = newScale > MinimumTextSizeMultiplier;
    if (!canShrinkMore)
        return E_FAIL;
    return setTextSizeMultiplier(newScale);
}

// IWebViewCSS -----------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::computedStyleForElement( 
        /* [in] */ IDOMElement* /*element*/,
        /* [in] */ BSTR /*pseudoElement*/,
        /* [retval][out] */ IDOMCSSStyleDeclaration** /*style*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// IWebViewEditing -------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::editableDOMRangeForPoint( 
        /* [in] */ LPPOINT /*point*/,
        /* [retval][out] */ IDOMRange** /*range*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setSelectedDOMRange( 
        /* [in] */ IDOMRange* /*range*/,
        /* [in] */ WebSelectionAffinity /*affinity*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::selectedDOMRange( 
        /* [retval][out] */ IDOMRange** /*range*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::selectionAffinity( 
        /* [retval][out][retval][out] */ WebSelectionAffinity* /*affinity*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setEditable( 
        /* [in] */ BOOL /*flag*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::isEditable( 
        /* [retval][out] */ BOOL* /*isEditable*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setTypingStyle( 
        /* [in] */ IDOMCSSStyleDeclaration* /*style*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::typingStyle( 
        /* [retval][out] */ IDOMCSSStyleDeclaration** /*style*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setSmartInsertDeleteEnabled( 
        /* [in] */ BOOL /*flag*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::smartInsertDeleteEnabled( 
        /* [in] */ BOOL /*enabled*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setContinuousSpellCheckingEnabled( 
        /* [in] */ BOOL /*flag*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::isContinuousSpellCheckingEnabled( 
        /* [retval][out] */ BOOL* /*enabled*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::spellCheckerDocumentTag( 
        /* [retval][out] */ int* /*tag*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::undoManager( 
        /* [retval][out] */ IWebUndoManager** /*manager*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setEditingDelegate( 
        /* [in] */ IWebViewEditingDelegate* /*d*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::editingDelegate( 
        /* [retval][out] */ IWebViewEditingDelegate** /*d*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::styleDeclarationWithText( 
        /* [in] */ BSTR /*text*/,
        /* [retval][out] */ IDOMCSSStyleDeclaration** /*style*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// IWebViewUndoableEditing -----------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::replaceSelectionWithNode( 
        /* [in] */ IDOMNode* /*node*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::replaceSelectionWithText( 
        /* [in] */ BSTR /*text*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::replaceSelectionWithMarkupString( 
        /* [in] */ BSTR /*markupString*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::replaceSelectionWithArchive( 
        /* [in] */ IWebArchive* /*archive*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::deleteSelection( void)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::applyStyle( 
        /* [in] */ IDOMCSSStyleDeclaration* /*style*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// IWebViewEditingActions ------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::copy( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::cut( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::paste( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::copyFont( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteFont( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::delete_( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteAsPlainText( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteAsRichText( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::changeFont( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::changeAttributes( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::changeDocumentBackgroundColor( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::changeColor( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::alignCenter( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::alignJustified( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::alignLeft( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::alignRight( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::checkSpelling( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::showGuessPanel( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::performFindPanelAction( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::startSpeaking( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::stopSpeaking( 
        /* [in] */ IUnknown* /*sender*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// IWebNotificationObserver -----------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::onNotify( 
    /* [in] */ IWebNotification* notification)
{
    IUnknown* unkPrefs;
    HRESULT hr = notification->getObject(&unkPrefs);
    if (FAILED(hr))
        return hr;

    WebPreferences *preferences;
    hr = unkPrefs->QueryInterface(IID_IWebPreferences, (void**)&preferences);
    if (FAILED(hr))
        return hr;

    hr = updateWebCoreSettingsFromPreferences(preferences);
    preferences->Release();

    return hr;
}

// IWebViewPrivate ------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::setInViewSourceMode( 
        /* [in] */ BOOL flag)
{
    if (!m_mainFrame)
        return E_FAIL;

    m_mainFrame->setInViewSourceMode(flag);
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebView::inViewSourceMode( 
        /* [retval][out] */ BOOL* flag)
{
    if (!m_mainFrame)
        return E_FAIL;

    m_mainFrame->inViewSourceMode(flag);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::viewWindow( 
        /* [retval][out] */ HWND *window)
{
    *window = m_viewWindow;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setFormDelegate( 
    /* [in] */ IWebFormDelegate *formDelegate)
{
    if (m_formDelegate)
        m_formDelegate->Release();
    m_formDelegate = formDelegate;
    if (formDelegate)
        formDelegate->AddRef();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::formDelegate( 
    /* [retval][out] */ IWebFormDelegate **formDelegate)
{
    if (m_formDelegate)
        m_formDelegate->AddRef();
    *formDelegate = m_formDelegate;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::setFrameLoadDelegatePrivate( 
    /* [in] */ IWebFrameLoadDelegatePrivate* d)
{
    if (m_frameLoadDelegatePrivate)
        m_frameLoadDelegatePrivate->Release();
    m_frameLoadDelegatePrivate = d;
    if (d)
        d->AddRef();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::frameLoadDelegatePrivate( 
    /* [out][retval] */ IWebFrameLoadDelegatePrivate** d)
{
    if (m_frameLoadDelegatePrivate)
        m_frameLoadDelegatePrivate->AddRef();
    *d = m_frameLoadDelegatePrivate;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebView::scrollOffset( 
    /* [retval][out] */ LPPOINT offset)
{
    IntSize offsetIntSize = m_page->mainFrame()->view()->scrollOffset();
    offset->x = offsetIntSize.width();
    offset->y = offsetIntSize.height();
    return S_OK;
}

class EnumTextMatches : public IEnumTextMatches
{
    long m_ref;
    UINT m_index;
    Vector<IntRect> m_rects;
public:
    EnumTextMatches(Vector<IntRect>* rects) : m_index(0), m_ref(1)
    {
        m_rects = *rects;
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv)
    {
        if (riid == IID_IUnknown || riid == IID_IEnumTextMatches) {
            *ppv = this;
            AddRef();
        }

        return *ppv?S_OK:E_NOINTERFACE;
    }

    virtual ULONG STDMETHODCALLTYPE AddRef()
    {
        return m_ref++;
    }
    
    virtual ULONG STDMETHODCALLTYPE Release()
    {
        if (m_ref == 1) {
            delete this;
            return 0;
        }
        else
            return m_ref--;
    }

    virtual HRESULT STDMETHODCALLTYPE Next(ULONG, RECT* rect, ULONG* pceltFetched)
    {
        if (m_index < m_rects.size()) {
            if (pceltFetched)
                *pceltFetched = 1;
            *rect = m_rects[m_index];
            m_index++;
            return S_OK;
        }

        if (pceltFetched)
            *pceltFetched = 0;

        return S_FALSE;
    }
    virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt)
    {
        m_index += celt;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE Reset(void)
    {
        m_index = 0;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE Clone(IEnumTextMatches**)
    {
        return E_NOTIMPL;
    }
};



HRESULT createMatchEnumerator(Vector<IntRect>* rects, IEnumTextMatches** matches)
{
    *matches = new EnumTextMatches(rects);
    return (*matches)?S_OK:E_OUTOFMEMORY;
}

Page* core(IWebView* iWebView)
{
    Page* page = 0;

    WebView* webView = 0;
    if (SUCCEEDED(iWebView->QueryInterface(CLSID_WebView, (void**)&webView) && webView)) {
        page = webView->page();
        webView->Release();
    }

    return page;
}