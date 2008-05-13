/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
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
 
#ifndef CodeBlock_h
#define CodeBlock_h

#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include "nodes.h"
#include "Instruction.h"
#include "UString.h"

namespace KJS {

    class ExecState;

    struct CodeBlock {
        CodeBlock(bool usesEval_, bool needsClosure_)
            : numTemporaries(0)
            , numVars(0)
            , numParameters(0)
            , needsActivation(usesEval_ || needsClosure_)
            , usesEval(usesEval_)
            , needsClosure(needsClosure_)
        {
        }
        
        void dump(ExecState*);
        unsigned numRegisters() { return numTemporaries + numVars; }
        
        Vector<Instruction> instructions;
        
        int numTemporaries;
        int numVars;
        int numParameters;
        
        bool needsActivation;
        bool usesEval;
        bool needsClosure;

        // Constant pool
        Vector<Identifier> identifiers;
        Vector<RefPtr<FuncDeclNode> > functions;
        Vector<RefPtr<FuncExprNode> > functionExpressions;
        Vector<JSValue*> jsValues;
        
    private:
        void dump(ExecState*, const Vector<Instruction>::iterator& begin, Vector<Instruction>::iterator&);
    };
    
} // namespace KJS

#endif // CodeBlock_h
