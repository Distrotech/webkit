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

#include <edit_caretimpl.h>

#include <dom_docimpl.h>
#include <dom_nodeimpl.h>
#include <dom2_traversalimpl.h>

#if APPLE_CHANGES
#include <KWQAssertions.h>
#endif

using DOM::DocumentImpl;
using DOM::EditNodeFilter;
using DOM::Node;
using DOM::NodeImpl;
using DOM::NodeFilter;
using DOM::TreeWalker;
using khtml::Caret;
using khtml::CaretImpl;

CaretImpl::CaretImpl(NodeImpl *node, long offset, bool startOfLine) 
    : m_offset(offset), m_startOfLine(startOfLine) 
{
    if (node) {
        m_node = node;
        m_node->ref();
    }
}

CaretImpl::~CaretImpl()
{
    if (m_node)
        m_node->deref();
}


void CaretImpl::setPosition(DOM::NodeImpl *node, long offset, bool startOfLine)
{
    if (m_node != node) {
        m_node->deref();
        m_node = node;
        m_node->ref();
    }
    m_offset = offset;
    m_startOfLine = startOfLine;

    // Perform various checks on the new position and fix up as needed

    // 1. Ensure the node set is a leaf node
    //if (m_node && m_node.hasChildNodes())
    //    m_node = m_node.nextLeafNode();

}

void CaretImpl::setStartOfLine(bool startOfLine)
{
    m_startOfLine = startOfLine;
}

bool CaretImpl::needsAdjustmentForEditing() const
{
    if (offset() != 0) 
        return false;
    
    if (node() == node()->parentNode()->firstChild())
        return false;
        
    return true;
}

void CaretImpl::adjustForEditing()
{
    if (!needsAdjustmentForEditing())
        return;

    EditNodeFilter filter;
    NodeFilter nodeFilter = NodeFilter::createCustom(&filter);
    DocumentImpl *document = node()->getDocument();
    TreeWalker tree = document->createTreeWalker(document, NodeFilter::SHOW_ALL, nodeFilter, true);
    tree.handle()->setCurrentNode(node());
}

short EditNodeFilter::acceptNode(const DOM::Node &n)
{
    NodeImpl *node = n.handle();
    if (!node)
        return NodeFilter::FILTER_REJECT;
    
    if (!node->isHTMLElement()) 
        return NodeFilter::FILTER_REJECT;

    if (node->isTextNode() && node->isContentEditable())
        return NodeFilter::FILTER_ACCEPT;
        
    return NodeFilter::FILTER_SKIP;
}
