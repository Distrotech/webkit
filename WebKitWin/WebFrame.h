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
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/FrameWin.h>
#include <WebCore/KURL.h>
#include <WebCore/PlatformString.h>
#include <WebCore/ResourceHandleClient.h>
#pragma warning(pop)

#include <WTF/RefPtr.h>
#include <WTF/HashMap.h>

namespace WebCore {
    class AuthenticationChallenge;
    class DocumentLoader;
    class Element;
    class Frame;
    class HTMLFrameOwnerElement;
    class IntRect;
    class Page;
    class ResourceError;
    class SharedBuffer;
}

typedef const struct OpaqueJSContext* JSContextRef;
typedef struct OpaqueJSValue* JSObjectRef;

class WebFrame;
class WebView;
class WebHistory;
class WebFramePolicyListener;

interface IWebHistoryItemPrivate;

unsigned long long WebSystemMainMemory();
WebFrame* kit(WebCore::Frame* frame);

extern const GUID IID_WebFrame;

class WebFrame : public IWebFrame, IWebFramePrivate
    , public WebCore::FrameWinClient
    , public WebCore::FrameLoaderClient
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

    virtual HRESULT STDMETHODCALLTYPE firstLayoutDone(
        /* [retval][out] */ BOOL* result);

    virtual HRESULT STDMETHODCALLTYPE loadType( 
        /* [retval][out] */ WebFrameLoadType* type);

    // FrameWinClient
    virtual void ref();
    virtual void deref();

    virtual WebCore::Frame* createFrame(const WebCore::KURL&, const WebCore::String& name, WebCore::HTMLFrameOwnerElement*, const WebCore::String& referrer);
    virtual void stopMainResourceLoad();
    virtual void openURL(const WebCore::String& URL, const WebCore::Event* triggeringEvent, bool newWindow, bool lockHistory);
    virtual void submitForm(const WebCore::FrameLoadRequest&, WebCore::Element* form, HashMap<WebCore::String, WebCore::String>& formValues);
    virtual void setTitle(const WebCore::String& title);
    virtual void setStatusText(const WebCore::String& title);
    virtual void textFieldDidBeginEditing(WebCore::Element*);
    virtual void textFieldDidEndEditing(WebCore::Element*);
    virtual void textDidChangeInTextField(WebCore::Element*);
    virtual bool doTextFieldCommandFromEvent(WebCore::Element*, const WebCore::PlatformKeyboardEvent*);
    virtual void textWillBeDeletedInTextField(WebCore::Element* input);
    virtual void textDidChangeInTextArea(WebCore::Element*);
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
    virtual void windowScriptObjectAvailable(JSContextRef context, JSObjectRef windowObject);
    
    // FrameLoaderClient
    virtual void frameLoaderDestroyed();
    virtual bool hasWebView() const;
    virtual bool hasFrameView() const;
    virtual bool privateBrowsingEnabled() const;
    virtual void makeDocumentView();
    virtual void makeRepresentation(WebCore::DocumentLoader*);
    virtual void forceLayout();
    virtual void forceLayoutForNonHTML();
    virtual void setCopiesOnScroll();
    virtual void detachedFromParent1();
    virtual void detachedFromParent2();
    virtual void detachedFromParent3();
    virtual void detachedFromParent4();
    virtual void loadedFromPageCache();
    virtual void dispatchDidHandleOnloadEvents();
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
    virtual void dispatchDidCancelClientRedirect();
    virtual void dispatchWillPerformClientRedirect(const WebCore::KURL&, double interval, double fireDate);
    virtual void dispatchDidChangeLocationWithinPage();
    virtual void dispatchWillClose();
    virtual void dispatchDidReceiveIcon();
    virtual void dispatchDidStartProvisionalLoad();
    virtual void dispatchDidReceiveTitle(const WebCore::String& title);
    virtual void dispatchDidCommitLoad();
    virtual void dispatchDidFinishLoad();
    virtual void dispatchDidFirstLayout();
    virtual void dispatchShow();
    virtual void cancelPolicyCheck();
    virtual void dispatchWillSubmitForm(WebCore::FramePolicyFunction, PassRefPtr<WebCore::FormState>);
    virtual void dispatchDidLoadMainResource(WebCore::DocumentLoader*);
    virtual void revertToProvisionalState(WebCore::DocumentLoader*);
    virtual void clearUnarchivingState(WebCore::DocumentLoader*);
    virtual void progressStarted();
    virtual void progressCompleted();
    virtual void setMainFrameDocumentReady(bool);
    virtual void willChangeTitle(WebCore::DocumentLoader*);
    virtual void didChangeTitle(WebCore::DocumentLoader*);
    virtual void finishedLoading(WebCore::DocumentLoader*);
    virtual void finalSetupForReplace(WebCore::DocumentLoader*);
    virtual void setDefersLoading(bool);
    virtual bool isArchiveLoadPending(WebCore::ResourceLoader*) const;
    virtual void cancelPendingArchiveLoad(WebCore::ResourceLoader*);
    virtual void clearArchivedResources();
    virtual bool canHandleRequest(const WebCore::ResourceRequest&) const;
    virtual bool canShowMIMEType(const WebCore::String& MIMEType) const;
    virtual bool representationExistsForURLScheme(const WebCore::String& URLScheme) const;
    virtual WebCore::String generatedMIMETypeForURLScheme(const WebCore::String& URLScheme) const;
    virtual void frameLoadCompleted();
    virtual void restoreScrollPositionAndViewState();
    virtual void provisionalLoadStarted();
    virtual bool shouldTreatURLAsSameAsCurrent(const WebCore::KURL&) const;
    virtual void addHistoryItemForFragmentScroll();
    virtual void didFinishLoad();
    virtual void prepareForDataSourceReplacement();
    virtual void setTitle(const WebCore::String& title, const WebCore::KURL&);
    virtual WebCore::String userAgent();
    virtual void setDocumentViewFromPageCache(WebCore::PageCache *);
    virtual void updateGlobalHistoryForStandardLoad(const WebCore::KURL &);
    virtual void updateGlobalHistoryForReload(const WebCore::KURL &);
    virtual bool shouldGoToHistoryItem(WebCore::HistoryItem *) const;
    virtual void saveScrollPositionAndViewStateToItem(WebCore::HistoryItem *);
    virtual void saveDocumentViewToPageCache(WebCore::PageCache *);
    virtual bool canCachePage(void) const;
    virtual PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&);
    virtual void setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&);
    virtual WebCore::ResourceError cancelledError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError cannotShowURLError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError interruptForPolicyChangeError(const WebCore::ResourceRequest&);
    virtual WebCore::ResourceError cannotShowMIMETypeError(const WebCore::ResourceResponse&);
    virtual WebCore::ResourceError fileDoesNotExistError(const WebCore::ResourceResponse&);
    virtual bool shouldFallBack(const WebCore::ResourceError&);
    virtual void committedLoad(WebCore::DocumentLoader*, const char*, int);
    virtual void dispatchDecidePolicyForMIMEType(WebCore::FramePolicyFunction, const WebCore::String& MIMEType, const WebCore::ResourceRequest&);
    virtual void dispatchDecidePolicyForNewWindowAction(WebCore::FramePolicyFunction, const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::String& frameName);
    virtual void dispatchDecidePolicyForNavigationAction(WebCore::FramePolicyFunction, const WebCore::NavigationAction&, const WebCore::ResourceRequest&);
    virtual void dispatchUnableToImplementPolicy(const WebCore::ResourceError&);
    virtual bool willUseArchive(WebCore::ResourceLoader*, const WebCore::ResourceRequest&, const WebCore::KURL& originalURL) const;
    virtual void download(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);

    virtual void assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&);
    virtual void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse);
    virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&);
    virtual void dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int lengthReceived);
    virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier);
    virtual void dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceError&);
    virtual bool dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int length);
    virtual void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&);
    virtual void dispatchDidFailLoad(const WebCore::ResourceError&);
    virtual WebCore::Frame* dispatchCreatePage();
    virtual void incrementProgress(unsigned long identifier, const WebCore::ResourceResponse&);
    virtual void incrementProgress(unsigned long identifier, const char*, int);
    virtual void completeProgress(unsigned long identifier);
    virtual void startDownload(const WebCore::ResourceRequest&);
    virtual void dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&);
    virtual void dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::AuthenticationChallenge&);

    // WebFrame
    void initWithWebFrameView(IWebFrameView*, IWebView*, WebCore::Page*, WebCore::HTMLFrameOwnerElement*);
    void layoutIfNeeded();
    void setNeedsLayout();
    WebCore::Frame* impl();
    void invalidate();
    void receivedData(const char*, int, const WebCore::String&);

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
    IWebBackForwardList* backForwardList();
    WebHistory* webHistory();

    WebFramePolicyListener* setUpPolicyListener(WebCore::FramePolicyFunction function);
    void receivedPolicyDecision(WebCore::PolicyAction);

protected:
    unsigned int getObjectCacheSize();

protected:
    ULONG               m_refCount;
    class WebFramePrivate;
    WebFramePrivate*    d;
    bool                m_quickRedirectComing;
    WebCore::KURL       m_originalRequestURL;
};

#endif
