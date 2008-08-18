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

#ifndef XBLNodeMap_h
#define XBLNodeMap_h

#include "config.h"

// This class is used only for document.bindingDocuments
// and that's why it requires XBL to be enabled.
#if ENABLE(XBL)

#include "Document.h"
#include "NamedNodeMap.h"
#include "Node.h"

namespace WebCore {

    class QualifiedName;

    class NamedStaticDocumentMap : public NamedNodeMap {

    public:
        static PassRefPtr<NamedStaticDocumentMap> create() { return adoptRef(new NamedStaticDocumentMap()); }

        virtual PassRefPtr<Node> getNamedItem(const String& name) const;
        virtual PassRefPtr<Node> removeNamedItem(const String& name, ExceptionCode&);

        virtual PassRefPtr<Node> getNamedItemNS(const String& namespaceURI, const String& localName) const;
        virtual PassRefPtr<Node> removeNamedItemNS(const String& namespaceURI, const String& localName, ExceptionCode&);

        virtual PassRefPtr<Node> getNamedItem(const QualifiedName& attrName) const;
        virtual PassRefPtr<Node> removeNamedItem(const QualifiedName& attrName, ExceptionCode&);
        virtual PassRefPtr<Node> setNamedItem(Node*, ExceptionCode&);

        virtual PassRefPtr<Node> item(unsigned index) const;
        virtual size_t length() const { return m_documents.size(); }

        // Modification methods (not exported to the DOM)

        void addItem(PassRefPtr<Document>);
        void clear() { m_documents.clear(); }

    private:
        NamedStaticDocumentMap();
        ~NamedStaticDocumentMap();

        Vector<RefPtr<Document> > m_documents;
    };

} // namespace WebCore

#endif // ENABLE(XBL)

#endif // XBLNodeMap_h
