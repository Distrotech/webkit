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
#include <windows.h>
#include "npapi.h"

void* NPN_MemAlloc(uint32 size)
{
    return malloc(size);
}

void NPN_MemFree(void* ptr)
{
    free(ptr);
}

uint32 NPN_MemFlush(uint32 size)
{
    // Do nothing
    return 0;
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
    DebugBreak();
}

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_GetURLNotify(NPP instance, const char* url, const char* target, void* notifyData)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_GetURL(NPP instance, const char* url, const char* target)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32 len, const char* but, NPBool file, void* notifyData)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_PostURL(NPP instance, const char* url, const char* target, uint32 len, const char* buf, NPBool file)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

int32 NPN_Write(NPP instance, NPStream* stream, int32 len, void* buffer)
{
    DebugBreak();
    return 0;
}

NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

const char* NPN_UserAgent(NPP instance)
{
    DebugBreak();
    return 0;
}

void NPN_Status(NPP instance, const char* message)
{
    DebugBreak();
}

void NPN_InvalidateRect(NPP instance, NPRect* invalidRect)
{
    DebugBreak();
}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
    DebugBreak();
}

void NPN_ForceRedraw(NPP instance)
{
    DebugBreak();
}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void* value)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void* value)
{
    DebugBreak();
    return NPERR_GENERIC_ERROR;
}

void* NPN_GetJavaEnv()
{
    // Unsupported
    return 0;
}

void* NPN_GetJavaPeer(NPP instance)
{
    // Unsupported
    return 0;
}
