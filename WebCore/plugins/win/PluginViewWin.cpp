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
#include "PluginViewWin.h"

#include <kjs/JSLock.h>
#include <kjs/value.h>
#include "Document.h"
#include "Element.h"
#include "FrameTree.h"
#include "FrameWin.h"
#include "PluginPackageWin.h"
#include "kjs_binding.h"
#include "kjs_proxy.h"
#include "npruntime_impl.h"
#include "PluginPackageWin.h"
#include "PluginStreamWin.h"

using KJS::Interpreter;
using KJS::JSLock;
using KJS::JSObject;
using KJS::JSValue;

namespace WebCore {

class PluginRequestWin {
public:
    PluginRequestWin(const KURL& url, const String& target, void* notifyData, bool sendNotification)
        : m_url(url)
        , m_target(target)
        , m_notifyData(notifyData)
        , m_sendNotification(sendNotification) { }
public:
    KURL url() const { return m_url; }
    String target() const { return m_target; }
    void* notifyData() const { return m_notifyData; }
    bool sendNotification() const { return m_sendNotification; }
private:
    KURL m_url;
    String m_target;
    void* m_notifyData;
    bool m_sendNotification;
    // FIXME: user gesture
};

static String scriptStringIfJavaScriptURL(const KURL& url)
{
    if (url.protocol() != "javascript")
        return String();

    // This returns an unescaped string
    return url.path();
}

PluginViewWin* PluginViewWin::s_currentPluginView = 0;

const LPCWSTR kWebPluginViewWindowClassName = L"WebPluginView";
const LPCWSTR kWebPluginViewProperty = L"WebPluginViewProperty";

static LRESULT CALLBACK PluginViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static bool registerPluginView()
{
    static bool haveRegisteredWindowClass = false;
    if (haveRegisteredWindowClass)
        return true;

    ASSERT(Widget::instanceHandle);

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_DBLCLKS;
    wcex.lpfnWndProc    = PluginViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = Widget::instanceHandle;
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)COLOR_WINDOW;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebPluginViewWindowClassName;
    wcex.hIconSm        = 0;

    return RegisterClassEx(&wcex);
}

static LRESULT CALLBACK PluginViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PluginViewWin* pluginView = reinterpret_cast<PluginViewWin*>(GetProp(hWnd, kWebPluginViewProperty));

    return DefWindowProc(hWnd, message, wParam, lParam);
}

void PluginViewWin::updateSize()
{
    RECT rect;

    GetClientRect(windowHandle(), &rect);

    m_window.x = 0;
    m_window.y = 0;
    m_window.width = rect.right - rect.left;
    m_window.height = rect.bottom - rect.top;

    m_window.clipRect.left = 0;
    m_window.clipRect.top = 0;
    m_window.clipRect.right = rect.right - rect.left;
    m_window.clipRect.left = rect.bottom - rect.top;

    if (m_plugin->pluginFuncs()->setwindow)
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_window);
}

void PluginViewWin::setFrameGeometry(const IntRect &rect)
{
    // FIXME: It makes more sense for this check to be in WidgetWin.cpp
    if (rect == m_contentRect)
        return;

    m_contentRect = rect;
    Widget::setFrameGeometry(rect);

    updateSize();
}

bool PluginViewWin::start()
{
    if (m_isStarted)
        return false;

    ASSERT(m_plugin);
    ASSERT(m_plugin->pluginFuncs()->newp);

    PluginViewWin::setCurrentPluginView(this);
    NPError npErr = m_plugin->pluginFuncs()->newp((NPMIMEType)(const char*)m_mimeType, m_instance, m_mode, m_paramCount, m_paramNames, m_paramValues, NULL);
    PluginViewWin::setCurrentPluginView(0);
        
    m_isStarted = true;

    if (m_url.isValid())
        loadURL("GET", m_url, String(), 0, false);

    return true;
}

void PluginViewWin::stop()
{
    if (!m_isStarted)
        return;

    HashSet<PluginStreamWin*>::iterator end = m_streams.end();
    for (HashSet<PluginStreamWin*>::iterator it = m_streams.begin(); it != end; ++it)
        (*it)->stop();

    m_isStarted = false;

    // Clear the window
    m_window.window = 0;
    if (m_plugin->pluginFuncs()->setwindow)
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_window);

    // Destroy the plugin
    NPError npErr = m_plugin->pluginFuncs()->destroy(m_instance, 0);

    m_instance->pdata = 0;
}

void PluginViewWin::setCurrentPluginView(PluginViewWin* pluginView)
{
    s_currentPluginView = pluginView;
}

PluginViewWin* PluginViewWin::currentPluginView()
{
    return s_currentPluginView;
}

static char* createUTF8String(const String& str)
{
    CString cstr = str.utf8();
    char* result = reinterpret_cast<char*>(fastMalloc(cstr.length() + 1));

    strncpy(result, cstr, cstr.length() + 1);

    return result;
}

static char** createUTF8StringArray(const Vector<String>& vector)
{
    char** result = reinterpret_cast<char**>(fastMalloc(sizeof(char*) * vector.size()));

    for (unsigned i = 0; i < vector.size(); i++)
        result[i] = createUTF8String(vector[i]);

    return result;
}

static void freeStringArray(char** stringArray, int length)
{
    for (unsigned i = 0; i < length; i++)
        fastFree(stringArray[i]);

    fastFree(stringArray);
}

void PluginViewWin::performRequest(PluginRequestWin* request)
{
    String jsString = scriptStringIfJavaScriptURL(request->url());

    if (!jsString.isNull()) {
        if (!request->target().isNull()) {
            // FIXME: If the result is a string, we probably want to put that string into the frame, just
            // like we do in KHTMLPartBrowserExtension::openURLRequest.
            if (request->sendNotification())
                m_plugin->pluginFuncs()->urlnotify(m_instance, request->url().url().utf8(), NPRES_DONE, request->notifyData());
        } else {
            JSValue* result = m_parentFrame->executeScript(0, jsString.deprecatedString(), true);
            String resultString;
            if (result && result->isString()) {
                JSLock lock;
                resultString = result->getString();
            }

            if (resultString.isEmpty())
                return;

            CString cstr = resultString.utf8();
            PluginStreamWin* stream = new PluginStreamWin(this, m_parentFrame->document()->docLoader(), "GET", request->url(), request->notifyData(), request->sendNotification());
            stream->startStream(m_url, cstr.length(), 0, "text/plain");
            stream->receivedData(0, cstr, cstr.length());
            stream->receivedAllData(0, 0);
        }
    } else {
        // FIXME: Open the window here.
        DebugBreak();
    }
}

void PluginViewWin::requestTimerFired(Timer<PluginViewWin>* timer)
{
    ASSERT(timer == &m_requestTimer);
    ASSERT(m_requests.size() > 0);

    PluginRequestWin* request = m_requests[0];
    m_requests.remove(0);

    performRequest(request);

    if (m_requests.size() > 0)
        m_requestTimer.startOneShot(0);
}

void PluginViewWin::scheduleRequest(PluginRequestWin* request)
{
    m_requests.append(request);
    m_requestTimer.startOneShot(0);
}

NPError PluginViewWin::loadURL(const String& method, const KURL& url, const String& target, void* notifyData, bool sendNotification)
{
    ASSERT(method == "GET" || method == "POST");

    if (!url.isValid())
        return NPERR_INVALID_URL;

    // FIXME: don't let a plugin start any loads if it is no longer part of a document that is being 
    // displayed

    String jsString = scriptStringIfJavaScriptURL(url);
    if (!jsString.isNull()) {
        if (!m_parentFrame->jScriptEnabled()) {
            // Return NPERR_GENERIC_ERROR if JS is disabled. This is what Mozilla does.
            return NPERR_GENERIC_ERROR;
        } else if (target.isNull() && m_mode == NP_FULL) {
            // Don't allow a JavaScript request from a standalone plug-in that is self-targetted
            // because this can cause the user to be redirected to a blank page (3424039).
            return NPERR_INVALID_PARAM;
        }
    }

    if (!jsString.isNull() || !target.isNull()) {
        if (!jsString.isNull() && !target.isNull() && m_parentFrame->tree()->find(target) != m_parentFrame) {
            // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
            return NPERR_INVALID_PARAM;
        }

        PluginRequestWin* request = new PluginRequestWin(url, target, notifyData, sendNotification);
        scheduleRequest(request);
    } else {
        PluginStreamWin* stream = new PluginStreamWin(this, m_parentFrame->document()->docLoader(), method, url, notifyData, sendNotification);
        m_streams.add(stream);

        stream->start();
    }

    return NPERR_NO_ERROR;
}

static KURL makeURL(const KURL& baseURL, const char* relativeURLString)
{
    DeprecatedString urlString = DeprecatedString::fromLatin1(relativeURLString);

    // Strip return characters
    urlString.replace('\n', "");
    urlString.replace('\r', "");

    return KURL(baseURL, urlString);
}

NPError PluginViewWin::getURLNotify(const char* url, const char* target, void* notifyData)
{
    return loadURL("GET", makeURL(m_url, url), DeprecatedString::fromLatin1(target), notifyData, true);
}

NPError PluginViewWin::getURL(const char* url, const char* target)
{
    return loadURL("GET", makeURL(m_url, url), DeprecatedString::fromLatin1(target), 0, false);
}

NPError PluginViewWin::postURLNotify(const char* url, const char* target, uint32 len, const char* but, NPBool file, void* notifyData)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

NPError PluginViewWin::postURL(const char* url, const char* target, uint32 len, const char* but, NPBool file)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

NPError PluginViewWin::newStream(NPMIMEType type, const char* target, NPStream** stream)
{
    // Unsupported
    return NPERR_GENERIC_ERROR;
}

int32 PluginViewWin::write(NPStream* stream, int32 len, void* buffer)
{
    // Unsupported
    return -1;
}

NPError PluginViewWin::destroyStream(NPStream* stream, NPReason reason)
{
    // Unsupported
    return NPERR_GENERIC_ERROR;
}

const char* PluginViewWin::userAgent()
{
    if (!m_userAgent.isNull())
        return m_userAgent;

    m_userAgent = m_parentFrame->userAgent().utf8();

    return m_userAgent;
}

void PluginViewWin::status(const char* message)
{
    String s = DeprecatedString::fromLatin1(message);

    m_parentFrame->setStatusBarText(s);
}

NPError PluginViewWin::getValue(NPNVariable variable, void* value)
{
    switch (variable) {
        case NPNVWindowNPObject: {
            NPObject* windowScriptObject = m_parentFrame->windowScriptNPObject();

            // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
            if (windowScriptObject)
                _NPN_RetainObject(windowScriptObject);

            void** v = (void**)value;
            *v = windowScriptObject;
            
            return NPERR_NO_ERROR;
        }
        default:
            DebugBreak();
            return NPERR_GENERIC_ERROR;
    }
}

NPError PluginViewWin::setValue(NPPVariable variable, void* value)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

PluginViewWin::~PluginViewWin()
{
    stop();

    deleteAllValues(m_requests);
    deleteAllValues(m_streams);

    freeStringArray(m_paramNames, m_paramCount);
    freeStringArray(m_paramValues, m_paramCount);
}

PluginViewWin::PluginViewWin(FrameWin* parentFrame, PluginPackageWin* plugin, Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType)
    : m_parentFrame(parentFrame)
    , m_plugin(plugin)
    , m_isStarted(false)
    , m_url(url)
    , m_requestTimer(this, &PluginViewWin::requestTimerFired)
{
    registerPluginView();

    m_instance = &m_instanceStruct;
    m_instance->ndata = this;

    m_mimeType = mimeType.utf8();

    m_paramCount = paramNames.size();
    m_paramNames = createUTF8StringArray(paramNames);
    m_paramValues = createUTF8StringArray(paramValues);

    m_mode = element->document()->isPluginDocument() ? NP_FULL : NP_EMBED;

    HWND pluginWnd = CreateWindowEx(0, kWebPluginViewWindowClassName, 0, WS_CHILD, 
                        0, 0, 0, 0, m_parentFrame->view()->windowHandle(), 0, Widget::instanceHandle, 0);

    SetProp(pluginWnd, kWebPluginViewProperty, this);
    setWindowHandle(pluginWnd);

    m_window.type = NPWindowTypeWindow;
    m_window.window = pluginWnd;

    start();
}

} // namespace WebCore
