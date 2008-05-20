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
 
#ifndef Machine_h
#define Machine_h

#include "Opcode.h"
#include "RegisterFileStack.h"
#include <wtf/HashMap.h>
#include <kjs/list.h>
#include <kjs/SavedBuiltins.h>

namespace KJS {

    class CodeBlock;
    class ExecState;
    class Register;
    class RegisterFile;
    class RegisterFileStack;
    
    class Machine {
    public:
        enum { CallerCodeBlock = 0, 
               ReturnVPC, 
               CallerScopeChain, 
               CallerRegisterOffset, 
               ReturnValueRegister,
               ArgumentStartRegister,
               CalledAsConstructor,
               OptionalCalleeScopeChain,
               CallFrameHeaderSize};

        Machine();
        
        Opcode getOpcode(OpcodeID id) {
            #if HAVE(COMPUTED_GOTO)
                return m_opcodeTable[id]; 
            #else
                return id;
            #endif
        }

        OpcodeID getOpcodeID(Opcode opcode) {
            #if HAVE(COMPUTED_GOTO)
                ASSERT(isOpcode(opcode));
                return m_opcodeIDTable.get(opcode);
            #else
                return opcode;
            #endif
        }

        bool isOpcode(Opcode opcode);
        
        JSValue* execute(ProgramNode*, ExecState*, RegisterFileStack*, ScopeChain*, JSValue** exception);
        JSValue* execute(FunctionBodyNode*, const List& args, JSObject* thisObj, ExecState*, RegisterFileStack*, ScopeChain*, JSValue** exception);
        
    private:
        typedef enum { Normal, InitializeAndReturn } ExecutionFlag;

        NEVER_INLINE Instruction* unwindCallFrame(CodeBlock*&, JSValue**&, ScopeChainNode*&, Register**, Register*&);
        NEVER_INLINE Instruction* throwException(ExecState*, JSValue*, CodeBlock*&, JSValue**&, ScopeChainNode*&, Register**, Register*&, const Instruction*);

        JSValue* privateExecute(ExecutionFlag, ExecState* = 0, RegisterFile* = 0, Register* = 0, ScopeChainNode* = 0, CodeBlock* = 0, JSValue** exception = 0);

        void dumpCallFrame(const CodeBlock*, const ScopeChain*, RegisterFile*, const Register*);
        void dumpRegisters(const CodeBlock*, RegisterFile*, const Register*);
        
        bool isGlobalCallFrame(Register** registerBase, const Register* r) { return (*registerBase) == r; }

#if HAVE(COMPUTED_GOTO)        
        Opcode m_opcodeTable[numOpcodeIDs]; // Maps OpcodeID => Opcode for compiling
        HashMap<Opcode, OpcodeID> m_opcodeIDTable; // Maps Opcode => OpcodeID for decompiling
#endif
    };
    
    Machine& machine();

} // namespace KJS

#endif // Machine_h
