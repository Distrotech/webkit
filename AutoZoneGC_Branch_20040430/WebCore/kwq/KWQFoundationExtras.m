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

#import "KWQFoundationExtras.h"

#if 0
// XXX_PCB the alternate proposal, subsumed by KWQRetain/KWQRelease/KWQRetainNSRelease.
#import <CoreFoundation/CFBase.h>
#import <objc/objc-auto.h>

@implementation NSObject (KWQFoundationExtras)

/**
 * What are these strange looking methods doing? Under GC, they are
 * manipulating the underlying allocator's retain count, but under
 * non-GC they aren't doing anything at all.
 */

- (id)_KWQ_retain_CFRelease {
    CFRelease([self retain]);
    return self;
}

- (id)_KWQ_CFRetain_release {
    [(id)CFRetain(self) release];
    return self;
}

// nil-checking covers for CFRetain/CFRelease.

- (id)_KWQ_CFRetain {
    return (id)CFRetain(self);
}

- (void)_KWQ_CFRelease {
    CFRelease(self);
}

@end
#endif

#if defined(MAC_OS_X_VERSION_10_3) && (MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_3)

// category implementation of -[NSAutoreleasePool drain] for Panther and earlier.

@implementation NSAutoreleasePool (KWQFoundationExtras)

- (void)drain {
    [self release];
}

@end

#endif
