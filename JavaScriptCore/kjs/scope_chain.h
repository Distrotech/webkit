/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003, 2008 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef ScopeChain_h
#define ScopeChain_h

#include <wtf/Assertions.h>

namespace KJS {

    class JSObject;
    class ExecState;
    class ScopeChainIterator;
    
    class ScopeChainNode {
    public:
        ScopeChainNode(ScopeChainNode* n, JSObject* o)
            : next(n), object(o), refCount(1) { }

        ScopeChainNode* next;
        JSObject* object;
        int refCount;

        void deref() { if (--refCount == 0) release(); }
        void ref() { ++refCount; }

        void release();

        ScopeChainIterator begin() const;
        ScopeChainIterator end() const;
    };

    class ScopeChainIterator {
    public:
        ScopeChainIterator(const ScopeChainNode* node) : m_node(node) {}

        JSObject* const & operator*() const { return m_node->object; }
        JSObject* const * operator->() const { return &(operator*()); }
    
        ScopeChainIterator& operator++() { m_node = m_node->next; return *this; }

        // postfix ++ intentionally omitted

        bool operator==(const ScopeChainIterator& other) const { return m_node == other.m_node; }
        bool operator!=(const ScopeChainIterator& other) const { return m_node != other.m_node; }

    private:
        const ScopeChainNode* m_node;
    };

    class ScopeChain {
    public:
        ScopeChain(JSObject* o)
            : _node(new ScopeChainNode(0, o))
        {
        }

        ScopeChain(const ScopeChain& c)
            : _node(c._node)
        {
            _node->ref();
        }

        explicit ScopeChain(ScopeChainNode* node)
            : _node(node)
        {
            _node->ref();
        }
    
        ~ScopeChain() { _node->deref(); }

        JSObject* top() const { return _node->object; }
        JSObject* bottom() const;

        ScopeChainIterator begin() const { return _node->begin(); }
        ScopeChainIterator end() const { return _node->end(); }

        void clear() { _node->deref(); _node = 0; }
        void push(JSObject *);
        void push(const ScopeChain &);
        void push(ScopeChainNode*);
        void pop();

        void mark() const;

#ifndef NDEBUG        
        void print() const;
#endif
        
        ScopeChainNode* node() const { return _node; }

    private:
        ScopeChainNode* _node;
    };

inline JSObject *ScopeChain::bottom() const
{
    ScopeChainNode *last = 0;
    for (ScopeChainNode *n = _node; n; n = n->next)
        last = n;
    if (!last)
        return 0;
    return last->object;
}

inline void ScopeChain::push(JSObject *o)
{
    ASSERT(o);
    _node = new ScopeChainNode(_node, o);
}

inline void ScopeChain::push(ScopeChainNode *node)
{
    ASSERT(node);
    ASSERT(node->object);
    node->refCount++;
    node->next = _node;
    _node = node;
}

inline void ScopeChain::pop()
{
    ScopeChainNode *oldNode = _node;
    ASSERT(oldNode);
    ScopeChainNode *newNode = oldNode->next;
    _node = newNode;
    
    if (--oldNode->refCount != 0) {
        if (newNode)
            ++newNode->refCount;
    } else {
        delete oldNode;
    }
}

inline void ScopeChainNode::release()
{
    // This function is only called by deref(),
    // Deref ensures these conditions are true.
    ASSERT(refCount == 0);
    ScopeChainNode* n = this;
    do {
        ScopeChainNode* next = n->next;
        delete n;
        n = next;
    } while (n && --n->refCount == 0);
}

inline ScopeChainIterator ScopeChainNode::begin() const
{
    return ScopeChainIterator(this); 
}

inline ScopeChainIterator ScopeChainNode::end() const
{ 
    return ScopeChainIterator(0); 
}

} // namespace KJS

#endif // ScopeChain_h
