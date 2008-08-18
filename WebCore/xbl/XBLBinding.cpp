/*
 * Copyright (C) 2008 Julien Chaffraix <jchaffraix@webkit.org>
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
#include "XBLBinding.h"

#if ENABLE(XBL)

#include "CachedXBLDocument.h"
#include "Document.h"
#include "DocLoader.h"
#include "Element.h"
#include "XBLElement.h"
#include "XBLBindingElement.h"
#include "XBLNames.h"

namespace WebCore {

XBLBinding::XBLBinding(Element* boundElement, const String& uri)
    : m_boundElement(boundElement)
    , m_uri(uri)
    , m_cachedDocument(0)
{
    String documentURL;
    int fragmentOffset = uri.find('#');
    if (fragmentOffset >= 0) {
        documentURL = uri.left(fragmentOffset);
        if (static_cast<unsigned>(fragmentOffset) + 1 < uri.length())
            m_bindingID = uri.right(uri.length() - fragmentOffset - 1);
    } else {
        documentURL = uri;
        m_bindingID = String();
    }

    if (documentURL == boundElement->document()->url())
        // The binding document is the bound document.
        getBindingElement(boundElement->document());
    else {
        // We need to get the binding document.
        m_cachedDocument = boundElement->document()->docLoader()->requestXBLDocument(documentURL);
        if (m_cachedDocument)
            m_cachedDocument->addClient(this);
     }
}

XBLBinding::~XBLBinding()
{
    if (m_cachedDocument) {
        m_cachedDocument->removeClient(this);
        m_cachedDocument = 0;
    }
}

void XBLBinding::setXBLDocument(const String& /*URL*/, XBLDocument* document)
{
    m_bindingDocument = document;

    if (m_cachedDocument)
        m_cachedDocument->removeClient(this);

    m_cachedDocument = 0;
    getBindingElement(m_bindingDocument.get());
}

void XBLBinding::getBindingElement(Document* bindingDocument)
{
    Element* bindingElement = bindingDocument->getElementById(m_bindingID);
    if (bindingElement && bindingElement->hasTagName(XBLNames::bindingTag))
            m_bindingElement = static_cast<XBLBindingElement*>(bindingElement);
    // FIXME: if no ID was given, the spec does not say what to do so for the moment
    // do nothing for now.
}

} // namespace WebCore

#endif // ENABLE(XBL)
