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

#include "config.h"
#include "FrameWin.h"

#include <winsock2.h>
#include <windows.h>
#include "BrowserExtensionWin.h"
#include "Decoder.h"
#include "Document.h"
#include "FramePrivate.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HTMLIFrameElement.h"
#include "kjs_window.h"
#include "NP_jsobject.h"
#include "npruntime_impl.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "Plugin.h"
#include "PluginDatabaseWin.h"
#include "PluginViewWin.h"
#include "RenderFrame.h"
#include "RenderView.h"
#include "ResourceLoader.h"
#include "runtime_root.h"
#include "ScrollbarMode.h"

namespace WebCore {

using namespace HTMLNames;

FrameWin::FrameWin(Page* page, Element* ownerElement, FrameWinClient* client, EditorClient* editorClient)
    : Frame(page, ownerElement, editorClient)
    , m_client(client)
    , m_bindingRoot(0)
    , m_windowScriptNPObject(0)
{
    d->m_extension = new BrowserExtensionWin(this);
}

FrameWin::~FrameWin()
{
    setView(0);
    clearRecordedFormValues();
    if (m_client)
        m_client->stopMainResourceLoad();

    cancelAndClear();
}

void FrameWin::urlSelected(const FrameLoadRequest& request, const Event* triggeringEvent)
{
    Frame* targetFrame = tree()->find(request.m_frameName);
    bool newWindow = !targetFrame;
    FrameWinClient* client = targetFrame ? Win(targetFrame)->m_client.get() : m_client.get();
    if (client)
        client->openURL(request.m_request.url().url(), triggeringEvent, newWindow, request.lockHistory());
}

void FrameWin::submitForm(const FrameLoadRequest& request)
{
    // FIXME: this is a hack inherited from FrameMac, and should be pushed into Frame
    if (d->m_submittedFormURL == request.m_request.url())
        return;
    d->m_submittedFormURL = request.m_request.url();

    if (m_client)
        m_client->submitForm(request.m_request.httpMethod(), request.m_request.url(), &request.m_request.httpBody(), d->m_formAboutToBeSubmitted.get(), d->m_formValuesAboutToBeSubmitted);

    clearRecordedFormValues();
}

String FrameWin::userAgent() const
{
    return m_client->userAgentForURL(originalRequestURL());
}

// Set or unset the printing mode in the view.  We only toy with this if we're printing.
void FrameWin::setupRootForPrinting(bool onOrOff)
{
    if (document() && document()->printing()) {
        RenderView *root = static_cast<RenderView *>(document()->renderer());
        if (root) {
            root->setPrintingMode(onOrOff);
        }
    }
}

Vector<IntRect> FrameWin::computePageRects(const IntRect& printRect, float userScaleFactor)
{
    setupRootForPrinting(true);
    
    Vector<IntRect> pages;

    if (!document() || !view()|| !document()->renderer()) return pages;
 
    RenderView* root = static_cast<RenderView *>(document()->renderer());

    if (!root) {
        LOG_ERROR("document to be printed has no renderer");
        return pages;
    }

    if (userScaleFactor <= 0) {
        LOG_ERROR("userScaleFactor has bad value %.2f", userScaleFactor);
        return pages;
    }
    
    float ratio = (float)printRect.height() / (float)printRect.width();
 
    float pageWidth  = (float) root->docWidth();
    float pageHeight = pageWidth * ratio;

    if (pageHeight <= 0) {
        LOG_ERROR("pageHeight has bad value %.2f", pageHeight);
        return pages;
    }

    float currPageHeight = pageHeight / userScaleFactor;
    float docHeight      = root->layer()->height();
    float docWidth       = root->layer()->width();
    float currPageWidth  = pageWidth / userScaleFactor;
    
    for (float i = 0; i < docHeight; i += currPageHeight) {
        float proposedBottom = min(docHeight, i + pageHeight);
        adjustPageHeight(&proposedBottom, i, proposedBottom, i);
        currPageHeight = max(1.0f, proposedBottom - i);
       
        int x = 0;
        int y = i;
        int wide = currPageWidth;
        int high = currPageHeight;
        IntRect pageRect(x, y, wide, high);
        pages.append(pageRect);
    }
    
    setupRootForPrinting (false);
    
    return pages;
}

KURL FrameWin::originalRequestURL() const
{
    return m_client->originalRequestURL();
}

void FrameWin::runJavaScriptAlert(String const& message)
{
    String text = message;
    text.replace('\\', backslashAsCurrencySymbol());
    m_client->runJavaScriptAlert(text);
}

bool FrameWin::runJavaScriptConfirm(String const& message)
{
    String text = message;
    text.replace('\\', backslashAsCurrencySymbol());
    return m_client->runJavaScriptConfirm(text);
}

bool FrameWin::runJavaScriptPrompt(String const& message, String const& defaultValue, String& result)
{
    String modifiedMessage = message;
    modifiedMessage.replace('\\', backslashAsCurrencySymbol());
    String modifiedDefaultValue = defaultValue;
    modifiedDefaultValue.replace('\\', backslashAsCurrencySymbol());
    bool succeeded = m_client->runJavaScriptPrompt(modifiedMessage, modifiedDefaultValue, result);
    result.replace(backslashAsCurrencySymbol(), '\\');
    return succeeded;
}

// FIXME: This needs to be unified with the keyPress method on FrameMac
bool FrameWin::keyPress(const PlatformKeyboardEvent& keyEvent)
{
    bool result;
    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    Document *doc = document();
    if (!doc)
        return false;
    Node *node = doc->focusNode();
    if (!node) {
        if (doc->isHTMLDocument())
            node = doc->body();
        else
            node = doc->documentElement();
        if (!node)
            return false;
    }
    
    if (!keyEvent.isKeyUp())
        prepareForUserAction();

    result = !EventTargetNodeCast(node)->dispatchKeyEvent(keyEvent);

    // FIXME: FrameMac has a keyDown/keyPress hack here which we are not copying.

    return result;
}

bool FrameWin::tabsToLinks() const
{
    if (m_client)
        return m_client->tabsToLinks();

    return false;
}

bool FrameWin::tabsToAllControls() const
{
    return true;
}

void FrameWin::setTitle(const String &title)
{
    String text = title;
    text.replace('\\', backslashAsCurrencySymbol());

    if (m_client)
        m_client->setTitle(text);
}

void FrameWin::setStatusBarText(const String& status)
{
    String text = status;
    text.replace('\\', backslashAsCurrencySymbol());

    if (m_client)
        m_client->setStatusText(text);
}

void FrameWin::textFieldDidBeginEditing(Element* e)
{
    if (m_client)
        m_client->textFieldDidBeginEditing(e);
}

void FrameWin::textFieldDidEndEditing(Element* e)
{
    if (m_client)
        m_client->textFieldDidEndEditing(e);
}

void FrameWin::textDidChangeInTextField(Element* e)
{
    if (m_client)
        m_client->textDidChangeInTextField(e);
}

bool FrameWin::doTextFieldCommandFromEvent(Element* e, const PlatformKeyboardEvent* pke)
{
    if (m_client)
        return m_client->doTextFieldCommandFromEvent(e, pke);

    return false;
}

void FrameWin::textWillBeDeletedInTextField(Element* input)
{
    if (m_client)
        m_client->textWillBeDeletedInTextField(input);
}

void FrameWin::textDidChangeInTextArea(Element* e)
{
    if (m_client)
        m_client->textDidChangeInTextArea(e);
}

void FrameWin::didFirstLayout()
{
    if (m_client)
        m_client->didFirstLayout();
}

void FrameWin::handledOnloadEvents()
{
    if (m_client)
        m_client->handledOnloadEvents();
}

enum WebCore::ObjectContentType FrameWin::objectContentType(const KURL& url, const String& mimeType)
{
    if (mimeType.isEmpty())
        // FIXME: Guess the MIME type from the url extension
        return WebCore::ObjectContentNone;

    if (PluginDatabaseWin::installedPlugins()->isMIMETypeRegistered(mimeType))
        return WebCore::ObjectContentPlugin;

    return (ObjectContentType)0;
}

Plugin* FrameWin::createPlugin(Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType)
{
    return new Plugin(PluginDatabaseWin::installedPlugins()->createPluginView(this, element, url, paramNames, paramValues, mimeType));
}

Frame* FrameWin::createFrame(const KURL& url, const String& name, Element* ownerElement, const String& referrer)
{
    Frame* result = 0;
    if (m_client) {
        result = m_client->createFrame(url, name, ownerElement, referrer);
        if (result) {
            // Propagate the marginwidth/height and scrolling modes to the view.
            if (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag)) {
                HTMLFrameElement* frameElt = static_cast<HTMLFrameElement*>(ownerElement);
                if (frameElt->scrollingMode() == ScrollbarAlwaysOff)
                    result->view()->setScrollbarsMode(ScrollbarAlwaysOff);
                int marginWidth = frameElt->getMarginWidth();
                int marginHeight = frameElt->getMarginHeight();
                if (marginWidth != -1)
                    result->view()->setMarginWidth(marginWidth);
                if (marginHeight != -1)
                    result->view()->setMarginHeight(marginHeight);
            }
        }
    }
    return result;
}

void FrameWin::frameDetached()
{
    // FIXME: This code should be unified w/ FrameMac and moved into Frame
    detachChildren();
    tree()->parent()->tree()->removeChild(this);
}

void FrameWin::addPluginRootObject(KJS::Bindings::RootObject* root)
{
    m_rootObjects.append(root);
}

void FrameWin::cleanupPluginObjects()
{
    // Delete old plug-in data structures
    KJS::JSLock;

    unsigned count = m_rootObjects.size();
    for (unsigned i = 0; i < count; i++)
        m_rootObjects[i]->removeAllNativeReferences();
    m_rootObjects.clear();

    m_bindingRoot = 0;

    if (m_windowScriptNPObject) {
        // Call _NPN_DeallocateObject() instead of _NPN_ReleaseObject() so that we don't leak if a plugin fails to release the window
        // script object properly.
        // This shouldn't cause any problems for plugins since they should have already been stopped and destroyed at this point.
        _NPN_DeallocateObject(m_windowScriptNPObject);
        m_windowScriptNPObject = 0;
    }
}

KJS::Bindings::RootObject* FrameWin::bindingRootObject()
{
    ASSERT(jScriptEnabled());
    if (!m_bindingRoot) {
        KJS::JSLock lock;
        m_bindingRoot = new KJS::Bindings::RootObject(0); // The root gets deleted by JavaScriptCore
        KJS::JSObject* win = KJS::Window::retrieveWindow(this);
        m_bindingRoot->setRootObjectImp(win);
        m_bindingRoot->setInterpreter(jScript()->interpreter());
        addPluginRootObject(m_bindingRoot);
    }
    return m_bindingRoot;
}

NPObject* FrameWin::windowScriptNPObject()
{
    if (!m_windowScriptNPObject) {
        if (jScriptEnabled()) {
            // JavaScript is enabled, so there is a JavaScript window object. Return an NPObject bound to the window
            // object.
            KJS::JSObject* win = KJS::Window::retrieveWindow(this);
            ASSERT(win);
            m_windowScriptNPObject = _NPN_CreateScriptObject(0, win, bindingRootObject(), bindingRootObject());
        } else {
            // JavaScript is not enabled, so we cannot bind the NPObject to the JavaScript window object.
            // Instead, we create an NPObject of a different class, one which is not bound to a JavaScript object.
            m_windowScriptNPObject = _NPN_CreateNoScriptObject();
        }
    }

    return m_windowScriptNPObject;
}

KJS::Bindings::Instance* FrameWin::getObjectInstanceForWidget(Widget* widget) 
{
    // FIXME: Ideally we'd assert that the widget is a plugin view here but that can't be done without changing the header in the open source tree
    return static_cast<PluginViewWin*>(widget)->bindingInstance();    
}

KJS::Bindings::Instance* FrameWin::getEmbedInstanceForWidget(Widget* widget)
{
    // FIXME: Ideally we'd assert that the widget is a plugin view here but that can't be done without changing the header in the open source tree
    return static_cast<PluginViewWin*>(widget)->bindingInstance();
}

// FIXME: These methods should really call through to a PageClient rather than using FrameClient.
IntRect FrameWin::windowResizerRect() const
{
    return m_client->windowResizerRect();
}

void FrameWin::addToDirtyRegion(const IntRect& dirtyRect)
{
    m_client->addToDirtyRegion(dirtyRect);
}

void FrameWin::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    m_client->scrollBackingStore(dx, dy, scrollViewRect, clipRect);
}

void FrameWin::updateBackingStore()
{
    m_client->updateBackingStore();
}

} // namespace WebCore
