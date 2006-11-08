/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include <WebKit/IWebURLResponse.h>
#include <WebKit/IWebViewPrivate.h>
#include <WebKit/WebKit.h>
#include <WebKit/IWebFramePrivate.h>
#include "atlstr.h"

#include "WaitUntilDoneDelegate.h"

const LPCWSTR kDumpRenderTreeClassName = L"DumpRenderTreeWindow";

IWebFrame* frame;
HWND hostWindow;

const unsigned maxViewWidth = 800;
const unsigned maxViewHeight = 600;

static void initialize(HINSTANCE hInstance)
{
    // Init COM
    CoInitialize(NULL);

    // Register a host window
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = DefWindowProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = 0;
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = 0;
    wcex.lpszMenuName  = 0;
    wcex.lpszClassName = kDumpRenderTreeClassName;
    wcex.hIconSm       = 0;

    RegisterClassEx(&wcex);

    hostWindow = CreateWindow(kDumpRenderTreeClassName, TEXT("DumpRenderTree"), WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, maxViewWidth, CW_USEDEFAULT, maxViewHeight, 0, 0, hInstance, NULL);

}

static bool dumpTree = true;
bool dumpAsText = false;
bool waitToDump = false;
static bool printSeparators;

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
            _putws(resultString);
            SysFreeString(resultString);
        }
    }

    if (printSeparators)
        puts("#EOF");
fail:
    // This will exit from our message loop
    PostQuitMessage(0);
    return;
}

static void runTest(const char* pathOrURL)
{
    CStringT<char, StrTraitATL<char> > urlString;
    BSTR urlBStr;
    static BSTR methodBStr = 0;
 
    if (!methodBStr)
        methodBStr = SysAllocString(TEXT("GET"));

    if (strncmp(pathOrURL, "http://", 7) == 0) {
        urlString = pathOrURL;
    } else {
        urlString = "file:///";
        urlString += pathOrURL;
    }

    urlBStr = urlString.AllocSysString();
    IWebMutableURLRequest* request;
    HRESULT hr = CoCreateInstance(CLSID_WebMutableURLRequest, 0, CLSCTX_ALL, IID_IWebMutableURLRequest, (void**)&request);
    if (FAILED(hr))
        goto exit;

    request->initWithURL(urlBStr, WebURLRequestUseProtocolCachePolicy, 0);
    SysFreeString(methodBStr);

    request->setHTTPMethod(methodBStr);
    frame->loadRequest(request);
    request->Release();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

exit:
    return;
}

int main(int argc, char* argv[])
{
    initialize(GetModuleHandle(NULL));

    // FIXME: options

    IWebView* webView;
    HRESULT hr = CoCreateInstance(CLSID_WebView, 0, CLSCTX_ALL, IID_IWebView, (void**)&webView);
    if (FAILED(hr))
        goto exit;

    hr = webView->setHostWindow(hostWindow);
    if (FAILED(hr))
        goto exit;

    RECT clientRect;
    clientRect.left = 0;
    clientRect.top = 0;
    clientRect.right = maxViewWidth;
    clientRect.bottom = maxViewHeight;

    hr = webView->initWithFrame(&clientRect, 0, 0);
    if (FAILED(hr))
        goto exit;

    IWebViewPrivate* viewPrivate;
    HWND viewHwnd;
    hr = webView->QueryInterface(IID_IWebViewPrivate, (void**)&viewPrivate);
    if (FAILED(hr))
        goto exit;
    viewPrivate->viewWindow(&viewHwnd);

    MoveWindow(viewHwnd, 0, 0, maxViewWidth, maxViewHeight, false);

    WaitUntilDoneDelegate* waitDelegate = new WaitUntilDoneDelegate();
    webView->setFrameLoadDelegate(waitDelegate);
    waitDelegate->Release();

    webView->setUIDelegate(waitDelegate);
    waitDelegate->Release();

    IWebPreferences* preferences;
    hr = webView->preferences(&preferences);
    if (FAILED(hr))
        goto exit;

    IWebIconDatabase* iconDatabase;
    IWebIconDatabase* tmpIconDatabase;
    CoCreateInstance(CLSID_WebIconDatabase, 0, CLSCTX_ALL, IID_IWebIconDatabase, (void**)&tmpIconDatabase);

    tmpIconDatabase->sharedIconDatabase(&iconDatabase);
        
    hr = webView->mainFrame(&frame);
    if (FAILED(hr))
        goto exit;

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

    webView->Release();
    iconDatabase->Release();

    return 0;
exit:
    return -1;
}
