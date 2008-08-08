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
#include "XBLBindingElement.h"

#if ENABLE(XBL)

#include "Attribute.h"
#include "CSSStyleSheet.h"
#include "Document.h"
#include "KURL.h"
#include "XBLNames.h"

namespace WebCore {

XBLBindingElement::XBLBindingElement(const QualifiedName& qName, Document* doc)
    : XBLElement(qName, doc)
    , m_bindingSheet(0)
{
}

void XBLBindingElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    const QualifiedName& attrName = attr->name();
    if (attrName == XBLNames::elementAttr) {
        String currentAttr = getAttribute(XBLNames::elementAttr);

        // Check that we are inline, in that case the bound document
        // is our document.
        // FIXME: If we have an XBLDocument, we need to get the bound document.
        Document* boundDocument = document();

        if (!currentAttr.isNull() && attr->value().isNull()) {
            ASSERT(m_bindingSheet);
            boundDocument->removeBindingSheet(m_bindingSheet.get());
            m_bindingSheet = 0;
        } else {
            if (m_bindingSheet)
                boundDocument->removeBindingSheet(m_bindingSheet.get());
            m_bindingSheet = generateBindingSheet();
            if (m_bindingSheet)
                boundDocument->addBindingSheet(m_bindingSheet);
        }
        // Update the styleSelector so that bindings are updated.
        // FIXME: is it always required?
        boundDocument->updateStyleSelector();
    } else
        XBLElement::attributeChanged(attr, preserveDecls);
}

PassRefPtr<CSSStyleSheet> XBLBindingElement::generateBindingSheet()
{
    RefPtr<CSSStyleSheet> sheet = CSSStyleSheet::create();
    String rule = getAttribute(XBLNames::elementAttr);
    rule.append(" { binding: url('");

    // FIXME: If we have an XBLDocument, we need to get the
    // real bound document.
    Document* boundDocument = document();

    rule.append(boundDocument->url().string());

    // If we have an ID tag, add it to the binding sheet to
    // match addBinding syntax.
    if (hasID()) {
        rule.append('#');
        rule.append(getIDAttribute());
    }

    rule.append("');}");

    ExceptionCode ec = 0;
    sheet->insertRule(rule, 0, ec);

    // If ec is not null, 'element' is not a valid selector, so
    // remove the newly created sheet.
    if (ec)
        sheet = 0;

    return sheet;
}

} // namespace WebCore

#endif // ENABLE(XBL)
