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
#include "WebKitDLL.h"

#include "WebLocalizableStrings.h"

#pragma warning(push, 0)
#include <WebCore/PlatformString.h>
#pragma warning(pop)

#include <WTF/Assertions.h>
#include <CoreFoundation/CoreFoundation.h>

WebLocalizableStringsBundle WebKitLocalizableStringsBundle = { "com.apple.WebKit", 0 };

CFBundleRef localizedStringsMainBundle;

static CFBundleRef createWebKitBundle()
{
    WCHAR pathStr[MAX_PATH];
    DWORD length = ::GetModuleFileNameW(gInstance, pathStr, MAX_PATH);
    if (!length || GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        return 0;

    bool found = false;
    for (int i=length; i>0; i--) {
        if (pathStr[i] == L'\\') {
            pathStr[i] = 0;
            found = true;
            break;
        }
    }
    if (!found)
        return 0;

    if (wcscat_s(pathStr, MAX_PATH, L"\\WebKit.resources"))
        return 0;

    WebCore::String bundlePathString(pathStr);
    CFStringRef bundlePathCFString = bundlePathString.createCFString();
    if (!bundlePathCFString)
        return 0;

    CFURLRef bundleURLRef = CFURLCreateWithFileSystemPath(0, bundlePathCFString, kCFURLWindowsPathStyle, true);
    CFRelease(bundlePathCFString);
    if (!bundleURLRef)
        return 0;

    CFBundleRef bundle = CFBundleCreate(0, bundleURLRef);
    CFRelease(bundleURLRef);
    return bundle;
}

void SetWebLocalizedStringMainBundle(CFBundleRef bundle)
{
    if (bundle)
        CFRetain(bundle);
    if (localizedStringsMainBundle)
        CFRelease(localizedStringsMainBundle);
    localizedStringsMainBundle = bundle;
}

CFStringRef WebLocalizedString(WebLocalizableStringsBundle* stringsBundle, const char* key)
{
    CFBundleRef bundle;
    if (!stringsBundle) {
        static CFBundleRef mainBundle;
        if (!mainBundle) {
            mainBundle = localizedStringsMainBundle;
            ASSERT(mainBundle);
        }
        bundle = mainBundle;
    } else {
        bundle = stringsBundle->bundle;
        if (!bundle) {
            bundle = createWebKitBundle();
            ASSERT(bundle);
            stringsBundle->bundle = bundle;
        }
    }
    static CFStringRef notFound = CFSTR("localized string not found");
    CFStringRef keyString = CFStringCreateWithCStringNoCopy(0, key, kCFStringEncodingUTF8, kCFAllocatorNull);
    CFStringRef result = CFCopyLocalizedStringWithDefaultValue(keyString, 0, bundle, notFound, 0);
    CFRelease(keyString);
    ASSERT_WITH_MESSAGE(result != notFound, "could not find localizable string %s in bundle", key);
    return result;
}
