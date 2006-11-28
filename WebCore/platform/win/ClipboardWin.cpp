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
#include "ClipboardWin.h"
#include "StringHash.h"

namespace WebCore {

bool ClipboardWin::isForDragging() const
{ return false; }

String ClipboardWin::dropEffect() const
{ return String(); }

void ClipboardWin::setDropEffect(const String&)
{}

String ClipboardWin::effectAllowed() const
{ return String(); }

void ClipboardWin::setEffectAllowed(const String&)
{}

void ClipboardWin::clearData(const String& type)
{}

void ClipboardWin::clearAllData()
{}

String ClipboardWin::getData(const String& type, bool& success) const
{ return String(); }

bool ClipboardWin::setData(const String& type, const String& data)
{ return false; }

// extensions beyond IE's API
HashSet<String> ClipboardWin::types() const
{ HashSet<String> results; return results; }

IntPoint ClipboardWin::dragLocation() const
{ return m_dragLoc; }

CachedImage* ClipboardWin::dragImage() const
{ return m_dragImage; }

void ClipboardWin::setDragImage(CachedImage*, const IntPoint&)
{}

Node* ClipboardWin::dragImageElement()
{ return m_dragImageElement.get(); }

void ClipboardWin::setDragImageElement(Node*, const IntPoint&)
{}

void ClipboardWin::setAccessPolicy(ClipboardAccessPolicy)
{}

} // namespace WebCore