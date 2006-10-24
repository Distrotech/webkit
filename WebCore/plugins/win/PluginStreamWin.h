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

#ifndef PluginStreamWin_H
#define PluginStreamWin_H

#include <winsock2.h>
#include <windows.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include "CString.h"
#include "FormData.h"
#include "KURL.h"
#include "npfunctions.h"
#include "PlatformString.h"
#include "ResourceLoaderClient.h"
#include "ResourceRequest.h"
#include "StringHash.h"
#include "Timer.h"

namespace WebCore {
    class DocLoader;
    class PluginViewWin;

    enum PluginStreamState { StreamBeforeStarted, StreamStarted, StreamStopped };

    class PluginStreamWin : ResourceLoaderClient {
    public:
        PluginStreamWin(PluginViewWin*, DocLoader*, const String& method, const KURL&, void* notifyData, bool sendNotification);
        ~PluginStreamWin();

        void setRequestHeaders(const ResourceRequest::HTTPHeaderMap& headers);
        void setPostData(const char* data, int len);

        void start();
        void stop();

        void startStream(const KURL& responseURL, long long expectedContentLength, int lastModifiedTime, const String& mimeType);
        
        // ResourceLoaderClient
        virtual void receivedResponse(ResourceLoader*, PlatformResponse);
        virtual void didReceiveData(ResourceLoader*, const char*, int);
        virtual void didFinishLoading(ResourceLoader*, PlatformData);
    private:
        void deliverData();
        void cancelAndDestroyStream(NPReason);
        void destroyStream(NPReason);
        void destroyStream();

        String m_method;
        KURL m_url;
        DocLoader* m_docLoader;
        RefPtr<ResourceLoader> m_resourceLoader;
        PluginViewWin* m_pluginView;
        void* m_notifyData;
        bool m_sendNotification;
        PluginStreamState m_streamState;

        ResourceRequest::HTTPHeaderMap m_headers;
        FormData m_postData;

        Timer<PluginStreamWin> m_delayDeliveryTimer;
        void delayDeliveryTimerFired(Timer<PluginStreamWin>*);

        Vector<char>* m_deliveryData;
        Vector<char>* m_completeDeliveryData;

        const NPPluginFuncs* m_pluginFuncs;
        NPP m_instance;
        uint16 m_transferMode;
        int32 m_offset;
        CString m_path;
        NPReason m_reason;
        NPStream m_stream;
    };

} // namespace WebCore

#endif
