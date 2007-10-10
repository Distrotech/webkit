/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import <Foundation/Foundation.h>

// nil-checked CFRetain/CFRelease covers.

FOUNDATION_STATIC_INLINE id KWQRetain(id obj) {
    if (obj) CFRetain(obj);
    return obj;
}

FOUNDATION_STATIC_INLINE void KWQRelease(id obj) {
    if (obj) CFRelease(obj);
}

// As if KWQ and Foundation had logically separate reference counts,
// this function first increments the KWQ retain count, and then
// decrements the NS retain count. These functions are needed
// to handle cases where -retain/-release aren't equivalent
// to KWQRetain/KWQRelease, such as when GC is used.

FOUNDATION_STATIC_INLINE id KWQRetainNSRelease(id obj) {
    return [KWQRetain(obj) release];
}

FOUNDATION_STATIC_INLINE id NSRetainKWQRelease(id obj) {
    KWQRelease([obj retain]);
    return obj;
}

#if 0
// XXX_PCB the alternate proposal...
@interface NSObject (KWQFoundationExtras)
- (id)_KWQ_retain_CFRelease;        // increments the NS retain count, then decrements the CF retain count.
- (id)_KWQ_CFRetain_release;        // increments the CF retain count, then decrements the NS retain count.
- (id)_KWQ_CFRetain;                // increments the CF retain count.
- (void)_KWQ_CFRelease;             // decrements the CF retain count.
@end
#endif

#if defined(MAC_OS_X_VERSION_10_2) && (MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_2)

@interface NSAutoreleasePool (KWQFoundationExtras)
- (void)drain;
@end

#endif
