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

#ifndef FrameWin_H
#define FrameWin_H

#include "Frame.h"
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>

typedef const struct OpaqueJSContext* JSContextRef;
typedef struct OpaqueJSValue* JSObjectRef;

class NPObject;

namespace KJS {
    namespace Bindings {
        class RootObject;
    }
}

namespace WebCore {

class FrameWinClient {
public:
    virtual void ref() = 0;
    virtual void deref() = 0;

    virtual Frame* createFrame(const KURL&, const String& name, Element* ownerElement, const String& referrer) = 0;
    virtual void openURL(const String& URL, const Event* triggeringEvent, bool newWindow, bool lockHistory) = 0;
    virtual void stopMainResourceLoad() = 0;
    virtual void submitForm(const String& method, const KURL&, const FormData*, Element* form, HashMap<String, String>& formValues) = 0;
    virtual void setTitle(const String& title) = 0;
    virtual void setStatusText(const String& statusText) = 0;

    virtual void textFieldDidBeginEditing(Element*) = 0;
    virtual void textFieldDidEndEditing(Element*) = 0;
    virtual void textDidChangeInTextField(Element*) = 0;
    virtual bool doTextFieldCommandFromEvent(Element*, const PlatformKeyboardEvent*) = 0;
    virtual void textWillBeDeletedInTextField(Element* input) = 0;
    virtual void textDidChangeInTextArea(Element*) = 0;

    virtual void didFirstLayout() = 0;

    virtual const String& userAgentForURL(const KURL&) = 0;
    virtual const KURL& originalRequestURL() = 0;
    virtual void runJavaScriptAlert(const WebCore::String&) = 0;
    virtual bool runJavaScriptConfirm(const WebCore::String&) = 0;
    virtual bool runJavaScriptPrompt(const WebCore::String& message, const WebCore::String& defaultValue, WebCore::String& result) = 0;

    virtual bool tabsToLinks() const = 0;

    // FIXME: These four methods should really be on a PageClient.  Many others probably should be too.
    virtual IntRect windowResizerRect() const = 0;
    virtual void addToDirtyRegion(const IntRect&) = 0;
    virtual void scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect) = 0;
    virtual void updateBackingStore() = 0;

    virtual void windowScriptObjectAvailable(JSContextRef context, JSObjectRef windowObject) = 0;
};

class FrameWin : public Frame {
public:
    FrameWin(Page*, Element*, FrameWinClient*, PassRefPtr<EditorClient>);
    ~FrameWin();

    FrameWinClient* client() { return m_client.get(); }

    virtual void setStatusBarText(const String&);

    virtual void scheduleClose();

    virtual void focusWindow();
    virtual void unfocusWindow();
    
    virtual void addMessageToConsole(const String& message,  unsigned int lineNumber, const String& sourceID);

    virtual void runJavaScriptAlert(const String& message);
    virtual bool runJavaScriptConfirm(const String& message);
    virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result);
    virtual bool shouldInterruptJavaScript();
    
    virtual Range* markedTextRange() const;

    virtual String mimeTypeForFileName(const String&) const;

    virtual void markMisspellingsInAdjacentWords(const VisiblePosition&);
    virtual void markMisspellings(const Selection&);

    virtual bool lastEventIsMouseUp() const;
    
    virtual void setupRootForPrinting(bool onOrOff);
    virtual Vector<IntRect> computePageRects(const IntRect& printRect, float userScaleFactor);

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);
    
    virtual void registerCommandForUndo(PassRefPtr<WebCore::EditCommand>);
    virtual void registerCommandForRedo(PassRefPtr<WebCore::EditCommand>);
    virtual void clearUndoRedoOperations();
    virtual void issueUndoCommand();
    virtual void issueRedoCommand();
    virtual void issueCutCommand();
    virtual void issueCopyCommand();
    virtual void issuePasteCommand();
    virtual void issuePasteAndMatchStyleCommand();
    virtual void issueTransposeCommand();
    virtual void respondToChangedSelection(const Selection& oldSelection, bool closeTyping);
    virtual void respondToChangedContents(const Selection &endingSelection);
    virtual bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const;

    virtual bool tabsToLinks() const;
    virtual bool tabsToAllControls() const;

    virtual bool canPaste() const;
    virtual bool canRedo() const;
    virtual bool canUndo() const;
    virtual void print();

    bool keyPress(const PlatformKeyboardEvent&);

    virtual void textFieldDidBeginEditing(Element*);
    virtual void textFieldDidEndEditing(Element*);
    virtual void textDidChangeInTextField(Element*);
    virtual bool doTextFieldCommandFromEvent(Element*, const PlatformKeyboardEvent*);
    virtual void textWillBeDeletedInTextField(Element* input);
    virtual void textDidChangeInTextArea(Element*);

    void addPluginRootObject(KJS::Bindings::RootObject* root);
    KJS::Bindings::RootObject* bindingRootObject();
    NPObject* windowScriptNPObject();

    IntRect windowResizerRect() const;
    
    void addToDirtyRegion(const IntRect&);
    void scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect);
    void updateBackingStore();

private:
    virtual void cleanupPluginObjects();
    
    RefPtr<FrameWinClient> m_client;

    KJS::Bindings::RootObject* m_bindingRoot; // The root object used for objects
                                              // bound outside the context of a plugin.
    Vector<KJS::Bindings::RootObject*> m_rootObjects;
    NPObject* m_windowScriptNPObject;
};

inline FrameWin* Win(Frame* frame) { return static_cast<FrameWin*>(frame); }
inline const FrameWin* Win(const Frame* frame) { return static_cast<const FrameWin*>(frame); }

}

#endif
