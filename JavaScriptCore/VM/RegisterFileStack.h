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
 
#ifndef RegisterFileStack_h
#define RegisterFileStack_h

#include "RegisterFile.h"
#include <wtf/Vector.h>

namespace KJS {

    class RegisterFileStack {
    public:
        RegisterFileStack()
        {
            allocateRegisterFile();
        }

        ~RegisterFileStack();

        RegisterFile* pushRegisterFile();
        void popRegisterFile();

        RegisterFile* current() { return m_stack.last(); }
        
        void mark()
        {
            Stack::iterator end = m_stack.end();
            for (Stack::iterator it = m_stack.begin(); it != end; ++it) {
                (*it)->mark();
            }
        }

        // Pointer to a value that holds the base of the top-most register file.
        Register** basePointer() { return &m_base; }

        void baseChanged(RegisterFile* registerFile)
        {
            ASSERT(registerFile == current());
            m_base = *registerFile->basePointer();
        }

    private:
        typedef Vector<RegisterFile*, 4> Stack;

        RegisterFile* allocateRegisterFile(size_t maxSize = RegisterFile::DefaultRegisterFileSize);

        RegisterFile* previous() { return m_stack[m_stack.size() - 2]; }
        bool hasPrevious() { return m_stack.size() > 1; }

        Stack m_stack;
        Register* m_base;
    };

} // namespace KJS

#endif // RegisterFileStack_h
