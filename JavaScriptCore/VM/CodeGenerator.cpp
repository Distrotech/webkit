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
    The layout of a register frame looks like this:
    
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
    <                        >  reserved: call frame  |  1 | <-- value held
    >         >snip<         <------------------------------
    <                        > +0 | +1 | +2 | +3 | +4 | +5 | <-- register index
    >                        <------------------------------
    | params->|<-locals      | temps->    
    
    The call instruction fills in the "call frame" registers. It also pads
    missing arguments at the end of the call:
    
    >                        <-----------------------------------
    <                        >  reserved: call frame  |  1 |  ? | <-- value held ("?" stands for "undefined")
    >         >snip<         <-----------------------------------
    <                        > +0 | +1 | +2 | +3 | +4 | +5 | +6 | <-- register index
    >                        <-----------------------------------
    | params->|<-locals      | temps->
    
    After filling in missing arguments, the call instruction sets up the new
    stack frame to overlap the end of the old stack frame:

                             |---------------------------------->                        <
                             |  reserved: call frame  |  1 |  ? <                        > <-- value held ("?" stands for "undefined")
                             |---------------------------------->         >snip<         <
                             | -7 | -6 | -5 | -4 | -3 | -2 | -1 <                        > <-- register index
                             |---------------------------------->                        <
                             |                        | params->|<-locals       | temps->
    
    That way, arguments are "copied" into the callee's stack frame for free.
    
    If the caller supplies too many arguments, this trick doesn't work. The
    extra arguments protrude into space reserved for locals and temporaries.
    In that case, the call instruction makes a real copy of the call frame header,
    along with just the arguments expected by the callee, leaving the original
    call frame header and arguments behind. (The call instruction can't just discard
    extra arguments, because the "arguments" object may access them later.)
    This copying strategy ensures that all named values will be at the indices
    expected by the callee.

*/

#ifndef NDEBUG
bool CodeGenerator::s_dumpsGeneratedCode = false;
#endif

void CodeGenerator::setDumpsGeneratedCode(bool dumpsGeneratedCode)
{
#ifndef NDEBUG
    s_dumpsGeneratedCode = dumpsGeneratedCode;
#else
    UNUSED_PARAM(dumpsGeneratedCode);
#endif
}

void CodeGenerator::generate()
{
    m_scopeNode->emitCode(*this);
#ifndef NDEBUG
    if (s_dumpsGeneratedCode) {
        JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(m_scopeChain->bottom());
        InterpreterExecState tmpExec(globalObject, globalObject, reinterpret_cast<ProgramNode*>(0x1));
        m_codeBlock->dump(&tmpExec);
    }
#endif
}

bool CodeGenerator::addVar(const Identifier& ident, RegisterID*& r0)
{
    int index = m_nextVar;
    pair<SymbolTable::iterator, bool> result = symbolTable().add(ident.ustring().rep(), index);

    if (!result.second)
        index = result.first->second;
    else {
        --m_nextVar;
        ++m_codeBlock->numVars;
        
        ASSERT(m_locals.size() < m_locals.capacity()); // FIXME: Handle growing the locals vector dynamically.
        m_locals.append(index);
    }

    r0 = &m_locals[localsIndex(index)];
    return result.second;
}

CodeGenerator::CodeGenerator(ProgramNode* programNode, const ScopeChain& scopeChain, SymbolTable* symbolTable, CodeBlock* codeBlock, VarStack& varStack, FunctionStack& functionStack)
    : m_scopeChain(&scopeChain)
    , m_symbolTable(symbolTable)
    , m_scopeNode(programNode)
    , m_codeBlock(codeBlock)
    , m_finallyDepth(0)
    , m_dynamicScopeDepth(0)
    , m_isEvalCode(false)
    , m_nextVar(-1)
    , m_propertyNames(CommonIdentifiers::shared())

{
    // Global code can inherit previously defined symbols.
    if (int size = symbolTable->size()) {
        ASSERT(symbolTable->contains(m_propertyNames->thisIdentifier.ustring().rep()));
        
        // Add previously defined symbols to bookkeeping.
        m_locals.resize(size);
        SymbolTable::iterator end = symbolTable->end();
        for (SymbolTable::iterator it = symbolTable->begin(); it != end; ++it)
            m_locals[localsIndex(it->second)].setIndex(it->second);

        // Shift new symbols so they get stored prior to previously defined symbols.
        m_nextVar -= size;
    } else
        addVar(m_propertyNames->thisIdentifier); // No need to make "this" a true parameter, since it's not passed by our caller.

    for (size_t i = 0; i < functionStack.size(); ++i) {
        FuncDeclNode* funcDecl = functionStack[i];
        emitNewFunction(addVar(funcDecl->m_ident), funcDecl);
    }

    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(scopeChain.bottom());
    ASSERT(globalObject->isGlobalObject());

    // FIXME: Remove this once we figure out how ExecState should work in squirrelfish.
    InterpreterExecState tmpExec(globalObject, globalObject, reinterpret_cast<ProgramNode*>(0x1));

    for (size_t i = 0; i < varStack.size(); ++i)
        if (!globalObject->hasProperty(&tmpExec, varStack[i].first))
             emitLoad(addVar(varStack[i].first), jsUndefined());
}

CodeGenerator::CodeGenerator(FunctionBodyNode* functionBody, const ScopeChain& scopeChain, SymbolTable* symbolTable, CodeBlock* codeBlock, VarStack& varStack, FunctionStack& functionStack, Vector<Identifier>& parameters)
    : m_scopeChain(&scopeChain)
    , m_symbolTable(symbolTable)
    , m_scopeNode(functionBody)
    , m_codeBlock(codeBlock)
    , m_finallyDepth(0)
    , m_dynamicScopeDepth(0)
    , m_isEvalCode(false)
    , m_nextVar(-1)
    , m_propertyNames(CommonIdentifiers::shared())
{
    for (size_t i = 0; i < functionStack.size(); ++i) {
        FuncDeclNode* funcDecl = functionStack[i];
        const Identifier& ident = funcDecl->m_ident;
        
        m_functions.add(ident.ustring().rep());
        emitNewFunction(addVar(ident), funcDecl);
    }

    for (size_t i = 0; i < varStack.size(); ++i) {
        const Identifier& ident = varStack[i].first;
        if (ident == m_propertyNames->arguments)
            continue;
        
        RegisterID* r0;
        if (addVar(ident, r0))
            emitLoad(r0, jsUndefined());
    }

    m_nextParameter = m_nextVar - parameters.size();
    ASSERT(localsIndex(m_nextParameter) + 1 < static_cast<int>(m_locals.capacity())); // FIXME: Handle growing the locals vector dynamically.
    m_locals.resize(localsIndex(m_nextParameter) + 1);

    addParameter(m_propertyNames->thisIdentifier);
    for (size_t i = 0; i < parameters.size(); ++i) {
        addParameter(parameters[i]);
    }
}

CodeGenerator::CodeGenerator(EvalNode* evalNode, const ScopeChain& scopeChain, SymbolTable* symbolTable, CodeBlock* codeBlock)
    : m_scopeChain(&scopeChain)
    , m_symbolTable(symbolTable)
    , m_scopeNode(evalNode)
    , m_codeBlock(codeBlock)
    , m_finallyDepth(0)
    , m_dynamicScopeDepth(0)
    , m_isEvalCode(true)
    , m_nextVar(-1)
    , m_propertyNames(CommonIdentifiers::shared())
{
    addVar(m_propertyNames->thisIdentifier);
}

void CodeGenerator::addParameter(const Identifier& ident)
{
    // Parameters overwrite var declarations, but not function declarations,
    // in the symbol table.
    UString::Rep* rep = ident.ustring().rep();
    if (!m_functions.contains(rep)) {
        symbolTable().set(rep, m_nextParameter);
        m_locals[localsIndex(m_nextParameter)].setIndex(m_nextParameter);
    }
    
    // To maintain the calling convention, we have to allocate unique space for
    // each parameter, even if the parameter doesn't make it into the symbol table.
    ++m_nextParameter;
    ++m_codeBlock->numParameters;
}

RegisterID* CodeGenerator::registerForLocal(const Identifier& ident)
{
    if (!shouldOptimizeLocals() && ident != m_propertyNames->thisIdentifier)
        return 0;

    int index = symbolTable().get(ident.ustring().rep());
    if (index == missingSymbolMarker())
        return 0;

    return &m_locals[localsIndex(index)];
}

RegisterID* CodeGenerator::newTemporary()
{
    // Reclaim free register IDs.
    while (m_temporaries.size() && !m_temporaries.last().refCount())
        m_temporaries.removeLast();

    // Allocate new register ID.
    ASSERT(m_temporaries.size() < m_temporaries.capacity()); // FIXME: Handle growing the temporaries vector dynamically.
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
    ASSERT(m_labels.size() < m_labels.capacity()); // FIXME: Handle growing the label vector dynamically.
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

RegisterID* CodeGenerator::emitPreDec(RegisterID* r0)
{
    instructions().append(machine().getOpcode(op_pre_dec));
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

RegisterID* CodeGenerator::emitAdd(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_add));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitMul(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_mul));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitDiv(RegisterID* dst, RegisterID* dividend, RegisterID* divisor)
{
    instructions().append(machine().getOpcode(op_div));
    instructions().append(dst->index());
    instructions().append(dividend->index());
    instructions().append(divisor->index());
    return dst;
}

RegisterID* CodeGenerator::emitMod(RegisterID* dst, RegisterID* dividend, RegisterID* divisor)
{
    instructions().append(machine().getOpcode(op_mod));
    instructions().append(dst->index());
    instructions().append(dividend->index());
    instructions().append(divisor->index());
    return dst;
}

RegisterID* CodeGenerator::emitSub(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_sub));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitLeftShift(RegisterID* dst, RegisterID* val, RegisterID* shift)
{
    instructions().append(machine().getOpcode(op_lshift));
    instructions().append(dst->index());
    instructions().append(val->index());
    instructions().append(shift->index());
    return dst;
}

RegisterID* CodeGenerator::emitRightShift(RegisterID* dst, RegisterID* val, RegisterID* shift)
{
    instructions().append(machine().getOpcode(op_rshift));
    instructions().append(dst->index());
    instructions().append(val->index());
    instructions().append(shift->index());
    return dst;
}

RegisterID* CodeGenerator::emitUnsignedRightShift(RegisterID* dst, RegisterID* val, RegisterID* shift)
{
    instructions().append(machine().getOpcode(op_urshift));
    instructions().append(dst->index());
    instructions().append(val->index());
    instructions().append(shift->index());
    return dst;
}

RegisterID* CodeGenerator::emitBitAnd(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_bitand));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitBitXOr(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_bitxor));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitBitOr(RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    instructions().append(machine().getOpcode(op_bitor));
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
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

RegisterID* CodeGenerator::emitIn(RegisterID* r0, RegisterID* r1, RegisterID* r2)
{
    instructions().append(machine().getOpcode(op_in));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(r2->index());
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
    
RegisterID* CodeGenerator::emitResolveBaseAndProperty(RegisterID* r0, RegisterID* r1, const Identifier& ident)
{
    instructions().append(machine().getOpcode(op_resolve_base_and_property));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(addConstant(ident));
    return r0;
}

RegisterID* CodeGenerator::emitResolveBaseAndFunc(RegisterID* r0, RegisterID* r1, const Identifier& ident)
{
    instructions().append(machine().getOpcode(op_resolve_base_and_func));
    instructions().append(r0->index());
    instructions().append(r1->index());
    instructions().append(addConstant(ident));
    return r0;
}

RegisterID* CodeGenerator::emitGetPropId(RegisterID* dst, RegisterID* base, const Identifier& ident)
{
    instructions().append(machine().getOpcode(op_get_prop_id));
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(addConstant(ident));
    return dst;
}

RegisterID* CodeGenerator::emitPutPropId(RegisterID* base, const Identifier& ident, RegisterID* val)
{
    instructions().append(machine().getOpcode(op_put_prop_id));
    instructions().append(base->index());
    instructions().append(addConstant(ident));
    instructions().append(val->index());
    return val;
}

RegisterID* CodeGenerator::emitDeletePropId(RegisterID* dst, RegisterID* base, const Identifier& ident)
{
    instructions().append(machine().getOpcode(op_delete_prop_id));
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(addConstant(ident));
    return dst;
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

RegisterID* CodeGenerator::emitDeletePropVal(RegisterID* dst, RegisterID* base, RegisterID* property)
{
    instructions().append(machine().getOpcode(op_delete_prop_val));
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(property->index());
    return dst;
}

RegisterID* CodeGenerator::emitPutPropIndex(RegisterID* base, unsigned index, RegisterID* value)
{
    instructions().append(machine().getOpcode(op_put_prop_index));
    instructions().append(base->index());
    instructions().append(index);
    instructions().append(value->index());
    return value;
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
    return emitCall(op_call, r0, r1, r2, argumentsNode);
}

RegisterID* CodeGenerator::emitCallEval(RegisterID* r0, RegisterID* r1, RegisterID* r2, ArgumentsNode* argumentsNode)
{
    return emitCall(op_call_eval, r0, r1, r2, argumentsNode);
}

RegisterID* CodeGenerator::emitCall(OpcodeID opcodeID, RegisterID* r0, RegisterID* r1, RegisterID* r2, ArgumentsNode* argumentsNode)
{
    ASSERT(opcodeID == op_call || opcodeID == op_call_eval);

    RefPtr<RegisterID> ref1 = r1;
    RefPtr<RegisterID> ref2 = r2;
    
    // Reserve space for call frame.
    Vector<RefPtr<RegisterID>, Machine::CallFrameHeaderSize> callFrame;
    for (int i = 0; i < Machine::CallFrameHeaderSize; ++i)
        callFrame.append(newTemporary());

    // Generate code for arguments.
    Vector<RefPtr<RegisterID>, 16> argv;
    argv.append(newTemporary()); // reserve space for "this"
    for (ArgumentListNode* n = argumentsNode->m_listNode.get(); n; n = n->m_next.get()) {
        argv.append(newTemporary());
        emitNode(argv.last().get(), n);
    }

    instructions().append(machine().getOpcode(opcodeID));
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
    // Reserve space for call frame.
    Vector<RefPtr<RegisterID>, Machine::CallFrameHeaderSize> callFrame;
    for (int i = 0; i < Machine::CallFrameHeaderSize; ++i)
        callFrame.append(newTemporary());

    // Generate code for arguments.
    Vector<RefPtr<RegisterID>, 16> argv;
    argv.append(newTemporary()); // reserve space for "this"
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

    ControlFlowContext scope;
    scope.isFinallyBlock = false;
    m_scopeContextStack.append(scope);
    m_dynamicScopeDepth++;
    return r0;
}

void CodeGenerator::emitPopScope()
{
    ASSERT(m_scopeContextStack.size());
    ASSERT(!m_scopeContextStack.last().isFinallyBlock);

    instructions().append(machine().getOpcode(op_pop_scope));

    m_scopeContextStack.removeLast();
    m_dynamicScopeDepth--;
}

void CodeGenerator::pushFinallyContext(LabelID* target, RegisterID* retAddrDst)
{
    ControlFlowContext scope;
    scope.isFinallyBlock = true;
    FinallyContext context = { target, retAddrDst };
    scope.finallyContext = context;
    m_scopeContextStack.append(scope);
    m_finallyDepth++;
}

void CodeGenerator::popFinallyContext()
{
    ASSERT(m_scopeContextStack.size());
    ASSERT(m_scopeContextStack.last().isFinallyBlock);
    ASSERT(m_finallyDepth > 0);
    m_scopeContextStack.removeLast();
    m_finallyDepth--;
}

void CodeGenerator::pushJumpContext(LabelStack* labels, LabelID* continueTarget, LabelID* breakTarget)
{
    JumpContext context = { labels, continueTarget, breakTarget, scopeDepth() };
    m_jumpContextStack.append(context);
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

PassRefPtr<LabelID> CodeGenerator::emitComplexJumpScopes(LabelID* target, ControlFlowContext* topScope, ControlFlowContext* bottomScope)
{
    while (topScope > bottomScope) {
        // First we count the number of dynamic scopes we need to remove to get
        // to a finally block.
        int nNormalScopes = 0;
        while (topScope > bottomScope) {
            if (topScope->isFinallyBlock)
                break;
            ++nNormalScopes;
            --topScope;
        }

        if (nNormalScopes) {
            // We need to remove a number of dynamic scopes to get to the next 
            // finally block
            instructions().append(machine().getOpcode(op_jmp_scopes));
            instructions().append(nNormalScopes);
            
            // If topScope == bottomScope then there isn't actually a finally block
            // left to emit, so make the jmp_scopes jump directly to the target label
            if (topScope == bottomScope) {
                instructions().append(target->offsetFrom(instructions().size()));
                return target;
            }

            // Otherwise we just use jmp_scopes to pop a group of scopes and go 
            // to the next instruction
            RefPtr<LabelID> nextInsn = newLabel();
            instructions().append(nextInsn->offsetFrom(instructions().size()));
            emitLabel(nextInsn.get());
        }

        // To get here there must be at least one finally block present
        do {
            ASSERT(topScope->isFinallyBlock);
            emitJumpSubroutine(topScope->finallyContext.retAddrDst, topScope->finallyContext.finallyAddr);
            --topScope;
            if (!topScope->isFinallyBlock)
                break;
        } while (topScope > bottomScope);
    }
    return emitJump(target);
}

PassRefPtr<LabelID> CodeGenerator::emitJumpScopes(LabelID* target, int targetScopeDepth)
{
    ASSERT(scopeDepth() - targetScopeDepth >= 0);

    size_t scopeDelta = scopeDepth() - targetScopeDepth;
    ASSERT(scopeDelta <= m_scopeContextStack.size());
    if (!scopeDelta)
        return emitJump(target);

    if (m_finallyDepth)
        return emitComplexJumpScopes(target, &m_scopeContextStack.last(), &m_scopeContextStack.last() - scopeDelta);

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

RegisterID* CodeGenerator::emitCatch(RegisterID* targetRegister, LabelID* start, LabelID* end)
{
    HandlerInfo info = { start->offsetFrom(0), end->offsetFrom(0), instructions().size(), m_dynamicScopeDepth };
    exceptionHandlers().append(info);
    instructions().append(machine().getOpcode(op_catch));
    instructions().append(targetRegister->index());
    return targetRegister;
}
    
void CodeGenerator::emitThrow(RegisterID* exception)
{
    instructions().append(machine().getOpcode(op_throw));
    instructions().append(exception->index());
}

RegisterID* CodeGenerator::emitCreateError(RegisterID* r0, ErrorType errorType, JSValue* errorMessage)
{
    instructions().append(machine().getOpcode(op_create_error));
    instructions().append(r0->index());
    instructions().append((int) errorType);
    instructions().append(addConstant(errorMessage));
    return r0;
}

PassRefPtr<LabelID> CodeGenerator::emitJumpSubroutine(RegisterID* retAddrDst, LabelID* finally)
{
    instructions().append(machine().getOpcode(op_jsr));
    instructions().append(retAddrDst->index());
    instructions().append(finally->offsetFrom(instructions().size()));
    return finally;
}

void CodeGenerator::emitSubroutineReturn(RegisterID* retAddrSrc)
{
    instructions().append(machine().getOpcode(op_sret));
    instructions().append(retAddrSrc->index());
}

} // namespace KJS
