/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "LayoutTestController.h"

#include "EditingDelegate.h"

#include <wtf/Platform.h>
#include <JavaScriptCore/JavaScriptCore.h>

static JSValueRef dumpAsTextCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    dumpAsText = true;   
    return JSValueMakeUndefined(context);
}

static JSValueRef waitUntilDoneCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    waitToDump = true;
    return JSValueMakeUndefined(context);
}

static JSValueRef notifyDoneCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (waitToDump && !topLoadingFrame)
        dump();
    waitToDump = false;

    return JSValueMakeUndefined(context);
}

static JSValueRef dumpEditingCallbacksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    shouldDumpEditingCallbacks = true;

    return JSValueMakeUndefined(context);
}

static JSValueRef setAcceptsEditingCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    bool acceptsEditing = JSValueToBoolean(context, arguments[0]);

    IWebView* webView;
    if (SUCCEEDED(frame->webView(&webView))) {
        IWebViewEditing* viewEditing;
        if (SUCCEEDED(webView->QueryInterface(IID_IWebViewEditing, (void**)&viewEditing))) {
            IWebEditingDelegate* delegate;
            if (SUCCEEDED(viewEditing->editingDelegate(&delegate))) {
                EditingDelegate* editingDelegate = (EditingDelegate*)delegate;
                editingDelegate->setAcceptsEditing(acceptsEditing);
                delegate->Release();
            }
            viewEditing->Release();
        }
        webView->Release();
    }

    return JSValueMakeUndefined(context);
}

static JSStaticFunction staticFunctions[] = {
    { "dumpAsText", dumpAsTextCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "waitUntilDone", waitUntilDoneCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "notifyDone", notifyDoneCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "dumpEditingCallbacks", dumpEditingCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { "setAcceptsEditing", setAcceptsEditingCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0 }
};

static JSClassRef getClass(JSContextRef context) {
    static JSClassRef layoutTestControllerClass = 0;

    if (!layoutTestControllerClass) {
        JSClassDefinition classDefinition = { 0 };
        classDefinition.staticFunctions = staticFunctions;

        layoutTestControllerClass = JSClassCreate(&classDefinition);
    }

    return layoutTestControllerClass;
}

JSObjectRef makeLayoutTestController(JSContextRef context)
{
    return JSObjectMake(context, getClass(context), 0);
}

