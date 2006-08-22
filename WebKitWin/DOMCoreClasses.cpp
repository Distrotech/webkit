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
#include "DOMHTMLClasses.h"

#pragma warning(push, 0)
#include "Element.h"
#include "HTMLFormElement.h"
#include "HTMLSelectElement.h"
#include "HTMLOptionElement.h"
#include "HTMLInputElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLNames.h"
#pragma warning(pop)

using namespace WebCore;
using namespace HTMLNames;

// DOMObject - IUnknown -------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMObject::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMObject))
        *ppvObject = static_cast<IDOMObject*>(this);
    else
        return WebScriptObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMNode - IUnknown ---------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMNode::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMNode))
        *ppvObject = static_cast<IDOMNode*>(this);
    else
        return DOMNode::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMNode --------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMNode::nodeName( 
    /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::nodeValue( 
    /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::setNodeValue( 
    /* [in] */ BSTR /*value*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::nodeType( 
    /* [retval][out] */ unsigned short* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::parentNode( 
    /* [retval][out] */ IDOMNode** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::childNodes( 
    /* [retval][out] */ IDOMNodeList** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::firstChild( 
    /* [retval][out] */ IDOMNode** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::lastChild( 
    /* [retval][out] */ IDOMNode** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::previousSibling( 
    /* [retval][out] */ IDOMNode** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::nextSibling( 
    /* [retval][out] */ IDOMNode** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::attributes( 
    /* [retval][out] */ IDOMNamedNodeMap** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::ownerDocument( 
    /* [retval][out] */ IDOMDocument** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::insertBefore( 
    /* [in] */ IDOMNode* /*newChild*/,
    /* [in] */ IDOMNode* /*refChild*/,
    /* [retval][out] */ IDOMNode** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::replaceChild( 
    /* [in] */ IDOMNode* /*newChild*/,
    /* [in] */ IDOMNode* /*oldChild*/,
    /* [retval][out] */ IDOMNode** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::removeChild( 
    /* [in] */ IDOMNode* /*oldChild*/,
    /* [retval][out] */ IDOMNode** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::appendChild( 
    /* [in] */ IDOMNode* /*oldChild*/,
    /* [retval][out] */ IDOMNode** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::hasChildNodes( 
    /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::cloneNode( 
    /* [in] */ BOOL /*deep*/,
    /* [retval][out] */ IDOMNode** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::normalize( void)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::isSupported( 
    /* [in] */ BSTR /*feature*/,
    /* [in] */ BSTR /*version*/,
    /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::namespaceURI( 
    /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::prefix( 
    /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::setPrefix( 
    /* [in] */ BSTR /*prefix*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::localName( 
    /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMNode::hasAttributes( 
    /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// DOMDocument - IUnknown -----------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMDocument::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMDocument))
        *ppvObject = static_cast<IDOMDocument*>(this);
    else
        return DOMNode::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMDocument ----------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMDocument::doctype( 
    /* [retval][out] */ IDOMDocumentType** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::implementation( 
    /* [retval][out] */ IDOMImplementation** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::documentElement( 
    /* [retval][out] */ IDOMElement** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::createElement( 
    /* [in] */ BSTR /*tagName*/,
    /* [retval][out] */ IDOMElement** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::createDocumentFragment( 
    /* [retval][out] */ IDOMDocumentFragment** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::createTextNode( 
    /* [in] */ BSTR /*data*/,
    /* [retval][out] */ IDOMText** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::createComment( 
    /* [in] */ BSTR /*data*/,
    /* [retval][out] */ IDOMComment** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::createCDATASection( 
    /* [in] */ BSTR /*data*/,
    /* [retval][out] */ IDOMCDATASection** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::createProcessingInstruction( 
    /* [in] */ BSTR /*target*/,
    /* [in] */ BSTR /*data*/,
    /* [retval][out] */ IDOMProcessingInstruction** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::createAttribute( 
    /* [in] */ BSTR /*name*/,
    /* [retval][out] */ IDOMAttr** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::createEntityReference( 
    /* [in] */ BSTR /*name*/,
    /* [retval][out] */ IDOMEntityReference** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::getElementsByTagName( 
    /* [in] */ BSTR /*tagName*/,
    /* [retval][out] */ IDOMNodeList** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::importNode( 
    /* [in] */ IDOMNode* /*importedNode*/,
    /* [in] */ BOOL /*deep*/,
    /* [retval][out] */ IDOMNode** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::createElementNS( 
    /* [in] */ BSTR /*namespaceURI*/,
    /* [in] */ BSTR /*qualifiedName*/,
    /* [retval][out] */ IDOMElement** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::createAttributeNS( 
    /* [in] */ BSTR /*namespaceURI*/,
    /* [in] */ BSTR /*qualifiedName*/,
    /* [retval][out] */ IDOMAttr** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::getElementsByTagNameNS( 
    /* [in] */ BSTR /*namespaceURI*/,
    /* [in] */ BSTR /*lcoalName*/,
    /* [retval][out] */ IDOMNodeList** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::getElementById( 
    /* [in] */ BSTR /*elementId*/,
    /* [retval][out] */ IDOMElement** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// DOMElement - IUnknown ------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMElement))
        *ppvObject = static_cast<IDOMElement*>(this);
    else
        return DOMNode::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMElement -----------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMElement::tagName( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::getAttribute( 
        /* [in] */ BSTR /*name*/,
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::setResult( 
        /* [in] */ BSTR /*name*/,
        /* [in] */ BSTR /*value*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::removeAttribute( 
        /* [in] */ BSTR /*name*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::getAttributeNode( 
        /* [in] */ BSTR /*name*/,
        /* [retval][out] */ IDOMAttr** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::setAttributeNode( 
        /* [in] */ IDOMAttr* /*newAttr*/,
        /* [retval][out] */ IDOMAttr** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::removeAttributeNode( 
        /* [in] */ IDOMAttr* /*oldAttr*/,
        /* [retval][out] */ IDOMAttr** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::getElementsByTagName( 
        /* [in] */ BSTR /*name*/,
        /* [retval][out] */ IDOMNodeList** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::getAttributeNS( 
        /* [in] */ BSTR /*namespaceURI*/,
        /* [in] */ BSTR /*localName*/,
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::setAttributeNS( 
        /* [in] */ BSTR /*namespaceURI*/,
        /* [in] */ BSTR /*qualifiedName*/,
        /* [in] */ BSTR /*value*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::removeAttributeNS( 
        /* [in] */ BSTR /*namespaceURI*/,
        /* [in] */ BSTR /*localName*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::getAttributeNodeNS( 
        /* [in] */ BSTR /*namespaceURI*/,
        /* [in] */ BSTR /*localName*/,
        /* [retval][out] */ IDOMAttr** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::setAttributeNodeNS( 
        /* [in] */ IDOMAttr* /*newAttr*/,
        /* [retval][out] */ IDOMAttr** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::getElementsByTagNameNS( 
        /* [in] */ BSTR /*namespaceURI*/,
        /* [in] */ BSTR /*localName*/,
        /* [retval][out] */ IDOMNodeList** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::hasAttribute( 
        /* [in] */ BSTR /*name*/,
        /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMElement::hasAttributeNS( 
        /* [in] */ BSTR /*namespaceURI*/,
        /* [in] */ BSTR /*localName*/,
        /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// DOMElement -------------------------------------------------------------

DOMElement::DOMElement(WebCore::Element* e)
: m_element(0)
{
    if (e)
        e->ref();

    m_element = e;
}

DOMElement::~DOMElement()
{
    if (m_element)
        m_element->deref();
}

IDOMElement* DOMElement::createInstance(WebCore::Element* e)
{
    if (!e)
        return 0;

    HRESULT hr;
    IDOMElement* domElement = 0;

    if (e->hasTagName(formTag)) {
        DOMHTMLFormElement* newElement = new DOMHTMLFormElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else if (e->hasTagName(selectTag)) {
        DOMHTMLSelectElement* newElement = new DOMHTMLSelectElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else if (e->hasTagName(optionTag)) {
        DOMHTMLOptionElement* newElement = new DOMHTMLOptionElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else if (e->hasTagName(inputTag)) {
        DOMHTMLInputElement* newElement = new DOMHTMLInputElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else if (e->hasTagName(textareaTag)) {
        DOMHTMLTextAreaElement* newElement = new DOMHTMLTextAreaElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else if (e->isHTMLElement()) {
        DOMHTMLElement* newElement = new DOMHTMLElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else {
        DOMElement* newElement = new DOMElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    }

    if (FAILED(hr))
        return 0;

    return domElement;
}
