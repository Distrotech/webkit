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

#ifndef XBLBinding_h
#define XBLBinding_h

#include "config.h"

#if ENABLE(XBL)

#include "CachedResourceClient.h"
#include "PlatformString.h"
#include "XBLBindingElement.h"
#include "XBLDocument.h"

namespace WebCore {

    class CachedXBLDocument;
    class Element;

    class XBLBinding : public CachedResourceClient {
    public:
        XBLBinding(Element* boundElement, const String& uri);
        ~XBLBinding();

        const String& uri() { return m_uri; }

        // From CacheResourceClient.
        virtual void setXBLDocument(const String& /*URL*/, XBLDocument*);
    private:
        void getBindingElement(Document* bindingDocument);

        // FIXME: These elements should be distributed among the prototype classes
        // and this one once they have been created.

        // Weak pointer: as the bound element is
        // responsible for cleaning its bindings.
        Element* m_boundElement;
        String m_uri;
        String m_bindingID;

        // We hold our bindingElement and the binding document.
        RefPtr<XBLBindingElement> m_bindingElement;
        CachedXBLDocument* m_cachedDocument;
        RefPtr<XBLDocument> m_bindingDocument;
    };

} // namespace WebCore

#endif // ENABLE(XBL)

#endif // XBLBindingManager_h
