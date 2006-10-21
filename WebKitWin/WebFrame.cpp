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

#include "DOMCoreClasses.h"
#include "IWebHistory.h"
#include "IWebHistoryItemPrivate.h"
#include "IWebFrameLoadDelegatePrivate.h"
#include "IWebFormDelegate.h"
#include "IWebUIDelegatePrivate.h"
#include "WebMutableURLRequest.h"
#include "WebEditorClient.h"
#include "WebFrame.h"
#include "WebHistory.h"
#include "WebKit.h"
#include "WebView.h"
#include "WebDataSource.h"
#include "WebHistoryItem.h"
#include "WebURLResponse.h"

#pragma warning( push, 0 )
#include <WebCore/platform/win/BString.h>
#include <WebCore/loader/Cache.h>
#include <WebCore/page/DOMWindow.h>
#include <WebCore/dom/Document.h>
#include <WebCore/dom/DOMImplementation.h>
#include <WebCore/dom/Event.h>
#include <WebCore/dom/KeyboardEvent.h>
#include <WebCore/dom/MouseRelatedEvent.h>
#include <WebCore/page/FrameView.h>
#include <WebCore/bridge/win/FrameWin.h>
#include <WebCore/platform/GraphicsContext.h>
#include <WebCore/html/HTMLFormElement.h>
#include <WebCore/html/HTMLGenericFormElement.h>
#include <WebCore/html/HTMLInputElement.h>
#include <WebCore/DerivedSources/HTMLNames.h>
#include <WebCore/page/FrameTree.h>
#include <WebCore/page/Page.h>
#include <WebCore/platform/PlatformKeyboardEvent.h>
#include <WebCore/platform/PlugInInfoStore.h>
#include <WebCore/rendering/RenderFrame.h>
#include <WebCore/rendering/RenderTreeAsText.h>
#include <WebCore/platform/ResourceLoader.h>
#include <WebCore/platform/win/ResourceLoaderWin.h>
#include <wtf/MathExtras.h>
#pragma warning(pop)

using namespace WebCore;
using namespace HTMLNames;

#define FLASH_REDRAW 0

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
    WebFramePrivate() :frame(0), frameView(0), webView(0), needsLayout(false) { }
    ~WebFramePrivate() { }

    Frame* frame;
    FrameView* frameView;
    WebView* webView;
    bool needsLayout;
};

// WebFrame ----------------------------------------------------------------

WebFrame::WebFrame()
: m_refCount(0)
, d(new WebFrame::WebFramePrivate)
, m_dataSource(0)
, m_dataSourcePrivate(0)
, m_provisionalDataSource(0)
, m_quickRedirectComing(false)
, m_continueFormSubmit(false)
, m_textEncoding(0)
{
    gClassCount++;
}

WebFrame::~WebFrame()
{
    delete d;
    SysFreeString(m_textEncoding);
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
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebFrame*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrame))
        *ppvObject = static_cast<IWebFrame*>(this);
    else if (IsEqualGUID(riid, IID_IWebFramePrivate))
        *ppvObject = static_cast<IWebFramePrivate*>(this);
    else if (IsEqualGUID(riid, IID_IWebFormSubmissionListener))
        *ppvObject = static_cast<IWebFormSubmissionListener*>(this);
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
    DebugBreak();
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
    DebugBreak();
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
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadRequest( 
    /* [in] */ IWebURLRequest* request)
{
    HRESULT hr = S_OK;

    if (m_provisionalDataSource) {
        m_provisionalDataSource->Release();
        m_provisionalDataSource = 0;
        // FIXME - cancel the outstanding request?
    }

    m_provisionalDataSource = WebDataSource::createInstance(this);
    BSTR encoding = 0;
    if (SUCCEEDED(d->webView->customTextEncodingName(&encoding)) && encoding) {
        hr = m_provisionalDataSource->setOverrideEncoding(encoding);
        if (FAILED(hr))
            goto exit;
    }

    hr = m_provisionalDataSource->initWithRequest(request);

exit:
    SysFreeString(encoding);
    return hr;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadData( 
    /* [in] */ IStream* data,
    /* [in] */ BSTR mimeType,
    /* [in] */ BSTR textEncodingName,
    /* [in] */ BSTR url)
{
    if (mimeType) {
        String mimeTypeString(mimeType, SysStringLen(mimeType));
        d->frame->setResponseMIMEType(mimeTypeString);
    }

    if (textEncodingName) {
        String encodingString(textEncodingName, SysStringLen(textEncodingName));
        d->frame->setEncoding(encodingString, false);
    }

    if (url) {
        DeprecatedString urlString((DeprecatedChar*)url, SysStringLen(url));
        m_originalRequestURL = KURL(urlString);
        d->frame->begin(m_originalRequestURL);
    }
    else
        d->frame->begin();

    STATSTG stat;
    if (SUCCEEDED(data->Stat(&stat, STATFLAG_NONAME))) {
        if (!stat.cbSize.HighPart && stat.cbSize.LowPart) {
            Vector<char> dataBuffer(stat.cbSize.LowPart);
            ULONG read;
            if (SUCCEEDED(data->Read(dataBuffer.data(), (ULONG)dataBuffer.size(), &read)))
                d->frame->write(dataBuffer.data(), read);
        }
    }

    d->frame->end();
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
        d->frame->begin(m_originalRequestURL);
    }
    else
        d->frame->begin();
    d->frame->write(htmlString);
    d->frame->end();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadAlternateHTMLString( 
    /* [in] */ BSTR /*str*/,
    /* [in] */ BSTR /*baseURL*/,
    /* [in] */ BSTR /*unreachableURL*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::loadArchive( 
    /* [in] */ IWebArchive* /*archive*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::dataSource( 
    /* [retval][out] */ IWebDataSource** source)
{
    if (m_dataSource)
        m_dataSource->AddRef();
    *source = m_dataSource;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::provisionalDataSource( 
    /* [retval][out] */ IWebDataSource** source)
{
    if (m_provisionalDataSource)
        m_provisionalDataSource->AddRef();
    *source = m_provisionalDataSource;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::stopLoading( void)
{
//    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::reload( void)
{
    if (!d->frame->url().url().startsWith("javascript:", false))
        d->frame->scheduleLocationChange(d->frame->url().url(), d->frame->referrer(), true/*lock history*/, true/*userGesture*/);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebFrame::findFrameNamed( 
    /* [in] */ BSTR /*name*/,
    /* [retval][out] */ IWebFrame** /*frame*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::parentFrame( 
    /* [retval][out] */ IWebFrame** /*frame*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebFrame::childFrames( 
    /* [out] */ int* /*frameCount*/,
    /* [retval][out] */ IWebFrame*** /*frames*/)
{
    DebugBreak();
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

// IWebFormSubmissionListener ---------------------------------------------

HRESULT STDMETHODCALLTYPE WebFrame::continueSubmit(void)
{
    m_continueFormSubmit = true;
    return S_OK;
}

// WebFrame ---------------------------------------------------------------

void WebFrame::initWithWebFrameView(IWebFrameView* /*view*/, IWebView* webView, Page* page, Element* ownerElement)
{
    if (FAILED(webView->QueryInterface(CLSID_WebView, (void**)&d->webView)))
        return;
    d->webView->Release(); // don't hold the extra ref

    HWND viewWindow;
    d->webView->viewWindow(&viewWindow);


    Frame* frame = new FrameWin(page, ownerElement, this, new WebEditorClient(d->webView));
    d->frame = frame;

    FrameView* frameView = new FrameView(frame);
    d->frameView = frameView;

    frame->setSettings(d->webView->settings());
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

HRESULT WebFrame::loadDataSource(WebDataSource* dataSource)
{
    HRESULT hr = S_OK;
    BSTR url = 0;
    BSTR method = 0;

    IWebMutableURLRequest* request;
    hr = dataSource->request(&request);
    if (SUCCEEDED(hr)) {
        HRESULT hr = request->URL(&url);
        if (SUCCEEDED(hr)) {
            hr = request->HTTPMethod(&method);
            if (SUCCEEDED(hr)) {
                KURL kurl(DeprecatedString((DeprecatedChar*)url, SysStringLen(url)));
                m_originalRequestURL = kurl;
                String methodString(method, SysStringLen(method));
                const FormData* formData = 0;
                if (wcscmp(method, TEXT("GET"))) {
                    WebMutableURLRequest* requestImpl;
                    if (SUCCEEDED(request->QueryInterface(CLSID_WebMutableURLRequest, (void**)&requestImpl))) {
                        formData = requestImpl->formData();
                        requestImpl->Release();
                    }
                }
                if (formData)
                    m_loader = ResourceLoader::create(this, methodString, kurl, *formData);
                else
                    m_loader = ResourceLoader::create(this, methodString, kurl);
                if (!d->frame->document())
                    d->frame->begin(); // FIXME - the frame should do this for us
                m_loader->start(d->frame->document()->docLoader());
                IWebFrameLoadDelegate* frameLoadDelegate;
                if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {
                    frameLoadDelegate->didStartProvisionalLoadForFrame(d->webView, this);
                    frameLoadDelegate->Release();
                }
            }
        }
        request->Release();
    }

    SysFreeString(url);
    SysFreeString(method);

    return hr;
}

void WebFrame::stopMainResourceLoad()
{
    // FIXME: Frames should be able to cancel all loads, not just the main resource
    // load. This is temporary glue code that should be replaced by a real loader inside
    // WebCore.
    if (m_loader)
        m_loader->kill();
    m_loader = 0;
}

bool WebFrame::loading()
{
    return !!m_provisionalDataSource;
}

HRESULT WebFrame::goToItem(IWebHistoryItem* item, WebFrameLoadType withLoadType)
{
    HRESULT hr = S_OK;
    IWebBackForwardList* list = backForwardList();
    if (list) {
        list->goToItem(item);
        list->Release();
        hr = loadItem(item, withLoadType);
    }
    return hr;
}

HRESULT WebFrame::loadItem(IWebHistoryItem* item, WebFrameLoadType withLoadType)
{
    m_loadType = withLoadType;

    BSTR urlBStr;
    HRESULT hr = item->URLString(&urlBStr);
    if (FAILED(hr))
        return hr;

    IWebMutableURLRequest* request = WebMutableURLRequest::createInstance();
    hr = request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 0);
    if (SUCCEEDED(hr))
        loadRequest(request);

    SysFreeString(urlBStr);
    request->Release();

    return hr;
}

static unsigned long long WebSystemMainMemory()
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
    if (memSize >= 2048 * 1024 * 1000 /*1024*/)
        multiplier = 8;
    else if (memSize >= 1024 * 1024 * 1000 /*1024*/)
        multiplier = 4;
    else if (memSize >= 512 * 1024 * 1024)
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

    if (!m_dataSource)
        return E_FAIL;

    IWebURLResponse* urlResponse;
    hr = m_dataSource->response(&urlResponse);
    if (SUCCEEDED(hr) && urlResponse) {
        BSTR mimeTypeBStr;
        if (SUCCEEDED(urlResponse->MIMEType(&mimeTypeBStr))) {
            String mimeType(mimeTypeBStr, SysStringLen(mimeTypeBStr));
            *result = (!WebCore::DOMImplementation::isTextMIMEType(mimeType) && !Image::supportsType(mimeType) && !PlugInInfoStore::supportsMIMEType(mimeType));
            SysFreeString(mimeTypeBStr);
        }
        urlResponse->Release();
    }
    return hr;
}

HRESULT WebFrame::reloadAllowingStaleDataWithOverrideEncoding(BSTR encoding)
{
    HRESULT hr = S_OK;
    IWebMutableURLRequest* dsRequest = 0;
    WebMutableURLRequest* request = 0;
    WebDataSource* newDataSource = 0;
    BSTR unreachableURL = 0;

    if (!m_dataSource)
        goto exit;

    hr = m_dataSource->request(&dsRequest);
    if (FAILED(hr))
        return hr;
    request = new WebMutableURLRequest(dsRequest);
    if (!request) {
        hr = E_OUTOFMEMORY;
        goto exit;
    }

    // FIXME - from Mac code
    //if (SUCCEEDED(m_dataSource->unreachableURL(&unreachableURL)) && unreachableURL) {
    //    if (FAILED(request->setURL(unreachableURL)))
    //        goto exit;
    //}

    if (FAILED(request->setCachePolicy(WebURLRequestReturnCacheDataElseLoad)))
        goto exit;

    newDataSource = WebDataSource::createInstance(this);
    if (FAILED(newDataSource->setOverrideEncoding(encoding)))
        goto exit;

    m_loadType = WebFrameLoadTypeReloadAllowingStaleData;

    if (m_provisionalDataSource) {
        m_provisionalDataSource->Release();
        m_provisionalDataSource = 0;
        // FIXME - cancel the outstanding request?
    }
    m_provisionalDataSource = newDataSource;

    if (FAILED(newDataSource->initWithRequest(request)))
        goto exit;

    request->Release();
    request = 0;

    // hold the ref for storage in m_provisionalDataSource
    newDataSource = 0;

exit:
    if (dsRequest)
        dsRequest->Release();
    if (request)
        request->Release();
    SysFreeString(unreachableURL);
    return hr;
}

// ResourceLoaderClient

void WebFrame::receivedRedirect(ResourceLoader*, const KURL& url)
{
    if (m_provisionalDataSource) {
        IWebFrameLoadDelegate* frameLoadDelegate;
        if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate)) && frameLoadDelegate) {
            IWebMutableURLRequest* mutableRequest;
            if (SUCCEEDED(m_provisionalDataSource->request(&mutableRequest))) {
                DeprecatedString urlStr = url.url();
                BString urlBStr((LPCOLESTR)urlStr.unicode(), urlStr.length());
                mutableRequest->setURL(urlBStr);
                frameLoadDelegate->didReceiveServerRedirectForProvisionalLoadForFrame(d->webView, this);
                mutableRequest->Release();
            }
            frameLoadDelegate->Release();
        }
    }
}

void WebFrame::receivedResponse(ResourceLoader* loader, PlatformResponse platformResponse)
{
    if (m_provisionalDataSource) {
        m_dataSource = m_provisionalDataSource;
        if (m_dataSourcePrivate)
            m_dataSourcePrivate->Release();
        m_dataSource->QueryInterface(IID_IWebDataSourcePrivate, (void**)&m_dataSourcePrivate);
        m_provisionalDataSource = 0;

        IWebFrameLoadDelegate* frameLoadDelegate;
        if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {
            frameLoadDelegate->didCommitLoadForFrame(d->webView, this);
            frameLoadDelegate->Release();
        }
    }

    d->frame->didOpenURL(loader->url());
    d->frame->begin(loader->url());

    IWebFrameLoadDelegate* frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {
        IWebPreferences* preferences;
        BOOL privateBrowsingEnabled = FALSE;
        if (SUCCEEDED(d->webView->preferences(&preferences)))
            preferences->privateBrowsingEnabled(&privateBrowsingEnabled);

        DeprecatedString urlStr = loader->url().url();
        BString urlBStr((LPCOLESTR)urlStr.unicode(), urlStr.length());

        SYSTEMTIME currentTime;
        GetSystemTime(&currentTime);
        DATE visitedTime = 0;
        SystemTimeToVariantTime(&currentTime, &visitedTime);

        if (m_loadType != WebFrameLoadTypeBack && m_loadType != WebFrameLoadTypeForward && m_loadType != WebFrameLoadTypeIndexedBackForward &&
            m_loadType != WebFrameLoadTypeReload && m_loadType != WebFrameLoadTypeReloadAllowingStaleData && m_loadType != WebFrameLoadTypeInternal &&
            !m_quickRedirectComing) {
            IWebHistoryItem* item;
            item = WebHistoryItem::createInstance();
            if (SUCCEEDED(item->initWithURLString(urlBStr, 0, visitedTime))) {
                
                // add this site to the back/forward list
                IWebBackForwardList* list = backForwardList();
                if (list) {
                    list->addItem(item);
                    list->Release();
                }
                
                // add this site to the history if private browsing is disabled
                if (!privateBrowsingEnabled) {
                    WebHistory* history = webHistory();
                    if (history) {
                        history->addItemForURL(urlBStr, 0);
                        history->Release();
                    }
                }
            }
        } else {
            if (!privateBrowsingEnabled) {
                WebHistory* history = webHistory();
                if (history) {
                    IWebHistoryItem* item;
                    if (SUCCEEDED(history->itemForURL(urlBStr, &item))) {
                        IWebHistoryItemPrivate* itemPrivate;
                        if (SUCCEEDED(item->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&itemPrivate))) {
                            itemPrivate->setLastVisitedTimeInterval(visitedTime);
                            // FIXME - bumping the last visited time doesn't mark the history as changed
                            itemPrivate->Release();
                        }
                        item->Release();
                    }
                    history->Release();
                }
            }
        }
        frameLoadDelegate->Release();
    }

    m_buffer.clear();

    WebURLResponse* response = WebURLResponse::createInstance(loader, platformResponse);

    m_dataSource->setResponse(response);
    
    if (m_textEncoding) {
        SysFreeString(m_textEncoding);
        m_textEncoding = 0;
    }
    response->textEncodingName(&m_textEncoding);

    BSTR mimeType;
    if (SUCCEEDED(response->MIMEType(&mimeType))) {
        d->frame->setResponseMIMEType(String(mimeType, SysStringLen(mimeType)));
        SysFreeString(mimeType);
    }

    response->Release();
}

void WebFrame::receivedData(ResourceLoader*, const char* data, int length)
{
    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    BSTR encoding = 0;
    m_dataSourcePrivate->overrideEncoding(&encoding);
    bool userChosen = !!encoding;
    d->frame->setEncoding(WebCore::String(encoding ? encoding : m_textEncoding), userChosen);
    SysFreeString(encoding);

    d->frame->write(data, length);

    // save off the data as it is received (matching Mac WebLoader)
    // FIXME - CFNetwork Integration - we need to share this with the CFNetwork in-memory cache!
    size_t oldSize = m_buffer.size();
    m_buffer.resize(oldSize + length);
    memcpy(m_buffer.data() + oldSize, data, length);
}

void WebFrame::receivedAllData(ResourceLoader* job)
{
    if (m_provisionalDataSource) {
        m_dataSource = m_provisionalDataSource;
        m_provisionalDataSource = 0;
    }
    if (m_dataSourcePrivate) {
        m_dataSourcePrivate->Release();
        m_dataSourcePrivate = 0;
    }
    m_dataSource->QueryInterface(IID_IWebDataSourcePrivate, (void**)&m_dataSourcePrivate);

    d->frame->end();

    IWebFrameLoadDelegate* frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {
        if (!job->error())
            frameLoadDelegate->didFinishLoadForFrame(d->webView, this);
        else
            frameLoadDelegate->didFailLoadWithError(d->webView, 0/*FIXME*/, this);

        frameLoadDelegate->Release();
    }

    m_quickRedirectComing = false;
    m_loadType = WebFrameLoadTypeStandard;

    // FIXME: It seems we can have more than one main ResourceLoader per-frame. Ideally,
    // we'd keep track of all of them. However, this is all expected to change as the loader
    // in WebCore becomes more full-featured, so we'll just do the bare minimum for now.
    if (job == m_loader)
        m_loader = 0;
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

Frame* WebFrame::createFrame(const KURL& URL, const String& name, Element* ownerElement, const String& /* referrer */)
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

    webFrame->m_loadType = WebFrameLoadTypeInternal;
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
        if (SUCCEEDED(d->webView->uiDelegate(&ui))) {
            if (SUCCEEDED(ui->createWebViewWithRequest(d->webView, request, &newWebView))) {
                if (shiftPressed) {
                    // Ctrl-Option-Shift-click:  Opens a link in a new window and selects it.
                    // Ctrl-Shift-click:  Opens a link in a new tab and selects it.
                    ui->webViewShow(d->webView);
                }
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

void WebFrame::submitForm(const String& method, const KURL& url, const FormData* submitFormData, Element* form, HashMap<String, String>& formValues)
{
    // FIXME: This is a dumb implementation, doesn't handle subframes, etc.

    m_quickRedirectComing = false;
    m_loadType = WebFrameLoadTypeStandard;
    DeprecatedString urlStr = url.url();
    BString urlBStr((LPCOLESTR)urlStr.unicode(), urlStr.length());
    BString methodBStr(method);
    WebMutableURLRequest* request = WebMutableURLRequest::createInstance();
    if (SUCCEEDED(request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 0))) {
        request->setHTTPMethod(methodBStr);
        request->setFormData(submitFormData);

        m_continueFormSubmit = true;
        IWebFormDelegate* formDelegate;
        if (SUCCEEDED(d->webView->formDelegate(&formDelegate)) && formDelegate) {
            IDOMElement* ele = DOMElement::createInstance(form);
            FormValuesPropertyBag formValuesPropBag(&formValues);
            m_continueFormSubmit = false;
            formDelegate->willSubmitForm(this, this, ele, &formValuesPropBag, this);
            formDelegate->Release();
            if (ele)
                ele->Release();
        }

        if (m_continueFormSubmit)
            loadRequest(request);
    }
    request->Release();
}

void WebFrame::setTitle(const String& title)
{
    BString titleBStr(title);

    IWebFrameLoadDelegate* frameLoadDelegate;
    if (SUCCEEDED(d->webView->frameLoadDelegate(&frameLoadDelegate))) {
        frameLoadDelegate->didReceiveTitle(d->webView, titleBStr, this);
        frameLoadDelegate->Release();
    }

    BOOL privateBrowsingEnabled = FALSE;
    IWebPreferences* preferences;
    if (SUCCEEDED(d->webView->preferences(&preferences)))
        preferences->privateBrowsingEnabled(&privateBrowsingEnabled);
    if (!privateBrowsingEnabled) {
        // update title in global history
        WebHistory* history = webHistory();
        if (history) {
            IWebHistoryItem* item;
            DeprecatedString urlStr = d->frame->url().url();
            BString urlBStr = BString((LPCOLESTR)urlStr.unicode(), urlStr.length());
            if (SUCCEEDED(history->itemForURL(urlBStr, &item))) {
                IWebHistoryItemPrivate* itemPrivate;
                if (SUCCEEDED(item->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&itemPrivate))) {
                    itemPrivate->setTitle(titleBStr);
                    itemPrivate->Release();
                }
                item->Release();
            }
            history->Release();
        }
    }
    // update title in back/forward list
    IWebBackForwardList* list = backForwardList();
    if (list) {
        IWebHistoryItem* item;
        if (SUCCEEDED(list->currentItem(&item))) {
            IWebHistoryItemPrivate* itemPrivate;
            if (SUCCEEDED(item->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&itemPrivate))) {
                itemPrivate->setTitle(titleBStr);
                itemPrivate->Release();
            }
            item->Release();
        }
        list->Release();
    }
}

void WebFrame::setStatusText(const String& statusText)
{
    IWebUIDelegate* uiDelegate;
    if (SUCCEEDED(d->webView->uiDelegate(&uiDelegate))) {
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

void WebFrame::didFirstLayout()
{
    IWebFrameLoadDelegatePrivate* frameLoadDelegatePriv;
    if (SUCCEEDED(d->webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv) {
        frameLoadDelegatePriv->didFirstLayoutInFrame(d->webView, this);
        frameLoadDelegatePriv->Release();
    }
}

void WebFrame::handledOnloadEvents()
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
    return m_originalRequestURL;
}

void WebFrame::runJavaScriptAlert(const String& message)
{
    IWebUIDelegate* ui;
    if (SUCCEEDED(d->webView->uiDelegate(&ui))) {
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
    if (SUCCEEDED(d->webView->uiDelegate(&ui))) {
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
    if (SUCCEEDED(d->webView->uiDelegate(&ui))) {
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
    IWebUIDelegate* ui;
    if (SUCCEEDED(d->webView->uiDelegate(&ui))) {
        IWebUIDelegatePrivate* uiPrivate;
        if (SUCCEEDED(ui->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&uiPrivate))) {
            RECT r;
            if (SUCCEEDED(uiPrivate->webViewResizerRect(d->webView, &r)))
                return IntRect(r.left, r.top, r.right-r.left, r.bottom-r.top);
            uiPrivate->Release();
        }
        ui->Release();
    }
    return IntRect();
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
