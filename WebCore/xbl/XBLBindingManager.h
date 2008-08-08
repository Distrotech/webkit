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

#ifndef XBLBindingManager_h
#define XBLBindingManager_h

#include "config.h"

#if ENABLE(XBL)

#include "StyleSheetList.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace WebCore {

    class CSSStyleSheet;
    class Document;
    class Element;
    class XBLBinding;
    class String;

    class XBLBindingManager {
    public:
        static XBLBindingManager* sharedInstance();

        void addBinding(Element* boundElement, const String& uri);
        void removeBinding(Element* boundElement, const String& uri);
        bool hasBinding(Element* boundElement, const String& uri);

        // Used by Element destructor to prevent leakage.
        void removeAllBindings(Element* boundElement);

        // Binding style sheets management.
        void addBindingSheet(Document*, PassRefPtr<CSSStyleSheet>);
        void removeBindingSheet(Document*, PassRefPtr<CSSStyleSheet>);
        void clearBindingSheets(Document*);
        StyleSheetVector* getBindingSheets(Document*);

    private:
        XBLBindingManager();

        HashMap<Element*, Vector<XBLBinding>* > m_bindings;
        HashMap<Document*, StyleSheetVector*> m_bindingSheets;
    };

} // namespace WebCore

#endif // ENABLE(XBL)

#endif // XBLBindingManager_h
