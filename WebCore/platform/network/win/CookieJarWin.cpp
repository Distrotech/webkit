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
#include "KURL.h"
#include "PlatformString.h"
#include "DeprecatedString.h"
#include <windows.h>
#if USE(CFNETWORK)
#include <CoreFoundation/CoreFoundation.h>
#include <CFNetwork/CFHTTPCookiesPriv.h>
#else
#include <Wininet.h>
#endif

namespace WebCore
{

#if USE(CFNETWORK)
    static const CFStringRef s_setCookieKeyCF = CFSTR("Set-Cookie");
    static const CFStringRef s_cookieCF = CFSTR("Cookie");
#endif


void setCookies(const KURL& url, const KURL& policyURL, const String& value)
{
#if USE(CFNETWORK)
    CFURLRef urlCF = url.createCFURL();
    CFURLRef policyUrlCF = policyURL.createCFURL();

    // <http://bugzilla.opendarwin.org/show_bug.cgi?id=6531>, <rdar://4409034>
    // cookiesWithResponseHeaderFields doesn't parse cookies without a value
    String cookieString = value.contains('=') ? value : value + "=";

    CFStringRef cookiesCF = cookieString.createCFString();
    CFDictionaryRef headerCF =  CFDictionaryCreate(kCFAllocatorDefault, (const void**)&s_setCookieKeyCF, 
        (const void**)&cookiesCF, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);


    // Create default store. Don't use CFHTTPCookieStorageGetBaseStorage (it is nyi)
    CFHTTPCookieStorageRef cfCookieStoreCF = CFHTTPCookieStorageCreateFromFile(0, 0, 0);

    _CFHTTPCookieStorageSetCookiesWithResponseHeaderFields(cfCookieStoreCF, urlCF, headerCF,
        policyUrlCF, CFHTTPCookieStorageAcceptPolicyAlways);

    CFRelease(headerCF);
    CFRelease(cookiesCF);
    CFRelease(policyUrlCF);
    CFRelease(urlCF);

    CFRelease(cfCookieStoreCF);
#else
    // FIXME: Deal with the policy URL.
    DeprecatedString str = url.url();
    str.append((UChar)'\0');
    DeprecatedString val = value.deprecatedString();
    val.append((UChar)'\0');
    InternetSetCookie((UChar*)str.unicode(), 0, (UChar*)val.unicode());
#endif
}

String cookies(const KURL& url)
{
#if USE(CFNETWORK)
    String cookieString;
    CFURLRef urlCF = url.createCFURL();
    // Create default store. Don't use CFHTTPCookieStorageGetBaseStorage (it is nyi)
    CFHTTPCookieStorageRef cfCookieStoreCF = CFHTTPCookieStorageCreateFromFile(0, 0, 0);

    CFArrayRef cookiesCF = CFHTTPCookieStorageCopyCookiesForURL(cfCookieStoreCF, urlCF, FALSE);
    CFDictionaryRef headerCF = CFHTTPCookieCopyRequestHeaderFields(kCFAllocatorDefault, cookiesCF);
    CFStringRef valueCF = (CFStringRef)CFDictionaryGetValue(headerCF, s_cookieCF);
    cookieString = String(valueCF); // No need to release valueCF, released by headerCF
    CFRelease(cookiesCF);
    CFRelease(headerCF);
    CFRelease(urlCF);
    CFRelease(cfCookieStoreCF);

    return cookieString;
#else
    DeprecatedString str = url.url();
    str.append((UChar)'\0');

    DWORD count = str.length();
    InternetGetCookie((UChar*)str.unicode(), 0, 0, &count);
    if (count <= 1) // Null terminator counts as 1.
        return String();

    UChar* buffer = new UChar[count];
    InternetGetCookie((UChar*)str.unicode(), 0, buffer, &count);
    String& result = String(buffer, count-1); // Ignore the null terminator.
    delete[] buffer;
    return result;
#endif
}

bool cookiesEnabled()
{
    // FIXME: For now just assume cookies are always on.
    return true;
}

}
