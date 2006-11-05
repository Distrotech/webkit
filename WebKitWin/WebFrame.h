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

#ifndef WebFrame_H
#define WebFrame_H

#include "DOMCore.h"
#include "IWebFormDelegate.h"
#include "IWebFrame.h"
#include "IWebFramePrivate.h"
#include "WebDataSource.h"

#pragma warning(push, 0)
#include <WebCore/platform/network/ResourceHandleClient.h>
#include <WebCore/bridge/win/FrameWin.h>
#include <WebCore/Platform/PlatformString.h>
#pragma warning(pop)

#include <WTF/RefPtr.h>
#include <WTF/HashMap.h>

namespace WebCore {
    class Element;
    class Frame;
    class Page;
    class IntRect;
}

class WebView;
class WebHistory;
interface IWebHistoryItemPrivate;

typedef enum {
    WebFrameLoadTypeStandard,
    WebFrameLoadTypeBack,
    WebFrameLoadTypeForward,
    WebFrameLoadTypeIndexedBackForward, // a multi-item hop in the backforward list
    WebFrameLoadTypeReload,
    WebFrameLoadTypeReloadAllowingStaleData,
    WebFrameLoadTypeSame,               // user loads same URL again (but not reload button)
    WebFrameLoadTypeInternal,
    WebFrameLoadTypeReplace
} WebFrameLoadType;

class WebFrame : public IWebFrame, IWebFramePrivate, public WebCore::ResourceHandleClient, public WebCore::FrameWinClient, public IWebFormSubmissionListener
{
public:
    static WebFrame* createInstance();
protected:
    WebFrame();
    ~WebFrame();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    //IWebFrame
    virtual HRESULT STDMETHODCALLTYPE name( 
        /* [retval][out] */ BSTR *frameName);
    
    virtual HRESULT STDMETHODCALLTYPE webView( 
        /* [retval][out] */ IWebView **view);
    
    virtual HRESULT STDMETHODCALLTYPE frameView( 
        /* [retval][out] */ IWebFrameView **view);
    
    virtual HRESULT STDMETHODCALLTYPE DOMDocument( 
        /* [retval][out] */ IDOMDocument** document);
    
    virtual HRESULT STDMETHODCALLTYPE frameElement( 
        /* [retval][out] */ IDOMHTMLElement **frameElement);
    
    virtual HRESULT STDMETHODCALLTYPE loadRequest( 
        /* [in] */ IWebURLRequest *request);
    
    virtual HRESULT STDMETHODCALLTYPE loadData( 
        /* [in] */ IStream *data,
        /* [in] */ BSTR mimeType,
        /* [in] */ BSTR textEncodingName,
        /* [in] */ BSTR url);
    
    virtual HRESULT STDMETHODCALLTYPE loadHTMLString( 
        /* [in] */ BSTR string,
        /* [in] */ BSTR baseURL);
    
    virtual HRESULT STDMETHODCALLTYPE loadAlternateHTMLString( 
        /* [in] */ BSTR str,
        /* [in] */ BSTR baseURL,
        /* [in] */ BSTR unreachableURL);
    
    virtual HRESULT STDMETHODCALLTYPE loadArchive( 
        /* [in] */ IWebArchive *archive);
    
    virtual HRESULT STDMETHODCALLTYPE dataSource( 
        /* [retval][out] */ IWebDataSource **source);
    
    virtual HRESULT STDMETHODCALLTYPE provisionalDataSource( 
        /* [retval][out] */ IWebDataSource **source);
    
    virtual HRESULT STDMETHODCALLTYPE stopLoading( void);
    
    virtual HRESULT STDMETHODCALLTYPE reload( void);
    
    virtual HRESULT STDMETHODCALLTYPE findFrameNamed( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IWebFrame **frame);
    
    virtual HRESULT STDMETHODCALLTYPE parentFrame( 
        /* [retval][out] */ IWebFrame **frame);
    
    virtual HRESULT STDMETHODCALLTYPE childFrames( 
        /* [out] */ int *frameCount,
        /* [retval][out] */ IWebFrame ***frames);

    // IWebFramePrivate
    virtual HRESULT STDMETHODCALLTYPE renderTreeAsExternalRepresentation(
        /* [retval][out] */ BSTR *result);

    // IWebFormSubmissionListener
    virtual HRESULT STDMETHODCALLTYPE continueSubmit( void);

    // ResourceHandleClient
    virtual void willSendRequest(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse);
    virtual void didReceiveResponse(WebCore::ResourceHandle*, const WebCore::ResourceResponse&);
    virtual void didReceiveData(WebCore::ResourceHandle*, const char*, int);
    virtual void didFinishLoading(WebCore::ResourceHandle*);

    // FrameWinClient
    virtual void ref();
    virtual void deref();
    virtual WebCore::Frame* createFrame(const WebCore::KURL&, const WebCore::String& name, WebCore::Element* ownerElement, const WebCore::String& referrer);
    virtual void stopMainResourceLoad();
    virtual void openURL(const WebCore::String& URL, const WebCore::Event* triggeringEvent, bool newWindow, bool lockHistory);
    virtual void submitForm(const WebCore::String& method, const WebCore::KURL&, const WebCore::FormData*, WebCore::Element* form, WTF::HashMap<WebCore::String, WebCore::String>& formValues);
    virtual void setTitle(const WebCore::String& title);
    virtual void setStatusText(const WebCore::String& title);
    virtual void textFieldDidBeginEditing(WebCore::Element*);
    virtual void textFieldDidEndEditing(WebCore::Element*);
    virtual void textDidChangeInTextField(WebCore::Element*);
    virtual bool doTextFieldCommandFromEvent(WebCore::Element*, const WebCore::PlatformKeyboardEvent*);
    virtual void textWillBeDeletedInTextField(WebCore::Element* input);
    virtual void textDidChangeInTextArea(WebCore::Element*);
    virtual void didFirstLayout();
    virtual void handledOnloadEvents();
    virtual const WebCore::String& userAgentForURL(const WebCore::KURL&);
    virtual const WebCore::KURL& originalRequestURL();
    virtual void runJavaScriptAlert(const WebCore::String&);
    virtual bool runJavaScriptConfirm(const WebCore::String&);
    virtual bool runJavaScriptPrompt(const WebCore::String& message, const WebCore::String& defaultValue, WebCore::String& result);
    virtual bool tabsToLinks() const;
    virtual WebCore::IntRect windowResizerRect() const;
    virtual void addToDirtyRegion(const WebCore::IntRect&);
    virtual void scrollBackingStore(int dx, int dy, const WebCore::IntRect& scrollViewRect, const WebCore::IntRect& clipRect);
    virtual void updateBackingStore();

    // WebFrame
    void initWithWebFrameView(IWebFrameView* /*view*/, IWebView* webView, WebCore::Page* page, WebCore::Element* ownerElement);
    void layoutIfNeeded();
    void setNeedsLayout();
    WebCore::Frame* impl();
    HRESULT loadDataSource(WebDataSource* dataSource);
    bool loading();
    HRESULT goToItem(IWebHistoryItem* item, WebFrameLoadType withLoadType);
    HRESULT loadItem(IWebHistoryItem* item, WebFrameLoadType withLoadType);
    void invalidate();
    // WebFrame (matching WebCoreFrameBridge)
    void setTextSizeMultiplier(float multiplier);
    void inViewSourceMode(BOOL *flag);
    void setInViewSourceMode(BOOL flag);
    HRESULT elementWithName(BSTR name, IDOMElement* form, IDOMElement** element);
    HRESULT formForElement(IDOMElement* element, IDOMElement** form);
    HRESULT elementDoesAutoComplete(IDOMElement* element, bool* result);
    HRESULT controlsInForm(IDOMElement* form, IDOMElement** controls, int* cControls);
    HRESULT elementIsPassword(IDOMElement* element, bool* result);
    HRESULT searchForLabelsBeforeElement(const BSTR* labels, int cLabels, IDOMElement* beforeElement, BSTR* result);
    HRESULT matchLabelsAgainstElement(const BSTR* labels, int cLabels, IDOMElement* againstElement, BSTR* result);
    HRESULT canProvideDocumentSource(bool* result);
    const Vector<char>* data() { return &m_buffer; }
    HRESULT reloadAllowingStaleDataWithOverrideEncoding(BSTR encoding);
    IWebBackForwardList* backForwardList();
    WebHistory* webHistory();

protected:
    unsigned int getObjectCacheSize();

protected:
    class WebFramePrivate;
    WebFramePrivate*    d;
    ULONG               m_refCount;
    WebDataSource*      m_dataSource;
    IWebDataSourcePrivate* m_dataSourcePrivate;
    WebDataSource*      m_provisionalDataSource;
    WebFrameLoadType    m_loadType;
    bool                m_quickRedirectComing;
    bool                m_continueFormSubmit;
    Vector<char>        m_buffer;
    BSTR                m_textEncoding;
    RefPtr<WebCore::ResourceHandle> m_loader;
    WebCore::KURL       m_originalRequestURL;
};

#endif
