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

#include "IWebURLResponse.h"
#include "WebView.h"

#include "IWebNotification.h"
#include "WebFrame.h"
#include "WebBackForwardList.h"
#include "WebNotificationCenter.h"
#include "WebPreferences.h"

#pragma warning( push, 0 )
#include "ResourceLoaderClient.h"
#include "FrameWin.h"
#include "Document.h"
#include "FrameView.h"
#include "IntRect.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "SelectionController.h"
#include "TypingCommand.h"
#pragma warning(pop)

using namespace WebCore;

const LPCWSTR kWebViewWindowClassName = L"WebViewWindowClass";
static bool nextCharIsInputText = false;

static ATOM registerWebView();
static LRESULT CALLBACK WebViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static int calculateScrollDelta(WPARAM wParam, int oldPosition, int pageSize);
static int scrollMessageForKey(WPARAM keyCode);

// WebView ----------------------------------------------------------------

WebView::WebView()
: m_refCount(0)
, m_frameName(0)
, m_groupName(0)
, m_hostWindow(0)
, m_viewWindow(0)
, m_mainFrame(0)
, m_frameLoadDelegate(0)
, m_uiDelegate(0)
, m_formDelegate(0)
, m_backForwardList(0)
, m_preferences(0)
, m_userAgentOverridden(false)
, m_userAgent(0)
, m_textSizeMultiplier(1)
{
    SetRectEmpty(&m_frame);

    m_mainFrame = WebFrame::createInstance();
    m_backForwardList = WebBackForwardList::createInstance();

    gClassCount++;
}

WebView::~WebView()
{
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->removeObserver(this, WebPreferences::webPreferencesChangedNotification(), 0);

    if (m_backForwardList)
        m_backForwardList->Release();
    if (m_mainFrame)
        m_mainFrame->Release();
    if (m_frameLoadDelegate)
        m_frameLoadDelegate->Release();
    if (m_uiDelegate)
        m_uiDelegate->Release();
    if (m_formDelegate)
        m_formDelegate->Release();
    if (m_preferences)
        m_preferences->Release();

    SysFreeString(m_frameName);
    SysFreeString(m_groupName);
    SysFreeString(m_userAgent);
    gClassCount--;
}

WebView* WebView::createInstance()
{
    WebView* instance = new WebView();
    instance->AddRef();
    return instance;
}

void WebView::mouseMoved(WPARAM wParam, LPARAM lParam)
{
    PlatformMouseEvent mouseEvent(m_viewWindow, wParam, lParam, 0);
    m_mainFrame->impl()->view()->handleMouseMoveEvent(mouseEvent);
}

void WebView::mouseDown(WPARAM wParam, LPARAM lParam)
{
    PlatformMouseEvent mouseEvent(m_viewWindow, wParam, lParam, 1);
    m_mainFrame->impl()->view()->handleMousePressEvent(mouseEvent);
}

void WebView::mouseUp(WPARAM wParam, LPARAM lParam)
{
    PlatformMouseEvent mouseEvent(m_viewWindow, wParam, lParam, 1);
    m_mainFrame->impl()->view()->handleMouseReleaseEvent(mouseEvent);
}

void WebView::mouseDoubleClick(WPARAM wParam, LPARAM lParam)
{
    PlatformMouseEvent mouseEvent(m_viewWindow, wParam, lParam, 2);
    m_mainFrame->impl()->view()->handleMouseReleaseEvent(mouseEvent);
}

bool WebView::keyPress(WPARAM wParam, LPARAM lParam)
{
    PlatformKeyboardEvent keyEvent(m_viewWindow, wParam, lParam);

    FrameWin* frame = static_cast<FrameWin*>(m_mainFrame->impl());
    bool handled = frame->keyPress(keyEvent);
    if (!handled && !keyEvent.isKeyUp()) {
        Node* start = frame->selection().start().node();
        if (start && start->isContentEditable()) {
            switch(keyEvent.WindowsKeyCode()) {
            case VK_BACK:
                TypingCommand::deleteKeyPressed(frame->document());
                break;
            case VK_DELETE:
                TypingCommand::forwardDeleteKeyPressed(frame->document());
                break;
            case VK_LEFT:
                frame->selection().modify(SelectionController::MOVE, SelectionController::LEFT, CharacterGranularity);
                break;
            case VK_RIGHT:
                frame->selection().modify(SelectionController::MOVE, SelectionController::RIGHT, CharacterGranularity);
                break;
            case VK_UP:
                frame->selection().modify(SelectionController::MOVE, SelectionController::BACKWARD, ParagraphGranularity);
                break;
            case VK_DOWN:
                frame->selection().modify(SelectionController::MOVE, SelectionController::FORWARD, ParagraphGranularity);
                break;
            case VK_RETURN:
                if (start->isContentRichlyEditable())
                    TypingCommand::insertParagraphSeparator(frame->document());
                else
                    TypingCommand::insertLineBreak(frame->document());
                break;
            default:
                nextCharIsInputText = true;
            }
            handled = true;
        }
    }
    return handled;
}

static ATOM registerWebView()
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
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = 0;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebViewWindowClassName;
    wcex.hIconSm        = 0;

    return RegisterClassEx(&wcex);
}

static LRESULT CALLBACK WebViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;
    HRESULT hr = S_OK;
    LONG_PTR longPtr = GetWindowLongPtr(hWnd, 0);
    WebView* webView = reinterpret_cast<WebView*>(longPtr);
    IWebFrame* mainFrame = 0;
    WebFrame* mainFrameImpl = 0;
    if (webView) {
        hr = webView->mainFrame(&mainFrame);
        if (SUCCEEDED(hr))
            mainFrameImpl = static_cast<WebFrame*>(mainFrame);
    }

    switch (message) {
        case WM_PAINT:
            if (mainFrameImpl) {
                IWebDataSource* dataSource = 0;
                mainFrameImpl->dataSource(&dataSource);
                if (!dataSource || !mainFrameImpl->loading())
                    mainFrameImpl->paint();
                else
                    ValidateRect(hWnd, 0);
                if (dataSource)
                    dataSource->Release();
            }
            break;
        case WM_DESTROY:
            // Do nothing?
            break;
        case WM_MOUSEMOVE:
            if (webView)
                webView->mouseMoved(wParam, lParam);
            break;
        case WM_LBUTTONDOWN:
            // Make ourselves the focused window before doing anything else
            // FIXME: I'm not sure if this is the "right" way to do this
            // but w/o this call, we never become focused since we don't allow
            // the default handling of mouse events.
            SetFocus(hWnd);
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            if (webView)
                webView->mouseDown(wParam, lParam);
            break;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            if (webView)
                webView->mouseUp(wParam, lParam);
            break;
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
            if (webView)
                webView->mouseDoubleClick(wParam, lParam);
            break;
        case WM_HSCROLL: {
            if (mainFrameImpl) {
                ScrollView* view = mainFrameImpl->impl()->view();
                view->scrollBy(calculateScrollDelta(wParam, view->contentsX(), view->visibleWidth()), 0);
                mainFrameImpl->impl()->sendScrollEvent();
            }
            break;
        }
        case WM_VSCROLL: {
            if (mainFrameImpl) {
                ScrollView* view = mainFrameImpl->impl()->view();
                view->scrollBy(0, calculateScrollDelta(wParam, view->contentsY(), view->visibleHeight()));
                mainFrameImpl->impl()->sendScrollEvent();
            }
            break;
        }
        case WM_KEYDOWN: {
            // FIXME: First we should send key events up through the DOM
            // to form controls, etc.  If they are not handled, we fall
            // through to the top level webView and do things like scrolling
            if (webView && webView->keyPress(wParam, lParam))
                break;
            DWORD wScrollNotify = scrollMessageForKey(wParam);
            if (wScrollNotify != -1)
                SendMessage(hWnd, WM_VSCROLL, MAKELONG(wScrollNotify, 0), 0L);
            break;
        }
        case WM_CHAR: {
            // FIXME: We need to use WM_UNICHAR to support international text.
            if (nextCharIsInputText && mainFrameImpl) {
                UChar c = (UChar)wParam;
                TypingCommand::insertText(mainFrameImpl->impl()->document(), String(&c, 1), false);
                nextCharIsInputText = false;
            }
            break;
        }
        case WM_KEYUP: {
            if (webView)
                webView->keyPress(wParam, lParam);
            break;
        }
        case WM_SIZE:
            if (mainFrameImpl && lParam != 0 && !mainFrameImpl->loading())
                mainFrameImpl->impl()->sendResizeEvent();
            break;
        case WM_SETFOCUS:
            if (mainFrameImpl) {
                mainFrameImpl->impl()->setWindowHasFocus(true);
                // FIXME: This is wrong. Should listen to WM_ACTIVATE instead.
                mainFrameImpl->impl()->setIsActive(true);
            }
            break;
        case WM_KILLFOCUS:
            if (mainFrameImpl) {
                mainFrameImpl->impl()->setWindowHasFocus(false);
                // FIXME: This is wrong.
                mainFrameImpl->impl()->setIsActive(false);
            }
            break;
        default:
            lResult = DefWindowProc(hWnd, message, wParam, lParam);
    }

    if (mainFrame)
        mainFrame->Release();

    return lResult;
}

#define LINE_SCROLL_SIZE 30

static int calculateScrollDelta(WPARAM wParam, int oldPosition, int pageSize)
{
    switch (LOWORD(wParam)) {
        case SB_PAGEUP: 
            return -(pageSize - LINE_SCROLL_SIZE); 
         case SB_PAGEDOWN: 
            return (pageSize - LINE_SCROLL_SIZE); 
        case SB_LINEUP: 
            return -LINE_SCROLL_SIZE;
        case SB_LINEDOWN: 
            return LINE_SCROLL_SIZE;
        case SB_THUMBPOSITION: 
        case SB_THUMBTRACK:
            return HIWORD(wParam) - oldPosition; 
    }
    return 0;
}

static int scrollMessageForKey(WPARAM keyCode)
{
    switch (keyCode) {
    case VK_UP:
        return SB_LINEUP;
    case VK_PRIOR: 
        return SB_PAGEUP;
    case VK_NEXT:
        return SB_PAGEDOWN;
    case VK_DOWN:
        return SB_LINEDOWN;
    case VK_HOME:
        return SB_TOP;
    case VK_END:
        return SB_BOTTOM;
    case VK_SPACE:
        return (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? SB_PAGEUP : SB_PAGEDOWN;
    }
    return -1;
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

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
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
    DebugBreak();
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
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setMIMETypesShownAsHTML( 
        /* [size_is][in] */ BSTR* /*mimeTypes*/,
        /* [in] */ int /*cMimeTypes*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::URLFromPasteboard( 
    /* [in] */ IDataObject* /*pasteboard*/,
    /* [retval][out] */ BSTR* /*url*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::URLTitleFromPasteboard( 
    /* [in] */ IDataObject* /*pasteboard*/,
    /* [retval][out] */ BSTR* /*urlTitle*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::initWithFrame( 
    /* [in] */ RECT* frame,
    /* [in] */ BSTR frameName,
    /* [in] */ BSTR groupName)
{
    HRESULT hr = S_OK;

    if (m_viewWindow)
        return E_FAIL;

    m_frame = *frame;
    m_frameName = SysAllocString(frameName);
    m_groupName = SysAllocString(groupName);

    registerWebView();

    m_viewWindow = CreateWindowEx(0, kWebViewWindowClassName, 0, WS_CHILD | WS_HSCROLL | WS_VSCROLL,
       CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, m_hostWindow, 0, gInstance, 0);

    m_mainFrame->initWithName(0, 0/*FIXME*/, this);

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

HRESULT STDMETHODCALLTYPE WebView::setUIDelegate( 
    /* [in] */ IWebUIDelegate* d)
{
    if (m_uiDelegate)
        m_uiDelegate->Release();
    m_uiDelegate = d;
    if (d)
        d->AddRef();

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
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::resourceLoadDelegate( 
    /* [out][retval] */ IWebResourceLoadDelegate** /*d*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setDownloadDelegate( 
    /* [in] */ IWebDownloadDelegate* /*d*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::downloadDelegate( 
    /* [out][retval] */ IWebDownloadDelegate** /*d*/)
{
    DebugBreak();
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
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::policyDelegate( 
    /* [out][retval] */ IWebPolicyDelegate** /*d*/)
{
    DebugBreak();
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
    /* [in] */ BSTR /*applicationName*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::applicationNameForUserAgent( 
    /* [retval][out] */ BSTR* /*applicationName*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setCustomUserAgent( 
    /* [in] */ BSTR /*userAgentString*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::customUserAgent( 
    /* [retval][out] */ BSTR* /*userAgentString*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::userAgentForURL( 
    /* [in] */ BSTR /*url*/,
    /* [retval][out] */ BSTR* /*userAgent*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::supportsTextEncoding( 
    /* [retval][out] */ BOOL* /*supports*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setCustomTextEncodingName( 
    /* [in] */ BSTR /*encodingName*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::customTextEncodingName( 
    /* [retval][out] */ BSTR* /*encodingName*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setMediaStyle( 
    /* [in] */ BSTR /*media*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::mediaStyle( 
    /* [retval][out] */ BSTR* /*media*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::stringByEvaluatingJavaScriptFromString( 
    /* [in] */ BSTR /*script*/,
    /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::windowScriptObject( 
    /* [retval][out] */ IWebScriptObject* /*webScriptObject*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::setPreferences( 
    /* [in] */ IWebPreferences* /*prefs*/)
{
    DebugBreak();
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
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::preferencesIdentifier( 
    /* [retval][out] */ BSTR* /*anIdentifier*/)
{
    DebugBreak();
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

HRESULT STDMETHODCALLTYPE WebView::searchFor( 
    /* [in] */ BSTR /*str*/,
    /* [in] */ BOOL /*forward*/,
    /* [in] */ BOOL /*caseFlag*/,
    /* [in] */ BOOL /*wrapFlag*/,
    /* [retval][out] */ BOOL* /*found*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebView::registerViewClass( 
    /* [in] */ IWebDocumentView* /*view*/,
    /* [in] */ IWebDocumentRepresentation* /*representation*/,
    /* [in] */ BSTR /*forMIMEType*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// IWebIBActions ---------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::takeStringURLFrom( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
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
    DebugBreak();
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
    DebugBreak();
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
    DebugBreak();
    return E_NOTIMPL;
}

// IWebViewEditing -------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::editableDOMRangeForPoint( 
        /* [in] */ LPPOINT /*point*/,
        /* [retval][out] */ IDOMRange** /*range*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setSelectedDOMRange( 
        /* [in] */ IDOMRange* /*range*/,
        /* [in] */ WebSelectionAffinity /*affinity*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::selectedDOMRange( 
        /* [retval][out] */ IDOMRange** /*range*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::selectionAffinity( 
        /* [retval][out][retval][out] */ WebSelectionAffinity* /*affinity*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setEditable( 
        /* [in] */ BOOL /*flag*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::isEditable( 
        /* [retval][out] */ BOOL* /*isEditable*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setTypingStyle( 
        /* [in] */ IDOMCSSStyleDeclaration* /*style*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::typingStyle( 
        /* [retval][out] */ IDOMCSSStyleDeclaration** /*style*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setSmartInsertDeleteEnabled( 
        /* [in] */ BOOL /*flag*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::smartInsertDeleteEnabled( 
        /* [in] */ BOOL /*enabled*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setContinuousSpellCheckingEnabled( 
        /* [in] */ BOOL /*flag*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::isContinuousSpellCheckingEnabled( 
        /* [retval][out] */ BOOL* /*enabled*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::spellCheckerDocumentTag( 
        /* [retval][out] */ int* /*tag*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::undoManager( 
        /* [retval][out] */ IWebUndoManager* /*manager*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::setEditingDelegate( 
        /* [in] */ IWebViewEditingDelegate* /*d*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::editingDelegate( 
        /* [retval][out] */ IWebViewEditingDelegate** /*d*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::styleDeclarationWithText( 
        /* [in] */ BSTR /*text*/,
        /* [retval][out] */ IDOMCSSStyleDeclaration** /*style*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// IWebViewUndoableEditing -----------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::replaceSelectionWithNode( 
        /* [in] */ IDOMNode* /*node*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::replaceSelectionWithText( 
        /* [in] */ BSTR /*text*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::replaceSelectionWithMarkupString( 
        /* [in] */ BSTR /*markupString*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::replaceSelectionWithArchive( 
        /* [in] */ IWebArchive* /*archive*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::deleteSelection( void)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::applyStyle( 
        /* [in] */ IDOMCSSStyleDeclaration* /*style*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// IWebViewEditingActions ------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebView::copy( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::cut( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::paste( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::copyFont( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteFont( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::delete_( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteAsPlainText( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::pasteAsRichText( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::changeFont( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::changeAttributes( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::changeDocumentBackgroundColor( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::changeColor( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::alignCenter( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::alignJustified( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::alignLeft( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::alignRight( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::checkSpelling( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::showGuessPanel( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::performFindPanelAction( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::startSpeaking( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebView::stopSpeaking( 
        /* [in] */ IUnknown* /*sender*/)
{
    DebugBreak();
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

    if (m_userAgentOverridden) {
        SysFreeString(m_userAgent);
        m_userAgent = 0;
    }

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
