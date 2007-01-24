/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "WebKitDLL.h"
#include "WebDownload.h"

#include "DefaultDownloadDelegate.h"
#include "MarshallingHelpers.h"
#include "WebError.h"
#include "WebKit.h"
#include "WebKitLogging.h"
#include "WebMutableURLRequest.h"
#include "WebURLResponse.h"

#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#pragma warning(pop)

using namespace WebCore;

// CFURLDownload Callbacks ----------------------------------------------------------------
static void didStartCallback(CFURLDownloadRef download, const void *clientInfo);
static CFURLRequestRef willSendRequestCallback(CFURLDownloadRef download, CFURLRequestRef request, CFURLResponseRef redirectionResponse, const void *clientInfo);
static void didReceiveAuthenticationChallengeCallback(CFURLDownloadRef download, CFURLAuthChallengeRef challenge, const void *clientInfo);
static void didReceiveResponseCallback(CFURLDownloadRef download, CFURLResponseRef response, const void *clientInfo);
static void willResumeWithResponseCallback(CFURLDownloadRef download, CFURLResponseRef response, UInt64 startingByte, const void *clientInfo);
static void didReceiveDataCallback(CFURLDownloadRef download, CFIndex length, const void *clientInfo);
static Boolean shouldDecodeDataOfMIMETypeCallback(CFURLDownloadRef download, CFStringRef encodingType, const void *clientInfo);
static void decideDestinationWithSuggestedObjectNameCallback(CFURLDownloadRef download, CFStringRef objectName, const void *clientInfo);
static void didCreateDestinationCallback(CFURLDownloadRef download, CFURLRef path, const void *clientInfo);
static void didFinishCallback(CFURLDownloadRef download, const void *clientInfo);
static void didFailCallback(CFURLDownloadRef download, CFErrorRef error, const void *clientInfo);

// WebDownload ----------------------------------------------------------------

WebDownload::WebDownload()
    : m_refCount(0)
{
    gClassCount++;
}

WebDownload::WebDownload(ResourceHandle* handle, const ResourceRequest& request, const ResourceResponse& response, IWebDownloadDelegate* delegate)
    : m_refCount(0)
    , m_delegate(delegate ? delegate : DefaultDownloadDelegate::sharedInstance())
{
    gClassCount++;
    LOG_ERROR("Delegate is %p", m_delegate.get());
    CFURLConnectionRef connection = handle->connection();
    if (!connection) {
        LOG_ERROR("WebDownload::WebDownload(ResourceHandle*,...) called with an inactive ResourceHandle");    
        return;
    }

    CFURLDownloadClient client = {0, this, 0, 0, 0, didStartCallback, willSendRequestCallback, didReceiveAuthenticationChallengeCallback, 
        didReceiveResponseCallback, willResumeWithResponseCallback, didReceiveDataCallback, shouldDecodeDataOfMIMETypeCallback,
        decideDestinationWithSuggestedObjectNameCallback, didCreateDestinationCallback, didFinishCallback, didFailCallback};

    m_download = CFURLDownloadCreateWithLoadingConnection(0, connection, request.cfURLRequest(), response.cfURLResponse(), 0, &client);
    m_request.adoptRef(WebMutableURLRequest::createInstance(request));
    
    // FIXME: Rework this once CFNetwork implements CFURLDownloadCreateWithLoadingConnection
    if (!m_download) {
        LOG_ERROR("Downloading from an existing connection is currently unimplemented in CFNetwork.  This WebDownload will likely leak");
        return;
    }

    CFURLDownloadScheduleWithCurrentMessageQueue(m_download.get());
    CFURLDownloadScheduleDownloadWithRunLoop(m_download.get(), ResourceHandle::loaderRunLoop(), kCFRunLoopDefaultMode);
    CFURLDownloadStart(m_download.get());
    
    // FIXME: 4950477 - CFURLDownload neglects to make the didStart() client call upon starting the download.
    // This is a somewhat critical call, so we'll fake it for now!
    didStart();

    LOG(Download, "WebDownload - Started download from existing connection (%s)", request.url().url().ascii());
}

WebDownload::WebDownload(const KURL& url, IWebDownloadDelegate* delegate)
    : m_refCount(0)
    , m_delegate(delegate ? delegate : DefaultDownloadDelegate::sharedInstance())
{
    gClassCount++;
    LOG_ERROR("Delegate is %p", m_delegate.get());

    ResourceRequest request(url);
    CFURLRequestRef cfRequest = request.cfURLRequest();

    CFURLDownloadClient client = {0, this, 0, 0, 0, didStartCallback, willSendRequestCallback, didReceiveAuthenticationChallengeCallback, 
                                  didReceiveResponseCallback, willResumeWithResponseCallback, didReceiveDataCallback, shouldDecodeDataOfMIMETypeCallback, 
                                  decideDestinationWithSuggestedObjectNameCallback, didCreateDestinationCallback, didFinishCallback, didFailCallback};
    m_download = CFURLDownloadCreate(0, cfRequest, &client);
    m_request.adoptRef(WebMutableURLRequest::createInstance(request));

    CFURLDownloadScheduleWithCurrentMessageQueue(m_download.get());
    CFURLDownloadScheduleDownloadWithRunLoop(m_download.get(), ResourceHandle::loaderRunLoop(), kCFRunLoopDefaultMode);
    CFURLDownloadStart(m_download.get());

    // FIXME: 4950477 - CFURLDownload neglects to make the didStart() client call upon starting the download.
    // This is a somewhat critical call, so we'll fake it for now!
    didStart();

    LOG(Download, "WebDownload - Started download of url %s", url.url().ascii());
}

WebDownload::~WebDownload()
{
    LOG(Download, "WebDownload - Destroying download (%p)", this);
    gClassCount--;
}

WebDownload* WebDownload::createInstance()
{
    WebDownload* instance = new WebDownload();
    instance->AddRef();
    return instance;
}

WebDownload* WebDownload::createInstance(ResourceHandle* handle, const ResourceRequest& request, const ResourceResponse& response, IWebDownloadDelegate* delegate)
{
    WebDownload* instance = new WebDownload(handle, request, response, delegate);
    instance->AddRef();
    return instance;
}

WebDownload* WebDownload::createInstance(const KURL& url, IWebDownloadDelegate* delegate)
{
    WebDownload* instance = new WebDownload(url, delegate);
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebDownload::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebDownload*>(this);
    else if (IsEqualGUID(riid, IID_IWebDownload))
        *ppvObject = static_cast<IWebDownload*>(this);
    else if (IsEqualGUID(riid, IID_IWebURLAuthenticationChallengeSender))
        *ppvObject = static_cast<IWebURLAuthenticationChallengeSender*>(this);
    else if (IsEqualGUID(riid, CLSID_WebDownload))
        *ppvObject = static_cast<WebDownload*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebDownload::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebDownload::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebDownload -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebDownload::initWithRequest(
        /* [in] */ IWebURLRequest* request, 
        /* [in] */ IWebDownloadDelegate* delegate)
{
    COMPtr<WebMutableURLRequest> webRequest;
    if (!request || FAILED(request->QueryInterface(CLSID_WebMutableURLRequest, (void**)&webRequest))) {
        LOG(Download, "WebDownload - initWithRequest failed - not a WebMutableURLRequest");    
        return E_FAIL;
    }

    // FIXME: Leave the DefaultDownloadDelegate in, or not?
    m_delegate = delegate ? delegate : DefaultDownloadDelegate::sharedInstance();
    LOG_ERROR("Delegate is %p", m_delegate.get());

    CFURLRequestRef cfRequest = webRequest->resourceRequest().cfURLRequest();

    CFURLDownloadClient client = {0, this, 0, 0, 0, didStartCallback, willSendRequestCallback, didReceiveAuthenticationChallengeCallback, 
                                  didReceiveResponseCallback, willResumeWithResponseCallback, didReceiveDataCallback, shouldDecodeDataOfMIMETypeCallback, 
                                  decideDestinationWithSuggestedObjectNameCallback, didCreateDestinationCallback, didFinishCallback, didFailCallback};
    m_download = CFURLDownloadCreate(0, cfRequest, &client);
    m_request.adoptRef(WebMutableURLRequest::createInstance(webRequest.get()));

    CFURLDownloadScheduleWithCurrentMessageQueue(m_download.get());
    CFURLDownloadScheduleDownloadWithRunLoop(m_download.get(), ResourceHandle::loaderRunLoop(), kCFRunLoopDefaultMode);
    CFURLDownloadStart(m_download.get());
    
    // FIXME: 4950477 - CFURLDownload neglects to make the didStart() client call upon starting the download.
    // This is a somewhat critical call, so we'll fake it for now!
    didStart();

    LOG(Download, "WebDownload - initWithRequest complete, started download of url %s", webRequest->resourceRequest().url().url().ascii());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownload::initWithResumeData(
        /* [in] */ IStream*, 
        /* [in] */ IWebDownloadDelegate*, 
        /* [in] */ BSTR)
{
    LOG_NOIMPL();
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebDownload::canResumeDownloadDecodedWithEncodingMIMEType(
        /* [in] */ BSTR, 
        /* [out, retval] */ BOOL*)
{
    LOG_NOIMPL();
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebDownload::cancel()
{
    LOG_NOIMPL();
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebDownload::deletesFileUponFailure(
        /* [out, retval] */ BOOL* result)
{
    *result = CFURLDownloadDeletesUponFailure(m_download.get());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownload::request(
        /* [out, retval] */ IWebURLRequest** request)
{
    if (request) {
        *request = m_request.get();
        (*request)->AddRef();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownload::resumeData(
        /* [out, retval] */ IStream**)
{
    LOG_NOIMPL();
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebDownload::setDeletesFileUponFailure(
        /* [in] */ BOOL deletesFileUponFailure)
{
    CFURLDownloadSetDeletesUponFailure(m_download.get(), !!deletesFileUponFailure);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDownload::setDestination(
        /* [in] */ BSTR path, 
        /* [in] */ BOOL allowOverwrite)
{
    CFStringRef cfPath = MarshallingHelpers::BSTRToCFStringRef(path);
    CFURLRef pathURL = CFURLCreateWithFileSystemPath(0, cfPath, kCFURLWindowsPathStyle, false);
    CFURLDownloadSetDestination(m_download.get(), pathURL, !!allowOverwrite);
    CFRelease(pathURL);
    CFRelease(cfPath);

#ifndef NDEBUG
    String str(path, SysStringLen(path));
    LOG(Download, "WebDownload - Set destination to %s", str.ascii().data());
#endif

    return S_OK;
}

// IWebURLAuthenticationChallengeSender -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebDownload::cancelAuthenticationChallenge(
        /* [in] */ IWebURLAuthenticationChallenge*)
{
    LOG_NOIMPL();
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebDownload::continueWithoutCredentialForAuthenticationChallenge(
        /* [in] */ IWebURLAuthenticationChallenge*)
{
    LOG_NOIMPL();
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebDownload::useCredential(
        /* [in] */ IWebURLCredential*, 
        /* [in] */ IWebURLAuthenticationChallenge*)
{
    LOG_NOIMPL();
    return E_FAIL;
}


// CFURLDownload Callbacks -------------------------------------------------------------------
void WebDownload::didStart()
{
    if (FAILED(m_delegate->didBegin(this)))
        LOG_ERROR("DownloadDelegate->didBegin failed");
}

CFURLRequestRef WebDownload::willSendRequest(CFURLRequestRef request, CFURLResponseRef response)
{
    COMPtr<WebMutableURLRequest> webRequest = WebMutableURLRequest::createInstance(ResourceRequest(request));
    COMPtr<WebURLResponse> webResponse = WebURLResponse::createInstance(ResourceResponse(response));
    COMPtr<IWebMutableURLRequest> finalRequest;

    if (FAILED(m_delegate->willSendRequest(this, webRequest.get(), webResponse.get(), &finalRequest)))
        LOG_ERROR("DownloadDelegate->willSendRequest failed");
    
    if (!finalRequest)
        return 0;

    COMPtr<WebMutableURLRequest> finalWebRequest = WebMutableURLRequest::createInstance(finalRequest.get());
    m_request = finalWebRequest.get();
    return finalWebRequest->resourceRequest().cfURLRequest();
}

void WebDownload::didReceiveAuthenticationChallenge(CFURLAuthChallengeRef )
{
    LOG_NOIMPL();
}

void WebDownload::didReceiveResponse(CFURLResponseRef response)
{
    COMPtr<WebURLResponse> webResponse = WebURLResponse::createInstance(ResourceResponse(response));
    if (FAILED(m_delegate->didReceiveResponse(this, webResponse.get())))
        LOG_ERROR("DownloadDelegate->didReceiveResponse failed");
}

void WebDownload::willResumeWithResponse(CFURLResponseRef response, UInt64 fromByte)
{
    COMPtr<WebURLResponse> webResponse = WebURLResponse::createInstance(ResourceResponse(response));
    if (FAILED(m_delegate->willResumeWithResponse(this, webResponse.get(), fromByte)))
        LOG_ERROR("DownloadDelegate->willResumeWithResponse failed");
}

void WebDownload::didReceiveData(CFIndex length)
{
    if (FAILED(m_delegate->didReceiveDataOfLength(this, length)))
        LOG_ERROR("DownloadDelegate->didReceiveData failed");
}

bool WebDownload::shouldDecodeDataOfMIMEType(CFStringRef mimeType)
{
    BOOL result;
    if (FAILED(m_delegate->shouldDecodeSourceDataOfMIMEType(this, BString(mimeType), &result))) {
        LOG_ERROR("DownloadDelegate->shouldDecodeSourceDataOfMIMEType failed");
        return false;
    }
    return !!result;
}

void WebDownload::decideDestinationWithSuggestedObjectName(CFStringRef name)
{    
    if (FAILED(m_delegate->decideDestinationWithSuggestedFilename(this, BString(name))))
        LOG_ERROR("DownloadDelegate->decideDestinationWithSuggestedObjectName failed");
}

void WebDownload::didCreateDestination(CFURLRef destination)
{
    if (FAILED(m_delegate->didCreateDestination(this, BString(CFURLGetString(destination)))))
        LOG_ERROR("DownloadDelegate->didCreateDestination failed");
}

void WebDownload::didFinish()
{
    if (FAILED(m_delegate->didFinish(this)))
        LOG_ERROR("DownloadDelegate->didFinish failed");
}

void WebDownload::didFail(CFErrorRef error)
{
    COMPtr<WebError> webError = WebError::createInstance(ResourceError(error));
    if (FAILED(m_delegate->didFailWithError(this, webError.get())))
        LOG_ERROR("DownloadDelegate->didFailWithError failed");
}


void didStartCallback(CFURLDownloadRef, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didStart(); }

CFURLRequestRef willSendRequestCallback(CFURLDownloadRef, CFURLRequestRef request, CFURLResponseRef redirectionResponse, const void *clientInfo)
{ return ((WebDownload*)clientInfo)->willSendRequest(request, redirectionResponse); }

void didReceiveAuthenticationChallengeCallback(CFURLDownloadRef, CFURLAuthChallengeRef challenge, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didReceiveAuthenticationChallenge(challenge); }

void didReceiveResponseCallback(CFURLDownloadRef, CFURLResponseRef response, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didReceiveResponse(response); }

void willResumeWithResponseCallback(CFURLDownloadRef, CFURLResponseRef response, UInt64 startingByte, const void *clientInfo)
{ ((WebDownload*)clientInfo)->willResumeWithResponse(response, startingByte); }

void didReceiveDataCallback(CFURLDownloadRef, CFIndex length, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didReceiveData(length); }

Boolean shouldDecodeDataOfMIMETypeCallback(CFURLDownloadRef, CFStringRef encodingType, const void *clientInfo)
{ return ((WebDownload*)clientInfo)->shouldDecodeDataOfMIMEType(encodingType); }

void decideDestinationWithSuggestedObjectNameCallback(CFURLDownloadRef, CFStringRef objectName, const void *clientInfo)
{ ((WebDownload*)clientInfo)->decideDestinationWithSuggestedObjectName(objectName); }

void didCreateDestinationCallback(CFURLDownloadRef, CFURLRef path, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didCreateDestination(path); }

void didFinishCallback(CFURLDownloadRef, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didFinish(); }

void didFailCallback(CFURLDownloadRef, CFErrorRef error, const void *clientInfo)
{ ((WebDownload*)clientInfo)->didFail(error); }

