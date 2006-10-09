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
    virtual void openURL(const String& URL, bool newWindow, bool lockHistory) = 0;
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
    virtual void handledOnloadEvents() = 0;

    virtual String userAgent() = 0;
    virtual void runJavaScriptAlert(const WebCore::String&) = 0;
    virtual bool runJavaScriptConfirm(const WebCore::String&) = 0;
    virtual bool runJavaScriptPrompt(const WebCore::String& message, const WebCore::String& defaultValue, WebCore::String& result) = 0;

    virtual bool tabsToLinks() const = 0;

    virtual IntRect windowResizerRect() const = 0;
};

class FrameWin : public Frame
{
public:
    FrameWin(Page*, Element*, FrameWinClient*);
    ~FrameWin();

    virtual bool openURL(const KURL&);
    virtual void openURLRequest(const ResourceRequest&);
    virtual void submitForm(const ResourceRequest&);
    virtual void urlSelected(const ResourceRequest&);

    virtual void setTitle(const String&);
    virtual void setStatusBarText(const String&);

    virtual ObjectContentType objectContentType(const KURL& url, const String& mimeType);
    virtual Plugin* createPlugin(Element*, const KURL&, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType);
    virtual Frame* createFrame(const KURL&, const String& name, Element* ownerElement, const String& referrer);
    virtual Widget* createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>& args);

    virtual void scheduleClose();

    virtual void frameDetached();

    virtual void focusWindow();
    virtual void unfocusWindow();
    
    virtual void saveDocumentState();
    virtual void restoreDocumentState();
    
    virtual void addMessageToConsole(const String& message,  unsigned int lineNumber, const String& sourceID);

    virtual void runJavaScriptAlert(const String& message);
    virtual bool runJavaScriptConfirm(const String& message);
    virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result);
    virtual bool shouldInterruptJavaScript();
    
    virtual bool locationbarVisible();
    virtual bool menubarVisible();
    virtual bool personalbarVisible();
    virtual bool statusbarVisible();
    virtual bool toolbarVisible();

    virtual void createEmptyDocument();
    virtual Range* markedTextRange() const;

    virtual String incomingReferrer() const;
    virtual String userAgent() const;

    virtual String mimeTypeForFileName(const String&) const;

    virtual void markMisspellingsInAdjacentWords(const VisiblePosition&);
    virtual void markMisspellings(const Selection&);

    virtual bool lastEventIsMouseUp() const;
    
    virtual String overrideMediaType() const;

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
    virtual void partClearedInBegin();

    virtual bool tabsToLinks() const;
    virtual bool tabsToAllControls() const;

    virtual bool canGoBackOrForward(int distance) const;
    virtual void handledOnloadEvents();

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

    virtual void didFirstLayout();

    void addPluginRootObject(KJS::Bindings::RootObject* root);
    KJS::Bindings::RootObject* bindingRootObject();
    NPObject* windowScriptNPObject();

    virtual IntRect windowResizerRect() const;

protected:
    virtual bool isLoadTypeReload();

private:
    virtual KURL originalRequestURL() const;
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
