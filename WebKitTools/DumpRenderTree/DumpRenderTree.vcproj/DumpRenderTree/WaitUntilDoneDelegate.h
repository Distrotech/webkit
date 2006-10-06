/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include <WebKit/IWebURLResponse.h>
#include <WebKit/WebKit.h>

class WaitUntilDoneDelegate : public IWebFrameLoadDelegate, public IWebUIDelegate
{
public:
    WaitUntilDoneDelegate() : m_refCount(1) { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebFrameLoadDelegate
    virtual HRESULT STDMETHODCALLTYPE didStartProvisionalLoadForFrame( 
        /* [in] */ IWebView* webView,
        /* [in] */ IWebFrame* /*frame*/);
    
    virtual HRESULT STDMETHODCALLTYPE didReceiveServerRedirectForProvisionalLoadForFrame( 
        /* [in] */ IWebView *webView,
        /* [in] */ IWebFrame *frame) { return S_OK; }
    
    virtual HRESULT STDMETHODCALLTYPE didFailProvisionalLoadWithError( 
        /* [in] */ IWebView *webView,
        /* [in] */ IWebError *error,
        /* [in] */ IWebFrame *frame) { return S_OK; }
    
    virtual HRESULT STDMETHODCALLTYPE didCommitLoadForFrame( 
        /* [in] */ IWebView *webView,
        /* [in] */ IWebFrame *frame) { return S_OK; }
    
    virtual HRESULT STDMETHODCALLTYPE didReceiveTitle( 
        /* [in] */ IWebView *webView,
        /* [in] */ BSTR title,
        /* [in] */ IWebFrame *frame);
    
    virtual HRESULT STDMETHODCALLTYPE didReceiveIcon( 
        /* [in] */ IWebView *webView,
        /* [in] */ IWebImage *image,
        /* [in] */ IWebFrame *frame) { return S_OK; }
    
    virtual HRESULT STDMETHODCALLTYPE didFinishLoadForFrame( 
        /* [in] */ IWebView* webView,
        /* [in] */ IWebFrame* /*frame*/);
    
    virtual HRESULT STDMETHODCALLTYPE didFailLoadWithError( 
        /* [in] */ IWebView *webView,
        /* [in] */ IWebError *error,
        /* [in] */ IWebFrame *forFrame) { return S_OK; }
    
    virtual HRESULT STDMETHODCALLTYPE didChangeLocationWithinPageForFrame( 
        /* [in] */ IWebView *webView,
        /* [in] */ IWebFrame *frame) { return S_OK; }
    
    virtual HRESULT STDMETHODCALLTYPE willPerformClientRedirectToURL( 
        /* [in] */ IWebView *webView,
        /* [in] */ BSTR url,
        UINT delaySeconds,
        /* [in] */ UINT fireDate,
        /* [in] */ IWebFrame *frame) { return S_OK; }
    
    virtual HRESULT STDMETHODCALLTYPE didCancelClientRedirectForFrame( 
        /* [in] */ IWebView *webView,
        /* [in] */ IWebFrame *frame) { return S_OK; }
    
    virtual HRESULT STDMETHODCALLTYPE willCloseFrame( 
        /* [in] */ IWebView *webView,
        /* [in] */ IWebFrame *frame) { return S_OK; }
    
    virtual HRESULT STDMETHODCALLTYPE windowScriptObjectAvailable( 
        /* [in] */ IWebView *webView,
        /* [in] */ IWebScriptObject *windowScriptObject);

    // IWebUIDelegate
    virtual HRESULT STDMETHODCALLTYPE createWebViewWithRequest( 
        /* [in] */ IWebView *sender,
        /* [in] */ IWebURLRequest *request,
        /* [retval][out] */ IWebView **newWebView) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewShow( 
        /* [in] */ IWebView *sender) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewClose( 
        /* [in] */ IWebView *sender) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewFocus( 
        /* [in] */ IWebView *sender) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewUnfocus( 
        /* [in] */ IWebView *sender) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewFirstResponder( 
        /* [in] */ IWebView *sender,
        /* [retval][out] */ HWND *responder) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE makeFirstResponder( 
        /* [in] */ IWebView *sender,
        /* [in] */ HWND responder) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE setStatusText( 
        /* [in] */ IWebView *sender,
        /* [in] */ BSTR text) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewStatusText( 
        /* [in] */ IWebView *sender,
        /* [retval][out] */ BSTR *text) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewAreToolbarsVisible( 
        /* [in] */ IWebView *sender,
        /* [retval][out] */ BOOL *visible) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE setToolbarsVisible( 
        /* [in] */ IWebView *sender,
        /* [in] */ BOOL visible) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewIsStatusBarVisible( 
        /* [in] */ IWebView *sender,
        /* [retval][out] */ BOOL *visible) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE setStatusBarVisible( 
        /* [in] */ IWebView *sender,
        /* [in] */ BOOL visible) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewIsResizable( 
        /* [in] */ IWebView *sender,
        /* [retval][out] */ BOOL *resizable) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE setResizable( 
        /* [in] */ IWebView *sender,
        /* [in] */ BOOL resizable) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE setFrame( 
        /* [in] */ IWebView *sender,
        /* [in] */ RECT *frame) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewFrame( 
        /* [in] */ IWebView *sender,
        /* [retval][out] */ RECT **frame) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE setContentRect( 
        /* [in] */ IWebView *sender,
        /* [in] */ RECT *contentRect) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE webViewContentRect( 
        /* [in] */ IWebView *sender,
        /* [retval][out] */ RECT **contentRect) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE runJavaScriptAlertPanelWithMessage( 
        /* [in] */ IWebView *sender,
        /* [in] */ BSTR message);
    
    virtual HRESULT STDMETHODCALLTYPE runJavaScriptConfirmPanelWithMessage( 
        /* [in] */ IWebView *sender,
        /* [in] */ BSTR message,
        /* [retval][out] */ BOOL *result) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE runJavaScriptTextInputPanelWithPrompt( 
        /* [in] */ IWebView *sender,
        /* [in] */ BSTR message,
        /* [in] */ BSTR defaultText,
        /* [retval][out] */ BSTR *result) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE runOpenPanelForFileButtonWithResultListener( 
        /* [in] */ IWebView *sender,
        /* [in] */ IWebOpenPanelResultListener *resultListener) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE mouseDidMoveOverElement( 
        /* [in] */ IWebView *sender,
        /* [in] */ IPropertyBag *elementInformation,
        /* [in] */ UINT modifierFlags) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE contextMenuItemsForElement( 
        /* [in] */ IWebView *sender,
        /* [in] */ IPropertyBag *element,
        /* [in] */ HMENU defaultItems,
        /* [retval][out] */ HMENU *resultMenu) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE validateUserInterfaceItem( 
        /* [in] */ IWebView *webView,
        /* [in] */ UINT itemCommandID,
        /* [in] */ BOOL defaultValidation,
        /* [retval][out] */ BOOL *isValid) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE shouldPerformAction( 
        /* [in] */ IWebView *webView,
        /* [in] */ UINT itemCommandID,
        /* [in] */ UINT sender) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE dragDestinationActionMaskForDraggingInfo( 
        /* [in] */ IWebView *webView,
        /* [in] */ IDropSource *draggingInfo,
        /* [retval][out] */ WebDragDestinationAction *action) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE willPerformDragDestinationAction( 
        /* [in] */ IWebView *webView,
        /* [in] */ IDropSource *draggingInfo) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE dragSourceActionMaskForPoint( 
        /* [in] */ IWebView *webView,
        /* [in] */ LPPOINT point,
        /* [retval][out] */ WebDragSourceAction *action) { DebugBreak(); return E_NOTIMPL; }
    
    virtual HRESULT STDMETHODCALLTYPE willPerformDragSourceAction( 
        /* [in] */ IWebView *webView,
        /* [in] */ WebDragSourceAction action,
        /* [in] */ LPPOINT point,
        /* [in] */ IDataObject *pasteboard) { DebugBreak(); return E_NOTIMPL; }

protected:
    ULONG                   m_refCount;
};