/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "dom/dom_exception.h"
#include "xml/dom_docimpl.h"

#if APPLE_CHANGES
#include <KWQAssertions.h>
#endif

using namespace DOM;

NodeIteratorImpl::NodeIteratorImpl(NodeImpl *_root, unsigned long _whatToShow,
				   NodeFilter _filter, bool _entityReferenceExpansion)
{
    m_root = _root;
    m_whatToShow = _whatToShow;
    m_filter = _filter;
    m_expandEntityReferences = _entityReferenceExpansion;

    m_referenceNode = _root;
    m_inFront = false;

    m_doc = m_root->getDocument();
    m_doc->attachNodeIterator(this);
    m_doc->ref();

    m_detached = false;
}

NodeIteratorImpl::~NodeIteratorImpl()
{
    m_doc->detachNodeIterator(this);
    m_doc->deref();
}

NodeImpl *NodeIteratorImpl::root()
{
    return m_root;
}

unsigned long NodeIteratorImpl::whatToShow()
{
    return m_whatToShow;
}

NodeFilter NodeIteratorImpl::filter()
{
    return m_filter;
}

bool NodeIteratorImpl::expandEntityReferences()
{
    return m_expandEntityReferences;
}

NodeImpl *NodeIteratorImpl::nextNode( int &exceptioncode )
{
    if (m_detached) {
	exceptioncode = DOMException::INVALID_STATE_ERR;
	return 0;
    }

    if (!m_referenceNode) {
	m_inFront = true;
	return 0;
    }

    if (!m_inFront) {
	m_inFront = true;
	if (isAccepted(m_referenceNode) == NodeFilter::FILTER_ACCEPT)
	    return m_referenceNode;
    }

    NodeImpl *_tempCurrent = getNextNode(m_referenceNode);
    while( _tempCurrent ) {
	m_referenceNode = _tempCurrent;
	if(isAccepted(_tempCurrent) == NodeFilter::FILTER_ACCEPT)
	    return m_referenceNode;
      _tempCurrent = getNextNode(_tempCurrent);
    }

    return 0;
}

NodeImpl *NodeIteratorImpl::getNextNode(NodeImpl *n)
{
  /*  1. my first child
   *  2. my next sibling
   *  3. my parents sibling, or their parents sibling (loop)
   *  4. not found
   */

  if( !n )
    return 0;

  if( n->hasChildNodes() )
    return n->firstChild();

  if( n->nextSibling() )
    return n->nextSibling();

  if( m_root == n)
     return 0;

  NodeImpl *parent = n->parentNode();
  while( parent )
    {
      n = parent->nextSibling();
      if( n )
        return n;

      if( m_root == parent )
        return 0;

      parent = parent->parentNode();
    }
  return 0;
}

NodeImpl *NodeIteratorImpl::previousNode( int &exceptioncode )
{
    if (m_detached) {
	exceptioncode = DOMException::INVALID_STATE_ERR;
	return 0;
    }

    if (!m_referenceNode) {
	m_inFront = false;
	return 0;
    }

    if (m_inFront) {
	m_inFront = false;
	if (isAccepted(m_referenceNode) == NodeFilter::FILTER_ACCEPT)
	    return m_referenceNode;
    }

    NodeImpl *_tempCurrent = getPreviousNode(m_referenceNode);
    while( _tempCurrent ) {
	m_referenceNode = _tempCurrent;
	if(isAccepted(_tempCurrent) == NodeFilter::FILTER_ACCEPT)
	    return m_referenceNode;
	_tempCurrent = getPreviousNode(_tempCurrent);
    }

    return 0;
}

NodeImpl *NodeIteratorImpl::getPreviousNode(NodeImpl *n)
{
/* 1. my previous sibling.lastchild
 * 2. my previous sibling
 * 3. my parent
 */
  NodeImpl *_tempCurrent;

  if( !n )
    return 0;

  _tempCurrent = n->previousSibling();
  if( _tempCurrent )
    {
      if( _tempCurrent->lastChild() )
        {
          while( _tempCurrent->lastChild() )
            _tempCurrent = _tempCurrent->lastChild();
          return _tempCurrent;
        }
      else
        return _tempCurrent;
    }


  if(n == m_root)
    return 0;

  return n->parentNode();


}

void NodeIteratorImpl::detach(int &/*exceptioncode*/)
{
    m_doc->detachNodeIterator(this);
    m_detached = true;
}


void NodeIteratorImpl::notifyBeforeNodeRemoval(NodeImpl *removed)
{
    // make sure the deleted node is with the root (but not the root itself)
    if (removed == m_root)
	return;

    NodeImpl *maybeRoot = removed->parentNode();
    while (maybeRoot && maybeRoot != m_root)
	maybeRoot = maybeRoot->parentNode();
    if (!maybeRoot)
	return;

    // did I get deleted, or one of my parents?
    NodeImpl *_tempDeleted = m_referenceNode;
    while( _tempDeleted && _tempDeleted != removed)
        _tempDeleted = _tempDeleted->parentNode();

    if( !_tempDeleted )  // someone that didn't consern me got deleted
        return;

    if( !m_inFront)
    {
        NodeImpl *_next = getNextNode(_tempDeleted);
        if( _next )
            m_referenceNode = _next;
        else
        {
	    // deleted node was at end of list
            m_inFront = true;
            m_referenceNode = getPreviousNode(_tempDeleted);
        }
    }
    else {
	NodeImpl *_prev = getPreviousNode(_tempDeleted);
	if ( _prev )
	    m_referenceNode = _prev;
	else
	{
	    // deleted node was at start of list
	    m_inFront = false;
	    m_referenceNode = getNextNode(_tempDeleted);
	}
    }

}

short NodeIteratorImpl::isAccepted(NodeImpl *n)
{
  // if XML is implemented we have to check expandEntityRerefences in this function
  if( ( ( 1 << n->nodeType()-1) & m_whatToShow) != 0 )
    {
        if(!m_filter.isNull())
            return m_filter.acceptNode(n);
        else
	    return NodeFilter::FILTER_ACCEPT;
    }
    return NodeFilter::FILTER_SKIP;
}

// --------------------------------------------------------------


NodeFilterImpl::NodeFilterImpl()
{
    m_customNodeFilter = 0;
}

NodeFilterImpl::~NodeFilterImpl()
{
    if (m_customNodeFilter)
	m_customNodeFilter->deref();
}

short NodeFilterImpl::acceptNode(const Node &n)
{
    if (m_customNodeFilter)
	return m_customNodeFilter->acceptNode(n);
    else
	return NodeFilter::FILTER_ACCEPT;
}

void NodeFilterImpl::setCustomNodeFilter(CustomNodeFilter *custom)
{
    m_customNodeFilter = custom;
    if (m_customNodeFilter)
	m_customNodeFilter->ref();
}

CustomNodeFilter *NodeFilterImpl::customNodeFilter()
{
    return m_customNodeFilter;
}

// --------------------------------------------------------------

TreeWalkerImpl::TreeWalkerImpl(NodeImpl *root, unsigned long whatToShow, NodeFilterImpl *filter, bool expandEntityReferences)
{
    assertNodeValid(root);
    setRoot(root);
    setWhatToShow(whatToShow);
    setFilter(filter);
    setExpandEntityReferences(expandEntityReferences);
    setCurrentNode(root);
}

TreeWalkerImpl::TreeWalkerImpl(const TreeWalkerImpl &other) : khtml::Shared<TreeWalkerImpl>()
{
    setRoot(other.root());
    setWhatToShow(other.whatToShow());
    setFilter(other.filter());
    setExpandEntityReferences(other.expandEntityReferences());
    setCurrentNode(other.currentNode());
}

TreeWalkerImpl &TreeWalkerImpl::operator=(const TreeWalkerImpl &other)
{
    setRoot(other.root());
    setWhatToShow(other.whatToShow());
    setFilter(other.filter());
    setExpandEntityReferences(other.expandEntityReferences());
    setCurrentNode(other.currentNode());
    return *this;
}

TreeWalkerImpl::~TreeWalkerImpl()
{
    if (m_root)
        m_root->deref();
    if (m_filter)
        m_filter->deref();
    if (m_currentNode)
        m_currentNode->deref();
}

void TreeWalkerImpl::setRoot(NodeImpl *node)
{
    assertNodeValid(node);
    
    if (node == m_root)
        return;
        
    if (m_root)
        m_root->deref();
        
    if (node) {
        m_root = node;
        m_root->ref();
    }
}

void TreeWalkerImpl::setFilter(NodeFilterImpl *filter)
{
    if (filter == m_filter)
        return;
        
    if (m_filter)
        m_filter->deref();
        
    if (filter) {
        m_filter = filter;
        m_filter->ref();
    }
}

void TreeWalkerImpl::setCurrentNode(NodeImpl *node)
{
    assertNodeValid(node);

    if (node == m_currentNode)
        return;
        
    if (m_currentNode)
        m_currentNode->deref();
        
    m_currentNode = node;
    m_currentNode->ref();
}

NodeImpl *TreeWalkerImpl::parentNode()
{
    assertNodeValid(root());
    assertNodeValid(currentNode());

    NodeImpl *stopNode = root()->parentNode();
    NodeImpl *node = currentNode()->parentNode();

    while (node != stopNode && !isAccepted(node))
        node = node->parentNode();

    if (node)
        setCurrentNode(node);
        
    return node;
}


NodeImpl *TreeWalkerImpl::firstChild()
{
    assertNodeValid(root());
    assertNodeValid(currentNode());
    
    NodeImpl *accepted = 0;
    NodeImpl *node = currentNode();

    if (!node->hasChildNodes())
        return 0;

    for (NodeImpl *child = node->firstChild(); child; child = child->nextSibling()) {
        if (isAccepted(child)) {
            accepted = child;
            break;
        }
    }
    
    if (accepted)
        setCurrentNode(node);
        
    return accepted;
}


NodeImpl *TreeWalkerImpl::lastChild()
{
    assertNodeValid(root());
    assertNodeValid(currentNode());
    
    NodeImpl *accepted = 0;
    NodeImpl *node = currentNode();

    if (!node->hasChildNodes())
        return 0;

    for (NodeImpl *child = node->lastChild(); child; child = child->previousSibling()) {
        if (isAccepted(child)) {
            accepted = child;
            break;
        }
    }
    
    if (accepted)
        setCurrentNode(node);
        
    return accepted;
}

NodeImpl *TreeWalkerImpl::previousSibling()
{
    assertNodeValid(root());
    assertNodeValid(currentNode());
    
    NodeImpl *accepted = 0;
    NodeImpl *node = currentNode();

    for (NodeImpl *sibling = node->previousSibling(); sibling; sibling = sibling->previousSibling()) {
        if (isAccepted(sibling)) {
            accepted = sibling;
            break;
        }
    }
    
    if (accepted)
        setCurrentNode(node);
        
    return accepted;
}

NodeImpl *TreeWalkerImpl::nextSibling()
{
    assertNodeValid(root());
    assertNodeValid(currentNode());
    
    NodeImpl *accepted = 0;
    NodeImpl *node = currentNode();

    for (NodeImpl *sibling = node->nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (isAccepted(sibling)) {
            accepted = sibling;
            break;
        }
    }
    
    if (accepted)
        setCurrentNode(node);
        
    return accepted;
}

NodeImpl *TreeWalkerImpl::previousNode()
{
    NodeImpl *result = previousSibling();
    if (result)
        return result;
    return 0;
}

NodeImpl *TreeWalkerImpl::nextNode()
{
    NodeImpl *result = nextSibling();
    if (result)
        return result;
    return 0;

#if 0
/*  1. my first child
 *  2. my next sibling
 *  3. my parents sibling, or their parents sibling (loop)
 *  4. not found
 */

    Node n = getFirstChild(m_currentNode);
    if (!n.isNull()  ) // my first child
    {
        m_currentNode = n;
        return n;
    }

    n = getNextSibling(m_currentNode); // my next sibling
    if (!n.isNull() )
    {
        m_currentNode = n;
        return m_currentNode;
    }
    Node parent = getParentNode(m_currentNode);
    while( !parent.isNull() ) // parents sibling
    {
        n = getNextSibling(parent);
        if (!n.isNull() )
        {
          m_currentNode = n;
          return m_currentNode;
        }
        else
            parent = getParentNode(parent);
    }
    return Node();
#endif
}

short TreeWalkerImpl::isAccepted(NodeImpl *n) const
{
    // if XML is implemented we have to check expandEntityRerefences in this function
  if (( ( 1 << n->nodeType()-1 ) & m_whatToShow) != 0 )
    {
      if(m_filter)
        return m_filter->acceptNode(n);
      else
        return NodeFilter::FILTER_ACCEPT;
    }
  return NodeFilter::FILTER_SKIP;
}

NodeImpl *firstDescendent(const NodeImpl *) 
{
    return 0;
}

NodeImpl *lastDescendent(const NodeImpl *)
{
    return 0;
}


