/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "DragClientWx.h"

#include <stdio.h>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d (%s)\n", __FILE__, __LINE__, __FUNCTION__); } while(0)

WebCore::DragDestinationAction DragClientWx::actionMaskForDrag(WebCore::DragData*)
{
    return WebCore::DragDestinationActionAny;
}

void DragClientWx::willPerformDragDestinationAction(WebCore::DragDestinationAction,
                                                    WebCore::DragData*)
{
    notImplemented();
}

void DragClientWx::willPerformDragSourceAction(WebCore::DragSourceAction, const WebCore::IntPoint&, WebCore::Clipboard*)
{
    notImplemented();
}

void DragClientWx::dragControllerDestroyed()
{    
}

WebCore::DragSourceAction DragClientWx::dragSourceActionMaskForPoint(const WebCore::IntPoint&)
{
    return WebCore::DragSourceActionAny;
}

void DragClientWx::startDrag(WebCore::DragImageRef dragImage, 
                        const WebCore::IntPoint& dragImageOrigin, 
                        const WebCore::IntPoint& eventPos, WebCore::Clipboard*, 
                        WebCore::Frame*, bool linkDrag)
{
    notImplemented();
}

WebCore::DragImageRef DragClientWx::createDragImageForLink(WebCore::KURL&, const WebCore::String& label, WebCore::Frame*)
{
    return 0;
}
