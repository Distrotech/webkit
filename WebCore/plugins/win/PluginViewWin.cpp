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

#include "Document.h"
#include "Element.h"
#include "FrameWin.h"
#include "PluginPackageWin.h"

namespace WebCore {

PluginViewWin* PluginViewWin::s_currentPluginView = 0;

const LPCWSTR kWebPluginViewWindowClassName = L"WebPluginViewWindowClass";

static LRESULT CALLBACK PluginViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static bool registerPluginView()
{
    static bool haveRegisteredWindowClass = false;
    if (haveRegisteredWindowClass)
        return true;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_DBLCLKS;
    wcex.lpfnWndProc    = PluginViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = sizeof(void*); // PluginViewWin pointer;
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
    PluginViewWin* pluginView = reinterpret_cast<PluginViewWin*>(GetWindowLongPtr(hWnd, 0));

    switch (message) {
        case WM_WINDOWPOSCHANGED:
            pluginView->invokeSetWindow((WINDOWPOS*)lParam);
            break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

void PluginViewWin::invokeSetWindow(WINDOWPOS* windowPos)
{
    if (!windowPos) {
        m_window.window = 0;
    } else {
        m_window.window = windowPos->hwnd;
        m_window.x = windowPos->x;
        m_window.y = windowPos->y;
        m_window.width = windowPos->cx;
        m_window.height = windowPos->cy;
    }

    ASSERT(m_plugin);

    if (m_plugin->pluginFuncs()->setwindow)
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_window);
}

bool PluginViewWin::start()
{
    if (m_isStarted)
        return false;

    ASSERT(m_plugin);
    ASSERT(m_plugin->pluginFuncs()->newp);

    PluginViewWin::setCurrentPluginView(this);
    NPError npErr = m_plugin->pluginFuncs()->newp(m_mimeType, m_instance, m_mode, m_paramCount, m_paramNames, m_paramValues, NULL);
    PluginViewWin::setCurrentPluginView(0);
        
    m_isStarted = true;

    if (m_url.isValid()) {
        // FIXME: Load the URL
    }
    return true;
}

void PluginViewWin::stop()
{
    if (!m_isStarted)
        return;

    m_isStarted = false;

    invokeSetWindow(0);

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
    DeprecatedCString cstr = str.deprecatedString().utf8();
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

NPError PluginViewWin::getURLNotify(const char* url, const char* target, void* notifyData)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

NPError PluginViewWin::getURL(const char* url, const char* target)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
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
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

int32 PluginViewWin::write(NPStream* stream, int32 len, void* buffer)
{
    DebugBreak();
    return 0;
}

NPError PluginViewWin::destroyStream(NPStream* stream, NPReason reason)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

const char* PluginViewWin::userAgent()
{
    if (m_userAgent)
        return m_userAgent;

    m_userAgent = createUTF8String(m_parentFrame->userAgent());

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
        case NPNVWindowNPObject:
            // FIXME: Implement
            return NPERR_GENERIC_ERROR;
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

    freeStringArray(m_paramNames, m_paramCount);
    freeStringArray(m_paramValues, m_paramCount);

    fastFree(m_mimeType);
    fastFree(m_userAgent);
}

PluginViewWin::PluginViewWin(FrameWin* parentFrame, PluginPackageWin* plugin, Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType)
    : m_parentFrame(parentFrame)
    , m_plugin(plugin)
    , m_isStarted(false)
    , m_url(url)
    , m_userAgent(0)
{
    registerPluginView();

    m_instance = &m_instanceStruct;
    m_instance->ndata = this;

    m_mimeType = createUTF8String(mimeType);

    m_paramCount = paramNames.size();
    m_paramNames = createUTF8StringArray(paramNames);
    m_paramValues = createUTF8StringArray(paramValues);

    m_mode = element->document()->isPluginDocument() ? NP_FULL : NP_EMBED;

    HWND pluginWnd = CreateWindowEx(0, kWebPluginViewWindowClassName, 0, WS_CHILD, 
                        0, 0, 0, 0, m_parentFrame->view()->windowHandle(), 0, Widget::instanceHandle, 0);

    SetWindowLongPtr(pluginWnd, 0, (LONG_PTR)this);
    setWindowHandle(pluginWnd);

    m_window.type = NPWindowTypeWindow;
    m_window.window = pluginWnd;

    start();
}

} // namespace WebCore
