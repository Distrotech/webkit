/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc.  All rights reserved.
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

#include "DumpRenderTree.h"

#include <atlcomcli.h>
#include <atlsafe.h>
#include <atlstr.h>
#include <atltypes.h>
#include <CoreFoundation/CoreFoundation.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <math.h>
#include <pthread.h>
#include <WebKit/IWebURLResponse.h>
#include <WebKit/IWebViewPrivate.h>
#include <WebKit/WebKit.h>
#include <WebKit/IWebFramePrivate.h>
#include <windows.h>

#include "EditingDelegate.h"
#include "WaitUntilDoneDelegate.h"

#define USE_MAC_FONTS

const LPCWSTR kDumpRenderTreeClassName = L"DumpRenderTreeWindow";

static bool dumpTree = true;
static bool printSeparators;
static bool leakChecking = false;
static bool timedOut = false;
static bool threaded = false;

static const char* currentTest;

bool done;
// This is the topmost frame that is loading, during a given load, or nil when no load is 
// in progress.  Usually this is the same as the main frame, but not always.  In the case
// where a frameset is loaded, and then new content is loaded into one of the child frames,
// that child frame is the "topmost frame that is loading".
IWebFrame* topLoadingFrame;     // !nil iff a load is in progress

bool dumpAsText = false;
bool waitToDump = false;
bool shouldDumpEditingCallbacks = false;
bool shouldDumpTitleChanges = false;
bool shouldDumpChildFrameScrollPositions = false;
bool testRepaint = false;
bool repaintSweepHorizontally = false;

IWebFrame* frame;
HWND webViewWindow;
static HWND hostWindow;

static const unsigned timeoutValue = 60000;
static const unsigned timeoutId = 10;

const unsigned maxViewWidth = 800;
const unsigned maxViewHeight = 600;

static LRESULT CALLBACK DumpRenderTreeWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_TIMER:
            // The test ran long enough to time out
            timedOut = true;
            PostQuitMessage(0);
            return 0;
            break;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

extern "C" BOOL InitializeCoreGraphics();

static void initialize(HMODULE hModule)
{
    static LPCTSTR fontsToInstall[] = {
        TEXT("AHEM____.ttf"),
        TEXT("Apple Chancery.ttf"),
        TEXT("Courier Bold.ttf"),
        TEXT("Courier.ttf"),
        TEXT("Helvetica Bold.ttf"),
        TEXT("Helvetica.ttf"),
        TEXT("Helvetica Neue Bold Italic.ttf"),
        TEXT("Helvetica Neue Bold.ttf"),
        TEXT("Helvetica Neue Condensed Black.ttf"),
        TEXT("Helvetica Neue Condensed Bold.ttf"),
        TEXT("Helvetica Neue Italic.ttf"),
        TEXT("Helvetica Neue Light Italic.ttf"),
        TEXT("Helvetica Neue Light.ttf"),
        TEXT("Helvetica Neue UltraLight Italic.ttf"),
        TEXT("Helvetica Neue UltraLight.ttf"),
        TEXT("Helvetica Neue.ttf"),
        TEXT("Lucida Grande.ttf"),
        TEXT("Lucida Grande Bold.ttf"),
        TEXT("Monaco.ttf"),
        TEXT("Papyrus.ttf"),
        TEXT("Times Bold Italic.ttf"),
        TEXT("Times Bold.ttf"),
        TEXT("Times Italic.ttf"),
        TEXT("Times Roman.ttf")
    };

    CString exePath;
    GetModuleFileName(hModule, exePath.GetBuffer(MAX_PATH), MAX_PATH);
    exePath.ReleaseBuffer();
    int lastSlash = exePath.ReverseFind('\\');
    if (lastSlash != -1 && lastSlash + 1< exePath.GetLength())
        exePath = exePath.Left(lastSlash + 1);
    exePath += TEXT("DumpRenderTree.resources\\");

    for (int i = 0; i < ARRAYSIZE(fontsToInstall); ++i)
        AddFontResourceEx(exePath + fontsToInstall[i], FR_PRIVATE, 0);

    // Init COM
    OleInitialize(0);

    // Initialize CG
    InitializeCoreGraphics();

    // Register a host window
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = DumpRenderTreeWndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hModule;
    wcex.hIcon         = 0;
    wcex.hCursor       = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground = 0;
    wcex.lpszMenuName  = 0;
    wcex.lpszClassName = kDumpRenderTreeClassName;
    wcex.hIconSm       = 0;

    RegisterClassEx(&wcex);

    hostWindow = CreateWindowEx(WS_EX_TOOLWINDOW, kDumpRenderTreeClassName, TEXT("DumpRenderTree"), WS_POPUP,
      -maxViewWidth, -maxViewHeight, maxViewWidth, maxViewHeight, 0, 0, hModule, 0);
}

#include <stdio.h>

void dumpFrameScrollPosition(IWebFrame* frame)
{
    if (!frame)
        return;

    CComQIPtr<IWebFramePrivate> framePrivate = frame;
    if (!framePrivate)
        return;

    CSize scrollPosition;
    if (FAILED(framePrivate->scrollOffset(&scrollPosition)))
        return;

    if (abs(scrollPosition.cx) > 0.00000001 || abs(scrollPosition.cy) > 0.00000001) {
        CComPtr<IWebFrame> parent;
        if (FAILED(frame->parentFrame(&parent)))
            return;
        if (parent) {
            CComBSTR name;
            if (FAILED(frame->name(&name)))
                return;
            printf("frame '%S' ", name ? name : L"");
        }
        printf("scrolled to %.f,%.f\n", (double)scrollPosition.cx, (double)scrollPosition.cy);
    }

    if (shouldDumpChildFrameScrollPositions) {
        unsigned kidsCount;
        SAFEARRAY* arrPtr;
        if (FAILED(frame->childFrames(&kidsCount, &arrPtr)))
            return;

        if (arrPtr) {
            CComSafeArray<IUnknown*> kids;
            kids.Attach(arrPtr);
            for (unsigned i = 0; i < kidsCount; ++i)
                dumpFrameScrollPosition(CComQIPtr<IWebFrame>(kids.GetAt(i)));
        }
    }
}

void dump()
{
    CComBSTR resultString;

    if (dumpTree) {
        if (dumpAsText) {
            CComPtr<IDOMDocument> document;
            frame->DOMDocument(&document);

            CComPtr<IDOMElement> documentElement;
            document->documentElement(&documentElement);

            CComQIPtr<IDOMHTMLElement> htmlElement(documentElement);
            if (htmlElement)
                htmlElement->innerText(&resultString);
            else
                documentElement->textContent(&resultString);
        } else {
            bool isSVGW3CTest = strstr(currentTest, "svg\\W3C-SVG-1.1");
            unsigned width;
            unsigned height;
            if (isSVGW3CTest) {
                width = 480;
                height = 360;
            } else {
                width = maxViewWidth;
                height = maxViewHeight;
            }
            ::SetWindowPos(webViewWindow, 0, 0, 0, width, height, SWP_NOMOVE);

            bool isSVGTest = isSVGW3CTest || strstr(currentTest, ".svg");
            if (isSVGTest) {
                ::InvalidateRect(webViewWindow, 0, TRUE);
                ::SendMessage(webViewWindow, WM_PAINT, 0, 0);
            }

            CComQIPtr<IWebFramePrivate> framePrivate(frame);
            if (!framePrivate)
                goto fail;
            framePrivate->renderTreeAsExternalRepresentation(&resultString);
        }
        
        if (!resultString)
            printf("ERROR: nil result from %s", dumpAsText ? "IDOMElement::innerText" : "IFrameViewPrivate::renderTreeAsExternalRepresentation");
        else {
            unsigned stringLength = SysStringLen(resultString);
            int bufferSize = ::WideCharToMultiByte(CP_UTF8, 0, resultString, stringLength, 0, 0, 0, 0);
            char* buffer = (char*)malloc(bufferSize + 1);
            int result = ::WideCharToMultiByte(CP_UTF8, 0, resultString, stringLength, buffer, bufferSize + 1, 0, 0);
            buffer[bufferSize] = '\0';
            printf("%s", buffer);
            free(buffer);
            if (dumpAsText)
                printf("\n");
            else
                dumpFrameScrollPosition(frame);
        }
    }

    if (printSeparators)
        puts("#EOF");
fail:
    // This will exit from our message loop
    PostQuitMessage(0);
    done = true;
    return;
}

static void runTest(const char* pathOrURL)
{
    static CComBSTR methodBStr(TEXT("GET"));

    CComBSTR urlBStr;
 
    CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, pathOrURL, kCFStringEncodingWindowsLatin1);
    CFURLRef url = CFURLCreateWithString(kCFAllocatorDefault, str, 0);

    if (!url)
        url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, str, kCFURLWindowsPathStyle, false);

    CFRelease(str);

    str = CFURLGetString(url);

    CFIndex length = CFStringGetLength(str);
    UniChar* buffer = new UniChar[length];

    CFStringGetCharacters(str, CFRangeMake(0, length), buffer);
    urlBStr = SysAllocStringLen((OLECHAR*)buffer, length);
    delete[] buffer;

    CFRelease(url);

    currentTest = pathOrURL;

    done = false;
    topLoadingFrame = 0;
    waitToDump = false;
    dumpAsText = false;
    shouldDumpEditingCallbacks = false;
    shouldDumpTitleChanges = false;
    shouldDumpChildFrameScrollPositions = false;
    testRepaint = false;
    repaintSweepHorizontally = false;
    timedOut = false;

    // Set the test timeout timer
    SetTimer(hostWindow, timeoutId, timeoutValue, 0);

    CComPtr<IWebMutableURLRequest> request;
    HRESULT hr = request.CoCreateInstance(CLSID_WebMutableURLRequest);
    if (FAILED(hr))
        goto exit;

    request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 0);

    request->setHTTPMethod(methodBStr);
    frame->loadRequest(request);

    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    KillTimer(hostWindow, timeoutId);

    if (timedOut) {
        fprintf(stderr, "ERROR: Timed out running %s\n", pathOrURL);
        printf("ERROR: Timed out loading page\n");

        if (printSeparators)
            puts("#EOF");
    }
exit:
    return;
}

static void initializePreferences(IWebPreferences* preferences)
{
#ifdef USE_MAC_FONTS
    CComBSTR standardFamily("Times");
    CComBSTR fixedFamily("Courier");
    CComBSTR sansSerifFamily("Helvetica");
    CComBSTR cursiveFamily("Apple Chancery");
    CComBSTR fantasyFamily("Papyrus");
#else
    CComBSTR standardFamily("Times New Roman");
    CComBSTR fixedFamily("Courier New");
    CComBSTR sansSerifFamily("Arial");
    CComBSTR cursiveFamily("Comic Sans MS"); // Not actually cursive, but it's what IE and Firefox use.
    CComBSTR fantasyFamily("Times New Roman");
#endif

    preferences->setStandardFontFamily(standardFamily);
    preferences->setFixedFontFamily(fixedFamily);
    preferences->setSerifFontFamily(standardFamily);
    preferences->setSansSerifFontFamily(sansSerifFamily);
    preferences->setCursiveFontFamily(cursiveFamily);
    preferences->setFantasyFontFamily(fantasyFamily);

    preferences->setAutosaves(FALSE);
    preferences->setJavaEnabled(FALSE);
    preferences->setPlugInsEnabled(FALSE);
}

static Boolean pthreadEqualCallback(const void* value1, const void* value2)
{
    return (Boolean)pthread_equal(*(pthread_t*)value1, *(pthread_t*)value2);
}

static CFDictionaryKeyCallBacks pthreadKeyCallbacks = { 0, 0, 0, 0, pthreadEqualCallback, 0 };

static pthread_mutex_t javaScriptThreadsMutex = PTHREAD_MUTEX_INITIALIZER;
static bool javaScriptThreadsShouldTerminate;

static const int javaScriptThreadsCount = 4;
static CFMutableDictionaryRef javaScriptThreads()
{
    assert(pthread_mutex_trylock(&javaScriptThreadsMutex) == EBUSY);
    static CFMutableDictionaryRef staticJavaScriptThreads;
    if (!staticJavaScriptThreads)
        staticJavaScriptThreads = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &pthreadKeyCallbacks, 0);
    return staticJavaScriptThreads;
}

// Loops forever, running a script and randomly respawning, until 
// javaScriptThreadsShouldTerminate becomes true.
void* runJavaScriptThread(void* arg)
{
    const char* const script =
    " \
    var array = []; \
    for (var i = 0; i < 10; i++) { \
        array.push(String(i)); \
    } \
    ";

    while (true) {
        JSGlobalContextRef ctx = JSGlobalContextCreate(0);
        JSStringRef scriptRef = JSStringCreateWithUTF8CString(script);

        JSValueRef exception = 0;
        JSEvaluateScript(ctx, scriptRef, 0, 0, 0, &exception);
        assert(!exception);
        
        JSGlobalContextRelease(ctx);
        JSStringRelease(scriptRef);
        
        JSGarbageCollect(ctx);

        pthread_mutex_lock(&javaScriptThreadsMutex);

        // Check for cancellation.
        if (javaScriptThreadsShouldTerminate) {
            pthread_mutex_unlock(&javaScriptThreadsMutex);
            return 0;
        }

        // Respawn probabilistically.
        if (rand() % 5 == 0) {
            pthread_t pthread;
            pthread_create(&pthread, 0, &runJavaScriptThread, 0);
            pthread_detach(pthread);

            pthread_t self = pthread_self();
            CFDictionaryRemoveValue(javaScriptThreads(), self.p);
            CFDictionaryAddValue(javaScriptThreads(), pthread.p, 0);

            pthread_mutex_unlock(&javaScriptThreadsMutex);
            return 0;
        }

        pthread_mutex_unlock(&javaScriptThreadsMutex);
    }
}

static void startJavaScriptThreads(void)
{
    pthread_mutex_lock(&javaScriptThreadsMutex);

    for (int i = 0; i < javaScriptThreadsCount; i++) {
        pthread_t pthread;
        pthread_create(&pthread, 0, &runJavaScriptThread, 0);
        pthread_detach(pthread);
        CFDictionaryAddValue(javaScriptThreads(), pthread.p, 0);
    }

    pthread_mutex_unlock(&javaScriptThreadsMutex);
}

static void stopJavaScriptThreads(void)
{
    pthread_mutex_lock(&javaScriptThreadsMutex);

    javaScriptThreadsShouldTerminate = true;

    pthread_t* pthreads[javaScriptThreadsCount] = {0};
    int threadDictCount = CFDictionaryGetCount(javaScriptThreads());
    assert(threadDictCount == javaScriptThreadsCount);
    CFDictionaryGetKeysAndValues(javaScriptThreads(), (const void**)pthreads, 0);

    pthread_mutex_unlock(&javaScriptThreadsMutex);

    for (int i = 0; i < javaScriptThreadsCount; i++) {
        pthread_t* pthread = pthreads[i];
        pthread_join(*pthread, 0);
        free(pthread);
    }
}

int main(int argc, char* argv[])
{
    leakChecking = false;

    initialize(GetModuleHandle(0));

    // FIXME: options

    CComPtr<IWebView> webView;
    if (FAILED(webView.CoCreateInstance(CLSID_WebView)))
        return -1;

    if (FAILED(webView->setHostWindow(hostWindow)))
        return -1;

    RECT clientRect;
    clientRect.bottom = clientRect.left = clientRect.top = clientRect.right = 0;
    if (FAILED(webView->initWithFrame(clientRect, 0, 0)))
        return -1;

    CComQIPtr<IWebViewPrivate> viewPrivate(webView);
    if (!viewPrivate)
        return -1;

    if (FAILED(viewPrivate->viewWindow(&webViewWindow)))
        return -1;

    SetWindowPos(webViewWindow, 0, 0, 0, maxViewWidth, maxViewHeight, 0);
    ShowWindow(hostWindow, SW_SHOW);

    CComPtr<WaitUntilDoneDelegate> waitDelegate;
    waitDelegate.Attach(new WaitUntilDoneDelegate);
    if (FAILED(webView->setFrameLoadDelegate(waitDelegate)))
        return -1;

    if (FAILED(webView->setUIDelegate(waitDelegate)))
        return -1;

    CComQIPtr<IWebViewEditing> viewEditing(webView);
    if (!viewEditing)
        return -1;

    CComPtr<EditingDelegate> editingDelegate;
    editingDelegate.Attach(new EditingDelegate);

    if (FAILED(viewEditing->setEditingDelegate(editingDelegate)))
        return -1;

    CComPtr<IWebPreferences> preferences;
    if (FAILED(webView->preferences(&preferences)))
        return -1;

    initializePreferences(preferences);

    CComPtr<IWebIconDatabase> iconDatabase;
    CComPtr<IWebIconDatabase> tmpIconDatabase;
    if (FAILED(tmpIconDatabase.CoCreateInstance(CLSID_WebIconDatabase)))
        return -1;
    if (FAILED(tmpIconDatabase->sharedIconDatabase(&iconDatabase)))
        return -1;
        
    if (FAILED(webView->mainFrame(&frame)))
        return -1;

    _CrtMemState entryToMainMemCheckpoint;
    if (leakChecking)
        _CrtMemCheckpoint(&entryToMainMemCheckpoint);

    for (int i = 0; i < argc; ++i)
        if (!stricmp(argv[i], "--threaded")) {
            argv[i] = argv[argc - 1];
            argc--;
            threaded = true;
            break;
        }

    if (threaded)
        startJavaScriptThreads();

    if (argc == 2 && strcmp(argv[1], "-") == 0) {
        char filenameBuffer[2048];
        printSeparators = true;
        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
            char* newLineCharacter = strchr(filenameBuffer, '\n');
            if (newLineCharacter)
                *newLineCharacter = '\0';
            
            if (strlen(filenameBuffer) == 0)
                continue;

            runTest(filenameBuffer);
            fflush(stdout);
        }
    } else {
        printSeparators = argc > 2;
        for (int i = 1; i != argc; i++)
            runTest(argv[i]);
    }

    if (threaded)
        stopJavaScriptThreads();
    
    frame->Release();
    webView->close();

    if (leakChecking) {
        // dump leaks to stderr
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtMemDumpAllObjectsSince(&entryToMainMemCheckpoint);
    }

    return 0;
}
