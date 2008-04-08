/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include "config.h"
#include "RegisterFile.h"

#include "Register.h"
#include "collector.h"

namespace KJS {

using namespace std;

void RegisterFile::reallocate(size_t minCapacity)
{
    size_t numGlobals = this->numGlobals();
    size_t size = m_size + numGlobals;
    size_t capacity = m_capacity + numGlobals;
    minCapacity += numGlobals;

    capacity = (max(minCapacity, max<size_t>(16, capacity + capacity / 4 + 1)));
    Register* newBuffer = static_cast<Register*>(fastCalloc(capacity, sizeof(Register))); // zero-filled memory

    if (size) {
        ASSERT(m_buffer);
        memcpy(newBuffer, m_buffer, size * sizeof(Register));
    }

    if (m_buffer)
        fastFree(m_buffer);

    m_buffer = newBuffer;
    m_base = m_buffer + numGlobals;
    m_capacity = capacity - numGlobals;
}

void RegisterFile::addGlobals(size_t count)
{
    if (!count)
        return;

    size_t newCapacity = m_size + count;
    if (newCapacity > m_capacity)
        reallocate(newCapacity);

    if (m_base != m_buffer) {
        // FIXME: This move overwrites nested register frames, so it's only safe
        // when executed in the top-level register frame.
        ASSERT(!m_size);
        memmove(m_buffer + count, m_buffer, count * sizeof(Register));
    }
    m_base += count;
    m_capacity -= count;
}

void RegisterFile::clear()
{
    m_base = m_buffer;
    m_size = 0;
}

void RegisterFile::mark()
{
    Collector::markStackObjectsConservatively(m_buffer, m_base + m_size);
}

} // namespace KJS
