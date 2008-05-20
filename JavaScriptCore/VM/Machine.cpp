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
#include "ExceptionHelpers.h"
#include "ExecState.h"
#include "JSActivation.h"
#include "JSLock.h"
#include "JSPropertyNameIterator.h"
#include "Register.h"
#include "internal.h"
#include "operations.h"
#include <kjs/operations.h>

#if COMPILER(GCC)
#define UNLIKELY(x) \
  __builtin_expect ((x), 0)
#else
#define UNLIKELY(x) x
#endif

namespace KJS {

void Machine::dumpCallFrame(const CodeBlock* codeBlock, ScopeChainNode* scopeChain, RegisterFile* registerFile, const Register* r)
{
    ScopeChain sc(scopeChain);
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(sc.bottom());
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
        it = r - codeBlock->numVars - codeBlock->numParameters - CallFrameHeaderSize;
        end = it + CallFrameHeaderSize;
        if (it != end) {
            do {
                printf("[call frame] | %10p | %10p \n", it, (*it).u.jsValue);
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
            printf("----------------------------------------\n");
        }

        end = it + codeBlock->numVars;
        if (it != end) {
            do {
                printf("[var]         | %10p | %10p \n", it, (*it).u.jsValue);
                ++it;
            } while (it != end);
            printf("----------------------------------------\n");
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
        && HashTraits<Opcode>::isDeletedValue(opcode)
        && m_opcodeIDTable.contains(opcode);
#else
    return opcode >= 0 && opcode <= op_end;
#endif
}

static bool NEVER_INLINE resolve(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue*& exceptionValue)
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
            return true;
        }
    } while (++iter != end);
    exceptionValue = createUndefinedVariableError(exec, ident);
    return false;
}

static void NEVER_INLINE resolveBase(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock)
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

static bool NEVER_INLINE resolveBaseAndProperty(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue*& exceptionValue)
{
    int r0 = (vPC + 1)->u.operand;
    int r1 = (vPC + 2)->u.operand;
    int id0 = (vPC + 3)->u.operand;
    
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    
    // FIXME: add scopeDepthIsZero optimization
    
    ASSERT(iter != end);
    
    PropertySlot slot;
    Identifier& ident = codeBlock->identifiers[id0];
    JSObject* base;
    do {
        base = *iter;
        if (base->getPropertySlot(exec, ident, slot)) {            
            r[r0].u.jsValue = base;
            r[r1].u.jsValue = slot.getValue(exec, base, ident);
            return true;
        }
        ++iter;
    } while (iter != end);
    
    exceptionValue = createUndefinedVariableError(exec, ident);
    return false;
}

static bool NEVER_INLINE resolveBaseAndFunc(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue*& exceptionValue)
{
    int r0 = (vPC + 1)->u.operand;
    int r1 = (vPC + 2)->u.operand;
    int id0 = (vPC + 3)->u.operand;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    
    // FIXME: add scopeDepthIsZero optimization
    
    ASSERT(iter != end);
    
    PropertySlot slot;
    Identifier& ident = codeBlock->identifiers[id0];
    JSObject* base;
    do {
        base = *iter;
        if (base->getPropertySlot(exec, ident, slot)) {            
            JSObject* thisObj = base;
            // ECMA 11.2.3 says that in this situation the this value should be null.
            // However, section 10.2.3 says that in the case where the value provided
            // by the caller is null, the global object should be used. It also says
            // that the section does not apply to internal functions, but for simplicity
            // of implementation we use the global object anyway here. This guarantees
            // that in host objects you always get a valid object for this.
            if (thisObj->isActivationObject())
                thisObj = exec->dynamicGlobalObject();
            
            r[r0].u.jsValue = thisObj;
            r[r1].u.jsValue = slot.getValue(exec, base, ident);
            return true;
        }
        ++iter;
    } while (iter != end);

    exceptionValue = createUndefinedVariableError(exec, ident);
    return false;
}

ALWAYS_INLINE void initializeCallFrame(Register* callFrame, CodeBlock* codeBlock, Instruction* vPC, ScopeChainNode* scopeChain, int registerOffset, int returnValueRegister, int argv, int calledAsConstructor)
{
    callFrame[Machine::CallerCodeBlock].u.codeBlock = codeBlock;
    callFrame[Machine::ReturnVPC].u.vPC = vPC + 1;
    callFrame[Machine::CallerScopeChain].u.scopeChain = scopeChain;
    callFrame[Machine::CallerRegisterOffset].u.i = registerOffset;
    callFrame[Machine::ReturnValueRegister].u.i = returnValueRegister;
    callFrame[Machine::ArgumentStartRegister].u.i = argv; // original argument vector (for the sake of the "arguments" object)
    callFrame[Machine::CalledAsConstructor].u.i = calledAsConstructor;
    // callFrame[Machine::OptionalCalleeActivation] gets optionally set later
}

ALWAYS_INLINE Register* slideRegisterWindowForCall(CodeBlock* newCodeBlock, RegisterFile* registerFile, Register** registerBase, int registerOffset, int argv, int argc)
{
    Register* r;
    registerOffset += argv + argc + newCodeBlock->numVars;

    if (argc == newCodeBlock->numParameters) { // correct number of arguments
        size_t size = registerOffset + newCodeBlock->numTemporaries;
        registerFile->grow(size);
        r = (*registerBase) + registerOffset;
    } else if (argc < newCodeBlock->numParameters) { // too few arguments -- fill in the blanks
        int omittedArgCount = newCodeBlock->numParameters - argc;
        size_t size = registerOffset + omittedArgCount + newCodeBlock->numTemporaries;
        registerFile->grow(size);
        r = (*registerBase) + omittedArgCount + registerOffset;
        
        Register* endOfParams = r - newCodeBlock->numVars;
        for (Register* it = endOfParams - omittedArgCount; it != endOfParams; ++it)
            (*it).u.jsValue = jsUndefined();
    } else { // too many arguments -- copy return info and expected arguments, leaving the extra arguments behind
        size_t size = registerOffset + Machine::CallFrameHeaderSize + newCodeBlock->numParameters + newCodeBlock->numTemporaries;
        registerFile->grow(size);
        r = (*registerBase) + Machine::CallFrameHeaderSize + newCodeBlock->numParameters + registerOffset;
        
        int shift = Machine::CallFrameHeaderSize + argc;
        Register* it = r - newCodeBlock->numVars - newCodeBlock->numParameters - Machine::CallFrameHeaderSize - shift;
        Register* end = it + Machine::CallFrameHeaderSize + newCodeBlock->numParameters;
        for ( ; it != end; ++it)
            *(it + shift) = *it;
    }

    return r;
}

ALWAYS_INLINE ScopeChainNode* scopeChainForCall(CodeBlock* newCodeBlock, ScopeChain* callDataScopeChain, FunctionBodyNode* functionBody, Register* callFrame, Register** registerBase, Register* r)
{
    if (newCodeBlock->needsActivation) {
        char scMem[sizeof(ScopeChain)];
        ScopeChain* sc = new (&scMem) ScopeChain(*callDataScopeChain);

        JSActivation* activation = new JSActivation(functionBody, registerBase, r - (*registerBase));
        sc->push(activation);
        ScopeChainNode* result = sc->node();
        callFrame[Machine::OptionalCalleeActivation].u.jsValue = activation;
        return result;
    } 

    return callDataScopeChain->node();
}

NEVER_INLINE Instruction* Machine::unwindCallFrame(CodeBlock*& codeBlock, JSValue**& k, ScopeChainNode*& scopeChain, Register** registerBase, Register*& r)
{
    if (isGlobalCallFrame(registerBase, r)) {
        codeBlock = 0;
        return 0;
    }

    CodeBlock* oldCodeBlock = codeBlock;

    Register* callFrame = r - oldCodeBlock->numVars - oldCodeBlock->numParameters - CallFrameHeaderSize;
    
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
        scopeChain->deref();
    }
    
    codeBlock = callFrame[CallerCodeBlock].u.codeBlock;
    if (!codeBlock)
        return 0; // 0 means we've hit a native call frame

    k = codeBlock->jsValues.data();
    scopeChain = callFrame[CallerScopeChain].u.scopeChain;
    r = (*registerBase) + callFrame[CallerRegisterOffset].u.i;
    return callFrame[ReturnVPC].u.vPC;
}

NEVER_INLINE Instruction* Machine::throwException(ExecState* exec, JSValue* exceptionValue, CodeBlock*& codeBlock, JSValue**& k, ScopeChainNode*& scopeChain, Register** registerBase, Register*& r, const Instruction* vPC)
{
    if (exceptionValue->isObject()) {
        JSObject* exception = static_cast<JSObject*>(exceptionValue);
        if (!exception->hasProperty(exec, "line") && !exception->hasProperty(exec, "sourceURL")) {
            // Need to set line and sourceURL properties on the exception, but that is not currently possible
            exception->put(exec, "line", jsNumber(42));
            exception->put(exec, "sourceURL", jsString("FIXME: need sourceURL"));
        }
    }

    while (codeBlock) {
        int expectedDepth;        
        Instruction* handlerVPC = 0;
        if (!codeBlock->getHandlerForVPC(vPC, handlerVPC, expectedDepth)) {
            vPC = unwindCallFrame(codeBlock, k, scopeChain, registerBase, r);
            continue;
        }

        // Now unwind the scope chain
        ScopeChain sc(scopeChain);
        // Step 1) work out how deep the scope chain is
        int scopeDepth = sc.depth();
        
        // Step 2) reduce to the expect depth
        int scopeDelta = scopeDepth - expectedDepth;
        
        // Step 3) Cry :-(
        ASSERT(scopeDelta >= 0);
        while (scopeDelta--)
            sc.pop();

        scopeChain = sc.node();
        
        return handlerVPC;
    }
    return 0;
}

static NEVER_INLINE bool isNotObject(ExecState* exec, const Instruction*, CodeBlock*, JSValue* value, JSValue*& exceptionData)
{
    if (value->isObject())
        return false;
    exceptionData = createNotAnObjectError(exec, value, 0);
    return true;
}

#if HAVE(COMPUTED_GOTO)
static void* throwTarget;
#endif

JSValue* Machine::execute(ProgramNode* programNode, ExecState* exec, JSObject* thisObj, RegisterFileStack* registerFileStack, ScopeChain* scopeChain, JSValue** exception)
{
    RegisterFile* registerFile = registerFileStack->pushRegisterFile();

    CodeBlock* codeBlock = &programNode->code(scopeChain->node());
    registerFile->addGlobalSlots(codeBlock->numVars);
    registerFile->grow(codeBlock->numTemporaries);
    Register* r = (*registerFile->basePointer());

    ASSERT(exec->dynamicGlobalObject()->symbolTable().get(CommonIdentifiers::shared()->thisIdentifier.ustring().rep()) == ProgramCodeThisRegister);
    r[ProgramCodeThisRegister].u.jsValue = thisObj;

    JSValue* result = privateExecute(Normal, exec, registerFile, r, scopeChain->node(), codeBlock, exception);

    registerFileStack->popRegisterFile();
    return result;
}

JSValue* Machine::execute(FunctionBodyNode* functionBodyNode, const List& args, JSObject* thisObj, ExecState* exec, RegisterFileStack* registerFileStack, ScopeChain* sc, JSValue** exception)
{
    RegisterFile* registerFile = registerFileStack->current();

    int argv = CallFrameHeaderSize;
    int argc = args.size() + 1; // implicit "this" parameter
    
    size_t oldSize = registerFile->size();
    registerFile->grow(oldSize + CallFrameHeaderSize + argc);
    
    Register** registerBase = registerFile->basePointer();
    int registerOffset = oldSize;
    Register* callFrame = (*registerBase) + registerOffset;
    
    // put return info in place, using 0 codeBlock to indicate built-in caller
    callFrame[CallerCodeBlock].u.codeBlock = 0;

    // put args in place, including "this"
    Register* dst = callFrame + CallFrameHeaderSize;
    (*dst).u.jsValue = thisObj;
    
    List::const_iterator end = args.end();
    for (List::const_iterator it = args.begin(); it != end; ++it)
        (*++dst).u.jsValue = *it;

    ScopeChainNode* scopeChain = sc->node();
    CodeBlock* newCodeBlock = &functionBodyNode->code(scopeChain);
    Register* r = slideRegisterWindowForCall(newCodeBlock, registerFile, registerBase, registerOffset, argv, argc);
    scopeChain = scopeChainForCall(newCodeBlock, sc, functionBodyNode, callFrame, registerBase, r);            

    JSValue* result = privateExecute(Normal, exec, registerFile, r, scopeChain, newCodeBlock, exception);
    registerFile->shrink(oldSize);
    return result;
}

JSValue* Machine::privateExecute(ExecutionFlag flag, ExecState* exec, RegisterFile* registerFile, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue** exception)
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
        return 0;
    }
    
    // Any pointer arithmetic to compensate for incrementing the vPC, currently
    // we only use for opcodes that perform a single increment of vPC.  As we
    // start applying the branch free exception logic to operands that increment
    // vPC by more than one Instruction we'll need to make this buffer larger.
    Instruction builtinThrow = getOpcode(op_builtin_throw);
    Instruction builtinThrowBuffer[] = { builtinThrow,
                                          builtinThrow};
    Instruction* exceptionTarget = 0;
    JSValue* exceptionValue = 0;
    
    Register** registerBase = registerFile->basePointer();
    Instruction* vPC = codeBlock->instructions.begin();
    JSValue** k = codeBlock->jsValues.data();
#define VM_CHECK_EXCEPTION() \
     do { \
        if (UNLIKELY(exec->hadException())) { \
            exceptionValue = exec->exception(); \
            exec->clearException(); \
            goto vm_throw; \
        } \
    }while (0)

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
        Instruction* target;
        double d = r[r0].u.jsValue->toNumber(exec, vPC, builtinThrowBuffer, target);
        r[r0].u.jsValue = jsNumber(d + 1);
        vPC = target + 1;
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
        /* add dst(r) src1(r) src2(r)

           Adds the contents of registers src1 and src2, and puts the
           result in register dst.
        */

        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        r[dst].u.jsValue = jsAdd(exec, r[src1].u.jsValue, r[src2].u.jsValue);

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

        if (isNotObject(exec, vPC, codeBlock, v2, exceptionValue))
            goto vm_throw;

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
        if (isNotObject(exec, vPC, codeBlock, v2, exceptionValue))
            goto vm_throw;

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
        if (UNLIKELY(!resolve(exec, vPC, r, scopeChain, codeBlock, exceptionValue)))
            goto vm_throw;

        vPC += 3;
        
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_base) {
        resolveBase(exec, vPC, r, scopeChain, codeBlock);
        vPC += 3;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_base_and_property) {
        if (UNLIKELY(!resolveBaseAndProperty(exec, vPC, r, scopeChain, codeBlock, exceptionValue)))
            goto vm_throw;

        vPC += 4;
        
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_base_and_func) {
        if (UNLIKELY(!resolveBaseAndFunc(exec, vPC, r, scopeChain, codeBlock, exceptionValue)))
            goto vm_throw;
        vPC += 4;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_prop_id) {
        /* get_prop_id dst(r) base(r) property(id)

           Converts the contents of register base to Object, gets the
           specified property from the object, and puts the result in
           register dst.
        */

        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        JSObject* baseObj = r[base].u.jsValue->toObject(exec);

        Identifier& ident = codeBlock->identifiers[property];
        r[dst].u.jsValue = baseObj->get(exec, ident);
        VM_CHECK_EXCEPTION();
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_prop_id) {
        int r0 = (++vPC)->u.operand;
        int id0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;

        JSObject* base = r[r0].u.jsValue->toObject(exec);
        
        Identifier& ident = codeBlock->identifiers[id0];
        base->put(exec, ident, r[r1].u.jsValue);
        
        VM_CHECK_EXCEPTION();
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_delete_prop_id) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int id0 = (++vPC)->u.operand;

        JSObject* base = r[r1].u.jsValue->toObject(exec);
        
        Identifier& ident = codeBlock->identifiers[id0];
        r[r0].u.jsValue = jsBoolean(base->deleteProperty(exec, ident));
        
        VM_CHECK_EXCEPTION();
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_prop_val) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;

        JSObject* base = r[r1].u.jsValue->toObject(exec); // may throw
        
        JSValue* subscript = r[r2].u.jsValue;

        uint32_t i;
        if (subscript->getUInt32(i))
            r[r0].u.jsValue = base->get(exec, i);
        else {
            VM_CHECK_EXCEPTION(); // If toObject threw, we must not call toString, which may execute arbitrary code
            r[r0].u.jsValue = base->get(exec, Identifier(subscript->toString(exec)));
        }
        
        VM_CHECK_EXCEPTION();
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_prop_val) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;

        JSObject* base = r[r0].u.jsValue->toObject(exec);
        
        JSValue* subscript = r[r1].u.jsValue;

        uint32_t i;
        if (subscript->getUInt32(i))
            base->put(exec, i, r[r2].u.jsValue);
        else {
            VM_CHECK_EXCEPTION(); // If toObject threw, we must not call toString, which may execute arbitrary code
            base->put(exec, Identifier(subscript->toString(exec)), r[r2].u.jsValue);
        }
        
        VM_CHECK_EXCEPTION();
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_delete_prop_val) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;

        JSObject* base = r[r1].u.jsValue->toObject(exec); // may throw

        JSValue* subscript = r[r2].u.jsValue;

        uint32_t i;
        if (subscript->getUInt32(i))
            r[r0].u.jsValue = jsBoolean(base->deleteProperty(exec, i));
        else {
            VM_CHECK_EXCEPTION(); // This is needed as toString may have side effects
            r[r0].u.jsValue = jsBoolean(base->deleteProperty(exec, Identifier(subscript->toString(exec))));
        }
        
        VM_CHECK_EXCEPTION();
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

        r[r0].u.jsValue = codeBlock->functions[f0]->makeFunction(exec, scopeChain);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_func_exp) {
        int r0 = (++vPC)->u.operand;
        int f0 = (++vPC)->u.operand;

        r[r0].u.jsValue = codeBlock->functionExpressions[f0]->makeFunction(exec, scopeChain);

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
            Register* callFrame = r + argv - CallFrameHeaderSize;

            r[argv].u.jsValue = r2 == missingSymbolMarker() ? jsNull() : r[r2].u.jsValue; // "this" value
            initializeCallFrame(callFrame, codeBlock, vPC, scopeChain, registerOffset, r0, argv, 0);

            ScopeChain* callDataScopeChain = callData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = callData.js.functionBody;

            codeBlock = &functionBodyNode->code(callDataScopeChain->node());
            r = slideRegisterWindowForCall(codeBlock, registerFile, registerBase, registerOffset, argv, argc);
            scopeChain = scopeChainForCall(codeBlock, callDataScopeChain, functionBodyNode, callFrame, registerBase, r);            
            k = codeBlock->jsValues.data();
            vPC = codeBlock->instructions.begin();

            NEXT_OPCODE;
        }

        if (callType == CallTypeNative) {
            int registerOffset = r - (*registerBase);
            
            // FIXME: Substitute lexical global object for null.

            r[argv].u.jsValue = r2 == missingSymbolMarker() ? jsNull() : (r[r2].u.jsValue)->toObject(exec); // "this" value
            JSObject* thisObj = static_cast<JSObject*>(r[argv].u.jsValue);

            List args(&r[argv + 1].u.jsValue, argc - 1);

            JSValue* returnValue = static_cast<JSObject*>(v)->callAsFunction(exec, thisObj, args);

            r = (*registerBase) + registerOffset;
            r[r0].u.jsValue = returnValue;

            VM_CHECK_EXCEPTION();

            ++vPC;
            NEXT_OPCODE;
        }

        // FIXME throw type error
        ASSERT(callType == CallTypeNone);

        vPC++;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_ret) {
        int r1 = (++vPC)->u.operand;

        CodeBlock* oldCodeBlock = codeBlock;

        Register* callFrame = r - oldCodeBlock->numVars - oldCodeBlock->numParameters - CallFrameHeaderSize;
        JSValue* returnValue = r[r1].u.jsValue;
        
        if (oldCodeBlock->needsActivation) {
            ASSERT(scopeChain->object->isActivationObject());
            static_cast<JSActivation*>(scopeChain->object)->copyRegisters();
            scopeChain->deref();
        }

        if (callFrame[CalledAsConstructor].u.i && !returnValue->isObject()) {
            JSValue* thisObject = callFrame[CallFrameHeaderSize].u.jsValue;
            returnValue = thisObject;
        }
         
        codeBlock = callFrame[CallerCodeBlock].u.codeBlock;
        if (!codeBlock)
            return returnValue;
        
        k = codeBlock->jsValues.data();
        vPC = callFrame[ReturnVPC].u.vPC;
        scopeChain = callFrame[CallerScopeChain].u.scopeChain;
        r = (*registerBase) + callFrame[CallerRegisterOffset].u.i;
        int r0 = callFrame[ReturnValueRegister].u.i;
        r[r0].u.jsValue = returnValue;
        
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_construct) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int argv = (++vPC)->u.operand;
        int argc = (++vPC)->u.operand;

        JSValue* v = r[r1].u.jsValue;

        ConstructData constructData;
        ConstructType constructType = v->getConstructData(constructData);

        // Removing this line of code causes a measurable regression on squirrelfish.
        JSObject* constructor = static_cast<JSObject*>(v);

        if (constructType == ConstructTypeJS) {
            int registerOffset = r - (*registerBase);
            Register* callFrame = r + argv - CallFrameHeaderSize;

            JSObject* prototype;
            JSValue* p = constructor->get(exec, exec->propertyNames().prototype);
            if (p->isObject())
                prototype = static_cast<JSObject*>(p);
            else
                prototype = exec->lexicalGlobalObject()->objectPrototype();
            JSObject* newObject = new JSObject(prototype);
            r[argv].u.jsValue = newObject; // "this" value

            initializeCallFrame(callFrame, codeBlock, vPC, scopeChain, registerOffset, r0, argv, 1);
            
            ScopeChain* callDataScopeChain = constructData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = constructData.js.functionBody;

            codeBlock = &functionBodyNode->code(callDataScopeChain->node());
            r = slideRegisterWindowForCall(codeBlock, registerFile, registerBase, registerOffset, argv, argc);
            scopeChain = scopeChainForCall(codeBlock, callDataScopeChain, functionBodyNode, callFrame, registerBase, r);            
            k = codeBlock->jsValues.data();
            vPC = codeBlock->instructions.begin();

            NEXT_OPCODE;
        }

        if (constructType == ConstructTypeNative) {
            int registerOffset = r - (*registerBase);

            List args(&r[argv + 1].u.jsValue, argc - 1);
            JSValue* returnValue = constructor->construct(exec, args);
        
            r = (*registerBase) + registerOffset;
            r[r0].u.jsValue = returnValue;
            
            ++vPC;
            NEXT_OPCODE;
        }

        if (isNotObject(exec, vPC, codeBlock, v, exceptionValue))
            goto vm_throw;

        // throw type error for non-contructor object
        ASSERT(constructType == ConstructTypeNone);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_push_scope) {
        int r0 = (++vPC)->u.operand;
        JSValue* v = r[r0].u.jsValue;
        JSObject* o = v->toObject(exec);
        VM_CHECK_EXCEPTION();
        
        ScopeChain sc(scopeChain);
        sc.push(o);
        scopeChain = sc.node();

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pop_scope) {
        ScopeChain sc(scopeChain);
        sc.pop();
        scopeChain = sc.node();

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
        ScopeChain sc(scopeChain);
        while (scopeDelta--)
            sc.pop();
        scopeChain = sc.node();
        vPC += offset;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_catch) {
        ASSERT(exceptionValue);
        int r0 = (++vPC)->u.operand;
        r[r0].u.jsValue = exceptionValue;
        exceptionValue = 0;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_throw) {
        int e = (++vPC)->u.operand;
        exceptionValue = r[e].u.jsValue;
        exceptionTarget = throwException(exec, exceptionValue, codeBlock, k, scopeChain, registerBase, r, vPC);
        if (!exceptionTarget) {
            *exception = exceptionValue;
            return jsNull();
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
        return r[r0].u.jsValue;
    }
    BEGIN_OPCODE(op_builtin_throw) {
        ASSERT(exec->exceptionSource()->u.opcode != getOpcode(op_builtin_throw));
        exceptionValue = exec->exception();
        vPC = exec->exceptionSource();
        exec->clearExceptionSource();
        exec->clearException();
        // fall through to vm_throw to allow shared logic to do the actual
        // exception handling.
    }
    vm_throw: {
        exceptionTarget = throwException(exec, exceptionValue, codeBlock, k, scopeChain, registerBase, r, vPC);
        if (!exceptionTarget) {
            *exception = exceptionValue;
            return jsNull();
        }
        vPC = exceptionTarget;
        NEXT_OPCODE;
    }          
    }
    #undef NEXT_OPCODE
    #undef BEGIN_OPCODE
    #undef VM_CHECK_EXCEPTION
}

Machine& machine()
{
    ASSERT(JSLock::currentThreadIsHoldingLock());
    static Machine machine;
    return machine;
}

} // namespace KJS
