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
#include "Machine.h"

#include "CodeBlock.h"
#include "ExecState.h"
#include "JSLock.h"
#include "Register.h"
#include "internal.h"
#include "JSActivation.h"

namespace KJS {

void Machine::dumpRegisters(const Vector<Register>& registers, Register* r)
{
    printf("\nRegisters: ");

    size_t size = registers.size();
    for (size_t i = 0; i < size; ++i) {
        printf("| %10p ", registers[i].u.jsValue);
    }
    printf("|\n           ");

    Vector<Register>::const_iterator it = registers.begin();
    for ( ; it != r; ++it)
        printf("| %10p ", it);

    printf("|     r^     ");
    ++it;
    
    Vector<Register>::const_iterator end = registers.end();
    for ( ; it < end; ++it)
        printf("| %10p ", it);

    printf("|\n");
}

static inline bool jsLess(ExecState* exec, JSValue* v1, JSValue* v2)
{
    double n1;
    double n2;
    JSValue* p1;
    JSValue* p2;
    bool wasNotString1 = v1->getPrimitiveNumber(exec, n1, p1);
    bool wasNotString2 = v2->getPrimitiveNumber(exec, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 < n2;

    return static_cast<const StringImp*>(p1)->value() < static_cast<const StringImp*>(p2)->value();
}

static inline bool jsLessEq(ExecState* exec, JSValue* v1, JSValue* v2)
{
    double n1;
    double n2;
    JSValue* p1;
    JSValue* p2;
    bool wasNotString1 = v1->getPrimitiveNumber(exec, n1, p1);
    bool wasNotString2 = v2->getPrimitiveNumber(exec, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 <= n2;

    return !(static_cast<const StringImp*>(p2)->value() < static_cast<const StringImp*>(p1)->value());
}

static JSValue* jsAddSlowCase(ExecState* exec, JSValue* v1, JSValue* v2)
{
    // exception for the Date exception in defaultValue()
    JSValue* p1 = v1->toPrimitive(exec, UnspecifiedType);
    JSValue* p2 = v2->toPrimitive(exec, UnspecifiedType);

    if (p1->isString() || p2->isString()) {
        UString value = p1->toString(exec) + p2->toString(exec);
        if (value.isNull())
            return throwOutOfMemoryError(exec);
        return jsString(value);
    }

    return jsNumber(p1->toNumber(exec) + p2->toNumber(exec));
}

// Fast-path choices here are based on frequency data from SunSpider:
//    <times> Add case: <t1> <t2>
//    ---------------------------
//    5627160 Add case: 1 1
//    247427  Add case: 5 5
//    20901   Add case: 5 6
//    13978   Add case: 5 1
//    4000    Add case: 1 5
//    1       Add case: 3 5

static inline JSValue* jsAdd(ExecState* exec, JSValue* v1, JSValue* v2)
{
    JSType t1 = v1->type();
    JSType t2 = v2->type();
    const unsigned bothTypes = (t1 << 3) | t2;

    if (bothTypes == ((NumberType << 3) | NumberType))
        return jsNumber(v1->toNumber(exec) + v2->toNumber(exec));
    if (bothTypes == ((StringType << 3) | StringType)) {
        UString value = static_cast<StringImp*>(v1)->value() + static_cast<StringImp*>(v2)->value();
        if (value.isNull())
            return throwOutOfMemoryError(exec);
        return jsString(value);
    }

    // All other cases are pretty uncommon
    return jsAddSlowCase(exec, v1, v2);
}

Machine::Machine()
{
    privateExecute(InitializeAndReturn);
}

bool Machine::isOpcode(Opcode opcode)
{
#if HAVE(COMPUTED_GOTO)
    return opcode != HashTraits<Opcode>::emptyValue()
        && opcode != HashTraits<Opcode>::deletedValue()
        && m_opcodeIDTable.contains(opcode);
#else
    return opcode >= 0 && opcode < op_end;
#endif
}

static void NEVER_INLINE resolve(ExecState* exec, Instruction* vPC, Register* r, ScopeChain* scopeChain, CodeBlock* codeBlock)
{
    int r0 = (vPC + 1)->u.operand;
    int k0 = (vPC + 2)->u.operand;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    PropertySlot slot;
    Identifier& ident = codeBlock->identifiers[k0];
    do {
        JSObject* o = *iter;
        if (o->getPropertySlot(exec, ident, slot)) {
            r[r0].u.jsValue = slot.getValue(exec, o, ident);
            return;
        }
    } while (++iter != end);

    ASSERT_NOT_REACHED(); // FIXME: throw an undefined variable exception
}

static void NEVER_INLINE resolveBase(ExecState* exec, Instruction* vPC, Register* r, ScopeChain* scopeChain, CodeBlock* codeBlock)
{
    int r0 = (vPC + 1)->u.operand;
    int k0 = (vPC + 2)->u.operand;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    PropertySlot slot;
    Identifier& ident = codeBlock->identifiers[k0];
    JSObject* base;
    do {
        base = *iter;
        if (base->getPropertySlot(exec, ident, slot)) {
            r[r0].u.jsValue = base;
            return;
        }
    } while (++iter != end);

    r[r0].u.jsValue = base;
}

void Machine::privateExecute(ExecutionFlag flag, ExecState* exec, ScopeChain* scopeChain, CodeBlock* codeBlock)
{
    // One-time initialization of our address tables. We have to put this code
    // here because our labels are only in scope inside this function.
    if (flag == InitializeAndReturn) {
        #if HAVE(COMPUTED_GOTO)
            #define ADD_OPCODE(id) m_opcodeTable[id] = &&id;
                FOR_EACH_OPCODE_ID(ADD_OPCODE);
            #undef ADD_OPCODE

            #define ADD_OPCODE_ID(id) m_opcodeIDTable.add(&&id, id);
                FOR_EACH_OPCODE_ID(ADD_OPCODE_ID);
            #undef ADD_OPCODE
            ASSERT(m_opcodeIDTable.size() == numOpcodeIDs);
        #endif // HAVE(COMPUTED_GOTO)
        return;
    }
    
    Vector<Register> registers;
    registers.reserveCapacity(512);

    registers.resize(codeBlock->numRegisters());
    Register* r = registers.data() + codeBlock->numLocals;
    Instruction* vPC = codeBlock->instructions.begin();
    JSValue** k = codeBlock->jsValues.data();

#ifndef NDEBUG
    dumpRegisters(registers, r);
#endif

#if HAVE(COMPUTED_GOTO)
    #define NEXT_OPCODE goto *vPC->u.opcode
    #define BEGIN_OPCODE(opcode) opcode:
    NEXT_OPCODE;
#else
    #define NEXT_OPCODE continue
    #define BEGIN_OPCODE(opcode) case opcode:
    while(1) // iterator loop begins
    switch (vPC->u.opcode)
#endif
    {
    BEGIN_OPCODE(op_load) {
        int r0 = (++vPC)->u.operand;
        int k0 = (++vPC)->u.operand;
        r[r0].u.jsValue = k[k0];
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mov) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        r[r0] = r[r1];

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_equal) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsBoolean(equal(exec, r[r1].u.jsValue, r[r2].u.jsValue));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_nequal) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsBoolean(!equal(exec, r[r1].u.jsValue, r[r2].u.jsValue));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_stricteq) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsBoolean(strictEqual(exec, r[r1].u.jsValue, r[r2].u.jsValue));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_nstricteq) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsBoolean(!strictEqual(exec, r[r1].u.jsValue, r[r2].u.jsValue));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_less) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsBoolean(jsLess(exec, r[r1].u.jsValue, r[r2].u.jsValue));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_lesseq) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsBoolean(jsLessEq(exec, r[r1].u.jsValue, r[r2].u.jsValue));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pre_inc) {
        int r0 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber(r[r0].u.jsValue->toNumber(exec) + 1);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_post_inc) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        r[r0].u.jsValue = r[r1].u.jsValue->toJSNumber(exec);
        r[r1].u.jsValue = jsNumber(r[r0].u.jsValue->toNumber(exec) + 1);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_to_jsnumber) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        r[r0].u.jsValue = r[r1].u.jsValue->toJSNumber(exec);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_negate) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber(-r[r1].u.jsValue->toNumber(exec));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_add) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsAdd(exec, r[r1].u.jsValue, r[r2].u.jsValue);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mult) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber(r[r1].u.jsValue->toNumber(exec) * r[r2].u.jsValue->toNumber(exec));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_div) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber(r[r1].u.jsValue->toNumber(exec) / r[r2].u.jsValue->toNumber(exec));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mod) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber(fmod(r[r1].u.jsValue->toNumber(exec), r[r2].u.jsValue->toNumber(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_sub) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber(r[r1].u.jsValue->toNumber(exec) - r[r2].u.jsValue->toNumber(exec));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_lshift) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber((r[r1].u.jsValue->toInt32(exec)) << (r[r2].u.jsValue->toUInt32(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_rshift) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber((r[r1].u.jsValue->toInt32(exec)) >> (r[r2].u.jsValue->toUInt32(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_urshift) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber((r[r1].u.jsValue->toUInt32(exec)) >> (r[r2].u.jsValue->toUInt32(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitand) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber((r[r1].u.jsValue->toInt32(exec)) & (r[r2].u.jsValue->toInt32(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitxor) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber((r[r1].u.jsValue->toInt32(exec)) ^ (r[r2].u.jsValue->toInt32(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitor) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber((r[r1].u.jsValue->toInt32(exec)) | (r[r2].u.jsValue->toInt32(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitnot) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber(~r[r1].u.jsValue->toInt32(exec));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_not) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsBoolean(!r[r1].u.jsValue->toBoolean(exec));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_instance_of) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;

        JSValue* v2 = r[r2].u.jsValue;

        // FIXME: this should throw a TypeError excpetion
        ASSERT(v2->isObject());

        JSObject* o2 = static_cast<JSObject*>(v2);
        r[r0].u.jsValue = jsBoolean(o2->implementsHasInstance() ? o2->hasInstance(exec, r[r1].u.jsValue) : false);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve) {
        resolve(exec, vPC, r, scopeChain, codeBlock);
        vPC += 3;
        
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_base) {
        resolveBase(exec, vPC, r, scopeChain, codeBlock);
        vPC += 3;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_object_get) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int k0 = (++vPC)->u.operand;

        Identifier& ident = codeBlock->identifiers[k0];
        r[r0].u.jsValue = r[r1].u.jsObject->get(exec, ident);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_object_put) {
        int r0 = (++vPC)->u.operand;
        int k0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;

        Identifier& ident = codeBlock->identifiers[k0];
        r[r0].u.jsObject->put(exec, ident, r[r1].u.jsValue);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jmp) {
        int offset = (++vPC)->u.operand;
        vPC += offset;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jtrue) {
        int r0 = (++vPC)->u.operand;
        int offset = (++vPC)->u.operand;
        if (r[r0].u.jsValue->toBoolean(exec)) {
            vPC += offset;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jfalse) {
        int r0 = (++vPC)->u.operand;
        int offset = (++vPC)->u.operand;
        if (!r[r0].u.jsValue->toBoolean(exec)) {
            vPC += offset;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_func) {
        int r0 = (++vPC)->u.operand;
        int k0 = (++vPC)->u.operand;

        r[r0].u.jsValue = codeBlock->functions[k0]->makeFunction(exec, *scopeChain);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_call) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        int argv = (++vPC)->u.operand;
        int argc = (++vPC)->u.operand;
        
        JSValue* v = r[r1].u.jsValue;

        ASSERT(v->isObject(&FunctionImp::info));
        FunctionImp* function = static_cast<FunctionImp*>(v);
        
        int rOffset = r - registers.data();
        Register* returnInfo = r + argv - returnInfoSize;

        returnInfo[0].u.codeBlock = codeBlock; // codeBlock after return
        returnInfo[1].u.vPC = ++vPC; // vPC after return
        returnInfo[2].u.scopeChain = scopeChain; // scope chain after return
        returnInfo[3].u.i = rOffset; // r value after return
        returnInfo[4].u.i = r0; // return value slot
        returnInfo[5].u.i = argv; // original argument vector (for the sake of the "arguments" object)
        // returnInfo[6] gets optionally set later

        r[argv].u.jsValue = r2 == static_cast<int>(missingSymbolMarker()) ? jsNull() : r[r2].u.jsValue; // "this" value

        FunctionBodyNode* functionBody = function->body.get();

        // WARNING: If code generation wants to optimize resolves to parent scopes,
        // it needs to be aware that, for functions that require activations,
        // the scope chain is off by one, since the activation hasn't been pushed yet.
        CodeBlock* newCodeBlock = &functionBody->code(*scopeChain);

        int offset = rOffset + argv + argc + newCodeBlock->numLocals;
        if (argc == newCodeBlock->numParameters) { // correct number of arguments
            registers.resize(offset + newCodeBlock->numLocals + newCodeBlock->numTemporaries);
            r = registers.data() + offset;
        } else if (argc < newCodeBlock->numParameters) { // too few arguments -- fill in the blanks
            int omittedArgCount = newCodeBlock->numParameters - argc;
            registers.resize(offset + omittedArgCount + newCodeBlock->numLocals + newCodeBlock->numTemporaries);
            r = registers.data() + omittedArgCount + offset;
            
            Register* end = r;
            for (Register* it = r - omittedArgCount; it != end; ++it)
                (*it).u.jsValue = jsUndefined();
        } else { // too many arguments -- copy return info and expected arguments, leaving the extra arguments behind
            registers.resize(offset + returnInfoSize + newCodeBlock->numParameters + newCodeBlock->numLocals + newCodeBlock->numTemporaries);
            r = registers.data() + returnInfoSize + newCodeBlock->numParameters + offset;

            int shift = returnInfoSize + argc;
            Register* it = r - newCodeBlock->numLocals - newCodeBlock->numParameters - returnInfoSize - shift;
            Register* end = it + returnInfoSize + newCodeBlock->numParameters;
            for ( ; it != end; ++it)
                *(it + shift) = *it;
        }

        if (newCodeBlock->needsActivation) {
            COMPILE_ASSERT(sizeof(ScopeChain) <= sizeof(returnInfo[6]), ScopeChain_fits_in_register);
            scopeChain = new (&returnInfo[6]) ScopeChain(function->scope());
            scopeChain->push(new JSActivation(functionBody, &registers, r - registers.data()));
        } else
            scopeChain = &function->scope();
        
        k = newCodeBlock->jsValues.data();
        vPC = newCodeBlock->instructions.begin();
        codeBlock = newCodeBlock;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_ret) {
        int r1 = (++vPC)->u.operand;

        CodeBlock* oldCodeBlock = codeBlock;

        Register* returnInfo = r - oldCodeBlock->numLocals - oldCodeBlock->numParameters - returnInfoSize;
        Register* returnValue = &r[r1];
        
        if (oldCodeBlock->needsActivation) {
            ASSERT(scopeChain->top()->isActivationObject());
            static_cast<JSActivation*>(scopeChain->top())->copyRegisters();
            scopeChain->~ScopeChain();
        }

        codeBlock = returnInfo[0].u.codeBlock;
        k = codeBlock->jsValues.data();
        vPC = returnInfo[1].u.vPC;
        scopeChain = returnInfo[2].u.scopeChain;
        r = registers.data() + returnInfo[3].u.i;
        int r0 = returnInfo[4].u.i;
        r[r0] = *returnValue;
        
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_push_scope) {
        int r0 = (++vPC)->u.operand;
        JSValue* v = r[r0].u.jsValue;
        JSObject* o = v->toObject(exec);
        ASSERT(!exec->hadException()); // FIXME: handle once we support exceptions.
        scopeChain->push(o);
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pop_scope) {
        scopeChain->pop();
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_end) {
        int r0 = (++vPC)->u.operand;
#ifndef NDEBUG
        printf("End: %s\n", r[r0].u.jsValue->toString(exec).ascii());
#else
        UNUSED_PARAM(r0);
#endif
        return;
    }
    }
    #undef NEXT_OPCODE
    #undef BEGIN_OPCODE
}

Machine& machine()
{
    ASSERT(JSLock::currentThreadIsHoldingLock());
    static Machine machine;
    return machine;
}

} // namespace KJS
