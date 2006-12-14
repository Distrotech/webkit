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

#ifndef WebView_H
#define WebView_H

#include "IWebNotificationObserver.h"
#include "IWebUIDelegatePrivate.h"
#include "IWebView.h"
#include "IWebViewPrivate.h"
#include "WebFrame.h"

#include <WebCore/IntRect.h>
#include <WebCore/Settings.h>

class WebFrame;
class WebBackForwardList;

WebCore::Page* core(IWebView*);

class WebView 
    : public IWebView
    , public IWebViewPrivate
    , public IWebIBActions
    , public IWebViewCSS
    , public IWebViewEditing
    , public IWebViewUndoableEditing
    , public IWebViewEditingActions
    , public IWebNotificationObserver
{
public:
    static WebView* createInstance();
protected:
    WebView();
    ~WebView();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebView

    virtual HRESULT STDMETHODCALLTYPE canShowMIMEType( 
        /* [in] */ BSTR mimeType,
        /* [retval][out] */ BOOL *canShow);
    
    virtual HRESULT STDMETHODCALLTYPE canShowMIMETypeAsHTML( 
        /* [in] */ BSTR mimeType,
        /* [retval][out] */ BOOL *canShow);
    
    virtual HRESULT STDMETHODCALLTYPE MIMETypesShownAsHTML( 
        /* [out] */ int *count,
        /* [retval][out] */ BSTR **mimeTypes);
    
    virtual HRESULT STDMETHODCALLTYPE setMIMETypesShownAsHTML( 
        /* [size_is][in] */ BSTR *mimeTypes,
        /* [in] */ int cMimeTypes);
    
    virtual HRESULT STDMETHODCALLTYPE URLFromPasteboard( 
        /* [in] */ IDataObject *pasteboard,
        /* [retval][out] */ BSTR *url);
    
    virtual HRESULT STDMETHODCALLTYPE URLTitleFromPasteboard( 
        /* [in] */ IDataObject *pasteboard,
        /* [retval][out] */ BSTR *urlTitle);
    
    virtual HRESULT STDMETHODCALLTYPE initWithFrame( 
        /* [in] */ RECT *frame,
        /* [in] */ BSTR frameName,
        /* [in] */ BSTR groupName);
    
    virtual HRESULT STDMETHODCALLTYPE close();

    virtual HRESULT STDMETHODCALLTYPE setUIDelegate( 
        /* [in] */ IWebUIDelegate *d);
    
    virtual HRESULT STDMETHODCALLTYPE uiDelegate( 
        /* [out][retval] */ IWebUIDelegate **d);
    
    virtual HRESULT STDMETHODCALLTYPE setResourceLoadDelegate( 
        /* [in] */ IWebResourceLoadDelegate *d);
    
    virtual HRESULT STDMETHODCALLTYPE resourceLoadDelegate( 
        /* [out][retval] */ IWebResourceLoadDelegate **d);
    
    virtual HRESULT STDMETHODCALLTYPE setDownloadDelegate( 
        /* [in] */ IWebDownloadDelegate *d);
    
    virtual HRESULT STDMETHODCALLTYPE downloadDelegate( 
        /* [out][retval] */ IWebDownloadDelegate **d);
    
    virtual HRESULT STDMETHODCALLTYPE setFrameLoadDelegate( 
        /* [in] */ IWebFrameLoadDelegate *d);
    
    virtual HRESULT STDMETHODCALLTYPE frameLoadDelegate( 
        /* [out][retval] */ IWebFrameLoadDelegate **d);
    
    virtual HRESULT STDMETHODCALLTYPE setPolicyDelegate( 
        /* [in] */ IWebPolicyDelegate *d);
    
    virtual HRESULT STDMETHODCALLTYPE policyDelegate( 
        /* [out][retval] */ IWebPolicyDelegate **d);
    
    virtual HRESULT STDMETHODCALLTYPE mainFrame( 
        /* [out][retval] */ IWebFrame **frame);
    
    virtual HRESULT STDMETHODCALLTYPE backForwardList( 
        /* [out][retval] */ IWebBackForwardList **list);
    
    virtual HRESULT STDMETHODCALLTYPE setMaintainsBackForwardList( 
        /* [in] */ BOOL flag);
    
    virtual HRESULT STDMETHODCALLTYPE goBack( 
        /* [retval][out] */ BOOL *succeeded);
    
    virtual HRESULT STDMETHODCALLTYPE goForward( 
        /* [retval][out] */ BOOL *succeeded);
    
    virtual HRESULT STDMETHODCALLTYPE goToBackForwardItem( 
        /* [in] */ IWebHistoryItem *item,
        /* [retval][out] */ BOOL *succeeded);
    
    virtual HRESULT STDMETHODCALLTYPE setTextSizeMultiplier( 
        /* [in] */ float multiplier);
    
    virtual HRESULT STDMETHODCALLTYPE textSizeMultiplier( 
        /* [retval][out] */ float *multiplier);
    
    virtual HRESULT STDMETHODCALLTYPE setApplicationNameForUserAgent( 
        /* [in] */ BSTR applicationName);
    
    virtual HRESULT STDMETHODCALLTYPE applicationNameForUserAgent( 
        /* [retval][out] */ BSTR *applicationName);
    
    virtual HRESULT STDMETHODCALLTYPE setCustomUserAgent( 
        /* [in] */ BSTR userAgentString);
    
    virtual HRESULT STDMETHODCALLTYPE customUserAgent( 
        /* [retval][out] */ BSTR *userAgentString);
    
    virtual HRESULT STDMETHODCALLTYPE userAgentForURL( 
        /* [in] */ BSTR url,
        /* [retval][out] */ BSTR *userAgent);
    
    virtual HRESULT STDMETHODCALLTYPE supportsTextEncoding( 
        /* [retval][out] */ BOOL *supports);
    
    virtual HRESULT STDMETHODCALLTYPE setCustomTextEncodingName( 
        /* [in] */ BSTR encodingName);
    
    virtual HRESULT STDMETHODCALLTYPE customTextEncodingName( 
        /* [retval][out] */ BSTR *encodingName);
    
    virtual HRESULT STDMETHODCALLTYPE setMediaStyle( 
        /* [in] */ BSTR media);
    
    virtual HRESULT STDMETHODCALLTYPE mediaStyle( 
        /* [retval][out] */ BSTR *media);
    
    virtual HRESULT STDMETHODCALLTYPE stringByEvaluatingJavaScriptFromString( 
        /* [in] */ BSTR script,
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE windowScriptObject( 
        /* [retval][out] */ IWebScriptObject **webScriptObject);
    
    virtual HRESULT STDMETHODCALLTYPE setPreferences( 
        /* [in] */ IWebPreferences *prefs);
    
    virtual HRESULT STDMETHODCALLTYPE preferences( 
        /* [retval][out] */ IWebPreferences **prefs);
    
    virtual HRESULT STDMETHODCALLTYPE setPreferencesIdentifier( 
        /* [in] */ BSTR anIdentifier);
    
    virtual HRESULT STDMETHODCALLTYPE preferencesIdentifier( 
        /* [retval][out] */ BSTR *anIdentifier);
    
    virtual HRESULT STDMETHODCALLTYPE setHostWindow( 
        /* [in] */ HWND window);
    
    virtual HRESULT STDMETHODCALLTYPE hostWindow( 
        /* [retval][out] */ HWND *window);
    
    virtual HRESULT STDMETHODCALLTYPE searchFor( 
        /* [in] */ BSTR str,
        /* [in] */ BOOL forward,
        /* [in] */ BOOL caseFlag,
        /* [in] */ BOOL wrapFlag,
        /* [retval][out] */ BOOL *found);
    
    virtual HRESULT STDMETHODCALLTYPE registerViewClass( 
        /* [in] */ IWebDocumentView *view,
        /* [in] */ IWebDocumentRepresentation *representation,
        /* [in] */ BSTR forMIMEType);

    virtual HRESULT STDMETHODCALLTYPE setGroupName( 
        /* [in] */ BSTR groupName);
    
    virtual HRESULT STDMETHODCALLTYPE groupName( 
        /* [retval][out] */ BSTR *groupName);
    
    virtual HRESULT STDMETHODCALLTYPE estimatedProgress( 
        /* [retval][out] */ double *estimatedProgress);
    
    virtual HRESULT STDMETHODCALLTYPE isLoading( 
        /* [retval][out] */ BOOL *isLoading);
    
    virtual HRESULT STDMETHODCALLTYPE elementAtPoint( 
        /* [in] */ LPPOINT point,
        /* [retval][out] */ IPropertyBag **elementDictionary);
    
    virtual HRESULT STDMETHODCALLTYPE pasteboardTypesForSelection( 
        /* [out][in] */ int *count,
        /* [retval][out] */ BSTR **types);
    
    virtual HRESULT STDMETHODCALLTYPE writeSelectionWithPasteboardTypes( 
        /* [size_is][in] */ BSTR *types,
        /* [in] */ int cTypes,
        /* [in] */ IDataObject *pasteboard);
    
    virtual HRESULT STDMETHODCALLTYPE pasteboardTypesForElement( 
        /* [in] */ IPropertyBag *elementDictionary,
        /* [out][in] */ int *count,
        /* [retval][out] */ BSTR **types);
    
    virtual HRESULT STDMETHODCALLTYPE writeElement( 
        /* [in] */ IPropertyBag *elementDictionary,
        /* [size_is][in] */ BSTR *withPasteboardTypes,
        /* [in] */ int cWithPasteboardTypes,
        /* [in] */ IDataObject *pasteboard);
    
    virtual HRESULT STDMETHODCALLTYPE moveDragCaretToPoint( 
        /* [in] */ LPPOINT point);
    
    virtual HRESULT STDMETHODCALLTYPE removeDragCaret( void);
    
    virtual HRESULT STDMETHODCALLTYPE setDrawsBackground( 
        /* [in] */ BOOL drawsBackground);
    
    virtual HRESULT STDMETHODCALLTYPE drawsBackground( 
        /* [retval][out] */ BOOL *drawsBackground);
    
    virtual HRESULT STDMETHODCALLTYPE setMainFrameURL( 
        /* [in] */ BSTR urlString);
    
    virtual HRESULT STDMETHODCALLTYPE mainFrameURL( 
        /* [retval][out] */ BSTR *urlString);
    
    virtual HRESULT STDMETHODCALLTYPE mainFrameDocument( 
        /* [retval][out] */ IDOMDocument **document);
    
    virtual HRESULT STDMETHODCALLTYPE mainFrameTitle( 
        /* [retval][out] */ BSTR *title);
    
    virtual HRESULT STDMETHODCALLTYPE mainFrameIcon( 
        /* [retval][out] */ HBITMAP *icon);

    // IWebIBActions

    virtual HRESULT STDMETHODCALLTYPE takeStringURLFrom( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE stopLoading( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE reload( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE canGoBack( 
        /* [in] */ IUnknown *sender,
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE goBack( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE canGoForward( 
        /* [in] */ IUnknown *sender,
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE goForward( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE canMakeTextLarger( 
        /* [in] */ IUnknown *sender,
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE makeTextLarger( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE canMakeTextSmaller( 
        /* [in] */ IUnknown *sender,
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE makeTextSmaller( 
        /* [in] */ IUnknown *sender);

    // IWebViewCSS

    virtual HRESULT STDMETHODCALLTYPE computedStyleForElement( 
        /* [in] */ IDOMElement *element,
        /* [in] */ BSTR pseudoElement,
        /* [retval][out] */ IDOMCSSStyleDeclaration **style);

    // IWebViewEditing

    virtual HRESULT STDMETHODCALLTYPE editableDOMRangeForPoint( 
        /* [in] */ LPPOINT point,
        /* [retval][out] */ IDOMRange **range);
    
    virtual HRESULT STDMETHODCALLTYPE setSelectedDOMRange( 
        /* [in] */ IDOMRange *range,
        /* [in] */ WebSelectionAffinity affinity);
    
    virtual HRESULT STDMETHODCALLTYPE selectedDOMRange( 
        /* [retval][out] */ IDOMRange **range);
    
    virtual HRESULT STDMETHODCALLTYPE selectionAffinity( 
        /* [retval][out][retval][out] */ WebSelectionAffinity *affinity);
    
    virtual HRESULT STDMETHODCALLTYPE setEditable( 
        /* [in] */ BOOL flag);
    
    virtual HRESULT STDMETHODCALLTYPE isEditable( 
        /* [retval][out] */ BOOL *isEditable);
    
    virtual HRESULT STDMETHODCALLTYPE setTypingStyle( 
        /* [in] */ IDOMCSSStyleDeclaration *style);
    
    virtual HRESULT STDMETHODCALLTYPE typingStyle( 
        /* [retval][out] */ IDOMCSSStyleDeclaration **style);
    
    virtual HRESULT STDMETHODCALLTYPE setSmartInsertDeleteEnabled( 
        /* [in] */ BOOL flag);
    
    virtual HRESULT STDMETHODCALLTYPE smartInsertDeleteEnabled( 
        /* [in] */ BOOL enabled);
    
    virtual HRESULT STDMETHODCALLTYPE setContinuousSpellCheckingEnabled( 
        /* [in] */ BOOL flag);
    
    virtual HRESULT STDMETHODCALLTYPE isContinuousSpellCheckingEnabled( 
        /* [retval][out] */ BOOL *enabled);
    
    virtual HRESULT STDMETHODCALLTYPE spellCheckerDocumentTag( 
        /* [retval][out] */ int *tag);
    
    virtual HRESULT STDMETHODCALLTYPE undoManager( 
        /* [retval][out] */ IWebUndoManager **manager);
    
    virtual HRESULT STDMETHODCALLTYPE setEditingDelegate( 
        /* [in] */ IWebViewEditingDelegate *d);
    
    virtual HRESULT STDMETHODCALLTYPE editingDelegate( 
        /* [retval][out] */ IWebViewEditingDelegate **d);
    
    virtual HRESULT STDMETHODCALLTYPE styleDeclarationWithText( 
        /* [in] */ BSTR text,
        /* [retval][out] */ IDOMCSSStyleDeclaration **style);

    // IWebViewUndoableEditing

    virtual HRESULT STDMETHODCALLTYPE replaceSelectionWithNode( 
        /* [in] */ IDOMNode *node);
    
    virtual HRESULT STDMETHODCALLTYPE replaceSelectionWithText( 
        /* [in] */ BSTR text);
    
    virtual HRESULT STDMETHODCALLTYPE replaceSelectionWithMarkupString( 
        /* [in] */ BSTR markupString);
    
    virtual HRESULT STDMETHODCALLTYPE replaceSelectionWithArchive( 
        /* [in] */ IWebArchive *archive);
    
    virtual HRESULT STDMETHODCALLTYPE deleteSelection( void);
    
    virtual HRESULT STDMETHODCALLTYPE applyStyle( 
        /* [in] */ IDOMCSSStyleDeclaration *style);

    // IWebViewEditingActions

    virtual HRESULT STDMETHODCALLTYPE copy( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE cut( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE paste( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE copyURL( 
        /* [in] */ BSTR url);

    virtual HRESULT STDMETHODCALLTYPE copyFont( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE pasteFont( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE delete_( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE pasteAsPlainText( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE pasteAsRichText( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE changeFont( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE changeAttributes( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE changeDocumentBackgroundColor( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE changeColor( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE alignCenter( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE alignJustified( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE alignLeft( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE alignRight( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE checkSpelling( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE showGuessPanel( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE performFindPanelAction( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE startSpeaking( 
        /* [in] */ IUnknown *sender);
    
    virtual HRESULT STDMETHODCALLTYPE stopSpeaking( 
        /* [in] */ IUnknown *sender);

    // IWebNotificationObserver

    virtual HRESULT STDMETHODCALLTYPE onNotify( 
        /* [in] */ IWebNotification *notification);

    // IWebViewPrivate
    virtual HRESULT STDMETHODCALLTYPE setInViewSourceMode( 
        /* [in] */ BOOL flag);
    
    virtual HRESULT STDMETHODCALLTYPE inViewSourceMode( 
        /* [retval][out] */ BOOL* flag);

    virtual HRESULT STDMETHODCALLTYPE viewWindow( 
        /* [retval][out] */ HWND *window);

    virtual HRESULT STDMETHODCALLTYPE setFormDelegate( 
        /* [in] */ IWebFormDelegate *formDelegate);
    
    virtual HRESULT STDMETHODCALLTYPE formDelegate( 
        /* [retval][out] */ IWebFormDelegate **formDelegate);

    virtual HRESULT STDMETHODCALLTYPE setFrameLoadDelegatePrivate( 
        /* [in] */ IWebFrameLoadDelegatePrivate *frameLoadDelegatePrivate);

    virtual HRESULT STDMETHODCALLTYPE frameLoadDelegatePrivate( 
        /* [retval][out] */ IWebFrameLoadDelegatePrivate **frameLoadDelegatePrivate);

    virtual HRESULT STDMETHODCALLTYPE scrollOffset( 
        /* [retval][out] */ LPPOINT offset);

    virtual HRESULT STDMETHODCALLTYPE startPrintJob( 
        /* [in] */ HDC printDC);
    
    virtual HRESULT STDMETHODCALLTYPE endPrintJob( 
        /* [in] */ HDC printDC);
    
    virtual HRESULT STDMETHODCALLTYPE getPrintedPageCount( 
        /* [in] */ HDC printDC,
        /* [retval][out] */ UINT *pageCount);
    
    virtual HRESULT STDMETHODCALLTYPE printPage( 
        /* [in] */ HDC printDC,
        UINT pageNumber);

    virtual HRESULT STDMETHODCALLTYPE markAllMatchesForText(
        BSTR search, BOOL caseSensitive, BOOL highlight, UINT limit, UINT* matches);

    virtual HRESULT STDMETHODCALLTYPE unmarkAllTextMatches();

    virtual HRESULT STDMETHODCALLTYPE rectsForTextMatches(
        IEnumTextMatches** pmatches);

    virtual HRESULT STDMETHODCALLTYPE generateSelectionImage(
        BOOL forceWhiteText, HBITMAP* image);

    virtual HRESULT STDMETHODCALLTYPE selectionImageRect(
        RECT* rc);

    // WebView
    WebCore::Page* page();
    bool handleMouseEvent(UINT, WPARAM, LPARAM);
    void setMouseActivated(bool flag) { m_mouseActivated = flag; }
    bool handleContextMenuEvent(WPARAM, LPARAM);
    void performContextMenuAction(WPARAM, LPARAM);
    bool mouseWheel(WPARAM, LPARAM);
    bool execCommand(WPARAM wParam, LPARAM lParam);
    bool keyDown(WPARAM, LPARAM);
    bool keyUp(WPARAM, LPARAM);
    HRESULT goToItem(IWebHistoryItem* item, WebFrameLoadType withLoadType);
    HRESULT updateWebCoreSettingsFromPreferences(IWebPreferences* preferences);
    WebCore::Settings* settings();
    bool inResizer(LPARAM lParam);
    void paint(HDC, LPARAM);
    void paintIntoBackingStore(WebCore::FrameView*, HDC bitmapDC, LPRECT dirtyRect);
    void paintIntoWindow(HDC bitmapDC, HDC windowDC, LPRECT dirtyRect);
    void print(HDC dc, LPARAM options);
    bool ensureBackingStore();
    void addToDirtyRegion(const WebCore::IntRect&);
    void addToDirtyRegion(HRGN);
    void scrollBackingStore(WebCore::FrameView*, int dx, int dy, const WebCore::IntRect& scrollViewRect, const WebCore::IntRect& clipRect);
    void updateBackingStore(WebCore::FrameView*, HDC, bool backingStoreCompletelyDirty);
    void deleteBackingStore();
    void frameRect(RECT* rect);

    // Convenient to be able to violate the rules of COM here for easy movement to the frame.
    WebFrame* topLevelFrame() { return m_mainFrame; }
    const WebCore::String& userAgentForKURL(const WebCore::KURL& url);

private:
    bool handleEditingKeyboardEvent(WebCore::FrameWin*, const WebCore::PlatformKeyboardEvent&);

protected:
    ULONG m_refCount;
    WebCore::String m_groupName;
    HWND m_hostWindow;
    HWND m_viewWindow;
    WebFrame* m_mainFrame;
    WebCore::Page* m_page;
    
    HBITMAP m_backingStoreBitmap;
    SIZE m_backingStoreSize;
    HRGN m_backingStoreDirtyRegion;

    IWebFrameLoadDelegate* m_frameLoadDelegate;
    IWebFrameLoadDelegatePrivate* m_frameLoadDelegatePrivate;
    IWebUIDelegate* m_uiDelegate;
    IWebUIDelegatePrivate* m_uiDelegatePrivate;
    IWebFormDelegate* m_formDelegate;
    IWebBackForwardList* m_backForwardList;
    IWebPreferences* m_preferences;
    WebCore::Settings m_settings;
    bool m_userAgentOverridden;
    WebCore::String m_userAgentCustom;
    WebCore::String m_userAgentStandard;
    float m_textSizeMultiplier;
    WebCore::String m_overrideEncoding;
    WebCore::String m_applicationName;
    Vector<WebCore::IntRect> m_pages;
    bool m_mouseActivated;
};

#endif
