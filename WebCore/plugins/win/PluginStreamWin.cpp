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
#include "PluginStreamWin.h"

#include "CString.h"
#include "PluginDebug.h"
#include "PluginPackageWin.h"
#include "PluginViewWin.h"
#include "ResourceLoader.h"

#if USE(CFNETWORK)
#include <CFNetwork/CFNetwork.h>
#include <CFNetwork/CFURLResponsePriv.h>
#elif USE(WININET)
#include "ResourceLoaderWin.h"
#else 
#error No loader framework defined
#endif

#define WEB_REASON_NONE -1

namespace WebCore {

PluginStreamWin::PluginStreamWin(PluginViewWin* pluginView, DocLoader* docLoader, const String& method, const KURL& url, void* notifyData, bool sendNotification)
    : m_method(method)
    , m_url(url)
    , m_docLoader(docLoader)
    , m_pluginView(pluginView)
    , m_notifyData(notifyData)
    , m_sendNotification(sendNotification)
    , m_streamState(StreamBeforeStarted)
    , m_delayDeliveryTimer(this, &PluginStreamWin::delayDeliveryTimerFired)
    , m_deliveryData(0)
    , m_completeDeliveryData(0)
    , m_pluginFuncs(pluginView->plugin()->pluginFuncs())
    , m_instance(pluginView->instance())
{
    m_stream.url = 0;
    m_stream.ndata = 0;
    m_stream.pdata = 0;
    m_stream.end = 0;
    m_stream.notifyData = 0;
    m_stream.lastmodified = 0;
}

PluginStreamWin::~PluginStreamWin()
{
    ASSERT(m_streamState != StreamStarted);
    ASSERT(!m_resourceLoader);

    delete m_deliveryData;
    delete m_completeDeliveryData;

    free((char*)m_stream.url);
}

void PluginStreamWin::setRequestHeaders(const HashMap<String, String>& headers)
{
    m_headers = headers;
}

void PluginStreamWin::setPostData(const char* data, int len)
{
    m_postData = FormData();
    m_postData.appendData(data, len);
}

void PluginStreamWin::start()
{
    // FIXME: Should ask the bridge if the URL can be loaded.

    m_resourceLoader = ResourceLoader::create(this, m_method, m_url, m_postData);

    // Assemble headers
    if (!m_headers.isEmpty()) {
        String requestHeaders;
        HashMap<String, String>::const_iterator end = m_headers.end();
        for (HashMap<String, String>::const_iterator it = m_headers.begin(); it != end; ++it) {
            requestHeaders += it->first;
            requestHeaders += ": ";
            requestHeaders += it->second;
            requestHeaders += "\r\n";
        }

        m_resourceLoader->addMetaData("customHTTPHeader", requestHeaders);
    }

    m_resourceLoader->start(m_docLoader);
}

void PluginStreamWin::stop()
{
    if (m_resourceLoader)
        m_resourceLoader->kill();
    m_resourceLoader = 0;
}

void PluginStreamWin::startStream(const KURL& responseURL, long long expectedContentLength, int lastModifiedTime, const String& mimeType)
{
    ASSERT(m_streamState == StreamBeforeStarted);

    // Some plugins (Flash) expect that javascript URLs are passed back decoded as this is the
    // format used when requesting the URL.
    if (responseURL.protocol() == "javascript")
        m_stream.url = _strdup(responseURL.decode_string(responseURL.url()).utf8());
    else
        m_stream.url = _strdup(responseURL.url().utf8());
    
    CString mimeTypeStr = mimeType.utf8();
    
    m_stream.pdata = 0;
    m_stream.ndata = this;
    m_stream.end = expectedContentLength > 0 ? expectedContentLength : 0;
    m_stream.lastmodified = lastModifiedTime;
    m_stream.notifyData = m_notifyData;

    m_transferMode = NP_NORMAL;
    m_offset = 0;
    m_reason = WEB_REASON_NONE;

    NPError npErr = m_pluginFuncs->newstream(m_instance, (NPMIMEType)(const char*)mimeTypeStr, &m_stream, false, &m_transferMode);
    m_streamState = StreamStarted;

    if (npErr != NPERR_NO_ERROR) {
        LOG_NPERROR(npErr);
        stop();
        return;
    }
}

void PluginStreamWin::cancelAndDestroyStream(NPReason reason)
{
    destroyStream(reason);
    stop();
}

void PluginStreamWin::destroyStream(NPReason reason)
{
    m_reason = reason;
    if (m_reason != NPRES_DONE) {
        // Stop any pending data from being streamed
        if (m_deliveryData)
            m_deliveryData->resize(0);
    } else if (m_deliveryData && m_deliveryData->size() > 0) {
        // There is more data to be streamed, don't destroy the stream now.
        return;
    }
    destroyStream();
    ASSERT(m_stream.ndata == 0);
}

void PluginStreamWin::destroyStream()
{
    if (m_streamState == StreamStopped)
        return;

    ASSERT (m_reason != WEB_REASON_NONE);
    ASSERT (!m_deliveryData || m_deliveryData->size() == 0);

    if (m_stream.ndata != 0) {
        if (m_reason == NPRES_DONE && (m_transferMode == NP_ASFILE || m_transferMode == NP_ASFILEONLY)) {
            ASSERT(m_path != 0);

            m_pluginFuncs->asfile(m_instance, &m_stream, m_path);
            DeleteFileA(m_path);
        }

        NPError npErr;
        npErr = m_pluginFuncs->destroystream(m_instance, &m_stream, m_reason);
        LOG_NPERROR(npErr);

        m_stream.ndata = 0;
    }

    if (m_sendNotification)
        m_pluginFuncs->urlnotify(m_instance, m_url.url().utf8(), m_reason, m_notifyData);

    m_streamState = StreamStopped;
    m_pluginView = 0;
}

void PluginStreamWin::delayDeliveryTimerFired(Timer<PluginStreamWin>* timer)
{
    ASSERT(timer == &m_delayDeliveryTimer);

    deliverData();
}

void PluginStreamWin::deliverData()
{
    ASSERT(m_deliveryData);
    
    if (m_streamState == StreamStopped)
        // FIXME: We should cancel our job in the ResourceLoader on error so we don't reach this case
        return;

    ASSERT(m_streamState != StreamBeforeStarted);

    if (!m_stream.ndata || m_deliveryData->size() == 0)
        return;

    int32 totalBytes = m_deliveryData->size();
    int32 totalBytesDelivered = 0;

    while (totalBytesDelivered < totalBytes) {
        int32 deliveryBytes = m_pluginFuncs->writeready(m_instance, &m_stream);

        if (deliveryBytes <= 0) {
            m_delayDeliveryTimer.startOneShot(0);
            break;
        } else {
            deliveryBytes = min(deliveryBytes, totalBytes - totalBytesDelivered);
            int32 dataLength = deliveryBytes;
            char* data = m_deliveryData->data() + totalBytesDelivered;

            // Write the data
            deliveryBytes = m_pluginFuncs->write(m_instance, &m_stream, m_offset, dataLength, (void*)data);
            if (deliveryBytes < 0) {
                LOG_PLUGIN_NET_ERROR();
                cancelAndDestroyStream(NPRES_NETWORK_ERR);
                return;
            }
            deliveryBytes = min(deliveryBytes, dataLength);
            m_offset += deliveryBytes;
            totalBytesDelivered += deliveryBytes;
        }
    }

    if (totalBytesDelivered > 0) {
        if (totalBytesDelivered < totalBytes) {
            int remainingBytes = totalBytes - totalBytesDelivered;
            memmove(m_deliveryData, m_deliveryData + totalBytesDelivered, remainingBytes);
            m_deliveryData->resize(remainingBytes);
        } else {
            m_deliveryData->resize(0);
            if (m_reason != WEB_REASON_NONE)
                destroyStream();
        }
    } 
}

void PluginStreamWin::receivedResponse(ResourceLoader* resourceLoader, PlatformResponse response)
{
    ASSERT(resourceLoader == m_resourceLoader);
#if USE(CFNETWORK)
    String mimeType = CFURLResponseGetMIMEType(response);
    int contentLength = CFURLResponseGetExpectedContentLength(response);
    KURL responseURL = CFURLResponseGetURL(response);
    int lastModifiedTime = CFURLResponseGetLastModifiedDate(response) + kCFAbsoluteTimeIntervalSince1970;
#endif
#if USE(WININET)
    String mimeType = String(response->contentType, wcslen(response->contentType));
    int contentLength = response->contentLength;
    // FIXME: Should pass the response URL
    KURL responseURL = m_url;
    // FIXME: Should use a valid value
    int lastModifiedTime = 0;
#endif

    startStream(responseURL, contentLength, lastModifiedTime, mimeType);
}

void PluginStreamWin::didReceiveData(ResourceLoader* resourceLoader, const char* data, int length)
{
    ASSERT(resourceLoader == m_resourceLoader);
    ASSERT(length > 0);

    if (m_streamState == StreamStopped)
        // FIXME: We should cancel our job in the ResourceLoader on error so we don't reach this case
        return;

    ASSERT(m_streamState != StreamBeforeStarted);

    if (!m_deliveryData)
        m_deliveryData = new Vector<char>;

    int oldSize = m_deliveryData->size();
    m_deliveryData->resize(oldSize + length);
    memcpy(m_deliveryData->data() + oldSize, data, length);

    if (m_transferMode == NP_ASFILE || m_transferMode == NP_ASFILEONLY) {
        // FIXME: Since this is only used when all data has been received, it would be nice
        // to be able to share it with the CFNetwork in-memory cache.
        if (!m_completeDeliveryData)
            m_completeDeliveryData = new Vector<char>;

        oldSize = m_completeDeliveryData->size();
        m_completeDeliveryData->resize(oldSize + length);
        memcpy(m_completeDeliveryData->data() + oldSize, data, length);
    }

    if (m_transferMode != NP_ASFILEONLY)
        deliverData();
}

void PluginStreamWin::didFinishLoading(ResourceLoader* resourceLoader, PlatformData platformData)
{
    ASSERT(resourceLoader == m_resourceLoader);

    if (m_streamState == StreamStopped)
        // FIXME: We should cancel our job in the ResourceLoader on error so we don't reach this case
        return;

    ASSERT(m_streamState != StreamBeforeStarted);

    // The resource loader gets deleted after having received all data
    if (m_resourceLoader)
        m_resourceLoader = 0;

    if (resourceLoader && resourceLoader->error() != 0) {
        LOG_PLUGIN_NET_ERROR();
        destroyStream(NPRES_NETWORK_ERR);
    }

    if ((m_transferMode == NP_ASFILE || m_transferMode == NP_ASFILEONLY) && !m_path) {
        char tempPath[MAX_PATH];

        if (GetTempPathA(sizeof(tempPath), tempPath) == 0) {
            LOG_PLUGIN_NET_ERROR();
            destroyStream(NPRES_NETWORK_ERR);
            return;
        }

        char tempName[MAX_PATH];

        if (GetTempFileNameA(tempPath, "WKP", 0, tempName) == 0) {
            LOG_PLUGIN_NET_ERROR();
            destroyStream(NPRES_NETWORK_ERR);
            return;
        }

        HANDLE tempFile = CreateFileA(tempName, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (tempFile == INVALID_HANDLE_VALUE) {
            LOG_PLUGIN_NET_ERROR();
            destroyStream(NPRES_NETWORK_ERR);
            return;
        }

        DWORD written;
        bool retval = WriteFile(tempFile, m_completeDeliveryData->data(), m_completeDeliveryData->size(), &written, 0);
        CloseHandle(tempFile);

        if (!retval || written != m_completeDeliveryData->size()) {
            LOG_PLUGIN_NET_ERROR();
            destroyStream(NPRES_NETWORK_ERR);
            return;
        }

        m_path = tempName;
    }

    destroyStream(NPRES_DONE);
}

}
