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
#include "CodeGenerator.h"

#include "Machine.h"

using namespace std;

namespace KJS {

/*
    The register layout of a stack frame looks like this:
    
    For 
    
    function f(x, y) {
        var v1;
        function g() { }
        var v2;
        return (x) * (y);
    }
    
    assuming (x) and (y) generated temporaries t1 and t2, you would have

    ------------------------------------
    |  x |  y |  g | v2 | v1 | t1 | t2 | <-- value held
    ------------------------------------
    | -5 | -4 | -3 | -2 | -1 | +0 | +1 | <-- register index
    ------------------------------------
    | params->|<-locals      | temps->
    
    Because temporary registers are allocated in a stack-like fashion, we
    can reclaim them with a simple popping algorithm. The same goes for labels.
    (We never reclaim parameter or local registers, because parameters and
    locals are DontDelete.)
    
    The register layout before a function call looks like this:
    
    For
    
    function f(x, y)
    {
    }
    
    f(1);
    
    >                        <------------------------------
    <                        >  reserved: return info |  1 | <-- value held
    >         >snip<         <------------------------------
    <                        > +0 | +1 | +2 | +3 | +4 | +5 | <-- register index
    >                        <------------------------------
    | params->|<-locals      | temps->    
    
    The call instruction fills in the "return info" registers. It also pads
    missing arguments at the end of the call:
    
    >                        <-----------------------------------
    <                        >  reserved: return info |  1 |  ? | <-- value held ("?" stands for "undefined")
    >         >snip<         <-----------------------------------
    <                        > +0 | +1 | +2 | +3 | +4 | +5 | +6 | <-- register index
    >                        <-----------------------------------
    | params->|<-locals      | temps->
    
    After filling in missing arguments, the call instruction sets up the new
    stack frame to overlap the end of the old stack frame:

                             |---------------------------------->                        <
                             |  reserved: return info |  1 |  ? <                        > <-- value held ("?" stands for "undefined")
                             |---------------------------------->         >snip<         <
                             | -7 | -6 | -5 | -4 | -3 | -2 | -1 <                        > <-- register index
                             |---------------------------------->                        <
                             |                        | params->|<-locals       | temps->
    
    That way, arguments are "copied" into the callee's stack frame for free.
    
    If the caller supplies too many arguments, this trick doesn't work. The
    extra arguments protrude into space reserved for locals and temporaries.
    In that case, the call instruction makes a real copy of the return info,
    along with just the arguments expected by the callee, leaving the original
    return info and arguments behind. (The call instruction can't just discard
    extra arguments, because the "arguments" object may access them later.)
    This copying strategy ensures that all named values will be at the indices
    expected by the callee.

*/

void CodeGenerator::generate()
{
    m_scopeNode->emitCode(*this);

#ifndef NDEBUG
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(m_scopeChain->bottom());
    InterpreterExecState tmpExec(globalObject, globalObject, reinterpret_cast<ProgramNode*>(0x1));
    m_codeBlock->dump(&tmpExec);
#endif
}

void CodeGenerator::addVar(const Identifier& ident)
{
    if (ident == *m_argumentsIdentifier)
        return;

    int index = m_nextLocal--;
    symbolTable().add(ident.ustring().rep(), index);

    m_localsMap.add(index, m_locals.size());
    m_locals.append(index);

    ++m_codeBlock->numLocals;
}

void CodeGenerator::addFunction(const Identifier& ident)
{
    int index = m_nextLocal--;
    symbolTable().set(ident.ustring().rep(), index);

    m_localsMap.set(index, m_locals.size());
    m_locals.append(index);

    ++m_codeBlock->numLocals;
}

void CodeGenerator::addParameter(const Identifier& ident)
{
    int index = m_nextParameter++;
    symbolTable().set(ident.ustring().rep(), index);

    m_localsMap.set(index, m_locals.size());
    m_locals.append(index);
    
    ++m_codeBlock->numParameters;
}

RegisterID* CodeGenerator::getRegister(const Identifier& ident)
{
    unsigned index = symbolTable().get(ident.ustring().rep());
    if (index == missingSymbolMarker())
        return 0;
    return &m_locals[m_localsMap.get(index)];
}

RegisterID* CodeGenerator::newTemporary()
{
    // Reclaim free register IDs.
    while (m_temporaries.size() && !m_temporaries.last().refCount())
        m_temporaries.removeLast();

    // Allocate new register ID.
    m_temporaries.append(m_temporaries.size());
    m_codeBlock->numTemporaries = max<int>(m_codeBlock->numTemporaries, m_temporaries.size());
    return &m_temporaries.last();
}

PassRefPtr<LabelID> CodeGenerator::newLabel()
{
    // Reclaim free label IDs.
    while (m_labels.size() && !m_labels.last().refCount())
        m_labels.removeLast();

    // Allocate new label ID.
    m_labels.append(m_codeBlock);
    return &m_labels.last();
}

PassRefPtr<LabelID> CodeGenerator::emitLabel(LabelID* l0)
{
    l0->setLocation(instructions().size());
    return l0;
}

PassRefPtr<LabelID> CodeGenerator::emitJump(LabelID* l0)
{
    instructions().append(machine().getOpcode(op_jmp));
    instructions().append(l0->offsetFrom(instructions().size()));
    return l0;
}

PassRefPtr<LabelID> CodeGenerator::emitJumpIfTrue(RegisterID* r0, LabelID* l0)
{
    instructions().append(machine().getOpcode(op_jtrue));
    instructions().append(r0->index());
    instructions().append(l0->offsetFrom(instructions().size()));
    return l0;
}

unsigned CodeGenerator::addConstant(FuncDeclNode* n)
{
    // No need to explicitly unique function body nodes -- they're unique already.
    int index = m_codeBlock->functions.size();
    m_codeBlock->functions.append(n);
    return index;
}

unsigned CodeGenerator::addConstant(const Identifier& ident)
{
    UString::Rep* rep = ident.ustring().rep();
    pair<SymbolTable::iterator, bool> result = m_identifierMap.add(rep, m_codeBlock->identifiers.size());
    if (result.second) // new entry
        m_codeBlock->identifiers.append(rep);
    
    return result.first->second;
}

unsigned CodeGenerator::addConstant(JSValue* v)
{
    pair<JSValueMap::iterator, bool> result = m_jsValueMap.add(v, m_codeBlock->jsValues.size());
    if (result.second) // new entry
        m_codeBlock->jsValues.append(v);
    
    return result.first->second;
}

RegisterID* CodeGenerator::emitMove(RegisterID* r0, RegisterID* r1)
{
    instructions().append(machine().getOpcode(op_mov));
    instructions().append(r0->index());
    instructions().append(r1->index());
    return r0;
}

RegisterID* CodeGenerator::emitLess(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_less));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitPreInc(RegisterID* r0)
{
    instructions().append(machine().getOpcode(op_pre_inc));
    instructions().append(r0->index());
    return r0;
}

RegisterID* CodeGenerator::emitPostInc(RegisterID* r0, RegisterID* r1)
{
    instructions().append(machine().getOpcode(op_post_inc));
    instructions().append(r0->index());
    instructions().append(r1->index());
    return r0;
}

RegisterID* CodeGenerator::emitAdd(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_add));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitLoad(RegisterID* r0, double d)
{
    instructions().append(machine().getOpcode(op_load));
    instructions().append(r0->index());
    instructions().append(addConstant(jsNumber(d)));
    return r0;
}

RegisterID* CodeGenerator::emitLoad(RegisterID* r0, JSValue* v)
{
    instructions().append(machine().getOpcode(op_load));
    instructions().append(r0->index());
    instructions().append(addConstant(v));
    return r0;
}

RegisterID* CodeGenerator::emitResolve(RegisterID* r0, const Identifier& ident)
{
    instructions().append(machine().getOpcode(op_resolve));
    instructions().append(r0->index());
    instructions().append(addConstant(ident));
    return r0;
}

RegisterID* CodeGenerator::emitResolveBase(RegisterID* r0, const Identifier& ident)
{
    instructions().append(machine().getOpcode(op_resolve_base));
    instructions().append(r0->index());
    instructions().append(addConstant(ident));
    return r0;
}

RegisterID* CodeGenerator::emitObjectGet(RegisterID* r0, RegisterID* r1, const Identifier& ident)
{
    instructions().append(machine().getOpcode(op_object_get));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(addConstant(ident));
    return r0;
}

RegisterID* CodeGenerator::emitObjectPut(RegisterID* r0, const Identifier& ident, RegisterID* r1)
{
    instructions().append(machine().getOpcode(op_object_put));
    instructions().append(r0->index());
    instructions().append(addConstant(ident));
    instructions().append(r1->index());
    return r1;
}

RegisterID* CodeGenerator::emitNewFunction(RegisterID* r0, FuncDeclNode* n)
{
    instructions().append(machine().getOpcode(op_new_func));
    instructions().append(r0->index());
    instructions().append(addConstant(n));
    return r0;
}

RegisterID* CodeGenerator::emitCall(RegisterID* r0, RegisterID* r1, ArgumentsNode* argumentsNode)
{
    // Reserve space for return info.
    Vector<RefPtr<RegisterID>, Machine::returnInfoSize> returnInfo;
    for (int i = 0; i < Machine::returnInfoSize; ++i)
        returnInfo.append(newTemporary());

    // Generate code for arguments.
    Vector<RefPtr<RegisterID>, 16> argv;
    for (ArgumentListNode* n = argumentsNode->m_listNode.get(); n; n = n->m_next.get()) {
        argv.append(newTemporary());
        emitNode(argv.last().get(), n);
    }

    instructions().append(machine().getOpcode(op_call));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(argv.size() ? argv[0]->index() : m_temporaries.size()); // argv
    instructions().append(argv.size()); // argc

    return r0;
}

RegisterID* CodeGenerator::emitReturn(RegisterID* r0)
{
    instructions().append(machine().getOpcode(op_ret));
    instructions().append(r0->index());
    return r0;
}

RegisterID* CodeGenerator::emitEnd(RegisterID* r0)
{
    instructions().append(machine().getOpcode(op_end));
    instructions().append(r0->index());
    return r0;
}

} // namespace KJS
