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

#include <atlstr.h>
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

bool done;
// This is the topmost frame that is loading, during a given load, or nil when no load is 
// in progress.  Usually this is the same as the main frame, but not always.  In the case
// where a frameset is loaded, and then new content is loaded into one of the child frames,
// that child frame is the "topmost frame that is loading".
IWebFrame* topLoadingFrame;     // !nil iff a load is in progress

bool dumpAsText = false;
bool waitToDump = false;
bool shouldDumpEditingCallbacks = false;
bool testRepaint = false;
bool repaintSweepHorizontally = false;

IWebFrame* frame;
HWND webViewWindow;
static HWND hostWindow;

static const unsigned timeoutValue = 7000;
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
        _T("AHEM____.ttf"),
        _T("Apple Chancery.ttf"),
        _T("Arial Bold Italic.ttf"),
        _T("Arial Bold.ttf"),
        _T("Arial Italic.ttf"),
        _T("Arial.ttf"),
        _T("Courier Bold.ttf"),
        _T("Courier.ttf"),
        _T("Helvetica Bold.ttf"),
        _T("Helvetica.ttf"),
        _T("Lucida Grande.ttf"),
        _T("Lucida Grande Bold.ttf"),
        _T("Monaco.ttf"),
        _T("Papyrus.ttf"),
        _T("Times Bold Italic.ttf"),
        _T("Times Bold.ttf"),
        _T("Times Italic.ttf"),
        _T("Times Roman.ttf")
    };

    CString exePath;
    GetModuleFileName(hModule, exePath.GetBuffer(MAX_PATH), MAX_PATH);
    exePath.ReleaseBuffer();
    int lastSlash = exePath.ReverseFind('\\');
    if (lastSlash != -1 && lastSlash + 1< exePath.GetLength())
        exePath = exePath.Left(lastSlash + 1);
    exePath += _T("DumpRenderTree.resources\\");

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

void dump()
{
    BSTR resultString = 0;

    if (dumpTree) {
        if (dumpAsText) {
            IDOMDocument* document;
            frame->DOMDocument(&document);

            IDOMElement* documentElement;
            document->documentElement(&documentElement);

            IDOMHTMLElement* htmlElement;
            HRESULT hr = documentElement->QueryInterface(IID_IDOMHTMLElement, (void**)&htmlElement);
            if (SUCCEEDED(hr)) {
                htmlElement->innerText(&resultString);
                htmlElement->Release();
            } else
                documentElement->textContent(&resultString);
            documentElement->Release();
            document->Release();
        } else {
            IWebFramePrivate* framePrivate;
            HRESULT hr = frame->QueryInterface(IID_IWebFramePrivate, (void**)&framePrivate);
            if (FAILED(hr))
                goto fail;
            framePrivate->renderTreeAsExternalRepresentation(&resultString);
            framePrivate->Release();
        }
        
        if (!resultString)
            printf("ERROR: nil result from %s", dumpAsText ? "IDOMElement::innerText" : "IFrameViewPrivate::renderTreeAsExternalRepresentation");
        else {
            wprintf(L"%s", resultString);
            if (dumpAsText)
                printf("\n");
            SysFreeString(resultString);
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
    BSTR urlBStr;
    static BSTR methodBStr = 0;
 
    if (!methodBStr)
        methodBStr = SysAllocString(TEXT("GET"));

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

    done = false;
    topLoadingFrame = 0;
    waitToDump = false;
    dumpAsText = false;
    shouldDumpEditingCallbacks = false;
    testRepaint = false;
    repaintSweepHorizontally = false;
    timedOut = false;

    // Set the test timeout timer
    SetTimer(hostWindow, timeoutId, timeoutValue, 0);

    IWebMutableURLRequest* request;
    HRESULT hr = CoCreateInstance(CLSID_WebMutableURLRequest, 0, CLSCTX_ALL, IID_IWebMutableURLRequest, (void**)&request);
    if (FAILED(hr))
        goto exit;

    request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 0);
    SysFreeString(urlBStr);

    request->setHTTPMethod(methodBStr);
    frame->loadRequest(request);
    request->Release();

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
    BSTR standardFamily = SysAllocString(L"Times");
    BSTR fixedFamily = SysAllocString(L"Courier");
    BSTR sansSerifFamily = SysAllocString(L"Helvetica");
    BSTR cursiveFamily = SysAllocString(L"Apple Chancery");
    BSTR fantasyFamily = SysAllocString(L"Papyrus");
#else
    BSTR standardFamily = SysAllocString(L"Times New Roman");
    BSTR fixedFamily = SysAllocString(L"Courier New");
    BSTR sansSerifFamily = SysAllocString(L"Arial");
    BSTR cursiveFamily = SysAllocString(L"Comic Sans MS"); // Not actually cursive, but it's what IE and Firefox use.
    BSTR fantasyFamily = SysAllocString(L"Times New Roman");
#endif

    preferences->setStandardFontFamily(standardFamily);
    preferences->setFixedFontFamily(fixedFamily);
    preferences->setSerifFontFamily(standardFamily);
    preferences->setSansSerifFontFamily(sansSerifFamily);
    preferences->setCursiveFontFamily(cursiveFamily);
    preferences->setFantasyFontFamily(fantasyFamily);

    SysFreeString(standardFamily);
    SysFreeString(fixedFamily);
    SysFreeString(sansSerifFamily);
    SysFreeString(cursiveFamily);
    SysFreeString(fantasyFamily);

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
            pthread_t* pthread = (pthread_t*)malloc(sizeof(pthread_t));
            pthread_create(pthread, 0, &runJavaScriptThread, 0);
            pthread_detach(*pthread);

            pthread_t self = pthread_self();
            CFDictionaryRemoveValue(javaScriptThreads(), &self);
            CFDictionaryAddValue(javaScriptThreads(), pthread, 0);

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
        pthread_t* pthread = (pthread_t*)malloc(sizeof(pthread_t));
        pthread_create(pthread, 0, &runJavaScriptThread, 0);
        pthread_detach(*pthread);
        CFDictionaryAddValue(javaScriptThreads(), pthread, 0);
    }

    pthread_mutex_unlock(&javaScriptThreadsMutex);
}

static void stopJavaScriptThreads(void)
{
    pthread_mutex_lock(&javaScriptThreadsMutex);

    javaScriptThreadsShouldTerminate = true;

    pthread_t* pthreads[javaScriptThreadsCount] = {0};
    assert(CFDictionaryGetCount(javaScriptThreads()) == javaScriptThreadsCount);
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

    IWebView* webView;
    HRESULT hr = CoCreateInstance(CLSID_WebView, 0, CLSCTX_ALL, IID_IWebView, (void**)&webView);
    if (FAILED(hr))
        goto exit;

    hr = webView->setHostWindow(hostWindow);
    if (FAILED(hr))
        goto exit;

    RECT clientRect;
    clientRect.bottom = clientRect.left = clientRect.top = clientRect.right = 0;
    hr = webView->initWithFrame(clientRect, 0, 0);
    if (FAILED(hr))
        goto exit;

    IWebViewPrivate* viewPrivate;
    hr = webView->QueryInterface(IID_IWebViewPrivate, (void**)&viewPrivate);
    if (FAILED(hr))
        goto exit;
    viewPrivate->viewWindow(&webViewWindow);
    viewPrivate->Release();

    SetWindowPos(webViewWindow, 0, 0, 0, maxViewWidth, maxViewHeight, 0);
    ShowWindow(hostWindow, SW_SHOW);

    WaitUntilDoneDelegate* waitDelegate = new WaitUntilDoneDelegate();
    webView->setFrameLoadDelegate(waitDelegate);
    waitDelegate->Release();

    webView->setUIDelegate(waitDelegate);

    IWebViewEditing* viewEditing;
    hr = webView->QueryInterface(IID_IWebViewEditing, (void**)&viewEditing);
    if (FAILED(hr))
        goto exit;

    EditingDelegate* editingDelegate = new EditingDelegate;
    viewEditing->setEditingDelegate(editingDelegate);
    editingDelegate->Release();
    viewEditing->Release();

    IWebPreferences* preferences;
    hr = webView->preferences(&preferences);
    if (FAILED(hr))
        goto exit;

    initializePreferences(preferences);

    IWebIconDatabase* iconDatabase;
    IWebIconDatabase* tmpIconDatabase;
    CoCreateInstance(CLSID_WebIconDatabase, 0, CLSCTX_ALL, IID_IWebIconDatabase, (void**)&tmpIconDatabase);

    tmpIconDatabase->sharedIconDatabase(&iconDatabase);
        
    hr = webView->mainFrame(&frame);
    if (FAILED(hr))
        goto exit;

    _CrtMemState entryToMainMemCheckpoint;
    if (leakChecking)
        _CrtMemCheckpoint(&entryToMainMemCheckpoint);

    for (int i = 0; i < argc; ++i)
        if (!stricmp(argv[i], "--threaded")) {
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
    
    webView->close();

    webView->Release();
    iconDatabase->Release();

    if (leakChecking) {
        // dump leaks to stderr
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtMemDumpAllObjectsSince(&entryToMainMemCheckpoint);
    }

    return 0;
exit:
    return -1;
}
