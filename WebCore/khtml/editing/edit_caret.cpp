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

#include <edit_caret.h>

#include <dom_nodeimpl.h>
#include <edit_caretimpl.h>

#if APPLE_CHANGES
#include <KWQAssertions.h>
#endif

using DOM::Node;
using khtml::Caret;
using khtml::CaretImpl;

Caret::Caret(const DOM::Node &node, long offset)
{
    impl = new CaretImpl(node.handle(), offset);
    impl->ref();
}

Caret::Caret(CaretImpl *i)
{
    impl = i;
    if(impl) 
        impl->ref();
}

Caret::Caret(const Caret &other)
{
    impl = other.impl;
    if (impl) 
        impl->ref();
}

Caret::Caret()
{
    impl = new CaretImpl();
    impl->ref();
}

Caret::~Caret()
{
    if (impl)
        impl->deref();
}

DOM::Node Caret::node() const
{
    if (!impl) throw CaretException();
    return impl->node();
}

long Caret::offset() const
{
    if (!impl) throw CaretException();
    return impl->offset();
}

bool Caret::startOfLine() const
{
    if (!impl) throw CaretException();
    return impl->startOfLine();
}

void Caret::setPosition(const Node &node, long offset)
{
    if (!impl) throw CaretException();
    impl->setPosition(node.handle(), offset);
}

void Caret::moveForwardByCharacter()
{
    if (!impl) throw CaretException();
    impl->moveForwardByCharacter();
}

void Caret::moveBackwardByCharacter()
{
    if (!impl) throw CaretException();
    impl->moveBackwardByCharacter();
}

void Caret::setStartOfLine(bool startOfLine)
{
    if (!impl) throw CaretException();
    impl->setStartOfLine(startOfLine);
}

void Caret::adjustPosition()
{
    if (!impl) throw CaretException();
    impl->adjustPosition();
}

Caret Caret::adjustedForEditing() const
{
    if (!impl) throw CaretException();
    impl->adjustPosition();
    return impl;
}

CaretImpl *Caret::handle() const
{
    return impl;
}

bool operator==(const Caret &a, const Caret &b)
{
    Caret _a = a.adjustedForEditing();
    Caret _b = b.adjustedForEditing();
    return _a.offset() == _b.offset() && _a.node() == _b.node();
}

