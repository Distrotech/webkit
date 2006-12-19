/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#if USE(CFNETWORK)
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"

#include "DocLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Logging.h"
#include "ResourceError.h"
#include "ResourceResponse.h"

#include <WTF/HashMap.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <process.h> // for _beginthread()

#include <CFNetwork/CFNetwork.h>
#include <CFNetwork/CFNetworkPriv.h>

namespace WebCore {

CFURLRequestRef willSendRequest(CFURLConnectionRef conn, CFURLRequestRef cfRequest, CFURLResponseRef cfRedirectResponse, const void* clientInfo)
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;

    LOG(Network, "CFNet - willSendRequest(conn=%p, handle=%p) (%s)", conn, handle, handle->url().url().ascii());

    if (ResourceHandleClient* client = handle->client()) {
        ResourceRequest request(cfRequest);
        client->willSendRequest(handle, request, cfRedirectResponse);
        return request.cfURLRequest();
    }

    return cfRequest;
}

void didReceiveResponse(CFURLConnectionRef conn, CFURLResponseRef cfResponse, const void* clientInfo) 
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;

    LOG(Network, "CFNet - didReceiveResponse(conn=%p, handle=%p) (%s)", conn, handle, handle->url().url().ascii());

    if (ResourceHandleClient* client = handle->client())
        client->didReceiveResponse(handle, cfResponse);
}

void didReceiveData(CFURLConnectionRef conn, CFDataRef data, CFIndex originalLength, const void* clientInfo) 
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;
    const UInt8* bytes = CFDataGetBytePtr(data);
    CFIndex length = CFDataGetLength(data);

    LOG(Network, "CFNet - didReceiveData(conn=%p, handle=%p, bytes=%d) (%s)", conn, handle, length, handle->url().url().ascii());

    handle->client()->didReceiveData(handle, (const char*)bytes, length, originalLength);
}

void didFinishLoading(CFURLConnectionRef conn, const void* clientInfo) 
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;

    LOG(Network, "CFNet - didFinishLoading(conn=%p, handle=%p) (%s)", conn, handle, handle->url().url().ascii());

    handle->client()->didFinishLoading(handle);
}

void didFail(CFURLConnectionRef conn, CFStreamError error, const void* clientInfo) 
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;

    LOG(Network, "CFNet - didFail(conn=%p, handle=%p, error = {%d, %d}) (%s)", conn, handle, error.domain, error.error, handle->url().url().ascii());

    String domain;
    switch(error.domain) {
    case kCFStreamErrorDomainPOSIX:
        domain = "NSPOSIXErrorDomain";
        break;
    case kCFStreamErrorDomainMacOSStatus:
        domain = "NSOSStatusErrorDomain";
        break;
    }

    // FIXME: we'd really like to include a failing URL and a localized description but you can't
    // get a CFErrorRef out of an NSURLConnection, only a CFStreamError
    handle->client()->didFail(handle, ResourceError(domain, error.error, String(), String()));
}

CFCachedURLResponseRef willCacheResponse(CFURLConnectionRef conn, CFCachedURLResponseRef cachedResponse, const void* clientInfo) 
{
    ResourceHandle* job = (ResourceHandle*)clientInfo;
    return cachedResponse;
}

void didReceiveChallenge(CFURLConnectionRef conn, CFURLAuthChallengeRef challenge, const void* clientInfo)
{
    ResourceHandle* job = (ResourceHandle*)clientInfo;

    // Do nothing right now
}

void addHeadersFromHashMap(CFMutableURLRequestRef request, const HTTPHeaderMap& requestHeaders) 
{
    if (!requestHeaders.size())
        return;

    HTTPHeaderMap::const_iterator end = requestHeaders.end();
    for (HTTPHeaderMap::const_iterator it = requestHeaders.begin(); it != end; ++it) {
        CFStringRef key = it->first.createCFString();
        CFStringRef value = it->second.createCFString();
        CFURLRequestSetHTTPHeaderFieldValue(request, key, value);
        CFRelease(key);
        CFRelease(value);
    }
}

ResourceHandleInternal::~ResourceHandleInternal()
{
    if (m_connection) {
        LOG(Network, "CFNet - Cancelling connection %p (%s)", m_connection, m_request.url().url().ascii());

        CFURLConnectionCancel(m_connection);
        CFRelease(m_connection);
        m_connection = 0;
    }
}

ResourceHandle::~ResourceHandle()
{
    LOG(Network, "CFNet - Destroying job %p (%s)", this, d->m_request.url().url().ascii());
}

CFArrayRef arrayFromFormData(const FormData& d)
{
    size_t size = d.elements().size();
    CFMutableArrayRef a = CFArrayCreateMutable(0, d.elements().size(), &kCFTypeArrayCallBacks);
    for (size_t i = 0; i < size; ++i) {
        const FormDataElement& e = d.elements()[i];
        if (e.m_type == FormDataElement::data) {
            CFDataRef data = CFDataCreate(0, (const UInt8*)e.m_data.data(), e.m_data.size());
            CFArrayAppendValue(a, data);
            CFRelease(data);
        } else {
            ASSERT(e.m_type == FormDataElement::encodedFile);
            CFStringRef filename = e.m_filename.createCFString();
            CFArrayAppendValue(a, filename);
            CFRelease(filename);
        }
    }
    return a;
}

void emptyPerform(void* unused) 
{
}

static CFRunLoopRef loaderRL = 0;
void runLoaderThread(void *unused)
{
    loaderRL = CFRunLoopGetCurrent();

    // Must add a source to the run loop to prevent CFRunLoopRun() from exiting
    CFRunLoopSourceContext ctxt = {0, (void *)1 /*must be non-NULL*/, 0, 0, 0, 0, 0, 0, 0, emptyPerform};
    CFRunLoopSourceRef bogusSource = CFRunLoopSourceCreate(0, 0, &ctxt);
    CFRunLoopAddSource(loaderRL, bogusSource,kCFRunLoopDefaultMode);

    CFRunLoopRun();
}

bool ResourceHandle::start(Frame* frame)
{
    d->m_request.setHTTPUserAgent(frame->loader()->userAgent());
    String referrer = frame->loader()->referrer();
    if (!referrer.isEmpty() && referrer.find("file:", 0, false) != 0)
        d->m_request.setHTTPReferrer(referrer);

    CFURLRequestRef request = d->m_request.cfURLRequest();

    CFURLConnectionClient client = {0, this, 0, 0, 0, willSendRequest, didReceiveResponse, didReceiveData, NULL, didFinishLoading, didFail, willCacheResponse, didReceiveChallenge};
    d->m_connection = CFURLConnectionCreate(0, request, &client);
    CFRelease(request);

    if (!loaderRL) {
        _beginthread(runLoaderThread, 0, 0);
        while (loaderRL == 0) {
            // FIXME: sleep 10? that can't be right...
            Sleep(10);
        }
    }

    CFURLConnectionScheduleWithCurrentMessageQueue(d->m_connection);
    CFURLConnectionScheduleDownloadWithRunLoop(d->m_connection, loaderRL, kCFRunLoopDefaultMode);
    CFURLConnectionStart(d->m_connection);

    LOG(Network, "CFNet - Starting URL %s (handle=%p, conn=%p)", d->m_request.url().url().ascii(), this, d->m_connection);

    return true;
}

void ResourceHandle::cancel()
{
    if (d->m_connection) {
        CFURLConnectionCancel(d->m_connection);
        CFRelease(d->m_connection);
        d->m_connection = 0;
    }

    // FIXME: need real cancel error
    d->m_client->didFail(this, ResourceError());
}

} // namespace WebCore

#endif // USE(CFNETWORK)
