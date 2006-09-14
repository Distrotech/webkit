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

#include "DOMHTMLClasses.h"

#pragma warning(push, 0)
#include <WebCore/dom/Element.h>
#include <WebCore/html/HTMLFormElement.h>
#include <WebCore/html/HTMLSelectElement.h>
#include <WebCore/html/HTMLOptionElement.h>
#include <WebCore/html/HTMLInputElement.h>
#include <WebCore/html/HTMLTextAreaElement.h>
#include <WebCore/DerivedSources/HTMLNames.h>
#pragma warning(pop)

using namespace WebCore;
using namespace HTMLNames;

// DOMHTMLCollection - IUnknown -----------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLCollection::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMHTMLCollection))
        *ppvObject = static_cast<IDOMHTMLCollection*>(this);
    else
        return DOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLCollection ----------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLCollection::length( 
    /* [retval][out] */ UINT* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMHTMLCollection::item( 
    /* [in] */ UINT /*index*/,
    /* [retval][out] */ IDOMNode** /*node*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMHTMLCollection::namedItem( 
    /* [in] */ BSTR /*name*/,
    /* [retval][out] */ IDOMNode** /*node*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// DOMHTMLOptionsCollection - IUnknown ----------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLOptionsCollection::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMHTMLOptionsCollection))
        *ppvObject = static_cast<IDOMHTMLOptionsCollection*>(this);
    else
        return DOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLOptionsCollection ---------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLOptionsCollection::length( 
    /* [retval][out] */ unsigned int* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMHTMLOptionsCollection::setLength( 
    /* [in] */ unsigned int /*length*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMHTMLOptionsCollection::item( 
    /* [in] */ unsigned int /*index*/,
    /* [retval][out] */ IDOMNode** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMHTMLOptionsCollection::namedItem( 
    /* [in] */ BSTR /*name*/,
    /* [retval][out] */ IDOMNode* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// DOMHTMLDocument - IUnknown -------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLDocument::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMHTMLDocument))
        *ppvObject = static_cast<IDOMHTMLDocument*>(this);
    else
        return DOMDocument::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLDocument ------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLDocument::title( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::setTitle( 
        /* [in] */ BSTR /*title*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::referrer( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::domain( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::URL( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::body( 
        /* [retval][out] */ IDOMHTMLElement* /*bodyElement*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::setBody( 
        /* [in] */ IDOMHTMLElement* /*body*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::images( 
        /* [retval][out] */ IDOMHTMLCollection** /*collection*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::applets( 
        /* [retval][out] */ IDOMHTMLCollection** /*collection*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::links( 
        /* [retval][out] */ IDOMHTMLCollection** /*collection*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::forms( 
        /* [retval][out] */ IDOMHTMLCollection** /*collection*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::anchors( 
        /* [retval][out] */ IDOMHTMLCollection** /*collection*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::cookie( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::setCookie( 
        /* [in] */ BSTR /*cookie*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::open( void)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::close( void)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::write( 
        /* [in] */ BSTR /*text*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::writeln( 
        /* [in] */ BSTR /*text*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::getElementById_( 
        /* [in] */ BSTR /*elementId*/,
        /* [retval][out] */ IDOMElement** /*element*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLDocument::getElementsByName( 
        /* [in] */ BSTR /*elementName*/,
        /* [retval][out] */ IDOMNodeList** /*nodeList*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// DOMHTMLElement - IUnknown --------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMHTMLElement))
        *ppvObject = static_cast<IDOMHTMLElement*>(this);
    else
        return DOMElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLElement -------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLElement::idName( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLElement::setIdName( 
        /* [in] */ BSTR /*idName*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLElement::title( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLElement::setTitle( 
        /* [in] */ BSTR /*title*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLElement::lang( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLElement::setLang( 
        /* [in] */ BSTR /*lang*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLElement::dir( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLElement::setDir( 
        /* [in] */ BSTR /*dir*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLElement::className( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLElement::setClassName( 
        /* [in] */ BSTR /*className*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// DOMHTMLFormElement - IUnknown ----------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMHTMLFormElement))
        *ppvObject = static_cast<IDOMHTMLFormElement*>(this);
    else
        return DOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLFormElement ---------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::elements( 
        /* [retval][out] */ IDOMHTMLCollection* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::length( 
        /* [retval][out] */ int* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::name( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::setName( 
        /* [in] */ BSTR /*name*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::acceptCharset( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::setAcceptCharset( 
        /* [in] */ BSTR /*acceptCharset*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::action( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::setAction( 
        /* [in] */ BSTR /*action*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::encType( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::setEnctype( 
        /* [retval][out] */ BSTR* /*encType*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::method( 
        /* [retval][out] */ BSTR* /*method*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::setMethod( 
        /* [in] */ BSTR /*method*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::target( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::setTarget( 
        /* [in] */ BSTR /*target*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::submit( void)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLFormElement::reset( void)
{
    DebugBreak();
    return E_NOTIMPL;
}

// DOMHTMLSelectElement - IUnknown ----------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMHTMLSelectElement))
        *ppvObject = static_cast<IDOMHTMLSelectElement*>(this);
    else if (IsEqualGUID(riid, IID_IFormsAutoFillTransitionSelect))
        *ppvObject = static_cast<IFormsAutoFillTransitionSelect*>(this);
    else
        return DOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLSelectElement -------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::type( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::selectedIndex( 
        /* [retval][out] */ int* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::setSelectedIndx( 
        /* [in] */ int /*selectedIndex*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::value( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::setValue( 
        /* [in] */ BSTR /*value*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::length( 
        /* [retval][out] */ int* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::form( 
        /* [retval][out] */ IDOMHTMLFormElement** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::options( 
        /* [retval][out] */ IDOMHTMLOptionsCollection** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::disabled( 
        /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::setDisabled( 
        /* [in] */ BOOL /*disabled*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::multiple( 
        /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::setMultiple( 
        /* [in] */ BOOL /*multiple*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::name( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::setName( 
        /* [in] */ BSTR /*name*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::size( 
        /* [retval][out] */ int* /*size*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::setSize( 
        /* [in] */ int /*size*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::tabIndex( 
        /* [retval][out] */ int* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::setTabIndex( 
        /* [in] */ int /*tabIndex*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::add( 
        /* [in] */ IDOMHTMLElement* /*element*/,
        /* [in] */ IDOMHTMLElement* /*before*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::remove( 
        /* [in] */ int /*index*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
// DOMHTMLSelectElement - IFormsAutoFillTransitionSelect ----------------------

HRESULT STDMETHODCALLTYPE DOMHTMLSelectElement::activateItemAtIndex( 
    /* [in] */ int /*index*/)
{
    DebugBreak();
    return E_NOTIMPL;    
}

// DOMHTMLOptionElement - IUnknown --------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMHTMLOptionElement))
        *ppvObject = static_cast<IDOMHTMLOptionElement*>(this);
    else
        return DOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLOptionElement -------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::form( 
        /* [retval][out] */ IDOMHTMLFormElement** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::defaultSelected( 
        /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::setDefaultSelected( 
        /* [in] */ BOOL /*defaultSelected*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::text( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::index( 
        /* [retval][out] */ int* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::disabled( 
        /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::setDisabled( 
        /* [in] */ BOOL /*disabled*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::label( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::setLabel( 
        /* [in] */ BSTR /*label*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::selected( 
        /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::setSelected( 
        /* [in] */ BOOL /*selected*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::value( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLOptionElement::setValue( 
        /* [in] */ BSTR /*value*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// DOMHTMLInputElement - IUnknown ----------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMHTMLInputElement))
        *ppvObject = static_cast<IDOMHTMLInputElement*>(this);
    else if (IsEqualGUID(riid, IID_IFormsAutoFillTransition))
        *ppvObject = static_cast<IFormsAutoFillTransition*>(this);
    else
        return DOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLInputElement --------------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::defaultValue( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setDefaultValue( 
        /* [in] */ BSTR /*val*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::defaultChecked( 
        /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setDefaultChecked( 
        /* [in] */ BSTR /*checked*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::form( 
        /* [retval][out] */ IDOMHTMLElement* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::accept( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setAccept( 
        /* [in] */ BSTR /*accept*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::accessKey( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setAccessKey( 
        /* [in] */ BSTR /*key*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::align( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setAlign( 
        /* [in] */ BSTR /*align*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::alt( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setAlt( 
        /* [in] */ BSTR /*alt*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::checked( 
        /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setChecked( 
        /* [in] */ BOOL /*checked*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::disabled( 
        /* [retval][out] */ BOOL* result)
{
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    *result = inputElement->disabled() ? TRUE : FALSE;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setDisabled( 
        /* [in] */ BOOL /*disabled*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::maxLength( 
        /* [retval][out] */ int* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setMaxLength( 
        /* [in] */ int /*maxLength*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::name( 
        /* [retval][out] */ BSTR* /*name*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setName( 
        /* [in] */ BSTR /*name*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::readOnly( 
        /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setReadOnly( 
        /* [in] */ BOOL /*readOnly*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::size( 
        /* [retval][out] */ unsigned int* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setSize( 
        /* [in] */ unsigned int /*size*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::src( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setSrc( 
        /* [in] */ BSTR /*src*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::tabIndex( 
        /* [retval][out] */ int* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setTabIndex( 
        /* [in] */ int /*tabIndex*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::type( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setType( 
        /* [in] */ BSTR /*type*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::useMap( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setUseMap( 
        /* [in] */ BSTR /*useMap*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::value( 
        /* [retval][out] */ BSTR* result)
{
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    WebCore::String valueString = inputElement->value();
    *result = SysAllocStringLen(valueString.characters(), valueString.length());
    if (valueString.length() && !*result)
        return E_OUTOFMEMORY;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setValue( 
        /* [in] */ BSTR value)
{
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    inputElement->setValue(String((UChar*) value, SysStringLen(value)));
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::select( void)
{
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    inputElement->select();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::click( void)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setSelectionStart( 
    /* [in] */ long start)
{
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    inputElement->setSelectionStart(start);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::selectionStart( 
    /* [retval][out] */ long *start)
{
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    *start = inputElement->selectionStart();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setSelectionEnd( 
    /* [in] */ long end)
{
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    inputElement->setSelectionEnd(end);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::selectionEnd( 
    /* [retval][out] */ long *end)
{
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    *end = inputElement->selectionEnd();
    return S_OK;
}

// DOMHTMLInputElement -- IFormsAutoFillTransition ----------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::isTextField(
    /* [retval][out] */ BOOL* result)
{
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    *result = inputElement->isTextField() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::rectOnScreen( 
    /* [retval][out] */ LPRECT /*rect*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::replaceCharactersInRange( 
    /* [in] */ int /*startTarget*/,
    /* [in] */ int /*endTarget*/,
    /* [in] */ BSTR /*replacementString*/,
    /* [in] */ int /*index*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::selectedRange( 
    /* [out] */ int* start,
    /* [out] */ int* end)
{
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    *start = inputElement->selectionStart();
    *end = inputElement->selectionEnd();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DOMHTMLInputElement::setAutofilled( 
    /* [in] */ BOOL filled)
{
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    inputElement->setAutofilled(!!filled);
    return S_OK;
}

// DOMHTMLTextAreaElement - IUnknown ----------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDOMHTMLTextAreaElement))
        *ppvObject = static_cast<IDOMHTMLTextAreaElement*>(this);
    else
        return DOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLTextAreaElement -----------------------------------------------------

HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::defaultValue( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::setDefaultValue( 
        /* [in] */ BSTR /*val*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::form( 
        /* [retval][out] */ IDOMHTMLElement* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::accessKey( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::setAccessKey( 
        /* [in] */ BSTR /*key*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::cols( 
        /* [retval][out] */ int* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::setCols( 
        /* [in] */ int /*cols*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::disabled( 
        /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::setDisabled( 
        /* [in] */ BOOL /*disabled*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::name( 
        /* [retval][out] */ BSTR* /*name*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::setName( 
        /* [in] */ BSTR /*name*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::readOnly( 
        /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::setReadOnly( 
        /* [in] */ BOOL /*readOnly*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::rows( 
        /* [retval][out] */ int* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::setRows( 
        /* [in] */ int /*rows*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::tabIndex( 
        /* [retval][out] */ int* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::setTabIndex( 
        /* [in] */ int /*tabIndex*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::type( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::value( 
        /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::setValue( 
        /* [in] */ BSTR /*value*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DOMHTMLTextAreaElement::select( void)
{
    DebugBreak();
    return E_NOTIMPL;
}
