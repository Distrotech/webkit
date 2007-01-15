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
#include "WebFrame.h"

#include "COMPtr.h"
#include "DOMCoreClasses.h"
#include "IWebHistory.h"
#include "IWebHistoryItemPrivate.h"
#include "IWebFrameLoadDelegatePrivate.h"
#include "IWebFormDelegate.h"
#include "IWebUIDelegatePrivate.h"
#include "MarshallingHelpers.h"
#include "WebDocumentLoader.h"
#include "WebError.h"
#include "WebMutableURLRequest.h"
#include "WebEditorClient.h"
#include "WebFramePolicyListener.h"
#include "WebHistory.h"
#include "WebKit.h"
#include "WebKitStatisticsPrivate.h"
#include "WebNotificationCenter.h"
#include "WebView.h"
#include "WebDataSource.h"
#include "WebHistoryItem.h"
#include "WebURLResponse.h"
#pragma warning( push, 0 )
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/BString.h>
#include <WebCore/Cache.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DOMImplementation.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/Event.h>
#include <WebCore/FormState.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/FrameWin.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLGenericFormElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/MouseRelatedEvent.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlugInInfoStore.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceHandleWin.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RenderFrame.h>
#include <WebCore/RenderTreeAsText.h>
#include <wtf/MathExtras.h>
#pragma warning(pop)

using namespace WebCore;
using namespace HTMLNames;

#define FLASH_REDRAW 0

// {A3676398-4485-4a9d-87DC-CB5A40E6351D}
const GUID IID_WebFrame = 
{ 0xa3676398, 0x4485, 0x4a9d, { 0x87, 0xdc, 0xcb, 0x5a, 0x40, 0xe6, 0x35, 0x1d } };


//-----------------------------------------------------------------------------
// Helpers to convert from WebCore to WebKit type
WebFrame* kit(Frame* frame)
{
    if (frame) {
        FrameWin* frameWin = Win(frame);
        FrameWinClient* frameWinClient = frameWin->client();
        if (frameWinClient)
            return static_cast<WebFrame*>(frameWinClient);  // eek, is there a better way than static cast?
    }
    return 0;
}

//-----------------------------------------------------------------------------

class FormValuesPropertyBag : public IPropertyBag, public IPropertyBag2
{
public:
    FormValuesPropertyBag(HashMap<String, String>* formValues) : m_formValues(formValues) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IPropertyBag
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read( 
        /* [in] */ LPCOLESTR pszPropName,
        /* [out][in] */ VARIANT* pVar,
        /* [in] */ IErrorLog* pErrorLog);

    virtual HRESULT STDMETHODCALLTYPE Write( 
        /* [in] */ LPCOLESTR pszPropName,
        /* [in] */ VARIANT* pVar);

    // IPropertyBag2
    virtual HRESULT STDMETHODCALLTYPE Read( 
        /* [in] */ ULONG cProperties,
        /* [in] */ PROPBAG2 *pPropBag,
        /* [in] */ IErrorLog *pErrLog,
        /* [out] */ VARIANT *pvarValue,
        /* [out] */ HRESULT *phrError);
    
    virtual HRESULT STDMETHODCALLTYPE Write( 
        /* [in] */ ULONG cProperties,
        /* [in] */ PROPBAG2 *pPropBag,
        /* [in] */ VARIANT *pvarValue);
    
    virtual HRESULT STDMETHODCALLTYPE CountProperties( 
        /* [out] */ ULONG *pcProperties);
    
    virtual HRESULT STDMETHODCALLTYPE GetPropertyInfo( 
        /* [in] */ ULONG iProperty,
        /* [in] */ ULONG cProperties,
        /* [out] */ PROPBAG2 *pPropBag,
        /* [out] */ ULONG *pcProperties);
    
    virtual HRESULT STDMETHODCALLTYPE LoadObject( 
        /* [in] */ LPCOLESTR pstrName,
        /* [in] */ DWORD dwHint,
        /* [in] */ IUnknown *pUnkObject,
        /* [in] */ IErrorLog *pErrLog);
    
protected:
    HashMap<String, String>* m_formValues;
};

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IPropertyBag))
        *ppvObject = static_cast<IPropertyBag*>(this);
    else if (IsEqualGUID(riid, IID_IPropertyBag2))
        *ppvObject = static_cast<IPropertyBag2*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE FormValuesPropertyBag::AddRef(void)
{
    return 1;
}

ULONG STDMETHODCALLTYPE FormValuesPropertyBag::Release(void)
{
    return 0;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::Read(LPCOLESTR pszPropName, VARIANT* pVar, IErrorLog* /*pErrorLog*/)
{
    HRESULT hr = S_OK;

    if (!pszPropName || !pVar)
        return E_POINTER;

    String key(pszPropName);
    if (!m_formValues->contains(key))
        return E_INVALIDARG;
    
    String value = m_formValues->get(key);

    VARTYPE requestedType = V_VT(pVar);
    VariantClear(pVar);
    V_VT(pVar) = VT_BSTR;
    V_BSTR(pVar) = SysAllocStringLen(value.characters(), value.length());
    if (value.length() && !V_BSTR(pVar))
        return E_OUTOFMEMORY;

    if (requestedType != VT_BSTR && requestedType != VT_EMPTY)
        hr = VariantChangeType(pVar, pVar, VARIANT_NOUSEROVERRIDE | VARIANT_ALPHABOOL, requestedType);
    
    return hr;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::Write(LPCOLESTR pszPropName, VARIANT* pVar)
{
    if (!pszPropName || !pVar)
        return E_POINTER;
    VariantClear(pVar);
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::Read( 
    /* [in] */ ULONG cProperties,
    /* [in] */ PROPBAG2* pPropBag,
    /* [in] */ IErrorLog* pErrLog,
    /* [out] */ VARIANT* pvarValue,
    /* [out] */ HRESULT* phrError)
{
    if (cProperties > (size_t)m_formValues->size())
        return E_INVALIDARG;
    if (!pPropBag || !pvarValue || !phrError)
        return E_POINTER;

    for (ULONG i=0; i<cProperties; i++) {
        VariantInit(&pvarValue[i]);
        phrError[i] = Read(pPropBag->pstrName, &pvarValue[i], pErrLog);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::Write( 
    /* [in] */ ULONG /*cProperties*/,
    /* [in] */ PROPBAG2* pPropBag,
    /* [in] */ VARIANT* pvarValue)
{
    if (!pPropBag || !pvarValue)
        return E_POINTER;
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::CountProperties( 
    /* [out] */ ULONG* pcProperties)
{
    *pcProperties = m_formValues->size();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::GetPropertyInfo( 
    /* [in] */ ULONG iProperty,
    /* [in] */ ULONG cProperties,
    /* [out] */ PROPBAG2* pPropBag,
    /* [out] */ ULONG* pcProperties)
{
    if (iProperty > (size_t)m_formValues->size() || iProperty+cProperties > (size_t)m_formValues->size())
        return E_INVALIDARG;
    if (!pPropBag || !pcProperties)
        return E_POINTER;

    *pcProperties = 0;
    ULONG i = 0;
    ULONG endProperty = iProperty + cProperties;
    for (HashMap<String, String>::iterator it = m_formValues->begin(); i<endProperty; i++) {
        if (i >= iProperty) {
            int storeIndex = (*pcProperties)++;
            pPropBag[storeIndex].dwType = PROPBAG2_TYPE_DATA;
            pPropBag[storeIndex].vt = VT_BSTR;
            pPropBag[storeIndex].cfType = CF_TEXT;
            pPropBag[storeIndex].dwHint = 0;
            pPropBag[storeIndex].pstrName = const_cast<LPOLESTR>(it->first.charactersWithNullTermination());
        }
        ++it;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::LoadObject( 
    /* [in] */ LPCOLESTR pstrName,
    /* [in] */ DWORD /*dwHint*/,
    /* [in] */ IUnknown* pUnkObject,
    /* [in] */ IErrorLog* /*pErrLog*/)
{
    if (!pstrName || !pUnkObject)
        return E_POINTER;
    return E_FAIL;
}

//-----------------------------------------------------------------------------

static HTMLFormElement *formElementFromDOMElement(IDOMElement *element)
{
    if (!element)
        return 0;

    IDOMElementPrivate* elePriv;
    HRESULT hr = element->QueryInterface(IID_IDOMElementPrivate, (void**) &elePriv);
    if (SUCCEEDED(hr)) {
        Element* ele;
        hr = elePriv->coreElement((void**)&ele);
        elePriv->Release();
        if (SUCCEEDED(hr) && ele && ele->hasTagName(formTag))
            return static_cast<HTMLFormElement*>(ele);
    }
    return 0;
}

static HTMLInputElement* inputElementFromDOMElement(IDOMElement* element)
{
    if (!element)
        return 0;

    IDOMElementPrivate* elePriv;
    HRESULT hr = element->QueryInterface(IID_IDOMElementPrivate, (void**) &elePriv);
    if (SUCCEEDED(hr)) {
        Element* ele;
        hr = elePriv->coreElement((void**)&ele);
        elePriv->Release();
        if (SUCCEEDED(hr) && ele && ele->hasTagName(inputTag))
            return static_cast<HTMLInputElement*>(ele);
    }
    return 0;
}

// WebFramePrivate ------------------------------------------------------------

class WebFrame::WebFramePrivate {
public:
    WebFramePrivate() :frame(0), frameView(0), webView(0), needsLayout(false), m_policyFunction(0) { }
    ~WebFramePrivate() { }

    Frame* frame;
    FrameView* frameView;
    WebView* webView;
    bool needsLayout;
    FramePolicyFunction m_policyFunction;
    COMPtr<WebFramePolicyListener> m_policyListener;
};

// WebFrame ----------------------------------------------------------------

WebFrame::WebFrame()
: m_refCount(0)
, d(new WebFrame::WebFramePrivate)
, m_quickRedirectComing(false)
{
    WebFrameCount++;
    gClassCount++;
}

WebFrame::~WebFrame()
{
    delete d;
    WebFrameCount--;
    gClassCount--;
}

WebFrame* WebFrame::createInstance()
{
    WebFrame* instance = new WebFrame();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebFrame::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_WebFrame))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebFrame*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrame))
        *ppvObject = static_cast<IWebFrame*>(this);
    else if (IsEqualGUID(riid, IID_IWebFramePrivate))
        *ppvObject = static_cast<IWebFramePrivate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebFrame::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebFrame::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebFrame -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebFrame::name( 
    /* [retval][out] */ BSTR* /*frameName*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::webView( 
    /* [retval][out] */ IWebView** view)
{
    *view = 0;
    if (!d->webView)
        return E_FAIL;
    *view = d->webView;
    (*view)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::frameView( 
    /* [retval][out] */ IWebFrameView** /*view*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::DOMDocument( 
    /* [retval][out] */ IDOMDocument** document)
{
    *document = DOMDocument::createInstance(d->frame->document());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::frameElement( 
    /* [retval][out] */ IDOMHTMLElement** /*frameElement*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadRequest( 
    /* [in] */ IWebURLRequest* request)
{
    COMPtr<WebMutableURLRequest> requestImpl;

    HRESULT hr = request->QueryInterface(CLSID_WebMutableURLRequest, (void**)&requestImpl);
    if (FAILED(hr))
        return hr;
 
    d->frame->loader()->load(requestImpl->resourceRequest());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadData( 
    /* [in] */ IStream* data,
    /* [in] */ BSTR mimeType,
    /* [in] */ BSTR textEncodingName,
    /* [in] */ BSTR url)
{
    if (mimeType) {
        String mimeTypeString(mimeType, SysStringLen(mimeType));
        d->frame->loader()->setResponseMIMEType(mimeTypeString);
    }

    if (textEncodingName) {
        String encodingString(textEncodingName, SysStringLen(textEncodingName));
        d->frame->loader()->setEncoding(encodingString, false);
    }

    if (url) {
        DeprecatedString urlString((DeprecatedChar*)url, SysStringLen(url));
        m_originalRequestURL = KURL(urlString);
        d->frame->loader()->didOpenURL(m_originalRequestURL);
        d->frame->loader()->begin(m_originalRequestURL);
    } else {
        d->frame->loader()->didOpenURL("about:blank");
        d->frame->loader()->begin();
    }

    STATSTG stat;
    if (SUCCEEDED(data->Stat(&stat, STATFLAG_NONAME))) {
        if (!stat.cbSize.HighPart && stat.cbSize.LowPart) {
            Vector<char> dataBuffer(stat.cbSize.LowPart);
            ULONG read;
            if (SUCCEEDED(data->Read(dataBuffer.data(), (ULONG)dataBuffer.size(), &read)))
                d->frame->loader()->write(dataBuffer.data(), read);
        }
    }

    d->frame->loader()->end();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadHTMLString( 
    /* [in] */ BSTR string,
    /* [in] */ BSTR baseURL)
{
    DeprecatedString htmlString((DeprecatedChar*)string, SysStringLen(string));

    if (baseURL) {
        DeprecatedString baseURLString((DeprecatedChar*)baseURL, SysStringLen(baseURL));
        m_originalRequestURL = KURL(baseURLString);
        d->frame->loader()->begin(m_originalRequestURL);
    }
    else
        d->frame->loader()->begin();
    d->frame->loader()->write(htmlString);
    d->frame->loader()->end();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadAlternateHTMLString( 
    /* [in] */ BSTR /*str*/,
    /* [in] */ BSTR /*baseURL*/,
    /* [in] */ BSTR /*unreachableURL*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadArchive( 
    /* [in] */ IWebArchive* /*archive*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

static inline WebDataSource *getWebDataSource(DocumentLoader* loader)
{
    return loader ? static_cast<WebDocumentLoader*>(loader)->dataSource() : 0;
}

HRESULT STDMETHODCALLTYPE WebFrame::dataSource( 
    /* [retval][out] */ IWebDataSource** source)
{
    FrameLoader* frameLoader = d->frame->loader();
    WebDataSource* webDataSource = frameLoader ? getWebDataSource(frameLoader->documentLoader()) : 0;

    *source = webDataSource;

    if (webDataSource)
        webDataSource->AddRef(); 

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::provisionalDataSource( 
    /* [retval][out] */ IWebDataSource** source)
{
    FrameLoader* frameLoader = d->frame->loader();
    WebDataSource* webDataSource = frameLoader ? getWebDataSource(frameLoader->provisionalDocumentLoader()) : 0;

    *source = webDataSource;

    if (webDataSource)
        webDataSource->AddRef(); 

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::stopLoading( void)
{
    d->frame->loader()->stopAllLoaders();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::reload( void)
{
    d->frame->loader()->reload();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::findFrameNamed( 
    /* [in] */ BSTR /*name*/,
    /* [retval][out] */ IWebFrame** /*frame*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::parentFrame( 
    /* [retval][out] */ IWebFrame** frame)
{
    HRESULT hr = S_OK;
    *frame = 0;
    Frame* coreFrame = impl();
    if (coreFrame) {
        WebFrame* webFrame = kit(coreFrame->tree()->parent());
        if (webFrame)
            hr = webFrame->QueryInterface(IID_IWebFrame, (void**) frame);
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE WebFrame::childFrames( 
    /* [out] */ int* /*frameCount*/,
    /* [retval][out] */ IWebFrame*** /*frames*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// IWebFramePrivaete ------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebFrame::renderTreeAsExternalRepresentation(
    /* [retval][out] */ BSTR *result)
{
    DeprecatedString representation = externalRepresentation(d->frame->renderer());

    *result = SysAllocStringLen((LPCOLESTR)representation.unicode(), representation.length());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::firstLayoutDone(
    /* [retval][out] */ BOOL* result)
{
    *result = d->frame->loader()->firstLayoutDone();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadType( 
    /* [retval][out] */ WebFrameLoadType* type)
{
    *type = (WebFrameLoadType)d->frame->loader()->loadType();
    return S_OK;
}


// WebFrame ---------------------------------------------------------------

void WebFrame::initWithWebFrameView(IWebFrameView* /*view*/, IWebView* webView, Page* page, HTMLFrameOwnerElement* ownerElement)
{
    if (FAILED(webView->QueryInterface(CLSID_WebView, (void**)&d->webView)))
        return;
    d->webView->Release(); // don't hold the extra ref

    HWND viewWindow;
    d->webView->viewWindow(&viewWindow);

    this->AddRef(); // We release this ref in frameLoaderDestroyed()
    Frame* frame = new FrameWin(page, ownerElement, this, this);
    d->frame = frame;

    FrameView* frameView = new FrameView(frame);
    d->frameView = frameView;

    frame->setSettings(core(webView)->settings());
    frame->setView(frameView);
    frameView->deref(); // FrameViews are created with a ref count of 1. Release this ref since we've assigned it to frame.

    frameView->setContainingWindow(viewWindow);

    // FIXME: This is one-time initialization, but it gets the value of the setting from the
    // current WebView. That's a mismatch and not good!
    static bool initializedObjectCacheSize = false;
    if (!initializedObjectCacheSize) {
        cache()->setMaximumSize(getObjectCacheSize());
        initializedObjectCacheSize = true;
    }
}

void WebFrame::layoutIfNeeded()
{
    if (d->needsLayout) {
        d->frameView->layout();
        d->needsLayout = false;
    } else if (d->frameView->layoutPending())
        d->frameView->layout();
}

void WebFrame::setNeedsLayout()
{
    d->needsLayout = true;
}

Frame* WebFrame::impl()
{
    return d->frame;
}

void WebFrame::stopMainResourceLoad()
{
    // FIXME: Remove this function
}

unsigned long long WebSystemMainMemory()
{
    MEMORYSTATUSEX statex;
    
    statex.dwLength = sizeof (statex);
    GlobalMemoryStatusEx (&statex);
    return statex.ullTotalPhys;
}

unsigned int WebFrame::getObjectCacheSize()
{
    unsigned int cacheSize = 33554432;
    IWebPreferences* preferences;
    if (SUCCEEDED(d->webView->preferences(&preferences)))
        preferences->objectCacheSize(&cacheSize);

    unsigned long long memSize = WebSystemMainMemory();
    unsigned int multiplier = 1;
    
    // 2GB and greater will be 128mb.  1gb and greater will be 64mb.
    // Otherwise just use 32mb.  On Windows the reported memory is
    // a bit smaller, so we use 1000 instead of 1024 as a fudge factor.
    if (memSize >= 2048 * 1024 * 1000 /*1024*/)
        multiplier = 4;
    else if (memSize >= 1024 * 1024 * 1000 /*1024*/)
        multiplier = 2;

    return cacheSize * multiplier;
}

void WebFrame::invalidate()
{
    Document* doc = d->frame->document();
    if (doc)
        doc->recalcStyle(Node::Force);
}

void WebFrame::setTextSizeMultiplier(float multiplier)
{
    int newZoomFactor = (int)round(multiplier * 100);
    if (d->frame->zoomFactor() == newZoomFactor)
        return;

    d->frame->setZoomFactor(newZoomFactor);
}

void WebFrame::inViewSourceMode(BOOL *flag)
{
    *flag = d->frame->inViewSourceMode() ? TRUE : FALSE;
}

void WebFrame::setInViewSourceMode(BOOL flag)
{
    d->frame->setInViewSourceMode(!!flag);
}

HRESULT WebFrame::elementWithName(BSTR name, IDOMElement* form, IDOMElement** element)
{
    if (!form)
        return E_INVALIDARG;

    HTMLFormElement *formElement = formElementFromDOMElement(form);
    if (formElement) {
        Vector<HTMLGenericFormElement*>& elements = formElement->formElements;
        AtomicString targetName((UChar*)name, SysStringLen(name));
        for (unsigned int i = 0; i < elements.size(); i++) {
            HTMLGenericFormElement *elt = elements[i];
            // Skip option elements, other duds
            if (elt->name() == targetName) {
                *element = DOMElement::createInstance(elt);
                return S_OK;
            }
        }
    }
    return E_FAIL;
}

HRESULT WebFrame::formForElement(IDOMElement* element, IDOMElement** form)
{
    if (!element)
        return E_INVALIDARG;

    HTMLInputElement *inputElement = inputElementFromDOMElement(element);
    if (!inputElement)
        return E_FAIL;

    HTMLFormElement *formElement = inputElement->form();
    if (!formElement)
        return E_FAIL;

    *form = DOMElement::createInstance(formElement);
    return S_OK;
}

HRESULT WebFrame::elementDoesAutoComplete(IDOMElement *element, bool *result)
{
    *result = false;
    if (!element)
        return E_INVALIDARG;

    HTMLInputElement *inputElement = inputElementFromDOMElement(element);
    if (!inputElement)
        *result = false;
    else
        *result = (inputElement->inputType() == HTMLInputElement::TEXT && inputElement->autoComplete());

    return S_OK;
}

HRESULT WebFrame::controlsInForm(IDOMElement* form, IDOMElement** controls, int* cControls)
{
    if (!form)
        return E_INVALIDARG;

    HTMLFormElement *formElement = formElementFromDOMElement(form);
    if (!formElement)
        return E_FAIL;

    int inCount = *cControls;
    int count = (int) formElement->formElements.size();
    *cControls = count;
    if (!controls)
        return S_OK;
    if (inCount < count)
        return E_FAIL;

    *cControls = 0;
    Vector<HTMLGenericFormElement*>& elements = formElement->formElements;
    for (int i = 0; i < count; i++) {
        if (elements.at(i)->isEnumeratable()) { // Skip option elements, other duds
            controls[*cControls] = DOMElement::createInstance(elements.at(i));
            (*cControls)++;
        }
    }
    return S_OK;
}

HRESULT WebFrame::elementIsPassword(IDOMElement *element, bool *result)
{
    HTMLInputElement *inputElement = inputElementFromDOMElement(element);
    *result = inputElement != 0
        && inputElement->inputType() == HTMLInputElement::PASSWORD;
    return S_OK;
}

HRESULT WebFrame::searchForLabelsBeforeElement(const BSTR* /*labels*/, int /*cLabels*/, IDOMElement* /*beforeElement*/, BSTR* /*result*/)
{
    // FIXME
    return E_NOTIMPL;
}

HRESULT WebFrame::matchLabelsAgainstElement(const BSTR* /*labels*/, int /*cLabels*/, IDOMElement* /*againstElement*/, BSTR* /*result*/)
{
    // FIXME
    return E_NOTIMPL;
}

HRESULT WebFrame::canProvideDocumentSource(bool* result)
{
    HRESULT hr = S_OK;
    *result = false;

    COMPtr<IWebDataSource> dataSource;
    hr = WebFrame::dataSource(&dataSource);
    if (FAILED(hr))
        return hr;

    COMPtr<IWebURLResponse> urlResponse;
    hr = dataSource->response(&urlResponse);
    if (SUCCEEDED(hr) && urlResponse) {
        BSTR mimeTypeBStr;
        if (SUCCEEDED(urlResponse->MIMEType(&mimeTypeBStr))) {
            String mimeType(mimeTypeBStr, SysStringLen(mimeTypeBStr));
            *result = (!WebCore::DOMImplementation::isTextMIMEType(mimeType) && !Image::supportsType(mimeType) && !PlugInInfoStore::supportsMIMEType(mimeType));
            SysFreeString(mimeTypeBStr);
        }
    }
    return hr;
}

// FrameWinClient

void WebFrame::ref()
{
    this->AddRef();
}

void WebFrame::deref()
{
    this->Release();
}

void WebFrame::frameLoaderDestroyed()
{
    this->Release();
}

Frame* WebFrame::createFrame(const KURL& URL, const String& name, HTMLFrameOwnerElement* ownerElement, const String& /* referrer */)
{
    WebFrame* webFrame = WebFrame::createInstance();
    webFrame->initWithWebFrameView(0, d->webView, d->frame->page(), ownerElement);

    Frame* frame = webFrame->impl();
    d->frame->tree()->appendChild(frame);
    frame->deref(); // Frames are created with a refcount of 1. Release this ref, since we've assigned it to d->frame.
    webFrame->Release(); // The same goes for webFrame, which is owned by frame.

    frame->tree()->setName(name);

    WebMutableURLRequest* request = WebMutableURLRequest::createInstance();
    String urlString = String(URL.url());
    BString urlBStr(urlString);
    BSTR method = SysAllocString(L"GET");

    HRESULT hr;
    hr = request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 0);
    if (FAILED(hr))
        return frame;

    hr = request->setHTTPMethod(method);
    if (FAILED(hr))
        return frame;

    SysFreeString(method);

    webFrame->loadRequest(request);

    return frame;
}

void WebFrame::openURL(const String& URL, const Event* triggeringEvent, bool newWindow, bool lockHistory)
{
    bool ctrlPressed = false;
    bool shiftPressed = false;
    if (triggeringEvent) {
        if (triggeringEvent->isMouseEvent()) {
            const MouseRelatedEvent* mouseEvent = static_cast<const MouseRelatedEvent*>(triggeringEvent);
            ctrlPressed = mouseEvent->ctrlKey();
            shiftPressed = mouseEvent->shiftKey();
        } else if (triggeringEvent->isKeyboardEvent()) {
            const KeyboardEvent* keyEvent = static_cast<const KeyboardEvent*>(triggeringEvent);
            ctrlPressed = keyEvent->ctrlKey();
            shiftPressed = keyEvent->shiftKey();
        }
    }

    if (ctrlPressed)
        newWindow = true;

    BString urlBStr = URL;

    IWebMutableURLRequest* request = WebMutableURLRequest::createInstance();
    if (FAILED(request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 0)))
        goto exit;

    if (newWindow) {
        // new tab/window
        IWebUIDelegate* ui;
        IWebView* newWebView;
        if (SUCCEEDED(d->webView->uiDelegate(&ui)) && ui) {
            if (SUCCEEDED(ui->createWebViewWithRequest(d->webView, request, &newWebView))) {
                if (shiftPressed) {
                    // Ctrl-Option-Shift-click:  Opens a link in a new window and selects it.
                    // Ctrl-Shift-click:  Opens a link in a new tab and selects it.
                    ui->webViewShow(d->webView);
                }
                newWebView->Release();
                newWebView = 0;
            }
            ui->Release();
        }
    } else {
        m_quickRedirectComing = lockHistory;
        loadRequest(request);
    }

exit:
    request->Release();
}

void WebFrame::submitForm(const FrameLoadRequest&, Element*, HashMap<String, String>&)
{
    // FIXME: Get rid of this
    ASSERT_NOT_REACHED();
}

void WebFrame::setTitle(const String&)
{
    ASSERT_NOT_REACHED();
}

void WebFrame::setStatusText(const String& statusText)
{
    IWebUIDelegate* uiDelegate;
    if (SUCCEEDED(d->webView->uiDelegate(&uiDelegate)) && uiDelegate) {
        BSTR statusBStr = SysAllocStringLen(statusText.characters(), statusText.length());
        uiDelegate->setStatusText(d->webView, statusBStr);
        SysFreeString(statusBStr);
        uiDelegate->Release();
    }
}

void WebFrame::textFieldDidBeginEditing(Element* e)
{
    IWebFormDelegate* formDelegate;
    if (SUCCEEDED(d->webView->formDelegate(&formDelegate)) && formDelegate) {
        IDOMElement* domElement = DOMElement::createInstance(e);
        if (domElement) {
            IDOMHTMLInputElement* domInputElement;
            if (SUCCEEDED(domElement->QueryInterface(IID_IDOMHTMLInputElement, (void**)&domInputElement))) {
                formDelegate->textFieldDidBeginEditing(domInputElement, this);
                domInputElement->Release();
            }
            domElement->Release();
        }
        formDelegate->Release();
    }
}

void WebFrame::textFieldDidEndEditing(Element* e)
{
    IWebFormDelegate* formDelegate;
    if (SUCCEEDED(d->webView->formDelegate(&formDelegate)) && formDelegate) {
        IDOMElement* domElement = DOMElement::createInstance(e);
        if (domElement) {
            IDOMHTMLInputElement* domInputElement;
            if (SUCCEEDED(domElement->QueryInterface(IID_IDOMHTMLInputElement, (void**)&domInputElement))) {
                formDelegate->textFieldDidEndEditing(domInputElement, this);
                domInputElement->Release();
            }
            domElement->Release();
        }
        formDelegate->Release();
    }
}

void WebFrame::textDidChangeInTextField(Element* e)
{
    IWebFormDelegate* formDelegate;
    if (SUCCEEDED(d->webView->formDelegate(&formDelegate)) && formDelegate) {
        IDOMElement* domElement = DOMElement::createInstance(e);
        if (domElement) {
            IDOMHTMLInputElement* domInputElement;
            if (SUCCEEDED(domElement->QueryInterface(IID_IDOMHTMLInputElement, (void**)&domInputElement))) {
                formDelegate->textDidChangeInTextField(domInputElement, this);
                domInputElement->Release();
            }
            domElement->Release();
        }
        formDelegate->Release();
    }
}

bool WebFrame::doTextFieldCommandFromEvent(Element* e, const PlatformKeyboardEvent* pke)
{
    BOOL result = FALSE;
    IWebFormDelegate* formDelegate;
    if (SUCCEEDED(d->webView->formDelegate(&formDelegate)) && formDelegate) {
        IDOMElement* domElement = DOMElement::createInstance(e);
        if (domElement) {
            IDOMHTMLInputElement* domInputElement;
            if (SUCCEEDED(domElement->QueryInterface(IID_IDOMHTMLInputElement, (void**)&domInputElement))) {
                formDelegate->doCommandBySelector(domInputElement, pke->WindowsKeyCode(), this, &result);
                domInputElement->Release();
            }
            domElement->Release();
        }
        formDelegate->Release();
    }
    return !!result;
}

void WebFrame::textWillBeDeletedInTextField(Element* e)
{
    // We're using the deleteBackward selector for all deletion operations since the autofill code treats all deletions the same way.
    IWebFormDelegate* formDelegate;
    if (SUCCEEDED(d->webView->formDelegate(&formDelegate)) && formDelegate) {
        IDOMElement* domElement = DOMElement::createInstance(e);
        if (domElement) {
            IDOMHTMLInputElement* domInputElement;
            if (SUCCEEDED(domElement->QueryInterface(IID_IDOMHTMLInputElement, (void**)&domInputElement))) {
                BOOL result;
                formDelegate->doCommandBySelector(domInputElement, 0 /*FIXME*/, this, &result);
                domInputElement->Release();
            }
            domElement->Release();
        }
        formDelegate->Release();
    }
}

void WebFrame::textDidChangeInTextArea(Element* e)
{
    IWebFormDelegate* formDelegate;
    if (SUCCEEDED(d->webView->formDelegate(&formDelegate)) && formDelegate) {
        IDOMElement* domElement = DOMElement::createInstance(e);
        if (domElement) {
            IDOMHTMLTextAreaElement* domTextAreaElement;
            if (SUCCEEDED(domElement->QueryInterface(IID_IDOMHTMLTextAreaElement, (void**)&domTextAreaElement))) {
                formDelegate->textDidChangeInTextArea(domTextAreaElement, this);
                domTextAreaElement->Release();
            }
            domElement->Release();
        }
        formDelegate->Release();
    }
}

void WebFrame::dispatchDidHandleOnloadEvents()
{
    IWebFrameLoadDelegatePrivate* frameLoadDelegatePriv;
    if (SUCCEEDED(d->webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv))  && frameLoadDelegatePriv) {
        frameLoadDelegatePriv->didHandleOnloadEventsForFrame(d->webView, this);
        frameLoadDelegatePriv->Release();
    }
}

const String& WebFrame::userAgentForURL(const KURL& url)
{
    return d->webView->userAgentForKURL(url);
}

const KURL& WebFrame::originalRequestURL()
{
    // FIXME: This should go away
    ASSERT_NOT_REACHED();
    return m_originalRequestURL;
}

void WebFrame::runJavaScriptAlert(const String& message)
{
    IWebUIDelegate* ui;
    if (SUCCEEDED(d->webView->uiDelegate(&ui)) && ui) {
        BSTR messageBSTR = SysAllocStringLen(message.characters(), message.length());
        ui->runJavaScriptAlertPanelWithMessage(d->webView, messageBSTR);
        SysFreeString(messageBSTR);
        ui->Release();
    }
}

bool WebFrame::runJavaScriptConfirm(const String& message)
{
    BOOL result = false;
    IWebUIDelegate* ui;
    if (SUCCEEDED(d->webView->uiDelegate(&ui)) && ui) {
        BSTR messageBSTR = SysAllocStringLen(message.characters(), message.length());
        ui->runJavaScriptConfirmPanelWithMessage(d->webView, messageBSTR, &result);
        SysFreeString(messageBSTR);
        ui->Release();
    }
    return !!result;
}

bool WebFrame::runJavaScriptPrompt(const String& message, const String& defaultValue, String& result)
{
    bool succeeded = false;
    IWebUIDelegate* ui;
    if (SUCCEEDED(d->webView->uiDelegate(&ui)) && ui) {
        BSTR messageBSTR = SysAllocStringLen(message.characters(), message.length());
        BSTR defaultValueBSTR = SysAllocStringLen(defaultValue.characters(), defaultValue.length());
        BSTR resultBSTR = 0;
        if (SUCCEEDED(ui->runJavaScriptTextInputPanelWithPrompt(d->webView, messageBSTR, defaultValueBSTR, &resultBSTR))) {
            succeeded = !!resultBSTR;
            if (succeeded)
                result = String(resultBSTR, SysStringLen(resultBSTR));
        }
        SysFreeString(messageBSTR);
        SysFreeString(defaultValueBSTR);
        SysFreeString(resultBSTR);
        ui->Release();
    }
    return succeeded;
}

bool WebFrame::tabsToLinks() const
{
    BOOL enabled = FALSE;
    IWebPreferences* preferences;
    if (SUCCEEDED(d->webView->preferences(&preferences)))
        preferences->tabsToLinks(&enabled);

    return !!enabled;
}

IntRect WebFrame::windowResizerRect() const
{
    IntRect intRect;

    IWebUIDelegate* ui;
    if (SUCCEEDED(d->webView->uiDelegate(&ui)) && ui) {
        IWebUIDelegatePrivate* uiPrivate;
        if (SUCCEEDED(ui->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&uiPrivate))) {
            RECT r;
            if (SUCCEEDED(uiPrivate->webViewResizerRect(d->webView, &r)))
                intRect = IntRect(r.left, r.top, r.right-r.left, r.bottom-r.top);
            uiPrivate->Release();
        }
        ui->Release();
    }
    return intRect;
}

void WebFrame::addToDirtyRegion(const IntRect& dirtyRect)
{
    d->webView->addToDirtyRegion(dirtyRect);
}

void WebFrame::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    d->webView->scrollBackingStore(d->webView->topLevelFrame()->impl()->view(), dx, dy, scrollViewRect, clipRect);
}

void WebFrame::updateBackingStore()
{
    d->webView->updateBackingStore(d->webView->topLevelFrame()->impl()->view(), 0, false);
}

void WebFrame::windowScriptObjectAvailable(JSContextRef context, JSObjectRef windowObject)
{
    IWebFrameLoadDelegate* frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)) && frameLoadDelegate) {
        frameLoadDelegate->windowScriptObjectAvailable(d->webView, context, windowObject);
        frameLoadDelegate->Release();
    }
}

IWebBackForwardList* WebFrame::backForwardList()
{
    if (this != d->webView->topLevelFrame())
        return 0; // FIXME - need to maintain back/forward for subframes
    
    IWebBackForwardList* backForwardList;
    if (FAILED(d->webView->backForwardList(&backForwardList)))
        return 0;

    return backForwardList;
}

WebHistory* WebFrame::webHistory()
{
    if (this != d->webView->topLevelFrame())
        return 0;

    IWebHistoryPrivate* historyInternal = WebHistory::optionalSharedHistoryInternal(); // does not add a ref
    if (!historyInternal)
        return 0;

    WebHistory* webHistory;
    if (FAILED(historyInternal->QueryInterface(CLSID_WebHistory, (void**)&webHistory)))
        return 0;

    return webHistory;
}

bool WebFrame::hasWebView() const
{
    return !!d->webView;
}

bool WebFrame::hasFrameView() const
{
    return !!d->frameView;
}

bool WebFrame::privateBrowsingEnabled() const
{
    LOG_NOIMPL();
    return false;
}

void WebFrame::makeDocumentView()
{
    LOG_NOIMPL();
}

void WebFrame::makeRepresentation(DocumentLoader*)
{
    LOG_NOIMPL();
}

void WebFrame::forceLayout()
{
    LOG_NOIMPL();
}

void WebFrame::forceLayoutForNonHTML()
{
    LOG_NOIMPL();
}

void WebFrame::setCopiesOnScroll()
{
    LOG_NOIMPL();
}

void WebFrame::detachedFromParent1()
{
    LOG_NOIMPL();
}

void WebFrame::detachedFromParent2()
{
    LOG_NOIMPL();
}

void WebFrame::detachedFromParent3()
{
    LOG_NOIMPL();
}

void WebFrame::detachedFromParent4()
{
    LOG_NOIMPL();
}

void WebFrame::loadedFromPageCache()
{
    LOG_NOIMPL();
}

void WebFrame::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didReceiveServerRedirectForProvisionalLoadForFrame(d->webView, this);
}

void WebFrame::dispatchDidCancelClientRedirect()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didCancelClientRedirectForFrame(d->webView, this);
}

void WebFrame::dispatchWillPerformClientRedirect(const KURL& url, double delay, double fireDate)
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->willPerformClientRedirectToURL(d->webView, BString(url.url()), delay, MarshallingHelpers::CFAbsoluteTimeToDATE(fireDate), this);
}

void WebFrame::dispatchDidChangeLocationWithinPage()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didChangeLocationWithinPageForFrame(d->webView, this);
}

void WebFrame::dispatchWillClose()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->willCloseFrame(d->webView, this);
}

void WebFrame::dispatchDidReceiveIcon()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        // FIXME: Pass in the right HBITMAP. 
        frameLoadDelegate->didReceiveIcon(d->webView, 0, this);
}

void WebFrame::dispatchDidStartProvisionalLoad()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didStartProvisionalLoadForFrame(d->webView, this);
}

void WebFrame::dispatchDidReceiveTitle(const String& title)
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didReceiveTitle(d->webView, BString(title), this);
}

void WebFrame::dispatchDidCommitLoad()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) 
        frameLoadDelegate->didCommitLoadForFrame(d->webView, this);
}

void WebFrame::dispatchDidFinishLoad()
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) 
        frameLoadDelegate->didFinishLoadForFrame(d->webView, this);
}

void WebFrame::dispatchDidFirstLayout()
{
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (SUCCEEDED(d->webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv)
        frameLoadDelegatePriv->didFirstLayoutInFrame(d->webView, this);
}

void WebFrame::dispatchShow()
{
    COMPtr<IWebUIDelegate> ui;

    if (SUCCEEDED(d->webView->uiDelegate(&ui)))
        ui->webViewShow(d->webView);
}

void WebFrame::cancelPolicyCheck()
{
    if (d->m_policyListener) {
        d->m_policyListener->invalidate();
        d->m_policyListener = 0;
    }

    d->m_policyFunction = 0;
}

void WebFrame::dispatchWillSubmitForm(FramePolicyFunction function, PassRefPtr<FormState> formState)
{
    COMPtr<IWebFormDelegate> formDelegate;

    if (FAILED(d->webView->formDelegate(&formDelegate))) {
        (d->frame->loader()->*function)(PolicyUse);
        return;
    }

    COMPtr<IDOMElement> formElement;
    formElement.adoptRef(DOMElement::createInstance(formState->form()));

    // FIXME: The FormValuesPropertyBag constructor should take a const pointer
    FormValuesPropertyBag formValuesPropBag(const_cast<HashMap<String, String>*>(&formState->values()));

    COMPtr<WebFrame> sourceFrame(kit(formState->sourceFrame()));
    formDelegate->willSubmitForm(this, sourceFrame.get(), formElement.get(), &formValuesPropBag, setUpPolicyListener(function));
}

void WebFrame::dispatchDidLoadMainResource(DocumentLoader*)
{
    LOG_NOIMPL();
}

void WebFrame::revertToProvisionalState(DocumentLoader*)
{
    LOG_NOIMPL();
}

void WebFrame::clearUnarchivingState(DocumentLoader*)
{
    LOG_NOIMPL();
}
    
void WebFrame::progressStarted()
{
    LOG_NOIMPL();
}

void WebFrame::progressCompleted()
{
    LOG_NOIMPL();
}

void WebFrame::setMainFrameDocumentReady(bool)
{
    LOG_NOIMPL();
}

void WebFrame::willChangeTitle(DocumentLoader*)
{
    LOG_NOIMPL();
}

void WebFrame::didChangeTitle(DocumentLoader*)
{
    LOG_NOIMPL();
}

void WebFrame::finishedLoading(DocumentLoader* loader)
{
    // Telling the frame we received some data and passing 0 as the data is our
    // way to get work done that is noramlly done when the first bit of data is
    // received, even for the case of a documenbt with no data (like about:blank)
    committedLoad(loader, 0, 0);
}

void WebFrame::finalSetupForReplace(DocumentLoader*)
{
    LOG_NOIMPL();
}

void WebFrame::setDefersLoading(bool)
{
    LOG_NOIMPL();
}

bool WebFrame::isArchiveLoadPending(ResourceLoader*) const
{
    LOG_NOIMPL();
    return false;
}

void WebFrame::cancelPendingArchiveLoad(ResourceLoader*)
{
    LOG_NOIMPL();
}

void WebFrame::clearArchivedResources()
{
    LOG_NOIMPL();
}

bool WebFrame::canHandleRequest(const ResourceRequest&) const
{
    LOG_NOIMPL();
    return true;
}

bool WebFrame::canShowMIMEType(const String& /*MIMEType*/) const
{
    LOG_NOIMPL();
    return true;
}

bool WebFrame::representationExistsForURLScheme(const String& /*URLScheme*/) const
{
    LOG_NOIMPL();
    return false;
}

String WebFrame::generatedMIMETypeForURLScheme(const String& /*URLScheme*/) const
{
    STOP_NOIMPL();
    return String();
}

void WebFrame::frameLoadCompleted()
{
    LOG_NOIMPL();
}

void WebFrame::restoreScrollPositionAndViewState()
{
    LOG_NOIMPL();
}

void WebFrame::provisionalLoadStarted()
{
    LOG_NOIMPL();
}

bool WebFrame::shouldTreatURLAsSameAsCurrent(const KURL&) const
{
    LOG_NOIMPL();
    return false;
}

void WebFrame::addHistoryItemForFragmentScroll()
{
    LOG_NOIMPL();
}

void WebFrame::didFinishLoad()
{
    LOG_NOIMPL();
}

void WebFrame::prepareForDataSourceReplacement()
{
    LOG_NOIMPL();
}

void WebFrame::setTitle(const String& title, const KURL& url)
{
    BOOL privateBrowsingEnabled = FALSE;
    COMPtr<IWebPreferences> preferences;
    if (SUCCEEDED(d->webView->preferences(&preferences)))
        preferences->privateBrowsingEnabled(&privateBrowsingEnabled);
    if (!privateBrowsingEnabled) {
        // update title in global history
        WebHistory* history = webHistory();
        if (history) {
            COMPtr<IWebHistoryItem> item;
            if (SUCCEEDED(history->itemForURL(BString(url.url()), &item))) {
                COMPtr<IWebHistoryItemPrivate> itemPrivate;
                if (SUCCEEDED(item->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&itemPrivate)))
                    itemPrivate->setTitle(BString(title));
            }
        }
    }
}

String WebFrame::userAgent()
{
    return userAgentForURL(KURL());
}

void WebFrame::setDocumentViewFromPageCache(PageCache*)
{
    LOG_NOIMPL();
}

void WebFrame::updateGlobalHistoryForStandardLoad(const KURL& url)
{
    COMPtr<WebHistory> history;
    history.adoptRef(webHistory());

    if (!history)
        return;

    history->addItemForURL(BString(url.url()), 0);                 
}

void WebFrame::updateGlobalHistoryForReload(const KURL& url)
{
    BString urlBStr((LPCOLESTR)url.url().unicode(), url.url().length());

    COMPtr<WebHistory> history;
    history.adoptRef(webHistory());

    if (!history)
        return;

    COMPtr<IWebHistoryItem> item;
    if (SUCCEEDED(history->itemForURL(urlBStr, &item))) {
        COMPtr<IWebHistoryItemPrivate> itemPrivate;
        if (SUCCEEDED(item->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&itemPrivate))) {
            SYSTEMTIME currentTime;
            GetSystemTime(&currentTime);
            DATE visitedTime = 0;
            SystemTimeToVariantTime(&currentTime, &visitedTime);

            // FIXME - bumping the last visited time doesn't mark the history as changed
            itemPrivate->setLastVisitedTimeInterval(visitedTime);
        }
    }
}

bool WebFrame::shouldGoToHistoryItem(HistoryItem*) const
{
    LOG_NOIMPL();
    return true;
}

void WebFrame::saveScrollPositionAndViewStateToItem(HistoryItem*)
{
    LOG_NOIMPL();
}

void WebFrame::saveDocumentViewToPageCache(PageCache*)
{
    LOG_NOIMPL();
}

bool WebFrame::canCachePage() const
{
    LOG_NOIMPL();
    return false;
}

PassRefPtr<DocumentLoader> WebFrame::createDocumentLoader(const ResourceRequest& request)
{
    RefPtr<WebDocumentLoader> loader = new WebDocumentLoader(request);
 
    COMPtr<WebDataSource> dataSource;
    dataSource.adoptRef(WebDataSource::createInstance(loader.get()));

    loader->setDataSource(dataSource.get());
    return loader.release();
}

void WebFrame::setMainDocumentError(DocumentLoader*, const ResourceError&)
{
    LOG_NOIMPL();
}

ResourceError WebFrame::cancelledError(const ResourceRequest&)
{
    LOG_NOIMPL();
    return ResourceError();
}

ResourceError WebFrame::cannotShowURLError(const ResourceRequest&)
{
    LOG_NOIMPL();
    return ResourceError();
}

ResourceError WebFrame::interruptForPolicyChangeError(const ResourceRequest&)
{
    LOG_NOIMPL();
    return ResourceError();
}

ResourceError WebFrame::cannotShowMIMETypeError(const ResourceResponse&)
{
    LOG_NOIMPL();
    return ResourceError();
}

ResourceError WebFrame::fileDoesNotExistError(const ResourceResponse&)
{
    LOG_NOIMPL();
    return ResourceError();
}

bool WebFrame::shouldFallBack(const ResourceError&)
{
    LOG_NOIMPL();
    return false;
}

void WebFrame::receivedData(const char* data, int length, const String& textEncoding)
{
    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    String encoding = d->frame->loader()->documentLoader()->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = textEncoding;
    d->frame->loader()->setEncoding(encoding, userChosen);

    d->frame->loader()->addData(data, length);
}

WebFramePolicyListener* WebFrame::setUpPolicyListener(WebCore::FramePolicyFunction function)
{
    ASSERT(!d->m_policyListener);
    ASSERT(!d->m_policyFunction);

    d->m_policyListener.adoptRef(WebFramePolicyListener::createInstance(d->frame));
    d->m_policyFunction = function;

    return d->m_policyListener.get();
}

void WebFrame::receivedPolicyDecision(PolicyAction action)
{
    ASSERT(d->m_policyListener);
    ASSERT(d->m_policyFunction);

    FramePolicyFunction function = d->m_policyFunction;

    d->m_policyListener = 0;
    d->m_policyFunction = 0;

    (d->frame->loader()->*function)(action);
}

void WebFrame::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    // FIXME: This should probably go through the data source.
    const String& textEncoding = loader->response().textEncodingName();

    receivedData(data, length, textEncoding);
}

void WebFrame::dispatchDecidePolicyForMIMEType(FramePolicyFunction function, const String&, const ResourceRequest&)
{
    (d->frame->loader()->*function)(PolicyUse);
}

void WebFrame::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const NavigationAction&, const ResourceRequest&, const String&)
{
    LOG_NOIMPL();

    (d->frame->loader()->*function)(PolicyUse);
}

void WebFrame::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const NavigationAction&, const ResourceRequest&)
{
    LOG_NOIMPL();

    (d->frame->loader()->*function)(PolicyUse);
}

void WebFrame::dispatchUnableToImplementPolicy(const ResourceError& error)
{
    COMPtr<IWebPolicyDelegate> policyDelegate;
    if (SUCCEEDED(d->webView->policyDelegate(&policyDelegate))) {
        COMPtr<IWebError> webError;
        webError.adoptRef(WebError::createInstance(error));
        policyDelegate->unableToImplementPolicyWithError(d->webView, webError.get(), this);
    }
}

void WebFrame::download(ResourceHandle*, const ResourceRequest&, const ResourceResponse&)
{
    LOG_NOIMPL();
}

bool WebFrame::willUseArchive(ResourceLoader*, const ResourceRequest&, const KURL&) const
{
    LOG_NOIMPL();
    return false;
}

void WebFrame::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate))) {
        COMPtr<IWebURLRequest> webURLRequest;
        webURLRequest.adoptRef(WebMutableURLRequest::createInstance(request));

        resourceLoadDelegate->identifierForInitialRequest(d->webView, webURLRequest.get(), getWebDataSource(loader), identifier);
    }
}

void WebFrame::dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, const ResourceResponse&)
{
    LOG_NOIMPL();
}

void WebFrame::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& response)
{
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate))) {
        COMPtr<IWebURLResponse> webURLResponse;
        webURLResponse.adoptRef(WebURLResponse::createInstance(response));

        resourceLoadDelegate->didReceiveResponse(d->webView, identifier, webURLResponse.get(), getWebDataSource(loader));
    }
}

void WebFrame::dispatchDidReceiveContentLength(DocumentLoader* loader, unsigned long identifier, int length)
{
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate)))
        resourceLoadDelegate->didReceiveContentLength(d->webView, identifier, length, getWebDataSource(loader));
}

void WebFrame::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long identifier)
{
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate)))
        resourceLoadDelegate->didFinishLoadingFromDataSource(d->webView, identifier, getWebDataSource(loader));
}

void WebFrame::dispatchDidFailLoading(DocumentLoader* loader, unsigned long identifier, const ResourceError&)
{
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(d->webView->resourceLoadDelegate(&resourceLoadDelegate))) {
        COMPtr<IWebError> webError;
        // FIXME: Assign error
        resourceLoadDelegate->didFailLoadingWithError(d->webView, identifier, webError.get(), getWebDataSource(loader));
    }
}

bool WebFrame::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int)
{
    LOG_NOIMPL();
    return false;
}

void WebFrame::dispatchDidFailProvisionalLoad(const ResourceError& error)
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {
        COMPtr<IWebError> webError;
        webError.adoptRef(WebError::createInstance(error));
        frameLoadDelegate->didFailProvisionalLoadWithError(d->webView, webError.get(), this);
    }
}

void WebFrame::dispatchDidFailLoad(const ResourceError& error)
{
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {
        COMPtr<IWebError> webError;
        webError.adoptRef(WebError::createInstance(error));
        frameLoadDelegate->didFailLoadWithError(d->webView, webError.get(), this);
    }
}

Frame* WebFrame::dispatchCreatePage()
{
    COMPtr<IWebUIDelegate> ui;

    if (SUCCEEDED(d->webView->uiDelegate(&ui))) {
        COMPtr<IWebView> newWebView;

        if (SUCCEEDED(ui->createWebViewWithRequest(d->webView, 0, &newWebView))) {
            COMPtr<IWebFrame> mainFrame;

            if (SUCCEEDED(newWebView->mainFrame(&mainFrame))) {
                COMPtr<WebFrame> mainFrameImpl;

                if (SUCCEEDED(mainFrame->QueryInterface(IID_WebFrame, (void**)&mainFrameImpl)))
                    return mainFrameImpl->impl();
            }
        }
    }
    return 0;
}

void WebFrame::postProgressStartedNotification()
{
    static BSTR progressStartedName = SysAllocString(WebViewProgressStartedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressStartedName, static_cast<IWebView*>(d->webView), 0);
}

void WebFrame::postProgressEstimateChangedNotification()
{
    static BSTR progressEstimateChangedName = SysAllocString(WebViewProgressEstimateChangedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressEstimateChangedName, static_cast<IWebView*>(d->webView), 0);
}

void WebFrame::postProgressFinishedNotification()
{
    static BSTR progressFinishedName = SysAllocString(WebViewProgressFinishedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressFinishedName, static_cast<IWebView*>(d->webView), 0);
}

void WebFrame::incrementProgress(unsigned long, const ResourceResponse&)
{
    LOG_NOIMPL();
}

void WebFrame::incrementProgress(unsigned long, const char*, int)
{
    LOG_NOIMPL();
}

void WebFrame::completeProgress(unsigned long)
{
    LOG_NOIMPL();
}

void WebFrame::startDownload(const ResourceRequest&)
{
    LOG_NOIMPL();
}

void WebFrame::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&)
{
    LOG_NOIMPL();
}

void WebFrame::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&)
{
    LOG_NOIMPL();
}
