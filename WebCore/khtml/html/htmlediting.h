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

#ifndef __htmlediting_h__
#define __htmlediting_h__

#include <dom_string.h>
#include <dom_node.h>
#include <dom2_range.h>

using DOM::DOMString;
using DOM::Node;
using DOM::Range;

namespace khtml {

enum EditCommandID { TextInputCommandID, };

class EditCommand
{
public:    
    EditCommand(const Range &selection) : m_selection(selection) {};
    virtual ~EditCommand() {};
    
    virtual EditCommandID commandID() const = 0;
    
    Range selection() { return m_selection; }

private:
    Range m_selection;
};


class TextInputCommand : public EditCommand
{
public:
    TextInputCommand(const Range &selection, const DOMString &text) 
    : EditCommand(selection), m_text(text) {}
    
    virtual ~TextInputCommand() {};
    
    virtual EditCommandID commandID() const;

    DOMString text() { return m_text; }
    
private:
    DOMString m_text;
};

}; // end namespace khtml

#endif
