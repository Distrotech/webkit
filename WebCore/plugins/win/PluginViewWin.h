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

#ifndef PluginViewWin_H
#define PluginViewWin_H

#include <windows.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include "KURL.h"
#include "npapi.h"
#include "PlatformString.h"
#include "Timer.h"
#include "Widget.h"

namespace WebCore {
    class Element;
    class FrameWin;
    class KURL;
    class PluginPackageWin;

    class PluginViewWin : public Widget {
    public:
        PluginViewWin(FrameWin* parentFrame, PluginPackageWin* plugin, Element*, const KURL&, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType);
        virtual ~PluginViewWin();

        PluginPackageWin* plugin() const { return m_plugin.get(); }
        NPP instance() const { return m_instance; }

        void invokeSetWindow(WINDOWPOS* windowPos);
        static PluginViewWin* currentPluginView();

        // NPN functions
        NPError getURLNotify(const char* url, const char* target, void* notifyData);
        NPError getURL(const char* url, const char* target);
        NPError postURLNotify(const char* url, const char* target, uint32 len, const char* but, NPBool file, void* notifyData);
        NPError postURL(const char* url, const char* target, uint32 len, const char* but, NPBool file);
        NPError newStream(NPMIMEType type, const char* target, NPStream** stream);
        int32 write(NPP instance, NPStream* stream, int32 len, void* buffer);
        NPError destroyStream(NPP instance, NPStream* stream, NPReason reason);
        const char* userAgent();
        void status(const char* message);
        NPError getValue(NPNVariable variable, void* value);
        NPError setValue(NPPVariable variable, void* value);
    private:
        bool start();
        void stop();
        static void setCurrentPluginView(PluginViewWin* pluginView);

        RefPtr<PluginPackageWin> m_plugin;
        FrameWin* m_parentFrame;
        bool m_isStarted;
        KURL m_url;

        int m_mode;
        int m_paramCount;
        char** m_paramNames;
        char** m_paramValues;

        char* m_mimeType;
        
        NPP m_instance;
        NPP_t m_instanceStruct;
        NPWindow m_window;
        NPWindow m_lastSetWindow;

        char* m_userAgent;

        static PluginViewWin* s_currentPluginView;
    };

} // namespace WebCore

#endif 