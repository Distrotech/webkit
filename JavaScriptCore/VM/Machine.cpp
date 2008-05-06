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
#include "Parser.h"
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

#if HAVE(COMPUTED_GOTO)
static void* op_throw_end_indirect;
static void* op_call_indirect;
#endif

// Returns the depth of the scope chain within a given call frame.
static int depth(ScopeChain& sc)
{
    int scopeDepth = 0;
    ScopeChainIterator iter = sc.begin(); 
    ScopeChainIterator end = sc.end(); 
    while (!(*iter)->isVariableObject()) {
        ++iter;
        ++scopeDepth;
    }
    return scopeDepth;
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

static bool NEVER_INLINE resolve(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue*& exceptionValue)
{
    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    PropertySlot slot;
    Identifier& ident = codeBlock->identifiers[property];
    do {
        JSObject* o = *iter;
        if (o->getPropertySlot(exec, ident, slot)) {
            r[dst].u.jsValue = slot.getValue(exec, o, ident);
            exceptionValue = exec->exception();
            return !exceptionValue;
        }
    } while (++iter != end);
    exceptionValue = createUndefinedVariableError(exec, ident);
    return false;
}

static void NEVER_INLINE resolveBase(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock)
{
    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    PropertySlot slot;
    Identifier& ident = codeBlock->identifiers[property];
    JSObject* base;
    do {
        base = *iter;
        if (base->getPropertySlot(exec, ident, slot)) {
            r[dst].u.jsValue = base;
            return;
        }
    } while (++iter != end);

    r[dst].u.jsValue = base;
}

static bool NEVER_INLINE resolveBaseAndProperty(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue*& exceptionValue)
{
    int baseDst = (vPC + 1)->u.operand;
    int propDst = (vPC + 2)->u.operand;
    int property = (vPC + 3)->u.operand;
    
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    
    // FIXME: add scopeDepthIsZero optimization
    
    ASSERT(iter != end);
    
    PropertySlot slot;
    Identifier& ident = codeBlock->identifiers[property];
    JSObject* base;
    do {
        base = *iter;
        if (base->getPropertySlot(exec, ident, slot)) {            
            r[baseDst].u.jsValue = base;
            r[propDst].u.jsValue = slot.getValue(exec, base, ident);
            return true;
        }
        ++iter;
    } while (iter != end);
    
    exceptionValue = createUndefinedVariableError(exec, ident);
    return false;
}

static bool NEVER_INLINE resolveBaseAndFunc(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue*& exceptionValue)
{
    int baseDst = (vPC + 1)->u.operand;
    int funcDst = (vPC + 2)->u.operand;
    int property = (vPC + 3)->u.operand;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    
    // FIXME: add scopeDepthIsZero optimization
    
    ASSERT(iter != end);
    
    PropertySlot slot;
    Identifier& ident = codeBlock->identifiers[property];
    JSObject* base;
    do {
        base = *iter;
        if (base->getPropertySlot(exec, ident, slot)) {            
            // ECMA 11.2.3 says that if we hit an activation the this value should be null.
            // However, section 10.2.3 says that in the case where the value provided
            // by the caller is null, the global object should be used. It also says
            // that the section does not apply to internal functions, but for simplicity
            // of implementation we use the global object anyway here. This guarantees
            // that in host objects you always get a valid object for this.
            // We also handle wrapper substitution for the global object at the same time.
            JSObject* thisObj = base->toThisObject(exec);
            
            r[baseDst].u.jsValue = thisObj;
            r[funcDst].u.jsValue = slot.getValue(exec, base, ident);
            return true;
        }
        ++iter;
    } while (iter != end);

    exceptionValue = createUndefinedVariableError(exec, ident);
    return false;
}

ALWAYS_INLINE void initializeCallFrame(Register* callFrame, CodeBlock* codeBlock, Instruction* vPC, ScopeChainNode* scopeChain, int registerOffset, int returnValueRegister, int argv, int argc, int calledAsConstructor, JSValue* function)
{
    callFrame[Machine::CallerCodeBlock].u.codeBlock = codeBlock;
    callFrame[Machine::ReturnVPC].u.vPC = vPC + 1;
    callFrame[Machine::CallerScopeChain].u.scopeChain = scopeChain;
    callFrame[Machine::CallerRegisterOffset].u.i = registerOffset;
    callFrame[Machine::ReturnValueRegister].u.i = returnValueRegister;
    callFrame[Machine::ArgumentStartRegister].u.i = argv; // original argument vector (for the sake of the "arguments" object)
    callFrame[Machine::ArgumentCount].u.i = argc; // original argument count (for the sake of the "arguments" object)
    callFrame[Machine::CalledAsConstructor].u.i = calledAsConstructor;
    callFrame[Machine::Callee].u.jsValue = function;
    callFrame[Machine::OptionalCalleeActivation].u.jsValue = 0;
}

ALWAYS_INLINE Register* slideRegisterWindowForCall(ExecState* exec, CodeBlock* newCodeBlock, RegisterFile* registerFile, Register** registerBase, int registerOffset, int argv, int argc, JSValue*& exceptionValue)
{
    Register* r = 0;
    int oldOffset = registerOffset;
    registerOffset += argv + argc + newCodeBlock->numVars;

    if (argc == newCodeBlock->numParameters) { // correct number of arguments
        size_t size = registerOffset + newCodeBlock->numTemporaries;
        if (!registerFile->grow(size)) {
            exceptionValue = createStackOverflowError(exec);
            return *registerBase + oldOffset;
        }
        r = (*registerBase) + registerOffset;
    } else if (argc < newCodeBlock->numParameters) { // too few arguments -- fill in the blanks
        int omittedArgCount = newCodeBlock->numParameters - argc;
        size_t size = registerOffset + omittedArgCount + newCodeBlock->numTemporaries;
        if (!registerFile->grow(size)) {
            exceptionValue = createStackOverflowError(exec);
            return *registerBase + oldOffset;
        }
        r = (*registerBase) + omittedArgCount + registerOffset;
        
        Register* endOfParams = r - newCodeBlock->numVars;
        for (Register* it = endOfParams - omittedArgCount; it != endOfParams; ++it)
            (*it).u.jsValue = jsUndefined();
    } else { // too many arguments -- copy return info and expected arguments, leaving the extra arguments behind
        size_t size = registerOffset + Machine::CallFrameHeaderSize + newCodeBlock->numParameters + newCodeBlock->numTemporaries;
        if (!registerFile->grow(size)) {
            exceptionValue = createStackOverflowError(exec);
            return *registerBase + oldOffset;
        }
        r = (*registerBase) + Machine::CallFrameHeaderSize + newCodeBlock->numParameters + registerOffset;
        
        int shift = Machine::CallFrameHeaderSize + argc;
        Register* it = r - newCodeBlock->numLocals - Machine::CallFrameHeaderSize - shift;
        Register* end = it + Machine::CallFrameHeaderSize + newCodeBlock->numParameters;
        for ( ; it != end; ++it)
            *(it + shift) = *it;
    }

    return r;
}

ALWAYS_INLINE ScopeChainNode* scopeChainForCall(FunctionBodyNode* functionBodyNode, CodeBlock* newCodeBlock, ScopeChainNode* callDataScopeChain, Register* callFrame, Register** registerBase, Register* r)
{
    if (newCodeBlock->needsFullScopeChain) {
        JSActivation* activation = new JSActivation(functionBodyNode, registerBase, r - (*registerBase));
        callFrame[Machine::OptionalCalleeActivation].u.jsValue = activation;

        ScopeChainNode* scopeChain = callDataScopeChain->copy();
        scopeChain = scopeChain->push(activation);
        return scopeChain;
    }

    return callDataScopeChain;
}

static NEVER_INLINE bool isNotObject(ExecState* exec, const Instruction*, CodeBlock*, JSValue* value, JSValue*& exceptionData)
{
    if (value->isObject())
        return false;
    exceptionData = createNotAnObjectError(exec, value, 0);
    return true;
}

static NEVER_INLINE JSValue* eval(ExecState* exec, JSObject* thisObj, ScopeChainNode* scopeChain, RegisterFile* registerFile, Register* r, int argv, int argc, JSValue*& exceptionValue)
{
    JSValue* x = argc >= 2 ? r[argv + 1].u.jsValue : jsUndefined();
    
    if (!x->isString())
        return x;
    
    UString s = x->toString(exec);
    if (exec->hadException()) {
        exceptionValue = exec->exception();
        exec->clearException();
        return 0;
    }

    int sourceId;
    int errLine;
    UString errMsg;
    RefPtr<EvalNode> evalNode = parser().parse<EvalNode>(UString(), 0, s.data(), s.size(), &sourceId, &errLine, &errMsg);
    
    if (!evalNode) {
        exceptionValue = Error::create(exec, SyntaxError, errMsg, errLine, sourceId, NULL);
        return 0;
    }

    return machine().execute(evalNode.get(), exec, thisObj, registerFile, r - (*registerFile->basePointer()) + argv + argc, scopeChain, &exceptionValue);
}

Machine& machine()
{
    ASSERT(JSLock::currentThreadIsHoldingLock());
    static Machine machine;
    return machine;
}

Machine::Machine()
    : m_reentryDepth(0)
{
    privateExecute(InitializeAndReturn);
}

void Machine::dumpCallFrame(const CodeBlock* codeBlock, ScopeChainNode* scopeChain, RegisterFile* registerFile, const Register* r)
{
    ScopeChain sc(scopeChain);
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(sc.bottom());
    codeBlock->dump(globalObject->globalExec());
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
        it = r - codeBlock->numLocals - CallFrameHeaderSize;
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

NEVER_INLINE bool Machine::unwindCallFrame(ExecState* exec, Register** registerBase, const Instruction*& vPC, CodeBlock*& codeBlock, JSValue**& k, ScopeChainNode*& scopeChain, Register*& r)
{
    CodeBlock* oldCodeBlock = codeBlock;

    if (oldCodeBlock->needsFullScopeChain) {
        // If this call frame created an activation, tear it off.
        ScopeChainIterator end = scopeChain->end();
        for (ScopeChainIterator it = scopeChain->begin(); it != end; ++it) {
            if ((*it)->isActivationObject()) {
                static_cast<JSActivation*>(*it)->copyRegisters();
                break;
            }
        }

        scopeChain->deref();
    }
    
    if (isGlobalCallFrame(registerBase, r))
        return false;

    Register* callFrame = r - oldCodeBlock->numLocals - CallFrameHeaderSize;
    
    codeBlock = callFrame[CallerCodeBlock].u.codeBlock;
    if (!codeBlock)
        return false;

    k = codeBlock->jsValues.data();
    scopeChain = callFrame[CallerScopeChain].u.scopeChain;
    int callerRegisterOffset = callFrame[CallerRegisterOffset].u.i;
    r = (*registerBase) + callerRegisterOffset;
    exec->m_callFrameOffset = callerRegisterOffset - codeBlock->numLocals - CallFrameHeaderSize;
    vPC = callFrame[ReturnVPC].u.vPC;
    return true;
}

NEVER_INLINE Instruction* Machine::throwException(ExecState* exec, JSValue* exceptionValue, Register** registerBase, const Instruction* vPC, CodeBlock*& codeBlock, JSValue**& k, ScopeChainNode*& scopeChain, Register*& r)
{
    // Set up the exception object

    if (exceptionValue->isObject()) {
        JSObject* exception = static_cast<JSObject*>(exceptionValue);
        if (!exception->hasProperty(exec, "line") && !exception->hasProperty(exec, "sourceURL")) {
            // Need to set line and sourceURL properties on the exception, but that is not currently possible
            exception->put(exec, "line", jsNumber(codeBlock->lineNumberForVPC(vPC)));
            exception->put(exec, "sourceURL", jsOwnedString(codeBlock->sourceURL));
        }
    }

    // Calculate an exception handler vPC, unwinding call frames as necessary.

    int scopeDepth;        
    Instruction* handlerVPC;

    while (!codeBlock->getHandlerForVPC(vPC, handlerVPC, scopeDepth))
        if (!unwindCallFrame(exec, registerBase, vPC, codeBlock, k, scopeChain, r))
            return 0;

    // Now unwind the scope chain within the exception handler's call frame.
    
    ScopeChain sc(scopeChain);
    int scopeDelta = depth(sc) - scopeDepth;
    ASSERT(scopeDelta >= 0);
    while (scopeDelta--)
        sc.pop();
    setScopeChain(exec, scopeChain, sc.node());

    return handlerVPC;
}

JSValue* Machine::execute(ProgramNode* programNode, ExecState* exec, ScopeChainNode* scopeChain, JSObject* thisObj, RegisterFileStack* registerFileStack, JSValue** exception)
{
    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(exec);
        return 0;
    }

    RegisterFile* registerFile = registerFileStack->pushGlobalRegisterFile();
    CodeBlock* codeBlock = &programNode->code(scopeChain, registerFileStack->current()->isGlobal());
    registerFile->addGlobalSlots(codeBlock->numVars);

    registerFile->uncheckedGrow(codeBlock->numTemporaries);
    Register* r = (*registerFile->basePointer());

    ASSERT(exec->dynamicGlobalObject()->symbolTable().get(CommonIdentifiers::shared()->thisIdentifier.ustring().rep()).index == ProgramCodeThisRegister);
    r[ProgramCodeThisRegister].u.jsValue = thisObj;
    
    if (codeBlock->needsFullScopeChain)
        scopeChain = scopeChain->copy();
    
    ExecState newExec(exec, this, registerFile, scopeChain, -1);

    m_reentryDepth++;
    JSValue* result = privateExecute(Normal, &newExec, registerFile, r, scopeChain, codeBlock, exception);
    m_reentryDepth--;

    registerFileStack->popGlobalRegisterFile();
    return result;
}

JSValue* Machine::execute(FunctionBodyNode* functionBodyNode, ExecState* exec, FunctionImp* function, JSObject* thisObj, const List& args, RegisterFileStack* registerFileStack, ScopeChainNode* scopeChain, JSValue** exception)
{
    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(exec);
        return 0;
    }

    RegisterFile* registerFile = registerFileStack->current();

    int argv = CallFrameHeaderSize;
    int argc = args.size() + 1; // implicit "this" parameter
    
    size_t oldSize = registerFile->size();
    if (!registerFile->grow(oldSize + CallFrameHeaderSize + argc)) {
        *exception = createStackOverflowError(exec);
        return 0;
    }
        
    Register** registerBase = registerFile->basePointer();
    int registerOffset = oldSize;
    int callFrameOffset = registerOffset;
    Register* callFrame = (*registerBase) + callFrameOffset;
    
    // put args in place, including "this"
    Register* dst = callFrame + CallFrameHeaderSize;
    (*dst).u.jsValue = thisObj;
    
    List::const_iterator end = args.end();
    for (List::const_iterator it = args.begin(); it != end; ++it)
        (*++dst).u.jsValue = *it;

    // put call frame in place, using a 0 codeBlock to indicate a built-in caller
    initializeCallFrame(callFrame, 0, 0, 0, registerOffset, 0, argv, argc, 0, function);

    CodeBlock* newCodeBlock = &functionBodyNode->code(scopeChain);
    Register* r = slideRegisterWindowForCall(exec, newCodeBlock, registerFile, registerBase, registerOffset, argv, argc, *exception);
    if (*exception) {
        registerFile->shrink(oldSize);
        return 0;
    }

    callFrame = (*registerBase) + callFrameOffset; // registerBase may have moved, recompute callFrame
    scopeChain = scopeChainForCall(functionBodyNode, newCodeBlock, scopeChain, callFrame, registerBase, r);            

    ExecState newExec(exec, this, registerFile, scopeChain, callFrameOffset);

    m_reentryDepth++;
    JSValue* result = privateExecute(Normal, &newExec, registerFile, r, scopeChain, newCodeBlock, exception);
    m_reentryDepth--;

    registerFile->shrink(oldSize);
    return result;
    
}

JSValue* Machine::execute(EvalNode* evalNode, ExecState* exec, JSObject* thisObj, RegisterFile* registerFile, int registerOffset, ScopeChainNode* scopeChain, JSValue** exception)
{
    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(exec);
        return 0;
    }
    EvalCodeBlock* codeBlock = &evalNode->code(scopeChain);
    
    JSVariableObject* variableObject;
    for (ScopeChainNode* node = scopeChain; ; node = node->next) {
        ASSERT(node);
        if (node->object->isVariableObject()) {
            variableObject = static_cast<JSVariableObject*>(node->object);
            break;
        }
    }
    
    for (Vector<Identifier>::const_iterator iter = codeBlock->declaredVariableNames.begin(); iter != codeBlock->declaredVariableNames.end(); ++iter) {
        Identifier ident = *iter;
        
        if (!variableObject->hasProperty(exec, ident))
            variableObject->put(exec, ident, jsUndefined());
    }
    
    ASSERT(codeBlock->functions.size() == codeBlock->declaredFunctionNames.size());
    for (size_t i = 0; i < codeBlock->functions.size(); ++i)
        variableObject->put(exec, codeBlock->declaredFunctionNames[i], codeBlock->functions[i]->makeFunction(exec, scopeChain));
    
    size_t oldSize = registerFile->size();
    size_t newSize = registerOffset + codeBlock->numVars + codeBlock->numTemporaries + CallFrameHeaderSize;
    if (!registerFile->grow(newSize)) {
        *exception = createStackOverflowError(exec);
        return 0;
    }
    Register* r = (*registerFile->basePointer()) + registerOffset + codeBlock->numVars + CallFrameHeaderSize;
    
    ((*registerFile->basePointer()) + registerOffset)[CallerCodeBlock].u.codeBlock = 0;
    r[ProgramCodeThisRegister].u.jsValue = thisObj;

    if (codeBlock->needsFullScopeChain)
        scopeChain = scopeChain->copy();

    ExecState newExec(exec, this, registerFile, scopeChain, -1);

    m_reentryDepth++;
    JSValue* result = privateExecute(Normal, &newExec, registerFile, r, scopeChain, codeBlock, exception);
    m_reentryDepth--;

    registerFile->shrink(oldSize);
    return result;
}

JSValue* Machine::execute(EvalNode* evalNode, ExecState* exec, JSObject* thisObj, RegisterFileStack* registerFileStack, ScopeChainNode* scopeChain, JSValue** exception)
{
    RegisterFile* registerFile = registerFileStack->current();
    return Machine::execute(evalNode, exec, thisObj, registerFile, registerFile->size(), scopeChain, exception);
}

ALWAYS_INLINE void Machine::setScopeChain(ExecState* exec, ScopeChainNode*& scopeChain, ScopeChainNode* newScopeChain)
{
    scopeChain = newScopeChain;
    exec->m_scopeChain = newScopeChain;
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
            op_throw_end_indirect = &&op_throw_end;
            op_call_indirect = &&op_call;
        #endif // HAVE(COMPUTED_GOTO)
        return 0;
    }
    
    // Any pointer arithmetic to compensate for incrementing the vPC, currently
    // we only use for opcodes that perform a single increment of vPC.  As we
    // start applying the branch free exception logic to operands that increment
    // vPC by more than one Instruction we'll need to make this buffer larger.
    Instruction builtinThrow = getOpcode(op_builtin_throw);
    Instruction builtinThrowBuffer[] = { builtinThrow, builtinThrow };
    JSValue* exceptionValue = 0;
    Instruction* handlerVPC = 0;
    
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
    } while (0)

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
        /* load dst(r) src(k)

           Copies constant src to register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst].u.jsValue = k[src];
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_object) {
        /* new_object dst(r)

           Constructs a new empty Object instance using the original
           constructor, and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        r[dst].u.jsValue = scopeChain->globalObject()->objectConstructor()->construct(exec, exec->emptyList());
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_array) {
        /* new_array dst(r)

           Constructs a new empty Array instance using the original
           constructor, and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        r[dst].u.jsValue = scopeChain->globalObject()->arrayConstructor()->construct(exec, exec->emptyList());
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_regexp) {
        /* new_regexp dst(r) regExp(re)

           Constructs a new RegExp instance using the original
           constructor from regexp regExp, and puts the result in
           register dst.
        */
        int dst = (++vPC)->u.operand;
        int regExp = (++vPC)->u.operand;
        r[dst].u.jsValue = new RegExpImp(scopeChain->globalObject()->regExpPrototype(), codeBlock->regexps[regExp]);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mov) {
        /* mov dst(r) src(r)

           Copies register src to register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst] = r[src];

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_eq) {
        /* eq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are equal,
           as with the ECMAScript '==' operator, and puts the result
           as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        JSValue* result = jsBoolean(equal(exec, r[src1].u.jsValue, r[src2].u.jsValue));
        VM_CHECK_EXCEPTION();
        r[dst].u.jsValue = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_neq) {
        /* neq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           equal, as with the ECMAScript '!=' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        JSValue* result = jsBoolean(!equal(exec, r[src1].u.jsValue, r[src2].u.jsValue));
        VM_CHECK_EXCEPTION();
        r[dst].u.jsValue = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_stricteq) {
        /* stricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are strictly
           equal, as with the ECMAScript '===' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        r[dst].u.jsValue = jsBoolean(strictEqual(r[src1].u.jsValue, r[src2].u.jsValue));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_nstricteq) {
        /* nstricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           strictly equal, as with the ECMAScript '!==' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        r[dst].u.jsValue = jsBoolean(!strictEqual(r[src1].u.jsValue, r[src2].u.jsValue));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_less) {
        /* less dst(r) src1(r) src2(r)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and puts the result as
           a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        r[dst].u.jsValue = jsBoolean(jsLess(exec, r[src1].u.jsValue, r[src2].u.jsValue));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_lesseq) {
        /* lesseq dst(r) src1(r) src2(r)

           Checks whether register src1 is less than or equal to
           register src2, as with the ECMAScript '<=' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        r[dst].u.jsValue = jsBoolean(jsLessEq(exec, r[src1].u.jsValue, r[src2].u.jsValue));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pre_inc) {
        /* pre_inc srcDst(r)

           Converts register srcDst to number, adds one, and puts the result
           back in register srcDst.
        */
        int srcDst = (++vPC)->u.operand;
        Instruction* target;
        double d = r[srcDst].u.jsValue->toNumber(exec, vPC, builtinThrowBuffer, target);
        r[srcDst].u.jsValue = jsNumber(d + 1);
        
        vPC = target + 1;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pre_dec) {
        /* pre_dec srcDst(r)

           Converts register srcDst to number, subtracts one, and puts the result
           back in register srcDst.
        */
        int srcDst = (++vPC)->u.operand;
        Instruction* target;
        double d = r[srcDst].u.jsValue->toNumber(exec, vPC, builtinThrowBuffer, target);
        r[srcDst].u.jsValue = jsNumber(d - 1);
        
        vPC = target + 1;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_post_inc) {
        /* post_inc dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number plus one is written
           back to register srcDst.
        */
        int dst = (++vPC)->u.operand;
        int srcDst = (++vPC)->u.operand;
        r[dst].u.jsValue = r[srcDst].u.jsValue->toJSNumber(exec);
        r[srcDst].u.jsValue = jsNumber(r[dst].u.jsValue->toNumber(exec) + 1);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_post_dec) {
        /* post_dec dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number minus one is written
           back to register srcDst.
        */
        int dst = (++vPC)->u.operand;
        int srcDst = (++vPC)->u.operand;
        r[dst].u.jsValue = r[srcDst].u.jsValue->toJSNumber(exec);
        r[srcDst].u.jsValue = jsNumber(r[dst].u.jsValue->toNumber(exec) - 1);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_to_jsnumber) {
        /* to_jsnumber dst(r) src(r)

           Converts register src to number, and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst].u.jsValue = r[src].u.jsValue->toJSNumber(exec);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_negate) {
        /* negate dst(r) src(r)

           Converts register src to number, negates it, and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        
        Instruction* target;
        double d = r[src].u.jsValue->toNumber(exec, vPC, builtinThrowBuffer, target);
        r[dst].u.jsValue = jsNumber(-d);

        vPC = target + 1;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_add) {
        /* add dst(r) src1(r) src2(r)

           Adds register src1 and register src2, and puts the result
           in register dst. (JS add may be string concatenation or
           numeric add, depending on the types of the operands.)
        */
        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        r[dst].u.jsValue = jsAdd(exec, r[src1].u.jsValue, r[src2].u.jsValue);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mul) {
        /* mul dst(r) src1(r) src2(r)

           Multiplies register src1 and register src2 (converted to
           numbers), and puts the product in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        r[dst].u.jsValue = jsNumber(r[src1].u.jsValue->toNumber(exec) * r[src2].u.jsValue->toNumber(exec));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_div) {
        /* div dst(r) dividend(r) divisor(r)

           Divides register dividend (converted to number) by the
           register divisor (converted to number), and puts the
           quotient in register dst.
        */
        int dst = (++vPC)->u.operand;
        int dividend = (++vPC)->u.operand;
        int divisor = (++vPC)->u.operand;
        r[dst].u.jsValue = jsNumber(r[dividend].u.jsValue->toNumber(exec) / r[divisor].u.jsValue->toNumber(exec));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mod) {
        /* mod dst(r) dividend(r) divisor(r)

           Divides register dividend (converted to number) by 
           register divisor (converted to number), and puts the
           remainder in register dst.
        */
        int dst = (++vPC)->u.operand;
        int dividend = (++vPC)->u.operand;
        int divisor = (++vPC)->u.operand;
        Instruction* firstTarget;
        Instruction* secondTarget;
        double left = r[dividend].u.jsValue->toNumber(exec, vPC, builtinThrowBuffer, firstTarget);
        double right = r[divisor].u.jsValue->toNumber(exec, firstTarget, builtinThrowBuffer, secondTarget);
        r[dst].u.jsValue = jsNumber(fmod(left, right));
        
        vPC = secondTarget + 1;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_sub) {
        /* sub dst(r) src1(r) src2(r)

           Subtracts register src2 (converted to number) from register
           src1 (converted to number), and puts the difference in
           register dst.
        */
        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        Instruction* firstTarget;
        Instruction* secondTarget;
        double left = r[src1].u.jsValue->toNumber(exec, vPC, builtinThrowBuffer, firstTarget);
        double right = r[src2].u.jsValue->toNumber(exec, firstTarget, builtinThrowBuffer, secondTarget);
        r[dst].u.jsValue = jsNumber(left - right);
        
        vPC = secondTarget + 1;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_lshift) {
        /* lshift dst(r) val(r) shift(r)

           Performs left shift of register val (converted to int32) by
           register shift (converted to uint32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int val = (++vPC)->u.operand;
        int shift = (++vPC)->u.operand;
        r[dst].u.jsValue = jsNumber((r[val].u.jsValue->toInt32(exec)) << (r[shift].u.jsValue->toUInt32(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_rshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs arithmetic right shift of register val (converted
           to int32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int val = (++vPC)->u.operand;
        int shift = (++vPC)->u.operand;
        r[dst].u.jsValue = jsNumber((r[val].u.jsValue->toInt32(exec)) >> (r[shift].u.jsValue->toUInt32(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_urshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs logical right shift of register val (converted
           to uint32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int val = (++vPC)->u.operand;
        int shift = (++vPC)->u.operand;
        r[dst].u.jsValue = jsNumber((r[val].u.jsValue->toUInt32(exec)) >> (r[shift].u.jsValue->toUInt32(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitand) {
        /* bitand dst(r) src1(r) src2(r)

           Computes bitwise AND of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        r[dst].u.jsValue = jsNumber((r[src1].u.jsValue->toInt32(exec)) & (r[src2].u.jsValue->toInt32(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitxor) {
        /* bitxor dst(r) src1(r) src2(r)

           Computes bitwise XOR of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        r[dst].u.jsValue = jsNumber((r[src1].u.jsValue->toInt32(exec)) ^ (r[src2].u.jsValue->toInt32(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitor) {
        /* bitor dst(r) src1(r) src2(r)

           Computes bitwise OR of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src1 = (++vPC)->u.operand;
        int src2 = (++vPC)->u.operand;
        r[dst].u.jsValue = jsNumber((r[src1].u.jsValue->toInt32(exec)) | (r[src2].u.jsValue->toInt32(exec)));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitnot) {
        /* bitnot dst(r) src(r)

           Computes bitwise NOT of register src1 (converted to int32),
           and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst].u.jsValue = jsNumber(~r[src].u.jsValue->toInt32(exec));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_not) {
        /* not dst(r) src1(r) src2(r)

           Computes logical NOT of register src1 (converted to
           boolean), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst].u.jsValue = jsBoolean(!r[src].u.jsValue->toBoolean(exec));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_instanceof) {
        /* instanceof dst(r) value(r) constructor(r)

           Tests whether register value is an instance of register
           constructor, and puts the boolean result in register dst.
          
           Raises an exception if register constructor is not an
           object.
        */
        int dst = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;

        JSValue* baseVal = r[base].u.jsValue;

        if (isNotObject(exec, vPC, codeBlock, baseVal, exceptionValue))
            goto vm_throw;

        JSObject* baseObj = static_cast<JSObject*>(baseVal);
        r[dst].u.jsValue = jsBoolean(baseObj->implementsHasInstance() ? baseObj->hasInstance(exec, r[value].u.jsValue) : false);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_typeof) {
        /* typeof dst(r) src(r)

           Determines the type string for src according to ECMAScript
           rules, and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst].u.jsValue = jsTypeStringForValue(r[src].u.jsValue);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_in) {
        /* in dst(r) property(r) base(r)

           Tests whether register base has a property named register
           property, and puts the boolean result in register dst.
          
           Raises an exception if register constructor is not an
           object.
        */
        int dst = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;

        JSValue* baseVal = r[base].u.jsValue;
        if (isNotObject(exec, vPC, codeBlock, baseVal, exceptionValue))
            goto vm_throw;

        JSObject* baseObj = static_cast<JSObject*>(baseVal);

        JSValue* propName = r[property].u.jsValue;

        uint32_t i;
        if (propName->getUInt32(i))
            r[dst].u.jsValue = jsBoolean(baseObj->hasProperty(exec, i));
        else
            r[dst].u.jsValue = jsBoolean(baseObj->hasProperty(exec, Identifier(propName->toString(exec))));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve) {
        /* resolve dst(r) property(id)

           Looks up the property named by identifier property in the
           scope chain, and writes the resulting value to register
           dst. If the property is not found, raises an exception.
        */
        if (UNLIKELY(!resolve(exec, vPC, r, scopeChain, codeBlock, exceptionValue)))
            goto vm_throw;

        vPC += 3;
        
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_base) {
        /* resolve_base dst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes it to
           register dst. If none is found, the outermost scope (which
           will be the global object) is stored in register dst.
        */
        resolveBase(exec, vPC, r, scopeChain, codeBlock);
        vPC += 3;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_with_base) {
        /* resolve_with_base baseDst(r) propDst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes it to
           register srcDst, and the retrieved property value to register
           propDst. If the property is not found, raises an exception.

           This is more efficient than doing resolve_base followed by
           resolve, or resolve_base followed by get_by_id, as it
           avoids duplicate hash lookups.
        */
        if (UNLIKELY(!resolveBaseAndProperty(exec, vPC, r, scopeChain, codeBlock, exceptionValue)))
            goto vm_throw;

        vPC += 4;
        
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_func) {
        /* resolve_func baseDst(r) funcDst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes the
           appropriate object to use as "this" when calling its
           properties to register baseDst; and the retrieved property
           value to register propDst. If the property is not found,
           raises an exception.

           This differs from resolve_with_base, because the
           global this value will be substituted for activations or
           the global object, which is the right behavior for function
           calls but not for other property lookup.
        */
        if (UNLIKELY(!resolveBaseAndFunc(exec, vPC, r, scopeChain, codeBlock, exceptionValue)))
            goto vm_throw;
        vPC += 4;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_id) {
        /* get_by_id dst(r) base(r) property(id)

           Converts register base to Object, gets the property
           named by identifier property from the object, and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        JSObject* baseObj = r[base].u.jsValue->toObject(exec);

        Identifier& ident = codeBlock->identifiers[property];
        JSValue *result = baseObj->get(exec, ident);
        VM_CHECK_EXCEPTION();
        r[dst].u.jsValue = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_id) {
        /* put_by_id base(r) property(id) value(r)

           Sets register value on register base as the property named
           by identifier property. Base is converted to object first.
 
           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;

        JSObject* baseObj = r[base].u.jsValue->toObject(exec);
        
        Identifier& ident = codeBlock->identifiers[property];
        baseObj->put(exec, ident, r[value].u.jsValue);
        
        VM_CHECK_EXCEPTION();
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_del_by_id) {
        /* del_by_id dst(r) base(r) property(id)

           Converts register base to Object, deletes the property
           named by identifier property from the object, and writes a
           boolean indicating success (if true) or failure (if false)
           to register dst.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        JSObject* baseObj = r[base].u.jsValue->toObject(exec);
        
        Identifier& ident = codeBlock->identifiers[property];
        JSValue* result = jsBoolean(baseObj->deleteProperty(exec, ident));
        VM_CHECK_EXCEPTION();
        r[dst].u.jsValue = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_val) {
        /* get_by_val dst(r) base(r) property(r)

           Converts register base to Object, gets the property named
           by register property from the object, and puts the result
           in register dst. property is nominally converted to string
           but numbers are treated more efficiently.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        JSObject* baseObj = r[base].u.jsValue->toObject(exec); // may throw
        
        JSValue* subscript = r[property].u.jsValue;
        JSValue* result;
        uint32_t i;
        if (subscript->getUInt32(i))
            result = baseObj->get(exec, i);
        else {
            VM_CHECK_EXCEPTION(); // If toObject threw, we must not call toString, which may execute arbitrary code
            result = baseObj->get(exec, Identifier(subscript->toString(exec)));
        }
        
        VM_CHECK_EXCEPTION();
        r[dst].u.jsValue = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_val) {
        /* put_by_val base(r) property(r) value(r)

           Sets register value on register base as the property named
           by register property. Base is converted to object
           first. register property is nominally converted to string
           but numbers are treated more efficiently.
 
           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;

        JSObject* baseObj = r[base].u.jsValue->toObject(exec);
        
        JSValue* subscript = r[property].u.jsValue;

        uint32_t i;
        if (subscript->getUInt32(i))
            baseObj->put(exec, i, r[value].u.jsValue);
        else {
            VM_CHECK_EXCEPTION(); // If toObject threw, we must not call toString, which may execute arbitrary code
            baseObj->put(exec, Identifier(subscript->toString(exec)), r[value].u.jsValue);
        }
        
        VM_CHECK_EXCEPTION();
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_del_by_val) {
        /* del_by_val dst(r) base(r) property(r)

           Converts register base to Object, deletes the property
           named by register property from the object, and writes a
           boolean indicating success (if true) or failure (if false)
           to register dst.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        JSObject* baseObj = r[base].u.jsValue->toObject(exec); // may throw

        JSValue* subscript = r[property].u.jsValue;
        JSValue* result;
        uint32_t i;
        if (subscript->getUInt32(i))
            result = jsBoolean(baseObj->deleteProperty(exec, i));
        else {
            VM_CHECK_EXCEPTION(); // If toObject threw, we must not call toString, which may execute arbitrary code
            result = jsBoolean(baseObj->deleteProperty(exec, Identifier(subscript->toString(exec))));
        }
        
        VM_CHECK_EXCEPTION();
        r[dst].u.jsValue = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_index) {
        /* put_by_index base(r) property(n) value(r)

           Sets register value on register base as the property named
           by the immediate number property. Base is converted to
           object first. register property is nominally converted to
           string but numbers are treated more efficiently.
 
           Unlike many opcodes, this one does not write any output to
           the register file.

           This opcode is mainly used to initialize array literals.
        */
        int base = (++vPC)->u.operand;
        unsigned property = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;

        r[base].u.jsObject->put(exec, property, r[value].u.jsValue);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jmp) {
        /* jmp target(offset)
         
           Jumps unconditionally to offset target from the current
           instruction.
        */
        int target = (++vPC)->u.operand;
        vPC += target;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jtrue) {
        /* jtrue cond(r) target(offset)
         
           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as true.
        */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (r[cond].u.jsValue->toBoolean(exec)) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jfalse) {
        /* jfalse cond(r) target(offset)
         
           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as false.
        */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (!r[cond].u.jsValue->toBoolean(exec)) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_func) {
        /* new_func dst(r) func(f)

           Constructs a new Function instance from function func and
           the current scope chain using the original Function
           constructor, using the rules for function declarations, and
           puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int func = (++vPC)->u.operand;

        r[dst].u.jsValue = codeBlock->functions[func]->makeFunction(exec, scopeChain);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_func_exp) {
        /* new_func_exp dst(r) func(f)

           Constructs a new Function instance from function func and
           the current scope chain using the original Function
           constructor, using the rules for function expressions, and
           puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int func = (++vPC)->u.operand;

        r[dst].u.jsValue = codeBlock->functionExpressions[func]->makeFunction(exec, scopeChain);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_call_eval) {
        int r0 = (++vPC)->u.operand;
        int r1 = (++vPC)->u.operand;
        int r2 = (++vPC)->u.operand;
        int argv = (++vPC)->u.operand;
        int argc = (++vPC)->u.operand;

        JSValue* v = r[r1].u.jsValue;
        JSValue* base = r[r2].u.jsValue;
        
        if (base == scopeChain->globalObject() && v == scopeChain->globalObject()->evalFunction()) {
            int registerOffset = r - (*registerBase);

            int thisRegister = (codeBlock->codeType == FunctionCode) ? -codeBlock->numLocals : ProgramCodeThisRegister;
            JSObject* thisObject = r[thisRegister].u.jsObject;

            JSValue* result = eval(exec, thisObject, scopeChain, registerFile, r, argv, argc, exceptionValue);
            r = (*registerBase) + registerOffset;

            if (exceptionValue)
                goto vm_throw;

            r[r0].u.jsValue = result;
            
            ++vPC;
            NEXT_OPCODE;
        }
        
        // We didn't find the blessed version of eval, so reset vPC and process
        // this instruction as a normal function call, supplying the proper 'this'
        // value.
        vPC -= 5;
        r[r2].u.jsValue = base->toObject(exec)->toThisObject(exec);

#if HAVE(COMPUTED_GOTO)
        // Hack around gcc performance quirk by performing an indirect goto
        // in order to set the vPC -- attempting to do so directly results in a
        // significant regression.
        goto *op_call_indirect; // indirect goto -> op_call
#endif
        // fall through to op_call
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
            int callFrameOffset = registerOffset + argv - CallFrameHeaderSize;

            r[argv].u.jsValue = r2 == missingSymbolMarker() ? exec->globalThisValue() : r[r2].u.jsValue; // "this" value
            initializeCallFrame(callFrame, codeBlock, vPC, scopeChain, registerOffset, r0, argv, argc, 0, v);

            ScopeChainNode* callDataScopeChain = callData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = callData.js.functionBody;

            CodeBlock* newCodeBlock = &functionBodyNode->code(callDataScopeChain);
            r = slideRegisterWindowForCall(exec, newCodeBlock, registerFile, registerBase, registerOffset, argv, argc, exceptionValue);
            if (UNLIKELY(exceptionValue != 0))
                goto vm_throw;

            codeBlock = newCodeBlock;
            callFrame = (*registerBase) + callFrameOffset; // registerBase may have moved, recompute callFrame
            exec->m_callFrameOffset = callFrameOffset;
            setScopeChain(exec, scopeChain, scopeChainForCall(functionBodyNode, codeBlock, callDataScopeChain, callFrame, registerBase, r));
            k = codeBlock->jsValues.data();
            vPC = codeBlock->instructions.begin();

            NEXT_OPCODE;
        }

        if (callType == CallTypeNative) {
            int registerOffset = r - (*registerBase);
            
            r[argv].u.jsValue = r2 == missingSymbolMarker() ? exec->globalThisValue() : (r[r2].u.jsValue)->toObject(exec); // "this" value
            JSObject* thisObj = static_cast<JSObject*>(r[argv].u.jsValue);

            List args(&r[argv + 1].u.jsValue, argc - 1);

            JSValue* returnValue = static_cast<JSObject*>(v)->callAsFunction(exec, thisObj, args);

            r = (*registerBase) + registerOffset;
            r[r0].u.jsValue = returnValue;

            VM_CHECK_EXCEPTION();

            ++vPC;
            NEXT_OPCODE;
        }

        ASSERT(callType == CallTypeNone);

        exceptionValue = createNotAFunctionError(exec, v, 0);
        goto vm_throw;
    }
    BEGIN_OPCODE(op_ret) {
        int r1 = (++vPC)->u.operand;

        CodeBlock* oldCodeBlock = codeBlock;

        Register* callFrame = r - oldCodeBlock->numLocals - CallFrameHeaderSize;
        JSValue* returnValue = r[r1].u.jsValue;

        if (JSActivation* activation = static_cast<JSActivation*>(callFrame[OptionalCalleeActivation].u.jsValue)) {
            ASSERT(!codeBlock->needsFullScopeChain || scopeChain->object == activation);
            ASSERT(activation->isActivationObject());
            activation->copyRegisters();
        }

        if (codeBlock->needsFullScopeChain)
            scopeChain->deref();

        if (callFrame[CalledAsConstructor].u.i && !returnValue->isObject()) {
            JSValue* thisObject = callFrame[CallFrameHeaderSize].u.jsValue;
            returnValue = thisObject;
        }
         
        codeBlock = callFrame[CallerCodeBlock].u.codeBlock;
        if (!codeBlock)
            return returnValue;
        
        k = codeBlock->jsValues.data();
        vPC = callFrame[ReturnVPC].u.vPC;
        setScopeChain(exec, scopeChain, callFrame[CallerScopeChain].u.scopeChain);
        int callerRegisterOffset = callFrame[CallerRegisterOffset].u.i;
        r = (*registerBase) + callerRegisterOffset;
        exec->m_callFrameOffset = callerRegisterOffset - codeBlock->numLocals - CallFrameHeaderSize;
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
            int callFrameOffset = registerOffset + argv - CallFrameHeaderSize;

            JSObject* prototype;
            JSValue* p = constructor->get(exec, exec->propertyNames().prototype);
            if (p->isObject())
                prototype = static_cast<JSObject*>(p);
            else
                prototype = scopeChain->globalObject()->objectPrototype();
            JSObject* newObject = new JSObject(prototype);
            r[argv].u.jsValue = newObject; // "this" value

            initializeCallFrame(callFrame, codeBlock, vPC, scopeChain, registerOffset, r0, argv, argc, 1, constructor);
            
            ScopeChainNode* callDataScopeChain = constructData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = constructData.js.functionBody;

            CodeBlock* newCodeBlock = &functionBodyNode->code(callDataScopeChain);
            r = slideRegisterWindowForCall(exec, newCodeBlock, registerFile, registerBase, registerOffset, argv, argc, exceptionValue);
            if (exceptionValue)
                goto vm_throw;

            codeBlock = newCodeBlock;
            callFrame = (*registerBase) + callFrameOffset; // registerBase may have moved, recompute callFrame
            exec->m_callFrameOffset = callFrameOffset;
            setScopeChain(exec, scopeChain, scopeChainForCall(functionBodyNode, codeBlock, callDataScopeChain, callFrame, registerBase, r));
            k = codeBlock->jsValues.data();
            vPC = codeBlock->instructions.begin();

            NEXT_OPCODE;
        }

        if (constructType == ConstructTypeNative) {
            int registerOffset = r - (*registerBase);

            List args(&r[argv + 1].u.jsValue, argc - 1);
            JSValue* returnValue = constructor->construct(exec, args);
        
            r = (*registerBase) + registerOffset;
            VM_CHECK_EXCEPTION();
            r[r0].u.jsValue = returnValue;
            
            ++vPC;
            NEXT_OPCODE;
        }

        ASSERT(constructType == ConstructTypeNone);

        exceptionValue = createNotAConstructorError(exec, v, 0);
        goto vm_throw;
    }
    BEGIN_OPCODE(op_push_scope) {
        /* push_scope scope(r)

           Converts register scope to object, and pushes it onto the top
           of the current scope chain.
        */
        int scope = (++vPC)->u.operand;
        JSValue* v = r[scope].u.jsValue;
        JSObject* o = v->toObject(exec);
        VM_CHECK_EXCEPTION();
        
        setScopeChain(exec, scopeChain, scopeChain->push(o));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pop_scope) {
        /* pop_scope

           Removes the top item from the current scope chain.
        */
        setScopeChain(exec, scopeChain, scopeChain->pop());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_pnames) {
        /* get_pnames dst(r) base(r)

           Creates a property name list for register base and puts it
           in register dst. This is not a true JavaScript value, just
           a synthetic value used to keep the iteration state in a
           register.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;

        r[dst].u.jsPropertyNameIterator = JSPropertyNameIterator::create(exec, r[base].u.jsValue);
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_next_pname) {
        /* next_pname dst(r) iter(r) target(offset)

           Tries to copies the next name from property name list in
           register iter. If there are names left, then copies one to
           register dst, and jumps to offset target. If there are none
           left, invalidates the iterator and continues to the next
           instruction.
        */
        int dst = (++vPC)->u.operand;
        int iter = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;

        JSPropertyNameIterator* it = r[iter].u.jsPropertyNameIterator;
        if (JSValue* temp = it->next(exec)) {
            r[dst].u.jsValue = temp;
            vPC += target;
            NEXT_OPCODE;
        }
        it->invalidate();

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jmp_scopes) {
        /* jmp_scopes count(n) target(offset)

           Removes the a number of items from the current scope chain
           specified by immediate number count, then jumps to offset
           target.
        */
        int count = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        
        ScopeChainNode* tmp = scopeChain;
        while (count--)
            tmp = tmp->pop();
        setScopeChain(exec, scopeChain, tmp);
            
        vPC += target;
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
        handlerVPC = throwException(exec, exceptionValue, registerBase, vPC, codeBlock, k, scopeChain, r);
        if (!handlerVPC) {
            *exception = exceptionValue;
            return jsNull();
        }

#if HAVE(COMPUTED_GOTO)
        // Hack around gcc performance quirk by performing an indirect goto
        // in order to set the vPC -- attempting to do so directly results in a
        // significant regression.
        goto *op_throw_end_indirect; // indirect goto -> op_throw_end
    }
    op_throw_end: {
#endif

        vPC = handlerVPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_error) {
        /* new_error dst(r) type(n) message(k)

           Constructs a new Error instance using the original
           constructor, using immediate number n as the type and
           constant message as the message string. The result is
           written to register dst.
        */
        int dst = (++vPC)->u.operand;
        int type = (++vPC)->u.operand;
        int message = (++vPC)->u.operand;
        
        r[dst].u.jsValue = Error::create(exec, (ErrorType)type, k[message]->toString(exec), -1, -1, 0); // lineNo(), currentSourceId(exec), currentSourceURL(exec)
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_end) {
        if (codeBlock->needsFullScopeChain) {
            ASSERT(scopeChain->refCount > 1);
            scopeChain->deref();
        }
        int r0 = (++vPC)->u.operand;
        return r[r0].u.jsValue;
    }
    BEGIN_OPCODE(op_put_getter) {
        /* put_getter base(r) property(id) function(r)
         
           Sets register function on register base as the getter named
           by identifier property. Base and function are assumed to be
           objects as this op should only be used for getters defined
           in object literal form.
         
           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int function = (++vPC)->u.operand;
        
        ASSERT(r[base].u.jsValue->isObject());
        JSObject* baseObj = static_cast<JSObject*>(r[base].u.jsValue);
        Identifier& ident = codeBlock->identifiers[property];
        ASSERT(r[function].u.jsValue->isObject());
        baseObj->defineGetter(exec, ident, static_cast<JSObject* >(r[function].u.jsValue));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_setter) {
        /* put_setter base(r) property(id) function(r)
         
           Sets register function on register base as the setter named
           by identifier property. Base and function are assumed to be
           objects as this op should only be used for setters defined
           in object literal form.
         
           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int function = (++vPC)->u.operand;
        
        ASSERT(r[base].u.jsValue->isObject());
        JSObject* baseObj = static_cast<JSObject*>(r[base].u.jsValue);
        Identifier& ident = codeBlock->identifiers[property];
        ASSERT(r[function].u.jsValue->isObject());
        baseObj->defineSetter(exec, ident, static_cast<JSObject* >(r[function].u.jsValue));
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jsr) {
        /* jsr retAddrDst(r) target(offset)
         
           Places the address of the next instruction into the retAddrDst
           register and jumps to offset target from the current instruction.
        */
        int retAddrDst = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        r[retAddrDst].u.vPC = vPC + 1;

        vPC += target;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_sret) {
        /* sret retAddrSrc(r)
         
         Jumps to the address stored in the retAddrSrc register. This
         differs from op_jmp because the target address is stored in a
         register, not as an immediate.
        */
        int retAddrSrc = (++vPC)->u.operand;
        vPC = r[retAddrSrc].u.vPC;
        NEXT_OPCODE;
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
        exec->clearException();
        handlerVPC = throwException(exec, exceptionValue, registerBase, vPC, codeBlock, k, scopeChain, r);
        if (!handlerVPC) {
            *exception = exceptionValue;
            return jsNull();
        }
        vPC = handlerVPC;
        NEXT_OPCODE;
    }          
    }
    #undef NEXT_OPCODE
    #undef BEGIN_OPCODE
    #undef VM_CHECK_EXCEPTION
}

JSValue* Machine::retrieveArguments(ExecState* exec, FunctionImp* function) const
{
    Register** registerBase;
    int callFrameOffset;

    if (!getCallFrame(exec, function, registerBase, callFrameOffset))
        return jsNull();

    Register* callFrame = (*registerBase) + callFrameOffset;
    JSActivation* activation = static_cast<JSActivation*>(callFrame[OptionalCalleeActivation].u.jsValue);
    if (!activation) {
        CodeBlock* codeBlock = &function->body->generatedCode();
        activation = new JSActivation(function->body, registerBase, callFrameOffset + CallFrameHeaderSize + codeBlock->numLocals);
        callFrame[OptionalCalleeActivation].u.jsValue = activation;
    }

    return activation->get(exec, exec->propertyNames().arguments);
}

bool Machine::getCallFrame(ExecState* exec, FunctionImp* function, Register**& registerBase, int& callFrameOffset) const
{
    callFrameOffset = exec->m_callFrameOffset;
    if (callFrameOffset < 0)
        return false;
    
    registerBase = exec->m_registerFile->basePointer();
    Register* callFrame = (*registerBase) + callFrameOffset;
    return callFrame[Callee].u.jsValue == function;
}

} // namespace KJS
