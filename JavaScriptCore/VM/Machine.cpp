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
#include "JSActivation.h"
#include "JSLock.h"
#include "JSPropertyNameIterator.h"
#include "Register.h"
#include "internal.h"

namespace KJS {

void Machine::dumpCallFrame(const CodeBlock* codeBlock, const ScopeChain* scopeChain, RegisterFile* registerFile, const Register* r)
{
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(scopeChain->bottom());
    InterpreterExecState tmpExec(globalObject, globalObject, reinterpret_cast<ProgramNode*>(0x1));
    codeBlock->dump(&tmpExec);
    dumpRegisters(codeBlock, registerFile, r);
}

void Machine::dumpRegisters(const CodeBlock* codeBlock, RegisterFile* registerFile, const Register* r)
{
    printf("Register frame: \n\n");
    printf("----------------------------------------\n");
    printf("     use      |   address  |    value   \n");
    printf("----------------------------------------\n");
    
    const Register* it;
    const Register* end;
    
    if (isGlobalCallFrame(registerFile->basePointer(), r)) {
        it = r - registerFile->numGlobalSlots();
        end = r;
        if (it != end) {
            do {
                printf("[global var]  | %10p | %10p \n", it, (*it).u.jsValue);
                ++it;
            } while (it != end);
            printf("----------------------------------------\n");
        }
    } else {
        it = r - codeBlock->numVars - codeBlock->numParameters - returnInfoSize;
        end = it + returnInfoSize;
        if (it != end) {
            do {
                printf("[return info] | %10p | %10p \n", it, (*it).u.jsValue);
                ++it;
            } while (it != end);
            printf("----------------------------------------\n");
        }
        
        end = it + codeBlock->numParameters;
        if (it != end) {
            do {
                printf("[param]       | %10p | %10p \n", it, (*it).u.jsValue);
                ++it;
            } while (it != end);
        }

        end = it + codeBlock->numVars;
        if (it != end) {
            do {
                printf("[var]         | %10p | %10p \n", it, (*it).u.jsValue);
                ++it;
            } while (it != end);
        }
    }

    end = it + codeBlock->numTemporaries;
    if (it != end) {
        do {
            printf("[temp]        | %10p | %10p \n", it, (*it).u.jsValue);
            ++it;
        } while (it != end);
    }
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

static JSValue* jsTypeStringForValue(JSValue* v)
{
    switch (v->type()) {
        case UndefinedType:
            return jsString("undefined");
        case NullType:
            return jsString("object");
        case BooleanType:
            return jsString("boolean");
        case NumberType:
            return jsString("number");
        case StringType:
            return jsString("string");
        default:
            if (v->isObject()) {
                // Return "undefined" for objects that should be treated
                // as null when doing comparisons.
                if (static_cast<JSObject*>(v)->masqueradeAsUndefined())
                    return jsString("undefined");
                else if (static_cast<JSObject*>(v)->implementsCall())
                    return jsString("function");
            }

            return jsString("object");
    }
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
    return opcode >= 0 && opcode <= op_end;
#endif
}

static void NEVER_INLINE resolve(ExecState* exec, Instruction* vPC, Register* r, ScopeChain* scopeChain, CodeBlock* codeBlock)
{
    int r0 = (vPC + 1)->u.operand;
    int id0 = (vPC + 2)->u.operand;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    PropertySlot slot;
    Identifier& ident = codeBlock->identifiers[id0];
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
    int id0 = (vPC + 2)->u.operand;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    PropertySlot slot;
    Identifier& ident = codeBlock->identifiers[id0];
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

NEVER_INLINE JSValue* prepareException(ExecState* exec, JSValue* exceptionValue) 
{
    if (exceptionValue->isObject()) {
        JSObject* exception = static_cast<JSObject*>(exceptionValue);
        if (!exception->hasProperty(exec, "line") && !exception->hasProperty(exec, "sourceURL")) {
            // Need to set line and sourceURL properties on the exception, but that is not currently possible
            exception->put(exec, "line", jsNumber(42));
            exception->put(exec, "sourceURL", jsString("FIXME: need sourceURL"));
        }
    }
    return exceptionValue;
}

NEVER_INLINE Instruction* Machine::unwindCallFrame(CodeBlock*& codeBlock, JSValue**& k, ScopeChain*& scopeChain, Register** registerBase, Register*& r)
{
    if (isGlobalCallFrame(registerBase, r)) {
        codeBlock = 0;
        return 0;
    }

    CodeBlock* oldCodeBlock = codeBlock;

    Register* returnInfo = r - oldCodeBlock->numVars - oldCodeBlock->numParameters - returnInfoSize;
    
    if (oldCodeBlock->needsActivation) {
        // Find the functions activation in the scope chain
        ScopeChainIterator iter = scopeChain->begin(); 
        ScopeChainIterator end = scopeChain->end();
        while (!((*iter)->isActivationObject())) {
            ++iter;
            ASSERT(iter != end);
        }

        // Clean up the activation if'n it's necessary
        ASSERT((*iter)->isActivationObject());
        static_cast<JSActivation*>(*iter)->copyRegisters();
        scopeChain->~ScopeChain();
    }
    
    codeBlock = returnInfo[0].u.codeBlock;
    if (!codeBlock)
        return 0; // 0 means we've hit a native call frame

    k = codeBlock->jsValues.data();
    scopeChain = returnInfo[2].u.scopeChain;
    r = (*registerBase) + returnInfo[3].u.i;
    return returnInfo[1].u.vPC;
}

NEVER_INLINE Instruction* Machine::throwException(CodeBlock*& codeBlock, JSValue**& k, ScopeChain*& scopeChain, Register** registerBase, Register*& r, const Instruction* vPC)
{
    while (codeBlock) {
        int expectedDepth;        
        Instruction* handlerPC = 0;
        if (!codeBlock->getHandlerForVPC(vPC, handlerPC, expectedDepth)) {
            vPC = unwindCallFrame(codeBlock, k, scopeChain, registerBase, r);
            continue;
        }
        // Now unwind the scope chain
        // Step 1) work out how deep the scope chain is
        int scopeDepth = scopeChain->depth();
        
        // Step 2) reduce to the expect depth
        int scopeDelta = scopeDepth - expectedDepth;
        
        // Step 3) Cry :-(
        ASSERT(scopeDelta >= 0);
        while (scopeDelta--)
            scopeChain->pop();
        return handlerPC;
    }
    return 0;
}

#if HAVE(COMPUTED_GOTO)
static void* throwTarget = 0;
#endif

void Machine::execute(ProgramNode* programNode, ExecState* exec, RegisterFileStack* registerFileStack, ScopeChain* scopeChain)
{
    RegisterFile* registerFile = registerFileStack->pushRegisterFile();
    CodeBlock* codeBlock = &programNode->code(*scopeChain);
    registerFile->addGlobalSlots(codeBlock->numVars);
    registerFile->resize(codeBlock->numTemporaries);

    privateExecute(Normal, exec, registerFile, scopeChain, codeBlock);

    registerFileStack->popRegisterFile();
}

void Machine::execute(FunctionBodyNode* functionBodyNode, ExecState* exec, RegisterFileStack* registerFileStack, ScopeChain* scopeChain)
{
    UNUSED_PARAM(exec);
    UNUSED_PARAM(registerFileStack);
    UNUSED_PARAM(scopeChain);
    UNUSED_PARAM(functionBodyNode);

    ASSERT_NOT_REACHED();
}

void Machine::privateExecute(ExecutionFlag flag, ExecState* exec, RegisterFile* registerFile, ScopeChain* scopeChain, CodeBlock* codeBlock)
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
            throwTarget = &&gcc_dependency_hack;
        #endif // HAVE(COMPUTED_GOTO)
        return;
    }

    JSValue* exceptionData = 0;
    
    Register** registerBase = registerFile->basePointer();
    Register* r = (*registerBase);
    Instruction* vPC = codeBlock->instructions.begin();
    Instruction* exceptionTarget = 0;
    JSValue** k = codeBlock->jsValues.data();
    
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
    BEGIN_OPCODE(op_new_object) {
        int r0 = (++vPC)->u.operand;
        r[r0].u.jsValue = exec->lexicalGlobalObject()->objectConstructor()->construct(exec, exec->emptyList());
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_array) {
        int r0 = (++vPC)->u.operand;
        r[r0].u.jsValue = exec->lexicalGlobalObject()->arrayConstructor()->construct(exec, exec->emptyList());
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_regexp) {
        int r0 = (++vPC)->u.operand;
        int re0 = (++vPC)->u.operand;
        r[r0].u.jsValue = exec->lexicalGlobalObject()->regExpConstructor()->createRegExpImp(exec, codeBlock->regexps[re0]);

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
    BEGIN_OPCODE(op_pre_dec) {
        int r0 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsNumber(r[r0].u.jsValue->toNumber(exec) - 1);

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
    BEGIN_OPCODE(op_post_dec) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        r[r0].u.jsValue = r[r1].u.jsValue->toJSNumber(exec);
        r[r1].u.jsValue = jsNumber(r[r0].u.jsValue->toNumber(exec) - 1);

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
    BEGIN_OPCODE(op_type_of) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        r[r0].u.jsValue = jsTypeStringForValue(r[r1].u.jsValue);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_in) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;

        JSValue* v2 = r[r2].u.jsValue;
        // FIXME: this should throw a TypeError excpetion
        ASSERT(v2->isObject());
        JSObject* o2 = static_cast<JSObject*>(v2);

        JSValue* v1 = r[r1].u.jsValue;

        uint32_t i;
        if (v1->getUInt32(i))
            r[r0].u.jsValue = jsBoolean(o2->hasProperty(exec, i));
        else
            r[r0].u.jsValue = jsBoolean(o2->hasProperty(exec, Identifier(v1->toString(exec))));

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
    BEGIN_OPCODE(op_get_prop_id) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int id0 = (++vPC)->u.operand;

        JSObject* base = r[r1].u.jsValue->toObject(exec);
        // FIXME: missing exception check
        Identifier& ident = codeBlock->identifiers[id0];
        r[r0].u.jsValue = base->get(exec, ident);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_prop_id) {
        int r0 = (++vPC)->u.operand;
        int id0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;

        JSObject* base = r[r0].u.jsValue->toObject(exec);
        // FIXME: missing exception check
        Identifier& ident = codeBlock->identifiers[id0];
        base->put(exec, ident, r[r1].u.jsValue);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_delete_prop_id) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int id0 = (++vPC)->u.operand;

        JSObject* base = r[r1].u.jsValue->toObject(exec);
        // FIXME: missing exception check
        Identifier& ident = codeBlock->identifiers[id0];
        r[r0].u.jsValue = jsBoolean(base->deleteProperty(exec, ident));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_prop_val) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;

        JSObject* base = r[r1].u.jsValue->toObject(exec);
        // FIXME: missing exception check
        JSValue* subscript = r[r2].u.jsValue;

        uint32_t i;
        if (subscript->getUInt32(i))
            r[r0].u.jsValue = base->get(exec, i);
        else
            r[r0].u.jsValue = base->get(exec, Identifier(subscript->toString(exec)));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_prop_val) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;

        JSObject* base = r[r0].u.jsValue->toObject(exec);
        // FIXME: missing exception check
        JSValue* subscript = r[r1].u.jsValue;

        uint32_t i;
        if (subscript->getUInt32(i))
            base->put(exec, i, r[r2].u.jsValue);
        else
            base->put(exec, Identifier(subscript->toString(exec)), r[r2].u.jsValue);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_delete_prop_val) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;

        JSObject* base = r[r1].u.jsValue->toObject(exec);
        // FIXME: missing exception check
        JSValue* subscript = r[r2].u.jsValue;

        uint32_t i;
        if (subscript->getUInt32(i))
            r[r0].u.jsValue = jsBoolean(base->deleteProperty(exec, i));
        else
            r[r0].u.jsValue = jsBoolean(base->deleteProperty(exec, Identifier(subscript->toString(exec))));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_prop_index) {
        int r0 = (++vPC)->u.operand;
        unsigned n0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;

        r[r0].u.jsObject->put(exec, n0, r[r1].u.jsValue);

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
        int f0 = (++vPC)->u.operand;

        r[r0].u.jsValue = codeBlock->functions[f0]->makeFunction(exec, *scopeChain);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_func_exp) {
        int r0 = (++vPC)->u.operand;
        int f0 = (++vPC)->u.operand;

        r[r0].u.jsValue = codeBlock->functionExpressions[f0]->makeFunction(exec, *scopeChain);

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
        
        CallData callData;
        CallType callType = v->getCallData(callData);
        
        if (callType == CallTypeJS) {
            int registerOffset = r - (*registerBase);
            Register* returnInfo = r + argv - returnInfoSize;

            returnInfo[0].u.codeBlock = codeBlock; // codeBlock after return
            returnInfo[1].u.vPC = ++vPC; // vPC after return
            returnInfo[2].u.scopeChain = scopeChain; // scope chain after return
            returnInfo[3].u.i = registerOffset; // offset of "r" after return
            returnInfo[4].u.i = r0; // return value slot
            returnInfo[5].u.i = argv; // original argument vector (for the sake of the "arguments" object)
            // returnInfo[6] gets optionally set later

            r[argv].u.jsValue = r2 == missingSymbolMarker() ? jsNull() : r[r2].u.jsValue; // "this" value

            // WARNING: If code generation wants to optimize resolves to parent scopes,
            // it needs to be aware that, for functions that require activations,
            // the scope chain is off by one, since the activation hasn't been pushed yet.
            CodeBlock* newCodeBlock = &callData.js.functionBody->code(*scopeChain);

            int offset = registerOffset + argv + argc + newCodeBlock->numVars;
            if (argc == newCodeBlock->numParameters) { // correct number of arguments
                size_t size = offset + newCodeBlock->numVars + newCodeBlock->numTemporaries;
                registerFile->resize(size);
                r = (*registerBase) + offset;
            } else if (argc < newCodeBlock->numParameters) { // too few arguments -- fill in the blanks
                int omittedArgCount = newCodeBlock->numParameters - argc;
                size_t size = offset + omittedArgCount + newCodeBlock->numVars + newCodeBlock->numTemporaries;
                registerFile->resize(size);
                r = (*registerBase) + omittedArgCount + offset;
                
                Register* end = r;
                for (Register* it = r - omittedArgCount; it != end; ++it)
                    (*it).u.jsValue = jsUndefined();
            } else { // too many arguments -- copy return info and expected arguments, leaving the extra arguments behind
                size_t size = offset + returnInfoSize + newCodeBlock->numParameters + newCodeBlock->numVars + newCodeBlock->numTemporaries;
                registerFile->resize(size);
                r = (*registerBase) + returnInfoSize + newCodeBlock->numParameters + offset;

                int shift = returnInfoSize + argc;
                Register* it = r - newCodeBlock->numVars - newCodeBlock->numParameters - returnInfoSize - shift;
                Register* end = it + returnInfoSize + newCodeBlock->numParameters;
                for ( ; it != end; ++it)
                    *(it + shift) = *it;
            }

            if (newCodeBlock->needsActivation) {
                COMPILE_ASSERT(sizeof(ScopeChain) <= sizeof(returnInfo[6]), ScopeChain_fits_in_register);
                scopeChain = new (&returnInfo[6]) ScopeChain(*callData.js.scopeChain);
                scopeChain->push(new JSActivation(callData.js.functionBody, registerBase, r - (*registerBase)));
            } else
                scopeChain = callData.js.scopeChain;
            
            k = newCodeBlock->jsValues.data();
            vPC = newCodeBlock->instructions.begin();
            codeBlock = newCodeBlock;

            NEXT_OPCODE;
        }
        
        if (callType == CallTypeNative) {
            int registerOffset = r - (*registerBase);
            
            // FIXME: Substitute lexical global object for null.

            r[argv].u.jsValue = r2 == missingSymbolMarker() ? jsNull() : (r[r2].u.jsValue)->toObject(exec); // "this" value
            JSObject* thisObj = static_cast<JSObject*>(r[argv].u.jsValue);
            // FIXME: needs exception check
            
            List args(&r[argv + 1].u.jsValue, argc - 1);

            r[r0].u.jsValue = static_cast<JSObject*>(v)->callAsFunction(exec, thisObj, args);

            r = (*registerBase) + registerOffset;

            ++vPC;
            NEXT_OPCODE;
        }

        ASSERT(callType == CallTypeNone);
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_ret) {
        int r1 = (++vPC)->u.operand;

        CodeBlock* oldCodeBlock = codeBlock;

        Register* returnInfo = r - oldCodeBlock->numVars - oldCodeBlock->numParameters - returnInfoSize;
        JSValue* returnValue = r[r1].u.jsValue;
        
        if (oldCodeBlock->needsActivation) {
            ASSERT(scopeChain->top()->isActivationObject());
            static_cast<JSActivation*>(scopeChain->top())->copyRegisters();
            scopeChain->~ScopeChain();
        }

        codeBlock = returnInfo[0].u.codeBlock;
        k = codeBlock->jsValues.data();
        vPC = returnInfo[1].u.vPC;
        scopeChain = returnInfo[2].u.scopeChain;
        r = (*registerBase) + returnInfo[3].u.i;
        int r0 = returnInfo[4].u.i;
        r[r0].u.jsValue = returnValue;
        
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_construct) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int argv = (++vPC)->u.operand;
        int argc = (++vPC)->u.operand;

        JSValue* v = r[r1].u.jsValue;

        // FIXME: We only support constructing native objects right now.
        // FIXME: We should not use CallType for ConstuctType. 
        CallData callData;
        CallType callType = v->getCallData(callData);
        ASSERT(callType == CallTypeNative);
        UNUSED_PARAM(callType);

        // FIXME: We need to throw a TypeError here if v is not an Object.
        ASSERT(v->isObject());

        JSObject* constructor = static_cast<JSObject*>(v);

        // FIXME: We need to throw a TypeError here if v doesn't implementConstuct.
        ASSERT(constructor->implementsConstruct());

        List args(&r[argv + 1].u.jsValue, argc - 1);
        r[r0].u.jsValue = constructor->construct(exec, args);

        ++vPC;
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
    BEGIN_OPCODE(op_get_pnames) {
        int iterReg = (++vPC)->u.operand;
        int objectReg = (++vPC)->u.operand;

        r[iterReg].u.jsPropertyNameIterator = JSPropertyNameIterator::create(exec, r[objectReg].u.jsValue);
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_next_pname) {
        int destReg = (++vPC)->u.operand;
        int iterReg = (++vPC)->u.operand;
        int offset = (++vPC)->u.operand;

        JSPropertyNameIterator* iter = r[iterReg].u.jsPropertyNameIterator;
        if (JSValue* temp = iter->next(exec)) {
            r[destReg].u.jsValue = temp;
            vPC += offset;
            NEXT_OPCODE;
        }
        iter->invalidate();
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jmp_scopes) {
        int scopeDelta = (++vPC)->u.operand;
        int offset = (++vPC)->u.operand;
        while (scopeDelta--)
            scopeChain->pop();
        vPC += offset;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_catch) {
        ASSERT(exceptionData);
        int r0 = (++vPC)->u.operand;
        r[r0].u.jsValue = exceptionData;
        exceptionData = 0;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_throw) {
        int e = (++vPC)->u.operand;
        exceptionData = prepareException(exec, r[e].u.jsValue);
        if (!(exceptionTarget = throwException(codeBlock, k, scopeChain, registerBase, r, vPC))) {
            // Removing this line of code causes a measurable regression on squirrelfish/function-missing-args.js.
            registerFile->resize(0);
            return;
        }

#if HAVE(COMPUTED_GOTO)
        // Hack around gcc performance quirk by performing an indirect goto
        // in order to set the vPC -- attempting to do so directly results in a
        // significant regression.
        goto *throwTarget; // indirect goto -> gcc_dependency_hack
    }
    gcc_dependency_hack: {
#endif

        vPC = exceptionTarget;
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
