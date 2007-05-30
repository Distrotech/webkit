/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
 *
 * All rights reserved.
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
#include "HTMLFormElement.h"
#include "FrameLoaderClientWx.h"
#include "FrameLoaderTypes.h"
#include "FrameView.h"
#include "FrameTree.h"
#include "Frame.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "DocumentLoader.h"
#include "ResourceResponse.h"
#include "NotImplemented.h"
#include <stdio.h>

#include "WebView.h"
#include "WebViewPrivate.h"

namespace WebCore {

inline int wxNavTypeFromWebNavType(NavigationType type){
    if (type == NavigationTypeLinkClicked)
        return wxWEBVIEW_NAV_LINK_CLICKED;
    
    if (type == NavigationTypeFormSubmitted)
        return wxWEBVIEW_NAV_FORM_SUBMITTED;
        
    if (type == NavigationTypeBackForward)
        return wxWEBVIEW_NAV_BACK_NEXT;
        
    if (type == NavigationTypeReload)
        return wxWEBVIEW_NAV_RELOAD;
        
    if (type == NavigationTypeFormResubmitted)
        return wxWEBVIEW_NAV_FORM_RESUBMITTED;
        
    return wxWEBVIEW_NAV_OTHER;
}

FrameLoaderClientWx::FrameLoaderClientWx()
    : m_frame(0)
{
}


FrameLoaderClientWx::~FrameLoaderClientWx()
{
}

void FrameLoaderClientWx::setFrame(Frame *frame)
{
    m_frame = frame;
}

void FrameLoaderClientWx::detachFrameLoader()
{
    m_frame = 0;
}

void FrameLoaderClientWx::ref()
{
    Shared<FrameLoaderClientWx>::ref();
}

void FrameLoaderClientWx::deref()
{
    Shared<FrameLoaderClientWx>::deref();
}

bool FrameLoaderClientWx::hasWebView() const
{
    //notImplemented();
    return true;
}


bool FrameLoaderClientWx::hasFrameView() const
{
    //notImplemented();
    return true;
}


bool FrameLoaderClientWx::hasBackForwardList() const
{
    notImplemented();
    return true;
}


void FrameLoaderClientWx::resetBackForwardList()
{
    notImplemented();
}


bool FrameLoaderClientWx::provisionalItemIsTarget() const
{
    notImplemented();
    return false;
}


bool FrameLoaderClientWx::privateBrowsingEnabled() const
{
    notImplemented();
    return false;
}


void FrameLoaderClientWx::makeDocumentView()
{
    //qDebug() << "FrameLoaderClientWx::makeDocumentView" << m_frame->document();
    
//     if (!m_frame->document()) 
//         m_frame->loader()->createEmptyDocument();
}


void FrameLoaderClientWx::makeRepresentation(DocumentLoader*)
{
    // don't need this for now I think.
}


void FrameLoaderClientWx::forceLayout()
{
    notImplemented();
}


void FrameLoaderClientWx::forceLayoutForNonHTML()
{
    notImplemented();
}


void FrameLoaderClientWx::updateHistoryForCommit()
{
    notImplemented();
}


void FrameLoaderClientWx::updateHistoryForBackForwardNavigation()
{
    notImplemented();
}


void FrameLoaderClientWx::updateHistoryForReload()
{
    notImplemented();
}


void FrameLoaderClientWx::updateHistoryForStandardLoad()
{
    notImplemented();
}


void FrameLoaderClientWx::updateHistoryForInternalLoad()
{
    notImplemented();
}


void FrameLoaderClientWx::updateHistoryAfterClientRedirect()
{
    notImplemented();
}


void FrameLoaderClientWx::setCopiesOnScroll()
{
    // apparently mac specific 
}


LoadErrorResetToken* FrameLoaderClientWx::tokenForLoadErrorReset()
{
    notImplemented();
    return 0;
}


void FrameLoaderClientWx::resetAfterLoadError(LoadErrorResetToken*)
{
    notImplemented();
}


void FrameLoaderClientWx::doNotResetAfterLoadError(LoadErrorResetToken*)
{
    notImplemented();
}


void FrameLoaderClientWx::willCloseDocument()
{
    notImplemented();
}


void FrameLoaderClientWx::detachedFromParent1()
{
    notImplemented();
}


void FrameLoaderClientWx::detachedFromParent2()
{
    notImplemented();
}


void FrameLoaderClientWx::detachedFromParent3()
{
    notImplemented();
}


void FrameLoaderClientWx::detachedFromParent4()
{
    notImplemented();
}


void FrameLoaderClientWx::loadedFromCachedPage()
{
    notImplemented();
}


void FrameLoaderClientWx::dispatchDidHandleOnloadEvents()
{
    //notImplemented();
}


void FrameLoaderClientWx::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    notImplemented();
}


void FrameLoaderClientWx::dispatchDidCancelClientRedirect()
{
    notImplemented();
}


void FrameLoaderClientWx::dispatchWillPerformClientRedirect(const KURL&,
                                                            double interval,
                                                            double fireDate)
{
    notImplemented();
}


void FrameLoaderClientWx::dispatchDidChangeLocationWithinPage()
{
    notImplemented();
}


void FrameLoaderClientWx::dispatchWillClose()
{
    notImplemented();
}


void FrameLoaderClientWx::dispatchDidStartProvisionalLoad()
{
    wxWindow* target = m_frame->view()->nativeWindow();
    if (target)
    {
        wxWebViewStateChangedEvent wkEvent(target);
        wkEvent.SetState(wxWEBVIEW_STATE_NEGOTIATING);
        //wkEvent.SetURL(m_frame->loader()->documentLoader()->request().url().url());
        target->GetEventHandler()->ProcessEvent(wkEvent);
    }
}


void FrameLoaderClientWx::dispatchDidReceiveTitle(const String& title)
{
    notImplemented();
}


void FrameLoaderClientWx::dispatchDidCommitLoad()
{
    wxWindow* target = m_frame->view()->nativeWindow();
    if (target)
    {
        wxWebViewStateChangedEvent wkEvent(target);
        wkEvent.SetState(wxWEBVIEW_STATE_TRANSFERRING);
        //wkEvent.SetURL(m_frame->loader()->documentLoader()->request().url().url());
        target->GetEventHandler()->ProcessEvent(wkEvent);
    }
}

void FrameLoaderClientWx::dispatchDidFinishDocumentLoad()
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidFinishLoad()
{
    wxWindow* target = m_frame->view()->nativeWindow();
    if (target)
    {
        wxWebViewStateChangedEvent wkEvent(target);
        wkEvent.SetState(wxWEBVIEW_STATE_STOP);
        //wkEvent.SetURL(m_frame->loader()->documentLoader()->request().url().url());
        target->GetEventHandler()->ProcessEvent(wkEvent);
    }
}


void FrameLoaderClientWx::dispatchDidFirstLayout()
{
    //notImplemented();
}


void FrameLoaderClientWx::dispatchShow()
{
    notImplemented();
}


void FrameLoaderClientWx::cancelPolicyCheck()
{
    // don't need to do anything here as long as we don't start doing asyncronous policy checks
}


void FrameLoaderClientWx::dispatchWillSubmitForm(FramePolicyFunction function,
                                                 PassRefPtr<FormState>)
{
    // FIXME: This is surely too simple
    if (!m_frame)
        return;
    (m_frame->loader()->*function)(PolicyUse);
}


void FrameLoaderClientWx::dispatchDidLoadMainResource(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientWx::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientWx::clearUnarchivingState(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClientWx::postProgressStartedNotification()
{
    notImplemented();
}

void FrameLoaderClientWx::postProgressEstimateChangedNotification()
{
    fprintf(stderr, "Load progress at %f%%.\n", m_frame->page()->progress()->estimatedProgress());
    notImplemented();
}

void FrameLoaderClientWx::postProgressFinishedNotification()
{
    wxWindow* target = m_frame->view()->nativeWindow();
    if (target)
    {
        wxWebViewStateChangedEvent wkEvent(target);
        wkEvent.SetState(wxWEBVIEW_STATE_STOP);
        wkEvent.SetURL(m_frame->loader()->url().url());
        target->GetEventHandler()->ProcessEvent(wkEvent);
    }
}

void FrameLoaderClientWx::progressStarted()
{
    // no progress notification for now
}


void FrameLoaderClientWx::progressCompleted()
{
    // no progress notification for now
}


void FrameLoaderClientWx::setMainFrameDocumentReady(bool b)
{
    // this is only interesting once we provide an external API for the DOM
}


void FrameLoaderClientWx::willChangeTitle(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientWx::didChangeTitle(DocumentLoader *l)
{
    setTitle(l->title(), l->URL());
}


void FrameLoaderClientWx::finishedLoading(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientWx::finalSetupForReplace(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientWx::setDefersLoading(bool)
{
    notImplemented();
}


bool FrameLoaderClientWx::isArchiveLoadPending(ResourceLoader*) const
{
    notImplemented();
    return false;
}


void FrameLoaderClientWx::cancelPendingArchiveLoad(ResourceLoader*)
{
    notImplemented();
}


void FrameLoaderClientWx::clearArchivedResources()
{
    // don't think we need to do anything here currently
}


bool FrameLoaderClientWx::canShowMIMEType(const String& MIMEType) const
{
    // FIXME: This is not good enough in the general case
    //qDebug() << "FrameLoaderClientWx::canShowMIMEType" << MIMEType;
    return true;
}


bool FrameLoaderClientWx::representationExistsForURLScheme(const String& URLScheme) const
{
    notImplemented();
    //qDebug() << "    scheme is" << URLScheme;
    return false;
}


String FrameLoaderClientWx::generatedMIMETypeForURLScheme(const String& URLScheme) const
{
    notImplemented();
    return String();
}


void FrameLoaderClientWx::frameLoadCompleted()
{
    notImplemented();
}

void FrameLoaderClientWx::saveViewStateToItem(HistoryItem*)
{
    notImplemented();
}

void FrameLoaderClientWx::restoreViewState()
{
    notImplemented();
}
        
void FrameLoaderClientWx::restoreScrollPositionAndViewState()
{
    notImplemented();
}


void FrameLoaderClientWx::provisionalLoadStarted()
{
    // don't need to do anything here
}


bool FrameLoaderClientWx::shouldTreatURLAsSameAsCurrent(const KURL&) const
{
    notImplemented();
    return false;
}


void FrameLoaderClientWx::addHistoryItemForFragmentScroll()
{
    notImplemented();
}


void FrameLoaderClientWx::didFinishLoad()
{
    notImplemented();
}


void FrameLoaderClientWx::prepareForDataSourceReplacement()
{
    if (m_frame && m_frame->loader())
        m_frame->loader()->detachChildren();
}


void FrameLoaderClientWx::setTitle(const String& title, const KURL&)
{
    //notImplemented();
}


String FrameLoaderClientWx::userAgent(const KURL&)
{
    return String("Mozilla/5.0 (Macintosh; U; Intel Mac OS X; en) AppleWebKit/418.9.1 (KHTML, like Gecko) Safari/419.3");
}

void FrameLoaderClientWx::dispatchDidReceiveIcon()
{
    notImplemented();
}

void FrameLoaderClientWx::frameLoaderDestroyed()
{
    m_frame = 0;
    delete this;
}

bool FrameLoaderClientWx::canHandleRequest(const WebCore::ResourceRequest&) const
{
    return true;
}

void FrameLoaderClientWx::partClearedInBegin()
{
    notImplemented();
}

void FrameLoaderClientWx::setDocumentViewFromCachedPage(WebCore::CachedPage*)
{
    notImplemented();
}

void FrameLoaderClientWx::updateGlobalHistoryForStandardLoad(const WebCore::KURL&)
{
    notImplemented();
}

void FrameLoaderClientWx::updateGlobalHistoryForReload(const WebCore::KURL&)
{
    notImplemented();
}

bool FrameLoaderClientWx::shouldGoToHistoryItem(WebCore::HistoryItem*) const
{
    notImplemented();
    return true;
}

void FrameLoaderClientWx::saveScrollPositionAndViewStateToItem(WebCore::HistoryItem*)
{
    notImplemented();
}

void FrameLoaderClientWx::saveDocumentViewToCachedPage(WebCore::CachedPage*)
{
    notImplemented();
}

bool FrameLoaderClientWx::canCachePage() const
{
    // don't do any caching for now
    return false;
}

void FrameLoaderClientWx::setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientWx::committedLoad(WebCore::DocumentLoader* loader, const char* data, int length)
{
    if (!m_frame)
        return;
    FrameLoader *fl = loader->frameLoader();
    fl->setEncoding(m_response.textEncodingName(), false);
    fl->addData(data, length);
}

WebCore::ResourceError FrameLoaderClientWx::cancelledError(const WebCore::ResourceRequest&)
{
    notImplemented();
    return ResourceError();
}

WebCore::ResourceError FrameLoaderClientWx::blockedError(const ResourceRequest&)
{
    notImplemented();
    return ResourceError();
}

WebCore::ResourceError FrameLoaderClientWx::cannotShowURLError(const WebCore::ResourceRequest&)
{
    notImplemented();
    return ResourceError();
}

WebCore::ResourceError FrameLoaderClientWx::interruptForPolicyChangeError(const WebCore::ResourceRequest&)
{
    notImplemented();
    return ResourceError();
}

WebCore::ResourceError FrameLoaderClientWx::cannotShowMIMETypeError(const WebCore::ResourceResponse&)
{
    notImplemented();
    return ResourceError();
}

WebCore::ResourceError FrameLoaderClientWx::fileDoesNotExistError(const WebCore::ResourceResponse&)
{
    notImplemented();
    return ResourceError();
}

bool FrameLoaderClientWx::shouldFallBack(const WebCore::ResourceError&)
{
    notImplemented();
    return false;
}

WTF::PassRefPtr<WebCore::DocumentLoader> FrameLoaderClientWx::createDocumentLoader(const WebCore::ResourceRequest& request, const WebCore::SubstituteData& substituteData)
{
    RefPtr<DocumentLoader> loader = new DocumentLoader(request, substituteData);
    return loader.release();
}

void FrameLoaderClientWx::download(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&)
{
    notImplemented();
}

void FrameLoaderClientWx::assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const WebCore::ResourceRequest&)
{
    notImplemented();   
}

void FrameLoaderClientWx::dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long, WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response)
{

}

void FrameLoaderClientWx::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidReceiveResponse(WebCore::DocumentLoader* loader, unsigned long id, const WebCore::ResourceResponse& response)
{

    m_response = response;
    m_firstData = true;
    //fprintf(stderr, "got response from %S\n", response.url().url().utf8());
}

void FrameLoaderClientWx::dispatchDidReceiveContentLength(WebCore::DocumentLoader* loader, unsigned long id, int length)
{
    //fprintf(stderr, "received %d bytes\n", length);
    //notImplemented();
}

void FrameLoaderClientWx::dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long)
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidFailLoading(WebCore::DocumentLoader*, unsigned long, const WebCore::ResourceError&)
{
    notImplemented();
}

bool FrameLoaderClientWx::dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int)
{
    notImplemented();
    return false;
}

void FrameLoaderClientWx::dispatchDidFailProvisionalLoad(const WebCore::ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidFailLoad(const WebCore::ResourceError&)
{
    notImplemented();
}

WebCore::Frame* FrameLoaderClientWx::dispatchCreatePage()
{
    notImplemented();
    return false;
}

void FrameLoaderClientWx::dispatchDecidePolicyForMIMEType(FramePolicyFunction function, const WebCore::String&, const WebCore::ResourceRequest&)
{
    if (!m_frame)
        return;
    // FIXME: This is maybe too simple
    (m_frame->loader()->*function)(PolicyUse);
}

void FrameLoaderClientWx::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const WebCore::NavigationAction&, const WebCore::ResourceRequest&, const WebCore::String&)
{
    if (!m_frame)
        return;
    // FIXME: This is maybe too simple
    (m_frame->loader()->*function)(PolicyUse);
}

void FrameLoaderClientWx::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const WebCore::NavigationAction& action, const WebCore::ResourceRequest& request)
{
    if (!m_frame)
        return;
        
    wxWindow* target = m_frame->view()->nativeWindow();
    if (target)
    {
        wxWebViewBeforeLoadEvent wkEvent(target);
        wkEvent.SetNavigationType( wxNavTypeFromWebNavType(action.type()) );
        wkEvent.SetURL(request.url().url());
        
        target->GetEventHandler()->ProcessEvent(wkEvent);
        if (wkEvent.IsCancelled())
            (m_frame->loader()->*function)(PolicyIgnore);
        else
            (m_frame->loader()->*function)(PolicyUse);
        
    }
    // FIXME: This is maybe too simple
    
}

void FrameLoaderClientWx::dispatchUnableToImplementPolicy(const WebCore::ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientWx::startDownload(const WebCore::ResourceRequest&)
{
    notImplemented();
}

bool FrameLoaderClientWx::willUseArchive(WebCore::ResourceLoader*, const WebCore::ResourceRequest&, const WebCore::KURL&) const
{
    notImplemented();
    return false;
}

Frame* FrameLoaderClientWx::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                        const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    wxWindow* parent = m_frame->view()->nativeWindow();

    WebViewFrameData* data = new WebViewFrameData();
    data->name = name;
    data->ownerElement = ownerElement;
    data->url = url;
    data->referrer = referrer;
    data->marginWidth = marginWidth;
    data->marginHeight = marginHeight;

    wxWebView* newWin = new wxWebView(parent, -1, wxDefaultPosition, wxDefaultSize, data);

    RefPtr<Frame> childFrame = newWin->m_impl->frame;

    // FIXME: All of the below should probably be moved over into WebCore
    childFrame->tree()->setName(name);
    m_frame->tree()->appendChild(childFrame);
    // ### set override encoding if we have one

    FrameLoadType loadType = m_frame->loader()->loadType();
    FrameLoadType childLoadType = FrameLoadTypeInternal;

    childFrame->loader()->load(url, referrer, childLoadType,
                            String(), 0, 0);
    
    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;
    
    delete data;
    
    return childFrame.get();
}

ObjectContentType FrameLoaderClientWx::objectContentType(const KURL& url, const String& mimeType)
{
    notImplemented();
    return ObjectContentType();
}

Widget* FrameLoaderClientWx::createPlugin(Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool)
{
    notImplemented();
    return 0;
}

void FrameLoaderClientWx::redirectDataToPlugin(Widget* pluginWidget)
{
    notImplemented();
    return;
}

Widget* FrameLoaderClientWx::createJavaAppletWidget(const IntSize&, Element*, const KURL& baseURL,
                                                    const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    notImplemented();
    return 0;
}

String FrameLoaderClientWx::overrideMediaType() const
{
    notImplemented();
    return String();
}

void FrameLoaderClientWx::windowObjectCleared() const
{
    notImplemented();
}

}
