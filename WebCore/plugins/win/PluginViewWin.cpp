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
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PluginPackageWin.h"
#include "kjs_binding.h"
#include "kjs_proxy.h"
#include "npruntime_impl.h"
#include "PluginDebug.h"
#include "PluginPackageWin.h"
#include "PluginStreamWin.h"

using KJS::Interpreter;
using KJS::JSLock;
using KJS::JSObject;
using KJS::JSValue;

namespace WebCore {

using namespace HTMLNames;

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

    ASSERT(Page::instanceHandle());

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_DBLCLKS;
    wcex.lpfnWndProc    = PluginViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = Page::instanceHandle();
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

void PluginViewWin::setFrameGeometry(const IntRect& rect)
{
    if (rect == frameGeometry())
        return;

    if (HWND window = m_window) {
        IntRect windowRect = parent() ? parent()->convertToContainingWindow(rect) : rect;
        MoveWindow(window, windowRect.x(), windowRect.y(), windowRect.width(), windowRect.height(), false);
    }

    setNPWindowSize(rect.size());

    Widget::setFrameGeometry(rect);
}

void PluginViewWin::setFocus()
{
    if (HWND window = m_window)
        SetFocus(window);

    Widget::setFocus();
}

void PluginViewWin::clearFocus()
{
    SetFocus(0);

    Widget::clearFocus();
}

void PluginViewWin::show()
{
    if (HWND window = m_window)
        ShowWindow(window, SW_SHOWNA);

    Widget::show();
}

void PluginViewWin::hide()
{
    if (HWND window = m_window)
        ShowWindow(window, SW_HIDE);

    Widget::hide();
}

void PluginViewWin::setNPWindowSize(const IntSize& size)
{
    m_npWindow.x = 0;
    m_npWindow.y = 0;
    m_npWindow.width = size.width();
    m_npWindow.height = size.height();

    m_npWindow.clipRect.left = 0;
    m_npWindow.clipRect.top = 0;
    m_npWindow.clipRect.right = size.width();
    m_npWindow.clipRect.bottom = size.height();

    if (m_plugin->pluginFuncs()->setwindow)
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
}

bool PluginViewWin::start()
{
    if (m_isStarted)
        return false;

    ASSERT(m_plugin);
    ASSERT(m_plugin->pluginFuncs()->newp);

    PluginViewWin::setCurrentPluginView(this);
    NPError npErr = m_plugin->pluginFuncs()->newp((NPMIMEType)(const char*)m_mimeType, m_instance, m_mode, m_paramCount, m_paramNames, m_paramValues, NULL);
    LOG_NPERROR(npErr);
    PluginViewWin::setCurrentPluginView(0);
        
    m_isStarted = true;

    if (m_url.isValid())
        loadURL("GET", m_url, String(), 0, false, 0, 0, 0);

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
    m_npWindow.window = 0;
    if (m_plugin->pluginFuncs()->setwindow)
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);

    // Destroy the plugin
    NPError npErr = m_plugin->pluginFuncs()->destroy(m_instance, 0);
    LOG_NPERROR(npErr);

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
            stream->startStream(request->url(), cstr.length(), 0, "text/plain");
            stream->receivedData(0, cstr, cstr.length());
            stream->receivedAllData(0, 0);
        }
    } else {
        // FIXME: radar://4730678 Support opening windows
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

NPError PluginViewWin::loadURL(const String& method, const KURL& url, const String& target, void* notifyData, bool sendNotification, HashMap<String, String>* headers, const char* postData, unsigned postDataLength)
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
        if (headers)
            stream->setRequestHeaders(*headers);
        if (postData)
            stream->setPostData(postData, postDataLength);

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
    return loadURL("GET", makeURL(m_url, url), DeprecatedString::fromLatin1(target), notifyData, true, 0, 0, 0);
}

NPError PluginViewWin::getURL(const char* url, const char* target)
{
    return loadURL("GET", makeURL(m_url, url), DeprecatedString::fromLatin1(target), 0, false, 0, 0, 0);
}

static inline bool startsWithBlankLine(const Vector<char>& buffer)
{
    return buffer.size() > 0 && buffer[0] == '\n';
}

static inline int locationAfterFirstBlankLine(const Vector<char>& buffer)
{
    const char* bytes = buffer.data();
    unsigned length = buffer.size();

    for (unsigned i = 0; i < length - 4; i++) {
        // Support for Acrobat. It sends "\n\n".
        if (bytes[i] == '\n' && bytes[i + 1] == '\n')
            return i + 2;
        
        // Returns the position after 2 CRLF's or 1 CRLF if it is the first line.
        if (bytes[i] == '\r' && bytes[i + 1] == '\n') {
            i += 2;
            if (i == 2)
                return i;
            else if (bytes[i] == '\n')
                // Support for Director. It sends "\r\n\n" (3880387).
                return i + 1;
            else if (bytes[i] == '\r' && bytes[i + 1] == '\n')
                // Support for Flash. It sends "\r\n\r\n" (3758113).
                return i + 2;
        }
    }

    return -1;
}

static inline const char* findEOL(const char* bytes, unsigned length)
{
    // According to the HTTP specification EOL is defined as
    // a CRLF pair. Unfortunately, some servers will use LF
    // instead. Worse yet, some servers will use a combination
    // of both (e.g. <header>CRLFLF<body>), so findEOL needs
    // to be more forgiving. It will now accept CRLF, LF or
    // CR.
    //
    // It returns NULL if EOLF is not found or it will return
    // a pointer to the first terminating character.
    for (unsigned i = 0; i < length; i++) {
        if (bytes[i] == '\n')
            return bytes + i;
        if (bytes[i] == '\r') {
            // Check to see if spanning buffer bounds
            // (CRLF is across reads). If so, wait for
            // next read.
            if (i + 1 == length)
                break;

            return bytes + i;
        }
    }

    return 0;
}

static inline String capitalizeRFC822HeaderFieldName(const String& name)
{
    bool capitalizeCharacter = true;
    String result;

    for (unsigned i = 0; i < name.length(); i++) {
        UChar c;

        if (capitalizeCharacter && name[i] >= 'a' && name[i] <= 'z')
            c = toupper(name[i]);
        else if (!capitalizeCharacter && name[i] >= 'A' && name[i] <= 'Z')
            c = tolower(name[i]);
        else
            c = name[i];

        if (name[i] == '-')
            capitalizeCharacter = true;
        else
            capitalizeCharacter = false;

        result.append(c);
    }

    return result;
}

static inline HashMap<String, String> parseRFC822HeaderFields(const Vector<char>& buffer, unsigned length)
{
    const char* bytes = buffer.data();
    const char* eol;
    String lastKey;
    HashMap<String, String> headerFields;

    // Loop ove rlines until we're past the header, or we can't find any more end-of-lines
    while ((eol = findEOL(bytes, length))) {
        const char* line = bytes;
        int lineLength = eol - bytes;
        
        // Move bytes to the character after the terminator as returned by findEOL.
        bytes = eol + 1;
        if ((*eol == '\r') && (*bytes == '\n'))
            bytes++; // Safe since findEOL won't return a spanning CRLF.

        length -= (bytes - line);
        if (lineLength == 0)
            // Blank line; we're at the end of the header
            break;
        else if (*line == ' ' || *line == '\t') {
            // Continuation of the previous header
            if (lastKey.isNull()) {
                // malformed header; ignore it and continue
                continue;
            } else {
                // Merge the continuation of the previous header
                String currentValue = headerFields.get(lastKey);
                String newValue = DeprecatedString::fromLatin1(line, lineLength);

                headerFields.set(lastKey, currentValue + newValue);
            }
        } else {
            // Brand new header
            const char* colon;
            for (colon = line; *colon != ':' && colon != eol; colon++) {
                // empty loop
            }
            if (colon == eol) 
                // malformed header; ignore it and continue
                continue;
            else {
                lastKey = capitalizeRFC822HeaderFieldName(DeprecatedString::fromLatin1(line, colon - line));
                String value;

                for (colon++; colon != eol; colon++) {
                    if (*colon != ' ' && *colon != '\t')
                        break;
                }
                if (colon == eol)
                    value = "";
                else
                    value = DeprecatedString::fromLatin1(colon, eol - colon);

                String oldValue = headerFields.get(lastKey);
                if (!oldValue.isNull()) {
                    String tmp = oldValue;
                    tmp += ", ";
                    tmp += value;
                    value = tmp;
                }

                headerFields.set(lastKey, value);
            }
        }
    }

    return headerFields;
}

NPError PluginViewWin::handlePost(const char* url, const char* target, uint32 len, const char* buf, bool file, void* notifyData, bool sendNotification, bool allowHeaders)
{
    if (!url || !len || !buf)
        return NPERR_INVALID_PARAM;

    HashMap<String, String> headerFields;
    Vector<char> buffer;
    
    if (file) {
        String filename = DeprecatedString::fromLatin1(buf, len);

        if (filename.startsWith("file:///"))
            filename = filename.deprecatedString().mid(8);

        // Get file info
        WIN32_FILE_ATTRIBUTE_DATA attrs;
        if (GetFileAttributesExW(filename.charactersWithNullTermination(), GetFileExInfoStandard, &attrs) == 0)
            return NPERR_FILE_NOT_FOUND;

        if (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            return NPERR_FILE_NOT_FOUND;

        HANDLE fileHandle = CreateFileW(filename.charactersWithNullTermination(), FILE_READ_DATA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        
        if (fileHandle == INVALID_HANDLE_VALUE)
            return NPERR_FILE_NOT_FOUND;

        buffer.resize(attrs.nFileSizeLow);

        DWORD bytesRead;
        int retval = ReadFile(fileHandle, buffer.data(), attrs.nFileSizeLow, &bytesRead, 0);

        CloseHandle(fileHandle);

        if (retval == 0 || bytesRead != attrs.nFileSizeLow)
            return NPERR_FILE_NOT_FOUND;
    } else {
        buffer.resize(len);
        memcpy(buffer.data(), buf, len);
    }

    const char* postData = buffer.data();
    int postDataLength = buffer.size();
    
    if (allowHeaders) {
        if (startsWithBlankLine(buffer)) {
            postData++;
            postDataLength--;
        } else {
            int location = locationAfterFirstBlankLine(buffer);
            if (location != -1) {
                // If the blank line is somewhere in the middle of the buffer, everything before is the header
                headerFields = parseRFC822HeaderFields(buffer, location);
                unsigned dataLength = buffer.size() - location;

                // Sometimes plugins like to set Content-Length themselves when they post,
                // but WebFoundation does not like that. So we will remove the header
                // and instead truncate the data to the requested length.
                String contentLength = headerFields.get("Content-Length");

                if (!contentLength.isNull())
                    dataLength = min(contentLength.toInt(), dataLength);
                headerFields.remove("Content-Length");

                postData += location;
                postDataLength = dataLength;
            }
        }
    }

    return loadURL("POST", makeURL(m_url, url), DeprecatedString::fromLatin1(target), notifyData, sendNotification, &headerFields, postData, postDataLength);
}

NPError PluginViewWin::postURLNotify(const char* url, const char* target, uint32 len, const char* buf, NPBool file, void* notifyData)
{
    return handlePost(url, target, len, buf, file, notifyData, true, true);
}

NPError PluginViewWin::postURL(const char* url, const char* target, uint32 len, const char* buf, NPBool file)
{
    // As documented, only allow headers to be specified via NPP_PostURL when using a file.
    return handlePost(url, target, len, buf, file, 0, false, file);
}

NPError PluginViewWin::newStream(NPMIMEType type, const char* target, NPStream** stream)
{
    LOG_NOIMPL();
    // Unsupported
    return NPERR_GENERIC_ERROR;
}

int32 PluginViewWin::write(NPStream* stream, int32 len, void* buffer)
{
    LOG_NOIMPL();
    // Unsupported
    return -1;
}

NPError PluginViewWin::destroyStream(NPStream* stream, NPReason reason)
{
    LOG_NOIMPL();
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

        case NPNVPluginElementNPObject: {
            NPObject* pluginScriptObject = 0;

//            if (m_element->hasTagName(appletTag) || m_element->hasTagName(embedTag) || m_element->hasTagName(objectTag))
//                pluginScriptObject = static_cast<HTMLPlugInElement*>(m_element)->getNPObject();

            // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
            if (pluginScriptObject)
                _NPN_RetainObject(pluginScriptObject);

            void** v = (void**)value;
            *v = pluginScriptObject;

            return NPERR_NO_ERROR;
        }
        default:
            return NPERR_GENERIC_ERROR;
    }
}

NPError PluginViewWin::setValue(NPPVariable variable, void* value)
{
    LOG_NOIMPL();
    return NPERR_GENERIC_ERROR;
}

PluginViewWin::~PluginViewWin()
{
    stop();

    deleteAllValues(m_requests);
    deleteAllValues(m_streams);

    freeStringArray(m_paramNames, m_paramCount);
    freeStringArray(m_paramValues, m_paramCount);

    if (m_window)
        DestroyWindow(m_window);
}

PluginViewWin::PluginViewWin(FrameWin* parentFrame, PluginPackageWin* plugin, Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType)
    : m_parentFrame(parentFrame)
    , m_plugin(plugin)
    , m_element(element)
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
                        0, 0, 0, 0, m_parentFrame->view()->containingWindow(), 0, Page::instanceHandle(), 0);

    SetProp(pluginWnd, kWebPluginViewProperty, this);

    m_npWindow.type = NPWindowTypeWindow;
    m_npWindow.window = pluginWnd;

    m_window = pluginWnd;

    start();
}

} // namespace WebCore
