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
    if (ident == argumentsIdentifier())
        return;
        
    ASSERT(m_nextVar >= m_nextParameter);

    int index = m_nextVar--;
    symbolTable().add(ident.ustring().rep(), index);

    m_localsMap.add(index, m_locals.size());
    m_locals.append(index);

    ++m_codeBlock->numVars;
}

void CodeGenerator::addFunction(const Identifier& ident)
{
    ASSERT(m_nextVar >= m_nextParameter);

    int index = m_nextVar--;
    symbolTable().set(ident.ustring().rep(), index);

    m_localsMap.set(index, m_locals.size());
    m_locals.append(index);

    ++m_codeBlock->numVars;
}

void CodeGenerator::addParameter(const Identifier& ident)
{
    ASSERT(m_nextVar >= m_nextParameter);

    int index = m_nextParameter++;
    symbolTable().set(ident.ustring().rep(), index);

    m_localsMap.set(index, m_locals.size());
    m_locals.append(index);
    
    ++m_codeBlock->numParameters;
}

RegisterID* CodeGenerator::registerForLocal(const Identifier& ident)
{
    if (m_scopeDepth)
        return 0;
    int index = symbolTable().get(ident.ustring().rep());
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

PassRefPtr<LabelID> CodeGenerator::emitJumpIfFalse(RegisterID* r0, LabelID* l0)
{
    instructions().append(machine().getOpcode(op_jfalse));
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

unsigned CodeGenerator::addConstant(FuncExprNode* n)
{
    // No need to explicitly unique function expression nodes -- they're unique already.
    int index = m_codeBlock->functionExpressions.size();
    m_codeBlock->functionExpressions.append(n);
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

unsigned CodeGenerator::addRegExp(RegExp* r)
{
    int index = m_codeBlock->regexps.size();
    m_codeBlock->regexps.append(r);
    return index;
}

RegisterID* CodeGenerator::emitMove(RegisterID* r0, RegisterID* r1)
{
    instructions().append(machine().getOpcode(op_mov));
    instructions().append(r0->index());
    instructions().append(r1->index());
    return r0;
}

RegisterID* CodeGenerator::emitNot(RegisterID* r0, RegisterID* r1)
{
    instructions().append(machine().getOpcode(op_not));
    instructions().append(r0->index());
    instructions().append(r1->index());
    return r0;
}

RegisterID* CodeGenerator::emitEqual(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_equal));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitNotEqual(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_nequal));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitStrictEqual(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_stricteq));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitNotStrictEqual(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_nstricteq));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
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

RegisterID* CodeGenerator::emitLessEq(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_lesseq));
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

RegisterID* CodeGenerator::emitPostDec(RegisterID* r0, RegisterID* r1)
{
    instructions().append(machine().getOpcode(op_post_dec));
    instructions().append(r0->index());
    instructions().append(r1->index());
    return r0;
}

RegisterID* CodeGenerator::emitToJSNumber(RegisterID* r0, RegisterID* r1)
{
    instructions().append(machine().getOpcode(op_to_jsnumber));
    instructions().append(r0->index());
    instructions().append(r1->index());
    return r0;
}

RegisterID* CodeGenerator::emitNegate(RegisterID* r0, RegisterID* r1)
{
    instructions().append(machine().getOpcode(op_negate));
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

RegisterID* CodeGenerator::emitMult(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_mult));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitDiv(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_div));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitMod(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_mod));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitSub(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_sub));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitLeftShift(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_lshift));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitRightShift(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_rshift));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitUnsignedRightShift(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_urshift));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitBitAnd(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_bitand));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitBitXOr(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_bitxor));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitBitOr(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_bitor));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitBitNot(RegisterID* r0, RegisterID* r1)
{
    instructions().append(machine().getOpcode(op_bitnot));
    instructions().append(r0->index());
    instructions().append(r1->index());
    return r0;
}

RegisterID* CodeGenerator::emitInstanceOf(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_instance_of));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
    return r0;
}

RegisterID* CodeGenerator::emitTypeOf(RegisterID* r0, RegisterID* r1)
{
    instructions().append(machine().getOpcode(op_type_of));
    instructions().append(r0->index());
    instructions().append(r1->index());
    return r0;
}

RegisterID* CodeGenerator::emitLoad(RegisterID* r0, bool b)
{
    instructions().append(machine().getOpcode(op_load));
    instructions().append(r0->index());
    instructions().append(addConstant(jsBoolean(b)));
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

RegisterID* CodeGenerator::emitNewObject(RegisterID* r0)
{
    instructions().append(machine().getOpcode(op_new_object));
    instructions().append(r0->index());
    return r0;
}

RegisterID* CodeGenerator::emitNewArray(RegisterID* r0)
{
    instructions().append(machine().getOpcode(op_new_array));
    instructions().append(r0->index());
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

RegisterID* CodeGenerator::emitGetPropId(RegisterID* r0, RegisterID* r1, const Identifier& ident)
{
    instructions().append(machine().getOpcode(op_get_prop_id));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(addConstant(ident));
    return r0;
}

RegisterID* CodeGenerator::emitPutPropId(RegisterID* r0, const Identifier& ident, RegisterID* r1)
{
    instructions().append(machine().getOpcode(op_put_prop_id));
    instructions().append(r0->index());
    instructions().append(addConstant(ident));
    instructions().append(r1->index());
    return r1;
}

RegisterID* CodeGenerator::emitGetPropVal(RegisterID* dst, RegisterID* base, RegisterID* property)
{
    instructions().append(machine().getOpcode(op_get_prop_val));
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(property->index());
    return dst;
}

RegisterID* CodeGenerator::emitPutPropVal(RegisterID* base, RegisterID* property, RegisterID* val)
{
    instructions().append(machine().getOpcode(op_put_prop_val));
    instructions().append(base->index());
    instructions().append(property->index());
    instructions().append(val->index());
    return val;
}

RegisterID* CodeGenerator::emitPutPropIndex(RegisterID* r0, unsigned index, RegisterID* r1)
{
    instructions().append(machine().getOpcode(op_put_prop_index));
    instructions().append(r0->index());
    instructions().append(index);
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

RegisterID* CodeGenerator::emitNewRegExp(RegisterID* r0, RegExp* re)
{
    instructions().append(machine().getOpcode(op_new_regexp));
    instructions().append(r0->index());
    instructions().append(addRegExp(re));
    return r0;
}


RegisterID* CodeGenerator::emitNewFunctionExpression(RegisterID* r0, FuncExprNode* n)
{
    instructions().append(machine().getOpcode(op_new_func_exp));
    instructions().append(r0->index());
    instructions().append(addConstant(n));
    return r0;
}

RegisterID* CodeGenerator::emitCall(RegisterID* r0, RegisterID* r1, RegisterID* r2, ArgumentsNode* argumentsNode)
{
    RefPtr<RegisterID> ref1 = r1;
    RefPtr<RegisterID> ref2 = r2;
    
    // Reserve space for return info.
    Vector<RefPtr<RegisterID>, Machine::returnInfoSize> returnInfo;
    for (int i = 0; i < Machine::returnInfoSize; ++i)
        returnInfo.append(newTemporary());

    // Generate code for arguments.
    Vector<RefPtr<RegisterID>, 16> argv;
    argv.append(newTemporary()); // reserve space for "this"
    for (ArgumentListNode* n = argumentsNode->m_listNode.get(); n; n = n->m_next.get()) {
        argv.append(newTemporary());
        emitNode(argv.last().get(), n);
    }

    instructions().append(machine().getOpcode(op_call));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2 ? r2->index() : missingSymbolMarker()); // We encode the "this" value in the instruction stream, to avoid an explicit instruction for copying or loading it.
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

RegisterID* CodeGenerator::emitConstruct(RegisterID* r0, RegisterID* r1, ArgumentsNode* argumentsNode)
{
    // Generate code for arguments.
    Vector<RefPtr<RegisterID>, 16> argv;
    for (ArgumentListNode* n = argumentsNode ? argumentsNode->m_listNode.get() : 0; n; n = n->m_next.get()) {
        argv.append(newTemporary());
        emitNode(argv.last().get(), n);
    }

    instructions().append(machine().getOpcode(op_construct));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(argv.size() ? argv[0]->index() : m_temporaries.size()); // argv
    instructions().append(argv.size()); // argc
    return r0;
}


RegisterID* CodeGenerator::emitPushScope(RegisterID* r0)
{
    m_codeBlock->needsActivation = true;
    instructions().append(machine().getOpcode(op_push_scope));
    instructions().append(r0->index());
    m_scopeDepth++;
    return r0;
}

void CodeGenerator::emitPopScope()
{
    ASSERT(m_scopeDepth > 0);
    instructions().append(machine().getOpcode(op_pop_scope));
    m_scopeDepth--;
}

void CodeGenerator::pushJumpContext(LabelStack* labels, LabelID* continueTarget, LabelID* breakTarget)
{
    JumpContext scope = { labels, continueTarget, breakTarget, m_scopeDepth};
    m_jumpContextStack.append(scope);
}

void CodeGenerator::popJumpContext()
{
    ASSERT(m_jumpContextStack.size());
    m_jumpContextStack.removeLast();
}

JumpContext* CodeGenerator::jumpContextForLabel(const Identifier& label)
{
    ASSERT(m_jumpContextStack.size());
    if (label.isEmpty())
        return &m_jumpContextStack.last();
    for (int i = m_jumpContextStack.size() - 1; i >= 0; i--) {
        JumpContext* scope = &m_jumpContextStack[i];
        if (scope->labels->contains(label))
            return scope;
    }
    return 0;
}

PassRefPtr<LabelID> CodeGenerator::emitJumpScopes(LabelID* target, int targetScopeDepth)
{
    int scopeDelta = m_scopeDepth - targetScopeDepth;
    ASSERT(scopeDelta >= 0);
    
    if (!scopeDelta)
        return emitJump(target);
    
    instructions().append(machine().getOpcode(op_jmp_scopes));
    instructions().append(scopeDelta);
    instructions().append(target->offsetFrom(instructions().size()));
    return target;
}

RegisterID* CodeGenerator::emitNextPropertyName(RegisterID* dest, RegisterID* iterator, LabelID* target)
{
    instructions().append(machine().getOpcode(op_next_pname));
    instructions().append(dest->index());
    instructions().append(iterator->index());
    instructions().append(target->offsetFrom(instructions().size()));
    return dest;
}

RegisterID* CodeGenerator::emitGetPropertyNames(RegisterID* iterator, RegisterID* object)
{
    instructions().append(machine().getOpcode(op_get_pnames));
    instructions().append(iterator->index());
    instructions().append(object->index());
    return iterator;
}

} // namespace KJS
