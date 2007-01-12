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

#include "Chrome.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FormData.h"
#include "FrameLoadRequest.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "FrameWin.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElement.h"
#include "HTMLNames.h"
#include "kjs_proxy.h"
#include "kjs_binding.h"
#include "kjs_window.h"
#include "Page.h"
#include "PluginDatabaseWin.h"
#include "PluginViewWin.h"
#include "MimeTypeRegistry.h"
#include "Settings.h"

namespace WebCore {

using namespace HTMLNames;

void FrameLoader::didChangeTitle(DocumentLoader* loader)
{
    m_client->didChangeTitle(loader);

    // The title doesn't get communicated to the WebView until we are committed.
    if (loader->isCommitted()) {
        // FIXME: This needs a Windows equivalent of canonicalURL
        // Must update the entries in the back-forward list too.
        // This must go through the WebFrame because it has the right notion of the current b/f item.
        m_client->setTitle(loader->title(), loader->urlForHistory());
        m_client->setMainFrameDocumentReady(true); // update observers with new DOMDocument
        m_client->dispatchDidReceiveTitle(loader->title());
    }
}

enum ObjectContentType FrameLoader::objectContentType(const KURL& url, const String& mimeTypeIn)
{
    String mimeType = mimeTypeIn;
    if (mimeType.isEmpty()) {
        mimeType = MimeTypeRegistry::getMIMETypeForExtension(url.path().mid(url.path().findRev('.')+1));
        if(mimeType.isEmpty())
            return WebCore::ObjectContentNone;
    }

    if (mimeType.isEmpty())
        return ObjectContentFrame; // Go ahead and hope that we can display the content.

    if (MimeTypeRegistry::isSupportedImageMIMEType(mimeType))
        return WebCore::ObjectContentFrame;

    if (PluginDatabaseWin::installedPlugins()->isMIMETypeRegistered(mimeType))
        return WebCore::ObjectContentPlugin;

    if (MimeTypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return WebCore::ObjectContentFrame;

    return (ObjectContentType)0;
}

Widget* FrameLoader::createPlugin(Element* element, const KURL& url,
    const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType)
{
    return PluginDatabaseWin::installedPlugins()->
        createPluginView(Win(m_frame), element, url, paramNames, paramValues, mimeType);
}

Frame* FrameLoader::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement, const String& referrer)
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

void FrameLoader::partClearedInBegin()
{
    if (!m_frame->settings()->isJavaScriptEnabled())
        return;

    if (FrameWinClient* client = Win(m_frame)->client()) {   
        JSContextRef context = reinterpret_cast<JSContextRef>(m_frame->scriptProxy()->interpreter()->globalExec());
        JSObjectRef object = reinterpret_cast<JSObjectRef>(KJS::Window::retrieve(m_frame));

        client->windowScriptObjectAvailable(context, object);
    }
}

void FrameLoader::redirectDataToPlugin(Widget*) 
{
    // FIXME: This should eventually go away, see 4852889 for more info
}

}
