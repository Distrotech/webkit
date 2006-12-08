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

#include "floatrect.h"
#include "Document.h"
#include "EditorClient.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FramePrivate.h"
#include "FrameView.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "NP_jsobject.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "Plugin.h"
#include "PluginDatabaseWin.h"
#include "PluginViewWin.h"
#include "RenderFrame.h"
#include "RenderView.h"
#include "ResourceHandle.h"
#include "TextResourceDecoder.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include "GraphicsContext.h"
#if PLATFORM(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

using std::min;

namespace WebCore {

using namespace HTMLNames;

FrameWin::FrameWin(Page* page, Element* ownerElement, FrameWinClient* frameWinClient, FrameLoaderClient* frameLoaderClient)
    : Frame(page, ownerElement, frameLoaderClient)
    , m_client(frameWinClient)
    , m_bindingRoot(0)
    , m_windowScriptNPObject(0)
{
}

FrameWin::~FrameWin()
{
    setView(0);
    loader()->clearRecordedFormValues();
    if (m_client)
        m_client->stopMainResourceLoad();

    loader()->cancelAndClear();
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

// FIXME: This needs to be unified with the keyEvent method on FrameMac
bool FrameWin::keyEvent(const PlatformKeyboardEvent& keyEvent)
{
    bool result;
    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    Document *doc = document();
    if (!doc)
        return false;
    Node *node = doc->focusedNode();
    if (!node) {
        if (doc->isHTMLDocument())
            node = doc->body();
        else
            node = doc->documentElement();
        if (!node)
            return false;
    }
    
#ifdef MULTIPLE_FORM_SUBMISSION_PROTECTION
    if (!keyEvent.isKeyUp())
        loader()->resetMultipleFormSubmissionProtection();
#endif

    result = !EventTargetNodeCast(node)->dispatchKeyEvent(keyEvent);

    // We want to send both a down and a press for the initial key event. (This is the behavior of other browsers)
    // To get the rest of WebCore to do this, we send a second KeyPress with "is repeat" set to true,
    // which causes it to send a press to the DOM.
    // We should do this a better way.
    if (!keyEvent.isKeyUp() && !keyEvent.isAutoRepeat()) {
        PlatformKeyboardEvent keyPressedEvent(keyEvent);
        keyPressedEvent.setIsAutoRepeat(true);
        if (!EventTargetNodeCast(node)->dispatchKeyEvent(keyPressedEvent))
            result = true;
    }

    return result;
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
    ASSERT(javaScriptEnabled());
    if (!m_bindingRoot) {
        KJS::JSLock lock;
        m_bindingRoot = new KJS::Bindings::RootObject(0); // The root gets deleted by JavaScriptCore
        KJS::JSObject* win = KJS::Window::retrieveWindow(this);
        m_bindingRoot->setRootObjectImp(win);
        m_bindingRoot->setInterpreter(scriptProxy()->interpreter());
        addPluginRootObject(m_bindingRoot);
    }
    return m_bindingRoot;
}

NPObject* FrameWin::windowScriptNPObject()
{
    if (!m_windowScriptNPObject) {
        if (javaScriptEnabled()) {
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

HBITMAP FrameWin::imageFromRect(IntRect rect)
{
#if PLATFORM(CG)
    void* bits;
    HDC hdc = CreateCompatibleDC(0);
    int w = rect.width();
    int h = rect.height();
    BITMAPINFO bmp = { { sizeof(BITMAPINFOHEADER), w, h, 1, 32 } };

    HBITMAP hbmp = CreateDIBSection(0, &bmp, DIB_RGB_COLORS, (void**)&bits, 0, 0);
    HBITMAP hbmpOld = (HBITMAP)SelectObject(hdc, hbmp);
    CGColorSpaceRef deviceRGB = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate((void*)bits, w, h,
        8, w * sizeof(RGBQUAD), deviceRGB, kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(deviceRGB);

    CGContextSaveGState(context);

    IntSize offset = view()->scrollOffset();
    rect.move(-offset.width(), -offset.height());
    rect = view()->convertToContainingWindow(rect);
    CGContextTranslateCTM(context, -rect.x(), -rect.y());

    GraphicsContext gc(context);
    view()->paint(&gc, rect);

    CGContextRelease(context);
    SelectObject(hdc, hbmpOld);
    DeleteDC(hdc);

    return hbmp;
#else
    return 0;
#endif
}

HBITMAP FrameWin::imageFromSelection(bool forceWhiteText)
{
    d->m_paintRestriction = forceWhiteText ? PaintRestrictionSelectionOnlyWhiteText : PaintRestrictionSelectionOnly;
    FloatRect fr = visibleSelectionRect();
    IntRect ir((int)fr.x(), (int)fr.y(),(int)fr.width(),(int)fr.height());

    HBITMAP hbmp = imageFromRect(ir);
    d->m_paintRestriction = PaintRestrictionNone;

    return hbmp;
}


} // namespace WebCore

