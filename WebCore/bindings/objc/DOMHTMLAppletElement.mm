/*
 * Copyright (C) 2004-2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#import "config.h"
#import "DOMHTMLAppletElement.h"

#import "DOMInternal.h"
#import "HTMLAppletElement.h"
#import "PlatformString.h"

@implementation DOMHTMLAppletElement

- (WebCore::HTMLAppletElement *)_appletElement
{
    return static_cast<WebCore::HTMLAppletElement *>(DOM_cast<WebCore::Node *>(_internal));
}

- (NSString *)align
{
    return [self _appletElement]->align();
}

- (void)setAlign:(NSString *)newAlign
{
    [self _appletElement]->setAlign(newAlign);
}

- (NSString *)alt
{
    return [self _appletElement]->alt();
}

- (void)setAlt:(NSString *)newAlt
{
    [self _appletElement]->setAlt(newAlt);
}

- (NSString *)archive
{
    return [self _appletElement]->archive();
}

- (void)setArchive:(NSString *)newArchive
{
    [self _appletElement]->setArchive(newArchive);
}

- (NSString *)code
{
    return [self _appletElement]->code();
}

- (void)setCode:(NSString *)newCode
{
    [self _appletElement]->setCode(newCode);
}

- (NSString *)codeBase
{
    return [self _appletElement]->codeBase();
}

- (void)setCodeBase:(NSString *)newCodeBase
{
    [self _appletElement]->setCodeBase(newCodeBase);
}

- (NSString *)height
{
    return [self _appletElement]->height();
}

- (void)setHeight:(NSString *)newHeight
{
    [self _appletElement]->setHeight(newHeight);
}

//FIXME: DOM spec says hspace should be a DOMString, not an int
- (int)hspace
{
    return [self _appletElement]->hspace().toInt();
}

- (void)setHspace:(int)newHspace
{
    [self _appletElement]->setHspace(WebCore::String::number(newHspace));
}

- (NSString *)name
{
    return [self _appletElement]->name();
}

- (void)setName:(NSString *)newName
{
    [self _appletElement]->setName(newName);
}

- (NSString *)object
{
    return [self _appletElement]->object();
}

- (void)setObject:(NSString *)newObject
{
    [self _appletElement]->setObject(newObject);
}

//FIXME: DOM spec says vspace should be a DOMString, not an int
- (int)vspace
{
    return [self _appletElement]->vspace().toInt();
}

- (void)setVspace:(int)newVspace
{
    [self _appletElement]->setVspace(WebCore::String::number(newVspace));
}

- (NSString *)width
{
    return [self _appletElement]->width();
}

- (void)setWidth:(NSString *)newWidth
{
    [self _appletElement]->setWidth(newWidth);
}

@end
