/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "CachedXBLDocument.h"

#if ENABLE(XBL)

#include "Cache.h"
#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "TextResourceDecoder.h"
#include "XBLDocument.h"
#include "loader.h"
#include <wtf/Vector.h>

namespace WebCore {

CachedXBLDocument::CachedXBLDocument(const String& url)
    : CachedResource(url, XBLDocument)
    , m_document(0)
    , m_decoder(TextResourceDecoder::create("application/xml"))
{
    // It's XML we want.
    setAccept("text/xml, application/xml, application/xhtml+xml, text/xsl, application/rss+xml, application/atom+xml");
}

CachedXBLDocument::~CachedXBLDocument()
{
}

void CachedXBLDocument::addClient(CachedResourceClient* client)
{
    CachedResource::addClient(client);
    if (!m_loading)
        client->setXBLDocument(m_url, m_document.get());
}

void CachedXBLDocument::setEncoding(const String& encoding)
{
    m_decoder->setEncoding(encoding, TextResourceDecoder::EncodingFromHTTPHeader);
}

String CachedXBLDocument::encoding() const
{
    return m_decoder->encoding().name();
}

void CachedXBLDocument::data(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    if (!allDataReceived)
        return;
    
    ASSERT(!m_document);
    
    m_data = data;
    setEncodedSize(m_data.get() ? m_data->size() : 0);

    m_document = XBLDocument::create();
    m_document->open();
    
    m_document->write(m_decoder->decode(m_data->data(), m_data->size()));
    
    m_document->finishParsing();
    m_document->close();
    m_loading = false;
    checkNotify();
}

void CachedXBLDocument::checkNotify()
{
    if (m_loading)
        return;
    
    CachedResourceClientWalker walker(m_clients);
    while (CachedResourceClient* client = walker.next())
        client->setXBLDocument(m_url, m_document.get());
}

void CachedXBLDocument::error()
{
    m_loading = false;
    m_errorOccurred = true;
    checkNotify();
}

} // namespace WebCore

#endif // ENABLE(XBL)
