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

#ifndef CodeGenerator_h
#define CodeGenerator_h

#include "CodeBlock.h"
#include "HashTraits.h"
#include "Instruction.h"
#include "LabelID.h"
#include "RegisterID.h"
#include "SymbolTable.h"
#include "nodes.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace KJS {

    class Identifier;
    class ScopeChain;
    class ScopeNode;

    // JumpContexts are used to track entry and exit points for javascript loops and switch statements
    struct JumpContext {
        LabelStack* labels;
        LabelID* continueTarget;
        LabelID* breakTarget;
        int scopeDepth;
    };

    class CodeGenerator {
    public:
        typedef DeclarationStacks::VarStack VarStack;
        typedef DeclarationStacks::FunctionStack FunctionStack;
        
        static void setDumpsGeneratedCode(bool dumpsGeneratedCode);
        
        CodeGenerator(ProgramNode*, const ScopeChain&, SymbolTable*, CodeBlock*, VarStack&, FunctionStack&);
        CodeGenerator(FunctionBodyNode*, const ScopeChain&, SymbolTable*, CodeBlock*, VarStack&, FunctionStack&, Vector<Identifier>& parameters);

        const CommonIdentifiers& propertyNames() const { return *m_propertyNames; }

        void generate();

        // Returns the register corresponding to a local variable, or 0 if no
        // such register exists. Registers returned by registerForLocal do not
        // require explicit reference counting.
        RegisterID* registerForLocal(const Identifier&);

        // Returns the next available temporary register. Registers returned by
        // newTemporary require a modified form of reference counting: any
        // register with a refcount of 0 is considered "available", meaning that
        // the next instruction may overwrite it.
        RegisterID* newTemporary();

        // The same as newTemporary(), but this function returns "suggestion" if
        // "suggestion" is a temporary. This function is helpful in situations
        // where you've put "suggestion" in a RefPtr, but you'd like to allow
        // the next instruction to overwrite it anyway.
        RegisterID* newTemporaryOr(RegisterID* suggestion) { return suggestion->isTemporary() ? suggestion : newTemporary(); }

        // Functions for handling of dst register

        // Returns  a place to write intermediate values of an operation
        // which reuses dst if it is safe to do so.

        RegisterID* tempDestination(RegisterID* dst) { return (dst && dst->isTemporary()) ? dst : newTemporary(); }

        // Returns the place to write the final output of an operation.
        RegisterID* finalDestination(RegisterID* originalDst, RegisterID* tempDst = 0) 
        { 
            if (originalDst)
                return originalDst;
            if (tempDst && tempDst->isTemporary())
                return tempDst;
            return newTemporary(); 
        }

        // moves src to dst if dst is not null and is different from src, otherwise just returns src
        RegisterID* moveToDestinationIfNeeded(RegisterID* dst, RegisterID* src) { return (dst && dst != src) ? emitMove(dst, src) : src; }


        PassRefPtr<LabelID> newLabel();
        
        // The emitNode functions are just syntactic sugar for calling
        // Node::emitCode. They're the only functions that accept a NULL register.
        RegisterID* emitNode(RegisterID* r0, Node* n) { return n->emitCode(*this, r0); }
        RegisterID* emitNode(Node* n) { return n->emitCode(*this); }

        RegisterID* emitLoad(RegisterID*, bool);
        RegisterID* emitLoad(RegisterID*, double);
        RegisterID* emitLoad(RegisterID*, JSValue*);
        
        RegisterID* emitNewObject(RegisterID*);
        RegisterID* emitNewArray(RegisterID*);

        RegisterID* emitNewFunction(RegisterID*, FuncDeclNode*);
        RegisterID* emitNewFunctionExpression(RegisterID*, FuncExprNode*);
        RegisterID* emitNewRegExp(RegisterID*, RegExp*);

        RegisterID* emitMove(RegisterID*, RegisterID*);

        RegisterID* emitNot(RegisterID*, RegisterID*);
        RegisterID* emitEqual(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitNotEqual(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitStrictEqual(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitNotStrictEqual(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitLess(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitLessEq(RegisterID*, RegisterID*, RegisterID*);

        RegisterID* emitToJSNumber(RegisterID*, RegisterID*);
        RegisterID* emitNegate(RegisterID*, RegisterID*);
        RegisterID* emitPreInc(RegisterID*);
        RegisterID* emitPreDec(RegisterID*);
        RegisterID* emitPostInc(RegisterID*, RegisterID*);
        RegisterID* emitPostDec(RegisterID*, RegisterID*);
        RegisterID* emitAdd(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitMult(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitDiv(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitMod(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitSub(RegisterID*, RegisterID*, RegisterID*);

        RegisterID* emitLeftShift(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitRightShift(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitUnsignedRightShift(RegisterID*, RegisterID*, RegisterID*);

        RegisterID* emitBitAnd(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitBitXOr(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitBitOr(RegisterID*, RegisterID*, RegisterID*);
        RegisterID* emitBitNot(RegisterID*, RegisterID*);

        RegisterID* emitInstanceOf(RegisterID*, RegisterID*, RegisterID*);

        RegisterID* emitTypeOf(RegisterID*, RegisterID*);

        RegisterID* emitIn(RegisterID*, RegisterID*, RegisterID*);

        RegisterID* emitResolve(RegisterID*, const Identifier&);
        RegisterID* emitResolve(RegisterID*, int);
        RegisterID* emitResolve(RegisterID*, int, int);

        RegisterID* emitResolveBase(RegisterID*, const Identifier&);
        RegisterID* emitResolveBase(RegisterID*, int);
        RegisterID* emitResolveBase(RegisterID*, int, int);
        RegisterID* emitResolveBaseAndProperty(RegisterID*, RegisterID*, const Identifier&);
        RegisterID* emitResolveBaseAndFunc(RegisterID*, RegisterID*, const Identifier&);
        
        RegisterID* emitGetPropId(RegisterID* dst, RegisterID* base, const Identifier&);
        RegisterID* emitPutPropId(RegisterID* base, const Identifier&, RegisterID* val);
        RegisterID* emitDeletePropId(RegisterID* dst, RegisterID* base, const Identifier&);
        RegisterID* emitGetPropVal(RegisterID* dst, RegisterID* base, RegisterID* property);
        RegisterID* emitPutPropVal(RegisterID* base, RegisterID* property, RegisterID* val);
        RegisterID* emitDeletePropVal(RegisterID* dst, RegisterID* base, RegisterID* property);
        RegisterID* emitPutPropIndex(RegisterID* base, unsigned index, RegisterID* val);

        RegisterID* emitCall(RegisterID*, RegisterID*, RegisterID*, ArgumentsNode*);
        RegisterID* emitReturn(RegisterID*);
        RegisterID* emitEnd(RegisterID*);

        RegisterID* emitConstruct(RegisterID*, RegisterID*, ArgumentsNode*);

        PassRefPtr<LabelID> emitLabel(LabelID*);
        PassRefPtr<LabelID> emitJump(LabelID*);
        PassRefPtr<LabelID> emitJumpIfTrue(RegisterID*, LabelID*);
        PassRefPtr<LabelID> emitJumpIfFalse(RegisterID*, LabelID*);
        PassRefPtr<LabelID> emitJumpScopes(LabelID* target, int targetScopeDepth);
        
        RegisterID* emitGetPropertyNames(RegisterID*, RegisterID*);
        RegisterID* emitNextPropertyName(RegisterID*, RegisterID*, LabelID*);
        
        void pushJumpContext(LabelStack*, LabelID* continueTarget, LabelID* breakTarget);
        void popJumpContext();
        JumpContext* jumpContextForLabel(const Identifier&);

        RegisterID* emitPushScope(RegisterID*);
        void emitPopScope();

        RegisterID* emitCatch(RegisterID*, LabelID* start, LabelID* end);
        void emitThrow(RegisterID*);
    private:
        struct JSValueHashTraits : HashTraits<JSValue*> {
            static void constructDeletedValue(JSValue** slot) { *slot = JSImmediate::impossibleValue(); }
            static bool isDeletedValue(JSValue* value) { return value == JSImmediate::impossibleValue(); }
        };

        typedef HashMap<JSValue*, unsigned, DefaultHash<JSValue*>::Hash, JSValueHashTraits> JSValueMap;
        
        // Maps a register index in the symbol table to a RegisterID index in m_locals.
        int localsIndex(int registerIndex) { return -registerIndex - 1; }
        
        // Returns the RegisterID corresponding to ident.
        RegisterID* addVar(const Identifier& ident)
        {
            RegisterID* r0;
            addVar(ident, r0);
            return r0;
        }

        // Returns true if a new RegisterID was added, false if a pre-existing RegisterID was re-used.
        bool addVar(const Identifier&, RegisterID*&);

        void addParameter(const Identifier&);

        unsigned addConstant(FuncDeclNode*);
        unsigned addConstant(FuncExprNode*);
        unsigned addConstant(const Identifier&);
        unsigned addConstant(JSValue*);
        unsigned addRegExp(RegExp* r);
        
        Vector<Instruction>& instructions() { return m_codeBlock->instructions; }
        SymbolTable& symbolTable() { return *m_symbolTable; }
        Vector<HandlerInfo>& exceptionHandlers() { return m_codeBlock->exceptionHandlers; }
        
        bool shouldOptimizeLocals() { return !m_scopeDepth; }

        const ScopeChain* m_scopeChain;
        SymbolTable* m_symbolTable;
        
        ScopeNode* m_scopeNode;
        CodeBlock* m_codeBlock;
        
        HashSet<RefPtr<UString::Rep>, IdentifierRepHash> m_functions;

        Vector<RegisterID, 128> m_locals;
        Vector<RegisterID, 128> m_temporaries;
        Vector<LabelID, 128> m_labels;
        int m_scopeDepth;
        
        Vector<JumpContext> m_jumpContextStack;

        int m_nextVar;
        int m_nextParameter;

        // Constant pool
        SymbolTable m_identifierMap;
        JSValueMap m_jsValueMap;

        CommonIdentifiers* m_propertyNames;

#ifndef NDEBUG        
        static bool s_dumpsGeneratedCode;
#endif
    };

}

#endif // CodeGenerator_h
