/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameLoader.h"

#include "Element.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "FrameWin.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElement.h"
#include "HTMLNames.h"
#include "PluginDatabaseWin.h"
#include "PluginViewWin.h"

namespace WebCore {

using namespace HTMLNames;

void FrameLoader::submitForm(const FrameLoadRequest& request, Event* event)
{
    // FIXME: We'd like to remove this altogether and fix the multiple form submission issue another way.
    // We do not want to submit more than one form from the same page,
    // nor do we want to submit a single form more than once.
    // This flag prevents these from happening; not sure how other browsers prevent this.
    // The flag is reset in each time we start handle a new mouse or key down event, and
    // also in setView since this part may get reused for a page from the back/forward cache.
    // The form multi-submit logic here is only needed when we are submitting a form that affects this frame.
    // FIXME: Frame targeting is only one of the ways the submission could end up doing something other
    // than replacing this frame's content, so this check is flawed. On the other hand, the check is hardly
    // needed any more now that we reset m_submittedFormURL on each mouse or key down event.
    Frame* target = m_frame->tree()->find(request.frameName());
    if (m_frame->tree()->isDescendantOf(target)) {
        if (m_submittedFormURL == request.resourceRequest().url())
            return;
        m_submittedFormURL = request.resourceRequest().url();
    }

    if (FrameWinClient* client = Win(m_frame)->client())
        client->submitForm(request.resourceRequest().httpMethod(),
            request.resourceRequest().url(),
            &request.resourceRequest().httpBody(),
            m_formAboutToBeSubmitted.get(),
            m_formValuesAboutToBeSubmitted);

    clearRecordedFormValues();
}

void FrameLoader::urlSelected(const FrameLoadRequest& request, Event* event)
{
    Frame* targetFrame = m_frame->tree()->find(request.frameName());
    bool newWindow = !targetFrame;
    Frame* frame = targetFrame ? targetFrame : m_frame;
    if (FrameWinClient* client = Win(frame)->client())
        client->openURL(request.resourceRequest().url().url(), event, newWindow, request.lockHistory());
}

KURL FrameLoader::originalRequestURL() const
{
    if (FrameWinClient* client = Win(m_frame)->client())
        return client->originalRequestURL();
    return KURL();
}

void FrameLoader::didFirstLayout()
{
    if (FrameWinClient* client = Win(m_frame)->client())
        client->didFirstLayout();
}

enum WebCore::ObjectContentType FrameLoader::objectContentType(const KURL& url, const String& mimeType)
{
    if (mimeType.isEmpty())
        // FIXME: Guess the MIME type from the url extension
        return WebCore::ObjectContentNone;

    if (PluginDatabaseWin::installedPlugins()->isMIMETypeRegistered(mimeType))
        return WebCore::ObjectContentPlugin;

    return (ObjectContentType)0;
}

Widget* FrameLoader::createPlugin(Element* element, const KURL& url,
    const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType)
{
    return PluginDatabaseWin::installedPlugins()->
        createPluginView(Win(m_frame), element, url, paramNames, paramValues, mimeType);
}

Frame* FrameLoader::createFrame(const KURL& url, const String& name, Element* ownerElement, const String& referrer)
{
    Frame* result = 0;
    if (FrameWinClient* client = Win(m_frame)->client()) {
        result = client->createFrame(url, name, ownerElement, referrer);
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

void FrameLoader::detachFromParent()
{
    detachChildren();
    m_frame->tree()->parent()->tree()->removeChild(m_frame);
}

Widget* FrameLoader::createJavaAppletWidget(const IntSize&, Element* element, const HashMap<String, String>& args)
{
    Vector<String> paramNames;
    Vector<String> paramValues;

    HashMap<String, String>::const_iterator end = args.end();
    for (HashMap<String, String>::const_iterator it = args.begin(); it != end; ++it) {
        paramNames.append(it->first);
        paramValues.append(it->second);
    }

    return PluginDatabaseWin::installedPlugins()->
        createPluginView(Win(m_frame), element, KURL(), paramNames, paramValues, "application/x-java-applet");
}

void FrameLoader::setTitle(const String& title)
{
    if (FrameWinClient* client = Win(m_frame)->client())
        client->setTitle(title);
}

void FrameLoader::partClearedInBegin()
{
    if (FrameWinClient* client = Win(m_frame)->client()) {   
        JSContextRef context = reinterpret_cast<JSContextRef>(scriptProxy()->interpreter()->globalExec());
        JSObjectRef object = reinterpret_cast<JSObjectRef>(KJS::Window::retrieve(this));

        client->windowScriptObjectAvailable(context, object);
    }
}


}
