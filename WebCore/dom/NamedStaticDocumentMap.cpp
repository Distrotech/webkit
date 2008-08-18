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
#include "NamedStaticDocumentMap.h"

#if ENABLE(XBL)

#include "PlatformString.h"
#include "QualifiedName.h"
#include "ExceptionCode.h"

namespace WebCore {

NamedStaticDocumentMap::NamedStaticDocumentMap()
{
}

NamedStaticDocumentMap::~NamedStaticDocumentMap()
{
    m_documents.clear();
}

PassRefPtr<Node> NamedStaticDocumentMap::getNamedItem(const String& documentURI) const
{
    for (Vector<RefPtr<Document> >::const_iterator it = m_documents.begin(); it != m_documents.end(); ++it)
        if ((*it)->url() == documentURI)
            return (*it);
    return 0;
}

PassRefPtr<Node> NamedStaticDocumentMap::removeNamedItem(const String& /*name*/, ExceptionCode& ec)
{
    // For all modification, we throw NO_MODIFICATIOM_ALLOWED_ERR.
    ec = NO_MODIFICATION_ALLOWED_ERR;
    return 0;
}

PassRefPtr<Node> NamedStaticDocumentMap::getNamedItemNS(const String& namespaceURI, const String& documentURI) const
{
    // According to the specification, we have to use 'null' as the namespace & prefix and the document URL
    // as the name.
    if (!namespaceURI.isNull())
        return 0;

    return getNamedItem(documentURI);
}

PassRefPtr<Node> NamedStaticDocumentMap::removeNamedItemNS(const String& /*namespaceURI*/, const String& /*localName*/, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
    return 0;
}

PassRefPtr<Node> NamedStaticDocumentMap::getNamedItem(const QualifiedName& documentURI) const
{
    if (!documentURI.namespaceURI().isNull() || !documentURI.prefix().isNull())
        return 0;

    return getNamedItem(documentURI.localName());
}

PassRefPtr<Node> NamedStaticDocumentMap::removeNamedItem(const QualifiedName& /*documentURI*/, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
    return 0;
}

PassRefPtr<Node> NamedStaticDocumentMap::setNamedItem(Node*, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
    return 0;
}

PassRefPtr<Node> NamedStaticDocumentMap::item(unsigned index) const
{
    if (index >= m_documents.size())
        return 0;

    return m_documents.at(index);
}

void NamedStaticDocumentMap::addItem(PassRefPtr<Document> document)
{
    m_documents.append(document);
}

} // namespace WebCore

#endif // ENABLE(XBL)
