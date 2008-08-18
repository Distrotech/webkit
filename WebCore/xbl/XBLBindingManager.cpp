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
#include "XBLBindingManager.h"

#if ENABLE(XBL)

#include "CSSStyleSheet.h"
#include "XBLBinding.h"

namespace WebCore {

XBLBindingManager* XBLBindingManager::sharedInstance()
{
    static XBLBindingManager manager;
    return &manager;
}

XBLBindingManager::XBLBindingManager()
{
}

void XBLBindingManager::addBinding(Element* boundElement, const String& uri)
{
    Vector<XBLBinding*>* elementBindings = m_bindings.get(boundElement);
    if (!elementBindings) {
        elementBindings = new Vector<XBLBinding*>();
        m_bindings.add(boundElement, elementBindings);
    }

    // CSS attachment might call us twice, so allow only one binding with the same URI per bound element.
    for (Vector<XBLBinding*>::iterator it = elementBindings->begin(); it != elementBindings->end(); ++it)
        if ((*it)->uri() == uri)
            return;

    elementBindings->append(new XBLBinding(boundElement, uri));
}

void XBLBindingManager::removeBinding(Element* boundElement, const String& uri)
{
    Vector<XBLBinding*>* elementBindings = m_bindings.get(boundElement);
    if (elementBindings) {
        for (Vector<XBLBinding*>::iterator it = elementBindings->begin(); it != elementBindings->end(); ++it)
            if ((*it)->uri() == uri) {
                size_t position = it - elementBindings->begin();
                delete elementBindings->at(position);
                elementBindings->remove(position);
                return;
            }
    }
}

bool XBLBindingManager::hasBinding(Element* boundElement, const String& uri)
{
    Vector<XBLBinding*>* elementBindings = m_bindings.get(boundElement);
    if (elementBindings)
        for (Vector<XBLBinding*>::iterator it = elementBindings->begin(); it != elementBindings->end(); ++it)
            if ((*it)->uri() == uri)
                return true;

    return false;
}

void XBLBindingManager::removeAllBindings(Element* boundElement)
{
    Vector<XBLBinding*>* elementBindings = m_bindings.take(boundElement);
    if (elementBindings)
        WTF::deleteAllValues(*elementBindings);

    delete elementBindings;
}


void XBLBindingManager::addBindingSheet(Document* document, PassRefPtr<CSSStyleSheet> sheet)
{
    StyleSheetVector* documentSheets = m_bindingSheets.get(document);
    if (!documentSheets) {
        documentSheets = new StyleSheetVector();
        m_bindingSheets.add(document, documentSheets);
    }
    documentSheets->append(sheet);
}

void XBLBindingManager::removeBindingSheet(Document* document, PassRefPtr<CSSStyleSheet> sheet)
{
    StyleSheetVector* documentSheets = m_bindingSheets.get(document);

    // documentSheets is null if no sheets were added for this document.
    if (documentSheets) {
        for (StyleSheetVector::iterator it = documentSheets->begin(); it != documentSheets->end(); ++it)
            if (*it == sheet) {
                documentSheets->remove(it - documentSheets->begin());
                return;
            }
    }

    // If we arrive here, it means that we did not find the sheet to remove, which
    // should NEVER occur.
    ASSERT_NOT_REACHED();
}

void XBLBindingManager::clearBindingSheets(Document* document)
{
    delete m_bindingSheets.take(document);
}

StyleSheetVector* XBLBindingManager::getBindingSheets(Document* document)
{
    return m_bindingSheets.get(document);
}

};

#endif // ENABLE(XBL)
