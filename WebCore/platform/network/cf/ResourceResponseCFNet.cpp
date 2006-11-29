// -*- mode: c++; c-basic-offset: 4 -*-
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
#include "ResourceResponseCFNet.h"

#include "ResourceResponse.h"
#include <CFNetwork/CFURLResponsePriv.h>

using std::min;

// We would like a better value for a maximum time_t,
// but there is no way to do that in C with any certainty.
// INT_MAX should work well enough for our purposes.
#define MAX_TIME_T ((time_t)INT_MAX)    

namespace WebCore {

void ResourceResponse::doUpdateResourceResponse()
{
    if (!m_cfResponse.get())
        return;

    // FIXME: We may need to do MIME type sniffing here (unless that is done in CFURLResponseGetMIMEType).

    m_url = CFURLResponseGetURL(m_cfResponse.get());
    m_mimeType = CFURLResponseGetMIMEType(m_cfResponse.get());
    m_expectedContentLength = CFURLResponseGetExpectedContentLength(m_cfResponse.get());
    m_textEncodingName = CFURLResponseGetTextEncodingName(m_cfResponse.get());

    m_suggestedFilename = "";

    CFAbsoluteTime expiration = CFURLResponseGetExpirationTime(m_cfResponse.get());
    m_expirationDate = min((time_t)(expiration + kCFAbsoluteTimeIntervalSince1970), MAX_TIME_T);

    CFAbsoluteTime lastModified = CFURLResponseGetLastModifiedDate(m_cfResponse.get());
    m_lastModifiedDate = min((time_t)(lastModified + kCFAbsoluteTimeIntervalSince1970), MAX_TIME_T);

    CFHTTPMessageRef httpResponse = CFURLResponseGetHTTPResponse(m_cfResponse.get());
    if (httpResponse) {
        m_httpStatusCode = CFHTTPMessageGetResponseStatusCode(httpResponse);

        CFStringRef statusLine = CFHTTPMessageCopyResponseStatusLine(httpResponse);
        String statusText(statusLine);
        CFRelease(statusLine);
        int spacePos = statusText.find(" ");
        if (spacePos != -1)
            statusText = statusText.substring(spacePos + 1);
        m_httpStatusText = statusText;

        CFDictionaryRef headers = CFHTTPMessageCopyAllHeaderFields(httpResponse);
        CFIndex headerCount = CFDictionaryGetCount(headers);
        Vector<const void*, 128> keys(headerCount);
        Vector<const void*, 128> values(headerCount);
        CFDictionaryGetKeysAndValues(headers, keys.data(), values.data());
        for (int i = 0; i < headerCount; ++i)
            m_httpHeaderFields.set((CFStringRef)keys[i], (CFStringRef)values[i]);
    }

}

}
