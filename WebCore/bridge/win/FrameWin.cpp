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

#include "FloatRect.h"
#include "Document.h"
#include "EditorClient.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FramePrivate.h"
#include "FrameView.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "NP_jsobject.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "Plugin.h"
#include "PluginDatabaseWin.h"
#include "PluginViewWin.h"
#include "RegularExpression.h"
#include "RenderFrame.h"
#include "RenderTableCell.h"
#include "RenderView.h"
#include "ResourceHandle.h"
#include "TextResourceDecoder.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include "GraphicsContext.h"
#include "Settings.h"

#if PLATFORM(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

using std::min;
using namespace KJS::Bindings;

namespace WebCore {

using namespace HTMLNames;

FrameWin::FrameWin(Page* page, HTMLFrameOwnerElement* ownerElement, FrameWinClient* frameWinClient, FrameLoaderClient* frameLoaderClient)
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
    loader()->cancelAndClear();
}

Vector<IntRect> FrameWin::computePageRects(const IntRect& printRect, float userScaleFactor)
{
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
    
    return pages;
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

PassRefPtr<RootObject> FrameWin::createRootObject(void* nativeHandle, PassRefPtr<KJS::Interpreter> interpreter) 
{ 
    RefPtr<RootObject> rootObject = RootObject::create(nativeHandle, interpreter); 
    m_rootObjects.append(rootObject); 
    return rootObject.release(); 
}

void FrameWin::cleanupPluginObjects()
{
    // Delete old plug-in data structures
    KJS::JSLock;

    unsigned count = m_rootObjects.size();
    for (unsigned i = 0; i < count; i++)
        m_rootObjects[i]->invalidate();
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
    ASSERT(settings()->isJavaScriptEnabled());
    if (!m_bindingRoot) {
        KJS::JSLock lock;
        m_bindingRoot = KJS::Bindings::RootObject::create(0, scriptProxy()->interpreter()); // The root gets deleted by JavaScriptCore
    }
    return m_bindingRoot.get();
}

NPObject* FrameWin::windowScriptNPObject()
{
    if (!m_windowScriptNPObject) {
        if (settings()->isJavaScriptEnabled()) {
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

// Either get cached regexp or build one that matches any of the labels.
// The regexp we build is of the form:  (STR1|STR2|STRN)
static RegularExpression *regExpForLabels(const Vector<String>& labels)
{
    // REVIEW- version of this call in FrameMac.mm caches based on the NSArray ptrs being
    // the same across calls.  We can't do that.

    static RegularExpression wordRegExp = RegularExpression("\\w");
    RegularExpression *result;
    DeprecatedString pattern("(");
    unsigned int numLabels = labels.size();
    unsigned int i;
    for (i = 0; i < numLabels; i++) {
        DeprecatedString label = labels[i].deprecatedString();

        bool startsWithWordChar = false;
        bool endsWithWordChar = false;
        if (label.length() != 0) {
            startsWithWordChar = wordRegExp.search(label.at(0)) >= 0;
            endsWithWordChar = wordRegExp.search(label.at(label.length() - 1)) >= 0;
        }
        
        if (i != 0)
            pattern.append("|");
        // Search for word boundaries only if label starts/ends with "word characters".
        // If we always searched for word boundaries, this wouldn't work for languages
        // such as Japanese.
        if (startsWithWordChar) {
            pattern.append("\\b");
        }
        pattern.append(label);
        if (endsWithWordChar) {
            pattern.append("\\b");
        }
    }
    pattern.append(")");
    return new RegularExpression(pattern, false);
}

String FrameWin::searchForLabelsAboveCell(RegularExpression* regExp, HTMLTableCellElement* cell)
{
    RenderTableCell* cellRenderer = static_cast<RenderTableCell*>(cell->renderer());

    if (cellRenderer && cellRenderer->isTableCell()) {
        RenderTableCell* cellAboveRenderer = cellRenderer->table()->cellAbove(cellRenderer);

        if (cellAboveRenderer) {
            HTMLTableCellElement *aboveCell =
                static_cast<HTMLTableCellElement*>(cellAboveRenderer->element());

            if (aboveCell) {
                // search within the above cell we found for a match
                for (Node *n = aboveCell->firstChild(); n; n = n->traverseNextNode(aboveCell)) {
                    if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE) {
                        // For each text chunk, run the regexp
                        DeprecatedString nodeString = n->nodeValue().deprecatedString();
                        int pos = regExp->searchRev(nodeString);
                        if (pos >= 0)
                            return nodeString.mid(pos, regExp->matchedLength());
                    }
                }
            }
        }
    }
    // Any reason in practice to search all cells in that are above cell?
    return String();
}

String FrameWin::searchForLabelsBeforeElement(const Vector<String>& labels, Element* element)
{
    RegularExpression *regExp = regExpForLabels(labels);
    // We stop searching after we've seen this many chars
    const unsigned int charsSearchedThreshold = 500;
    // This is the absolute max we search.  We allow a little more slop than
    // charsSearchedThreshold, to make it more likely that we'll search whole nodes.
    const unsigned int maxCharsSearched = 600;
    // If the starting element is within a table, the cell that contains it
    HTMLTableCellElement *startingTableCell = 0;
    bool searchedCellAbove = false;

    // walk backwards in the node tree, until another element, or form, or end of tree
    int unsigned lengthSearched = 0;
    Node *n;
    for (n = element->traversePreviousNode();
         n && lengthSearched < charsSearchedThreshold;
         n = n->traversePreviousNode())
    {
        if (n->hasTagName(formTag)
            || (n->isHTMLElement()
                && static_cast<HTMLElement*>(n)->isGenericFormElement()))
        {
            // We hit another form element or the start of the form - bail out
            break;
        } else if (n->hasTagName(tdTag) && !startingTableCell) {
            startingTableCell = static_cast<HTMLTableCellElement*>(n);
        } else if (n->hasTagName(trTag) && startingTableCell) {
            String result = searchForLabelsAboveCell(regExp, startingTableCell);
            if (!result.isEmpty())
                return result;
            searchedCellAbove = true;
        } else if (n->isTextNode() && n->renderer() && n->renderer()->style()->visibility() == VISIBLE) {
            // For each text chunk, run the regexp
            DeprecatedString nodeString = n->nodeValue().deprecatedString();
            // add 100 for slop, to make it more likely that we'll search whole nodes
            if (lengthSearched + nodeString.length() > maxCharsSearched)
                nodeString = nodeString.right(charsSearchedThreshold - lengthSearched);
            int pos = regExp->searchRev(nodeString);
            if (pos >= 0)
                return nodeString.mid(pos, regExp->matchedLength());
            else
                lengthSearched += nodeString.length();
        }
    }

    // If we started in a cell, but bailed because we found the start of the form or the
    // previous element, we still might need to search the row above us for a label.
    if (startingTableCell && !searchedCellAbove) {
         return searchForLabelsAboveCell(regExp, startingTableCell);
    }
    return String();
}

String FrameWin::matchLabelsAgainstElement(const Vector<String>& labels, Element* element)
{
    DeprecatedString name = element->getAttribute(nameAttr).deprecatedString();
    // Make numbers and _'s in field names behave like word boundaries, e.g., "address2"
    name.replace(RegularExpression("[[:digit:]]"), " ");
    name.replace('_', ' ');
    
    RegularExpression *regExp = regExpForLabels(labels);
    // Use the largest match we can find in the whole name string
    int pos;
    int length;
    int bestPos = -1;
    int bestLength = -1;
    int start = 0;
    do {
        pos = regExp->search(name, start);
        if (pos != -1) {
            length = regExp->matchedLength();
            if (length >= bestLength) {
                bestPos = pos;
                bestLength = length;
            }
            start = pos+1;
        }
    } while (pos != -1);

    if (bestPos != -1)
        return name.mid(bestPos, bestLength);
    return String();
}

} // namespace WebCore
