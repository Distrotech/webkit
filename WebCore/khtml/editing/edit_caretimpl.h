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

#ifndef __edit_caretimpl_h__
#define __edit_caretimpl_h__

#include <edit_caret.h>
#include <dom2_traversal.h>
#include <shared.h>

namespace DOM {
    class CustomNodeFilter;
    class Node;
    class NodeImpl;
}

namespace khtml {

class CaretImpl : public Shared<CaretImpl>
{
public:
    CaretImpl(DOM::NodeImpl *node, long offset, bool startOfLine);
    ~CaretImpl();

    DOM::NodeImpl *node() const { return m_node; }
    long offset() const { return m_offset; }
    bool startOfLine() const { return m_startOfLine; }
    
    void setPosition(DOM::NodeImpl *, long, bool startOfLine=false);
    void setStartOfLine(bool);
    
    void adjustForEditing();
    
    friend class Caret;

private:
    CaretImpl() : m_node(0), m_offset(0), m_startOfLine(false) {}
    CaretImpl(const CaretImpl *c) {};

    bool needsAdjustmentForEditing() const;

    DOM::NodeImpl *m_node;
    long m_offset;
    long m_startOfLine;
};

}; // end namespace khtml

namespace DOM {

class EditNodeFilter : public CustomNodeFilter
{
    virtual short acceptNode(const DOM::Node &);
};

};  // end namespace DOM

#endif