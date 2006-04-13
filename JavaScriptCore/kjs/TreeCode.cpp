/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "nodes.h"

#include <math.h>
#include <assert.h>
#ifdef KJS_DEBUG_MEM
#include <stdio.h>
#include <typeinfo>
#endif

#include "collector.h"
#include "context.h"
#include "debugger.h"
#include "function_object.h"
#include "internal.h"
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "lexer.h"
#include "operations.h"
#include "ustring.h"
#include "reference_list.h"
#include <kxmlcore/HashSet.h>
#include <kxmlcore/HashCountedSet.h>
#include "InterpreterState.h"

namespace KJS {

const char* nameForInterpreterState[LastInterpreterState+1] = {
    "InternalErrorState",
#define PRINT_AS_STRING(name) #name ,
    EVALUATE_MACRO_FOR_EACH_EVALUATE_STATE(PRINT_AS_STRING)

    "Evaluate_EvaluateList_Boundary",
    
    EVALUATE_MACRO_FOR_EACH_EVALUATE_LIST_STATE(PRINT_AS_STRING)
    
    "EvaluateList_Execute_Boundary",
    
    EVALUATE_MACRO_FOR_EACH_EXECUTE_STATE(PRINT_AS_STRING)
#undef PRINT_AS_STRING

    "JSObjectCallState",
    "DeclaredFunctionCallState",
    "DeclaredFunctionCallEndState",
    "DeclaredFunctionCallExplicitReturnState",
    "GlobalFuncCallEvalState",
    "GlobalFuncCallEvalEndState",

    "LastInterpreterState"
};

static inline int currentSourceId(ExecState* exec)
{
    return exec->context().imp()->currentBody()->sourceId();
}

static const char *dotExprNotAnObjectString()
{
    return "Value %s (result of expression %s.%s) is not object.";
}

static const char *dotExprDoesNotAllowCallsString() 
{
    return "Object %s (result of expression %s.%s) does not allow calls.";
}

JSValue *typeStringForValue(JSValue *v)
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

static ALWAYS_INLINE JSValue *valueForReadModifyAssignment(ExecState * exec, JSValue *v1, JSValue *v2, Operator oper)
{
    JSValue *v;
    int i1;
    int i2;
    unsigned int ui;
    switch (oper) {
        case OpMultEq:
            v = mult(exec, v1, v2, '*');
            break;
        case OpDivEq:
            v = mult(exec, v1, v2, '/');
            break;
        case OpPlusEq:
            v = add(exec, v1, v2, '+');
            break;
        case OpMinusEq:
            v = add(exec, v1, v2, '-');
            break;
        case OpLShift:
            i1 = v1->toInt32(exec);
            i2 = v2->toInt32(exec);
            v = jsNumber(i1 << i2);
            break;
        case OpRShift:
            i1 = v1->toInt32(exec);
            i2 = v2->toInt32(exec);
            v = jsNumber(i1 >> i2);
            break;
        case OpURShift:
            ui = v1->toUInt32(exec);
            i2 = v2->toInt32(exec);
            v = jsNumber(ui >> i2);
            break;
        case OpAndEq:
            i1 = v1->toInt32(exec);
            i2 = v2->toInt32(exec);
            v = jsNumber(i1 & i2);
            break;
        case OpXOrEq:
            i1 = v1->toInt32(exec);
            i2 = v2->toInt32(exec);
            v = jsNumber(i1 ^ i2);
            break;
        case OpOrEq:
            i1 = v1->toInt32(exec);
            i2 = v2->toInt32(exec);
            v = jsNumber(i1 | i2);
            break;
        case OpModEq: {
            double d1 = v1->toNumber(exec);
            double d2 = v2->toNumber(exec);
            v = jsNumber(fmod(d1, d2));
        }
            break;
        default:
            assert(false);
            v = jsUndefined();
    }
    
    return v;
}

#define RETURN_VALUE(v) \
do { \
    setValueReturn(v); \
    goto interpreter_state_switch_end; \
} while (0)

#define RETURN_COMPLETION(c) \
do { \
    setCompletionReturn(c); \
    goto interpreter_state_switch_end; \
} while (0)

#define RETURN_NORMAL_COMPLETION() do { \
    ASSERT(completionReturn().complType() == Normal); \
    goto interpreter_state_switch_end; \
} while (0)

#define KJS_CHECKEXCEPTION() \
if (exec->hadException()) { \
    currentNode->setExceptionDetailsIfNeeded(exec); \
    RETURN_COMPLETION(Completion(Throw, exec->exception())); \
}

// Use SET_CONTINUE_STATE instead whenever possible
#define SET_JUMP_STATE(nextState, node) \
do { \
    ASSERT(nextState != statePair.state); \
    m_stateStack.push(InterpreterImp::State(nextState, node)); \
} while (0)

// This can only be used for calling yourself.
#define SET_LOOP_STATE(nextState) \
do { \
    ASSERT(nextState == statePair.state); \
    m_stateStack.push(InterpreterImp::State(nextState, currentNode)); \
} while (0)

// FALL_THROUGH is used in debug builds to allow proper assert checking even
// when some states fall through to another (thus not changing the current state)
#ifndef NDEBUG
#define FALL_THROUGH() \
    statePair.state = (InterpreterState)(statePair.state + 1)
#else
#define FALL_THROUGH()
#endif

#define RETURN_LIST(list) \
do {\
    m_listStack.push(list); \
    goto interpreter_state_switch_end; \
} while (0)

#ifndef NDEBUG
static inline bool isEvaluateState(InterpreterState state)
{
    return (state > InternalErrorState && state < Evaluate_EvaluteList_Boundary);
}

static inline bool isEvaluateListState(InterpreterState state)
{
    return (state > Evaluate_EvaluteList_Boundary && state < EvaluateList_Execute_Boundary);
}
#endif

static inline bool isExecuteState(InterpreterState state)
{
    return (state > EvaluateList_Execute_Boundary && state < LastInterpreterState);
}

static inline void pushEvaluate(InterpreterImp* interpreter, Node* node)
{
    InterpreterState nextState = node->interpreterState();
    ASSERT(isEvaluateState(nextState));
    interpreter->m_stateStack.push(InterpreterImp::State(nextState, node));
}
    
static inline void pushEvaluateList(InterpreterImp* interpreter, Node* node)
{
    InterpreterState nextState = node->interpreterState();
    ASSERT(isEvaluateListState(nextState));
    interpreter->m_stateStack.push(InterpreterImp::State(nextState, node));
}

static inline void pushExecute(InterpreterImp* interpreter, Node* node)
{
    InterpreterState nextState = node->interpreterState();
    ASSERT(isExecuteState(nextState));
    interpreter->m_stateStack.push(InterpreterImp::State(nextState, node));
}

// This explicitly checks for continue state bugs
// FIXME: This should not take an argument once we have a way to check if currentState+1 == a substate
#define SET_CONTINUE_STATE(nextState) \
do { \
    ASSERT(nextState == statePair.state + 1); \
    SET_JUMP_STATE(nextState, currentNode); \
} while (0)

// FIXME: RETURN_ERROR is a stop-gap.  Eventually throwError* should be free functions which set the interpreter completion themselves
#define RETURN_ERROR(value)   RETURN_COMPLETION(Completion(Throw, value))

#define PUSH_UNWIND_BARRIER(barrierType, state) \
    pushUnwindBarrier(barrierType, InterpreterImp::State(state, currentNode));

#define POP_UNWIND_BARRIER(type) \
do { \
    ASSERT(interpreter->m_unwindBarrierStack.peek().barrierType == type); \
    interpreter->popUnwindBarrier(); \
} while (0)

#define EXECUTE_AND_CONTINUE(node, nextState) \
do { \
    SET_CONTINUE_STATE(nextState); \
    pushExecute(interpreter, node); \
    goto interpreter_state_switch_end; \
} while (0)

#define EXECUTE_AND_JUMP(node, nextState) \
do { \
    SET_JUMP_STATE(nextState, currentNode); \
    pushExecute(interpreter, node); \
    goto interpreter_state_switch_end; \
} while (0)

// This call can only be used for continuing with the next state
// Use other calls to jump or loop.
#define EVALUATE_AND_CONTINUE(node, nextState) \
do { \
    SET_CONTINUE_STATE(nextState); \
    pushEvaluate(interpreter, node); \
    goto interpreter_state_switch_end; \
} while (0)

// All code should use EVALUATE_AND_CONTINUE when possible, as it performs additional sanity checks.
#define EVALUATE_AND_JUMP(node, nextState) \
do { \
    SET_JUMP_STATE(nextState, currentNode); \
    pushEvaluate(interpreter, node); \
    goto interpreter_state_switch_end; \
} while (0)

// This call can only be used for continuing with the next state
// Use other calls to jump or loop.
#define EVALUATE_LIST_AND_CONTINUE(node, nextState) \
do { \
    SET_CONTINUE_STATE(nextState); \
    pushEvaluateList(interpreter, node); \
    goto interpreter_state_switch_end; \
} while (0)

// This call can only be used for jumping to the special JSObjectCallState
#define EVALUATE_ARGUMENT_LIST_AND_JUMP_TO_CALL(argsNode) \
{ \
    SET_JUMP_STATE(JSObjectCallState, argsNode); \
    pushEvaluateList(interpreter, argsNode); \
    goto interpreter_state_switch_end; \
}

void InterpreterImp::runInterpreterLoop(ExecState* exec)
{
    ASSERT(!exec->hadException());
    ASSERT(exec->dynamicInterpreter()->imp() == this);
    
    InterpreterImp* interpreter = this;
    InterpreterImp::UnwindBarrier stackBase = m_unwindBarrierStack.peek();
        
    while (m_stateStack.size() > stackBase.stateStackSize) {
    
        // FIXME: As a loop optimization, we should just peek, instead of pop here
        // Then every non-loop function would pop itself.
        InterpreterImp::State statePair = m_stateStack.pop();
        Node* currentNode = statePair.node;

        // FIXME: This is wrong, this won't actually return.
        if (m_debugger) {
            if (isExecuteState(statePair.state)) {
                StatementNode* statementNode = static_cast<StatementNode*>(currentNode);
                bool shouldContinue = m_debugger->atStatement(exec, currentSourceId(exec), statementNode->firstLine(), statementNode->lastLine());
                if (!shouldContinue)
                    RETURN_NORMAL_COMPLETION(); // FIXME: Do something the m_debugger can use
            }
            if (m_debugger->imp()->aborted())
                RETURN_NORMAL_COMPLETION(); // FIXME: Do something the m_debugger can use
        }

        // FIXME: ONLY check when checking if we've run too long
        if (Collector::isOutOfMemory())
            RETURN_COMPLETION(Completion(Throw, Error::create(exec, GeneralError, "Out of memory")));

        switch (statePair.state) {
            case InternalErrorState:
            default:
            {
                // should never be called
                ASSERT_NOT_REACHED();
                RETURN_VALUE((JSValue*)0);
            }
            
            case NullNodeEvaluateState:
            {
                RETURN_VALUE(jsNull());
            }
            
            case BooleanNodeEvaluateState:
            {
                RETURN_VALUE(jsBoolean(static_cast<BooleanNode*>(currentNode)->value));
            }
            
            case NumberNodeEvaluateState:
            {
                RETURN_VALUE(jsNumber(static_cast<NumberNode*>(currentNode)->value));
            }
            
            case StringNodeEvaluateState:
            {
                RETURN_VALUE(jsString(static_cast<StringNode*>(currentNode)->value));
            }
            
            case RegExpNodeEvaluateState:
            {
                RegExpNode* regexpNode = static_cast<RegExpNode*>(currentNode);
                List list;
                list.append(jsString(regexpNode->pattern));
                list.append(jsString(regexpNode->flags));
                
                JSObject *reg = exec->lexicalInterpreter()->imp()->builtinRegExp();
                RETURN_VALUE(reg->construct(exec, list));
            }
            
            case ThisNodeEvaluateState:
            {
                RETURN_VALUE(exec->context().imp()->thisValue());
            }
                
            case ResolveNodeEvaluateState:
            {
                ResolveNode* resolveNode = static_cast<ResolveNode*>(currentNode);
                const ScopeChain& chain = exec->context().imp()->scopeChain();
                ScopeChainIterator iter = chain.begin();
                ScopeChainIterator end = chain.end();
                
                // we must always have something in the scope chain
                assert(iter != end);
                
                PropertySlot slot;
                do { 
                    JSObject *o = *iter;
                    
                    if (o->getPropertySlot(exec, resolveNode->ident, slot))
                        RETURN_VALUE(slot.getValue(exec, o, resolveNode->ident));
                    
                    ++iter;
                } while (iter != end);
                 
                RETURN_ERROR(resolveNode->throwUndefinedVariableError(exec, resolveNode->ident));
            }
                
            case GroupNodeEvaluateState:
            {
                pushEvaluate(interpreter, static_cast<GroupNode*>(currentNode)->group.get());
                break;
            }
                
            case ElementNodeEvaluateState:
            {
                m_valueStack.push(exec->lexicalInterpreter()->builtinArray()->construct(exec, List::empty()));
                m_valueStack.push(jsNumber(0));
                FALL_THROUGH();
            }
            case ElementNodeEvaluateState1:
            {
                EVALUATE_AND_CONTINUE(static_cast<ElementNode*>(currentNode)->node.get(), ElementNodeEvaluateState2);
            }
            case ElementNodeEvaluateState2:
            {
                ElementNode* n = static_cast<ElementNode*>(currentNode);
                JSValue* val = valueReturn();
                int l = m_valueStack.pop()->toInt32(exec);
                JSObject* array = static_cast<JSObject*>(m_valueStack.peek());
                
                l += n->elision;
                array->put(exec, l++, val);
                
                n = n->next.get();
                if (n) {
                    m_valueStack.push(jsNumber(l));
                    // FIXME: pushing "p" as the current node is dangerous
                    // If this function used current node after this call, currentNode would be wrong.
                    SET_JUMP_STATE(ElementNodeEvaluateState1, n);
                    break;
                }
                RETURN_VALUE(m_valueStack.pop());
            }
                
            case ArrayNodeEvaluateState:
            {
                ArrayNode* arrayNode = static_cast<ArrayNode*>(currentNode);
                if (arrayNode->element)
                    EVALUATE_AND_CONTINUE(arrayNode->element.get(), ArrayNodeEvaluateState1);
                FALL_THROUGH();
            }
            case ArrayNodeEvaluateState1:
            {
                ArrayNode* arrayNode = static_cast<ArrayNode*>(currentNode);
                JSObject *array;
                int length;
                
                if (arrayNode->element) {
                    array = static_cast<JSObject*>(valueReturn());
                    length = arrayNode->opt ? array->get(exec, lengthPropertyName)->toInt32(exec) : 0;
                } else {
                    array = exec->lexicalInterpreter()->builtinArray()->construct(exec, List::empty());
                    length = 0;
                }
                
                if (arrayNode->opt)
                    array->put(exec, lengthPropertyName, jsNumber(arrayNode->elision + length), DontEnum | DontDelete);
                
                RETURN_VALUE(array);
            }
                
            case ObjectLiteralNodeEvaluateState:
            {
                ObjectLiteralNode* objectLiteralNode = static_cast<ObjectLiteralNode*>(currentNode);
                if (objectLiteralNode->list) {
                    pushEvaluate(interpreter, objectLiteralNode->list.get());
                    break;
                }
                
                RETURN_VALUE(exec->lexicalInterpreter()->builtinObject()->construct(exec, List::empty()));
            }
                
            case PropertyListNodeEvaluateState:
            {
                JSObject* obj = exec->lexicalInterpreter()->builtinObject()->construct(exec, List::empty());
                m_valueStack.push(obj);
                FALL_THROUGH();
            }
            case PropertyListNodeEvaluateState1:
            {
                EVALUATE_AND_CONTINUE(static_cast<PropertyListNode*>(currentNode)->node->name.get(), PropertyListNodeEvaluateState2);
            }
            case PropertyListNodeEvaluateState2:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<PropertyListNode*>(currentNode)->node->assign.get(), PropertyListNodeEvaluateState3);
            }
            case PropertyListNodeEvaluateState3:
            {
                PropertyListNode* p = static_cast<PropertyListNode*>(currentNode);
                JSValue* v = valueReturn();
                JSValue* name = m_valueStack.pop();
                JSObject* obj = static_cast<JSObject*>(m_valueStack.peek());
                
                Identifier propertyName = Identifier(name->toString(exec));
                switch (p->node->type) {
                case PropertyNode::Getter:
                    assert(v->isObject());
                    obj->defineGetter(exec, propertyName, static_cast<JSObject *>(v));
                    break;
                case PropertyNode::Setter:
                    assert(v->isObject());
                    obj->defineSetter(exec, propertyName, static_cast<JSObject *>(v));
                    break;
                case PropertyNode::Constant:
                    obj->put(exec, propertyName, v);
                    break;
                }
                
                p = p->next.get();
                if (p) {
                    // FIXME: pushing "p" as the current node is dangerous
                    // If this function had to continue after this loop, currentNode would be the wrong node.
                    SET_JUMP_STATE(PropertyListNodeEvaluateState1, p);
                    break;
                }
                
                RETURN_VALUE(m_valueStack.pop());
            }
                                
            case PropertyNameNodeEvaluateState:
            {
                JSValue *s;
                PropertyNameNode* propertyNameNode = static_cast<PropertyNameNode*>(currentNode);
                if (propertyNameNode->str.isNull())
                    s = jsString(UString::from(propertyNameNode->numeric));
                else
                    s = jsString(propertyNameNode->str.ustring());
                
                RETURN_VALUE(s);
            }
            
            case BracketAccessorNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<BracketAccessorNode*>(currentNode)->expr1.get(), BracketAccessorNodeEvaluateState1);
            }
            case BracketAccessorNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<BracketAccessorNode*>(currentNode)->expr2.get(), BracketAccessorNodeEvaluateState2);
            }
            case BracketAccessorNodeEvaluateState2:
            {
                JSValue *v1 = m_valueStack.pop();
                JSValue *v2 = valueReturn();
                JSObject *o = v1->toObject(exec);
                KJS_CHECKEXCEPTION();
                uint32_t i;
                if (v2->getUInt32(i))
                    RETURN_VALUE(o->get(exec, i));
                RETURN_VALUE(o->get(exec, Identifier(v2->toString(exec))));
            }
            
            case DotAccessorNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<DotAccessorNode*>(currentNode)->expr.get(), DotAccessorNodeEvaluateState1);
            }
            case DotAccessorNodeEvaluateState1:
            {
                RETURN_VALUE(valueReturn()->toObject(exec)->get(exec, static_cast<DotAccessorNode*>(currentNode)->ident));
            }
                
            case NewExprNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<NewExprNode*>(currentNode)->expr.get(), NewExprNodeEvaluateState1);
            }
            case NewExprNodeEvaluateState1:
            {
                NewExprNode* newExprNode = static_cast<NewExprNode*>(currentNode);
                m_valueStack.push(valueReturn());
                if (newExprNode->args)
                    EVALUATE_LIST_AND_CONTINUE(newExprNode->args.get(), NewExprNodeEvaluateState2);
                FALL_THROUGH();
            }
            case NewExprNodeEvaluateState2:
            {
                NewExprNode* newExprNode = static_cast<NewExprNode*>(currentNode);
                JSValue* v = m_valueStack.pop();
                List argList;
                if (newExprNode->args)
                    argList = m_listStack.pop();
                
                if (!v->isObject())
                    RETURN_ERROR(newExprNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with new.", v, newExprNode->expr.get()));
                JSObject *constr = static_cast<JSObject*>(v);
                if (!constr->implementsConstruct())
                    RETURN_ERROR(newExprNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not a constructor. Cannot be used with new.", v, newExprNode->expr.get()));

                RETURN_VALUE(constr->construct(exec, argList));
            }
                
            case FunctionCallValueNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<FunctionCallValueNode*>(currentNode)->expr.get(), FunctionCallValueNodeEvaluateState1);
            }
            case FunctionCallValueNodeEvaluateState1:
            {
                FunctionCallValueNode* functionCallValueNode = static_cast<FunctionCallValueNode*>(currentNode);
                JSValue* v = valueReturn();
                if (!v->isObject())
                    RETURN_ERROR(functionCallValueNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, functionCallValueNode->expr.get()));
                
                JSObject *func = static_cast<JSObject*>(v);
                if (!func->implementsCall())
                    RETURN_ERROR(functionCallValueNode->throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, functionCallValueNode->expr.get()));
                
                m_valueStack.push(func); // push "FunctionImp"
                m_valueStack.push(globalObject()); // push "thisObj"
                EVALUATE_ARGUMENT_LIST_AND_JUMP_TO_CALL(functionCallValueNode->args.get()); // argsList is pushed as part of call
            }
                
            case FunctionCallResolveNodeEvaluateState:
            {
                FunctionCallResolveNode* functionCallResolveNode = static_cast<FunctionCallResolveNode*>(currentNode);
                const ScopeChain& chain = exec->context().imp()->scopeChain();
                ScopeChainIterator iter = chain.begin();
                ScopeChainIterator end = chain.end();
                const Identifier& ident = functionCallResolveNode->ident;
                
                // we must always have something in the scope chain
                assert(iter != end);
                
                JSObject* resolvedFunction = 0;
                PropertySlot slot;
                JSObject* base;
                do {
                    base = *iter;
                    if (base->getPropertySlot(exec, ident, slot)) {
                        JSValue *v = slot.getValue(exec, base, ident);
                        KJS_CHECKEXCEPTION();
                        
                        if (!v->isObject())
                            RETURN_ERROR(functionCallResolveNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, ident));
                        
                        resolvedFunction = static_cast<JSObject*>(v);
                        
                        if (!resolvedFunction->implementsCall())
                            RETURN_ERROR(functionCallResolveNode->throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, ident));
                        break;
                    }
                    ++iter;
                } while (iter != end);
                
                if (!resolvedFunction)
                    RETURN_ERROR(functionCallResolveNode->throwUndefinedVariableError(exec, ident));
                
                // ECMA 11.2.3 says that in this situation the this value should be null.
                // However, section 10.2.3 says that in the case where the value provided
                // by the caller is null, the global object should be used. It also says
                // that the section does not apply to interal functions, but for simplicity
                // of implementation we use the global object anyway here. This guarantees
                // that in host objects you always get a valid object for this.
                if (base->isActivation())
                    base = globalObject();
                
                m_valueStack.push(resolvedFunction); // push "FunctionImp"
                m_valueStack.push(base); // push "thisObj"
                EVALUATE_ARGUMENT_LIST_AND_JUMP_TO_CALL(functionCallResolveNode->args.get()); // argsList is pushed as part of call
            }
                
            case FunctionCallBracketNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<FunctionCallBracketNode*>(currentNode)->base.get(), FunctionCallBracketNodeEvaluateState1);
            }
            case FunctionCallBracketNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<FunctionCallBracketNode*>(currentNode)->subscript.get(), FunctionCallBracketNodeEvaluateState2);
            }
            case FunctionCallBracketNodeEvaluateState2:
            {
                FunctionCallBracketNode* functionCallBracketNode = static_cast<FunctionCallBracketNode*>(currentNode);
                
                JSValue *subscriptVal = valueReturn();
                JSObject *baseObj = m_valueStack.pop()->toObject(exec);
                KJS_CHECKEXCEPTION();
                
                uint32_t i;
                PropertySlot slot;
                
                JSValue *funcVal;
                if (subscriptVal->getUInt32(i)) {
                    if (baseObj->getPropertySlot(exec, i, slot))
                        funcVal = slot.getValue(exec, baseObj, i);
                    else
                        funcVal = jsUndefined();
                } else {
                    Identifier ident(subscriptVal->toString(exec));
                    if (baseObj->getPropertySlot(exec, ident, slot))
                        funcVal = baseObj->get(exec, ident);
                    else
                        funcVal = jsUndefined();
                }
                
                KJS_CHECKEXCEPTION();
                
                if (!funcVal->isObject())
                    RETURN_ERROR(functionCallBracketNode->throwError(exec, TypeError, "Value %s (result of expression %s[%s]) is not object.", funcVal, functionCallBracketNode->base.get(), functionCallBracketNode->subscript.get()));
                
                JSObject *func = static_cast<JSObject*>(funcVal);
                
                if (!func->implementsCall())
                    RETURN_ERROR(functionCallBracketNode->throwError(exec, TypeError, "Object %s (result of expression %s[%s]) does not allow calls.", funcVal, functionCallBracketNode->base.get(), functionCallBracketNode->subscript.get()));
                                
                m_valueStack.push(func); // push "FunctionImp"
                m_valueStack.push(baseObj); // push "thisObj"
                EVALUATE_ARGUMENT_LIST_AND_JUMP_TO_CALL(functionCallBracketNode->args.get()); // argsList is pushed as part of call
            }
            
            case FunctionCallDotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<FunctionCallDotNode*>(currentNode)->base.get(), FunctionCallDotNodeEvaluateState1);
            }
            case FunctionCallDotNodeEvaluateState1:
            {
                FunctionCallDotNode* functionCallDotNode = static_cast<FunctionCallDotNode*>(currentNode);
                JSObject* baseObj = valueReturn()->toObject(exec);
                KJS_CHECKEXCEPTION();
                PropertySlot slot;
                const Identifier& ident = functionCallDotNode->ident;
                JSValue* funcVal = baseObj->getPropertySlot(exec, ident, slot) ? slot.getValue(exec, baseObj, ident) : jsUndefined();
                KJS_CHECKEXCEPTION();
                
                if (!funcVal->isObject())
                    RETURN_ERROR(functionCallDotNode->throwError(exec, TypeError, dotExprNotAnObjectString(), funcVal, functionCallDotNode->base.get(), ident));
                
                JSObject* func = static_cast<JSObject*>(funcVal);
                
                if (func->implementsCall()) {
                    m_valueStack.push(func); // push "FunctionImp"
                    m_valueStack.push(baseObj); // push "thisObj"
                    EVALUATE_ARGUMENT_LIST_AND_JUMP_TO_CALL(functionCallDotNode->args.get()); // argsList is pushed as part of call
                }
                
                RETURN_ERROR(functionCallDotNode->throwError(exec, TypeError, dotExprDoesNotAllowCallsString(), funcVal, functionCallDotNode->base.get(), ident));
            }


            case PostfixResolveNodeEvaluateState:
            {
                PostfixResolveNode* postfixResolveNode = static_cast<PostfixResolveNode*>(currentNode);
                const Identifier& ident = postfixResolveNode->m_ident;
                const ScopeChain& chain = exec->context().imp()->scopeChain();
                ScopeChainIterator iter = chain.begin();
                ScopeChainIterator end = chain.end();
                
                // we must always have something in the scope chain
                assert(iter != end);
                
                PropertySlot slot;
                JSObject *base;
                do { 
                    base = *iter;
                    if (base->getPropertySlot(exec, ident, slot)) {
                        JSValue *v = slot.getValue(exec, base, ident);
                        
                        double n = v->toNumber(exec);
                        double newValue = (postfixResolveNode->m_oper == OpPlusPlus) ? n + 1.0 : n - 1.0;
                        base->put(exec, ident, jsNumber(newValue));
                        
                        RETURN_VALUE(jsNumber(n));
                    }
                    
                    ++iter;
                } while (iter != end);
                         
                RETURN_ERROR(postfixResolveNode->throwUndefinedVariableError(exec, ident));
            }
                

            case PostfixBracketNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<PostfixBracketNode*>(currentNode)->m_base.get(), PostfixBracketNodeEvaluateState1);
            }
            case PostfixBracketNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<PostfixBracketNode*>(currentNode)->m_subscript.get(), PostfixBracketNodeEvaluateState2);
            }
            case PostfixBracketNodeEvaluateState2:
            {
                PostfixBracketNode* postfixBracketNode = static_cast<PostfixBracketNode*>(currentNode);
                
                JSValue *subscript = valueReturn();
                JSObject *base = m_valueStack.pop()->toObject(exec);
                KJS_CHECKEXCEPTION();
                
                uint32_t propertyIndex;
                if (subscript->getUInt32(propertyIndex)) {
                    PropertySlot slot;
                    JSValue *v = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
                    KJS_CHECKEXCEPTION();
                    
                    double n = v->toNumber(exec);
                    
                    double newValue = (postfixBracketNode->m_oper == OpPlusPlus) ? n + 1 : n - 1;
                    base->put(exec, propertyIndex, jsNumber(newValue));
                    
                    RETURN_VALUE(jsNumber(n));
                }
                
                Identifier propertyName(subscript->toString(exec));
                PropertySlot slot;
                JSValue *v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
                KJS_CHECKEXCEPTION();
                
                double n = v->toNumber(exec);
                
                double newValue = (postfixBracketNode->m_oper == OpPlusPlus) ? n + 1 : n - 1;
                base->put(exec, propertyName, jsNumber(newValue));
                
                RETURN_VALUE(jsNumber(n));
            }
                
                
            case PostfixDotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<PostfixDotNode*>(currentNode)->m_base.get(), PostfixDotNodeEvaluateState1);
            }
            case PostfixDotNodeEvaluateState1:
            {
                PostfixDotNode* postFixDotNode = static_cast<PostfixDotNode*>(currentNode);
                JSObject* base = valueReturn()->toObject(exec);
                KJS_CHECKEXCEPTION();
                
                PropertySlot slot;
                JSValue *v = base->getPropertySlot(exec, postFixDotNode->m_ident, slot) ? slot.getValue(exec, base, postFixDotNode->m_ident) : jsUndefined();
                KJS_CHECKEXCEPTION();
                
                double n = v->toNumber(exec);
                
                double newValue = (postFixDotNode->m_oper == OpPlusPlus) ? n + 1 : n - 1;
                base->put(exec, postFixDotNode->m_ident, jsNumber(newValue));
                
                RETURN_VALUE(jsNumber(n));
            }
                
                
            case DeleteResolveNodeEvaluateState:
            {
                DeleteResolveNode* deleteResolveNode = static_cast<DeleteResolveNode*>(currentNode);
                const ScopeChain& chain = exec->context().imp()->scopeChain();
                ScopeChainIterator iter = chain.begin();
                ScopeChainIterator end = chain.end();
                
                // we must always have something in the scope chain
                assert(iter != end);
                
                PropertySlot slot;
                JSObject *base;
                do {
                    base = *iter;
                    if (base->getPropertySlot(exec, deleteResolveNode->m_ident, slot))
                        RETURN_VALUE(jsBoolean(base->deleteProperty(exec, deleteResolveNode->m_ident)));
                    
                    ++iter;
                } while (iter != end);
                    
                RETURN_VALUE(jsBoolean(true));
            }
                
            case DeleteBracketNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<DeleteBracketNode*>(currentNode)->m_base.get(), DeleteBracketNodeEvaluateState1);
            }
            case DeleteBracketNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<DeleteBracketNode*>(currentNode)->m_subscript.get(), DeleteBracketNodeEvaluateState2);
            }
            case DeleteBracketNodeEvaluateState2:
            {
                JSValue *subscript = valueReturn();
                JSObject *base = m_valueStack.pop()->toObject(exec);
                KJS_CHECKEXCEPTION();
                
                uint32_t propertyIndex;
                if (subscript->getUInt32(propertyIndex))
                    RETURN_VALUE(jsBoolean(base->deleteProperty(exec, propertyIndex)));
                
                Identifier propertyName(subscript->toString(exec));
                RETURN_VALUE(jsBoolean(base->deleteProperty(exec, propertyName)));
            }
            
            case DeleteDotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<DeleteDotNode*>(currentNode)->m_base.get(), DeleteDotNodeEvaluateState1);
            }
            case DeleteDotNodeEvaluateState1:
            {
                JSObject *base = valueReturn()->toObject(exec);
                RETURN_VALUE(jsBoolean(base->deleteProperty(exec, static_cast<DeleteDotNode*>(currentNode)->m_ident)));
            }

            case DeleteValueNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<DeleteValueNode*>(currentNode)->m_expr.get(), DeleteValueNodeEvaluateState1);
            }
            case DeleteValueNodeEvaluateState1:
            {
                RETURN_VALUE(jsBoolean(true));
            }

            case VoidNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<VoidNode*>(currentNode)->expr.get(), VoidNodeEvaluateState1);
            }
            case VoidNodeEvaluateState1:
            {
                RETURN_VALUE(jsUndefined());
            }

            case TypeOfResolveNodeEvaluateState:
            {
                const ScopeChain& chain = exec->context().imp()->scopeChain();
                ScopeChainIterator iter = chain.begin();
                ScopeChainIterator end = chain.end();
                const Identifier& ident = static_cast<TypeOfResolveNode*>(currentNode)->m_ident;
                
                // we must always have something in the scope chain
                assert(iter != end);
                
                PropertySlot slot;
                JSObject *base;
                do { 
                    base = *iter;
                    if (base->getPropertySlot(exec, ident, slot)) {
                        JSValue *v = slot.getValue(exec, base, ident);
                        RETURN_VALUE(typeStringForValue(v));
                    }
                    
                    ++iter;
                } while (iter != end);
                   
                RETURN_VALUE(jsString("undefined"));
            }
                
            case TypeOfValueNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<TypeOfValueNode*>(currentNode)->m_expr.get(), TypeOfValueNodeEvaluateState1);
            }
            case TypeOfValueNodeEvaluateState1:
            {
                RETURN_VALUE(typeStringForValue(valueReturn()));
            }
            
            case PrefixResolveNodeEvaluateState:
            {
                PrefixResolveNode* prefixResolveNode = static_cast<PrefixResolveNode*>(currentNode);
                const ScopeChain& chain = exec->context().imp()->scopeChain();
                ScopeChainIterator iter = chain.begin();
                ScopeChainIterator end = chain.end();
                const Identifier& ident = prefixResolveNode->m_ident;
                
                // we must always have something in the scope chain
                assert(iter != end);
                
                PropertySlot slot;
                JSObject *base;
                do { 
                    base = *iter;
                    if (base->getPropertySlot(exec, ident, slot)) {
                        JSValue *v = slot.getValue(exec, base, ident);
                        
                        double n = v->toNumber(exec);
                        
                        double newValue = (prefixResolveNode->m_oper == OpPlusPlus) ? n + 1 : n - 1;
                        JSValue *n2 = jsNumber(newValue);
                        base->put(exec, ident, n2);
                        
                        RETURN_VALUE(n2);
                    }
                    
                    ++iter;
                } while (iter != end);
                
                RETURN_ERROR(prefixResolveNode->throwUndefinedVariableError(exec, ident));
            }
                
                
            case PrefixBracketNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<PrefixBracketNode*>(currentNode)->m_base.get(), PrefixBracketNodeEvaluateState1);
            }
            case PrefixBracketNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<PrefixBracketNode*>(currentNode)->m_subscript.get(), PrefixBracketNodeEvaluateState2);
            }
            case PrefixBracketNodeEvaluateState2:
            {
                PrefixBracketNode* prefixBracketNode = static_cast<PrefixBracketNode*>(currentNode);
                Operator oper = prefixBracketNode->m_oper;
                JSValue *subscript = valueReturn();
                JSObject *base = m_valueStack.pop()->toObject(exec);
                KJS_CHECKEXCEPTION();
                
                uint32_t propertyIndex;
                if (subscript->getUInt32(propertyIndex)) {
                    PropertySlot slot;
                    JSValue *v = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
                    KJS_CHECKEXCEPTION();
                    
                    double n = v->toNumber(exec);
                    
                    double newValue = (oper == OpPlusPlus) ? n + 1 : n - 1;
                    JSValue *n2 = jsNumber(newValue);
                    base->put(exec, propertyIndex, n2);
                    
                    RETURN_VALUE(n2);
                }
                
                Identifier propertyName(subscript->toString(exec));
                PropertySlot slot;
                JSValue *v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
                KJS_CHECKEXCEPTION();
                
                double n = v->toNumber(exec);
                
                double newValue = (oper == OpPlusPlus) ? n + 1 : n - 1;
                JSValue *n2 = jsNumber(newValue);
                base->put(exec, propertyName, n2);
                
                RETURN_VALUE(n2);
            }
                
                
            case PrefixDotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<PrefixDotNode*>(currentNode)->m_base.get(), PrefixDotNodeEvaluateState1);
            }
            case PrefixDotNodeEvaluateState1:
            {
                JSObject *base = valueReturn()->toObject(exec);
                KJS_CHECKEXCEPTION();
                PrefixDotNode* prefixDotNode = static_cast<PrefixDotNode*>(currentNode);
                const Identifier& ident = prefixDotNode->m_ident;
                
                PropertySlot slot;
                JSValue *v = base->getPropertySlot(exec, ident, slot) ? slot.getValue(exec, base, ident) : jsUndefined();
                KJS_CHECKEXCEPTION();
                
                double n = v->toNumber(exec);
                double newValue = (prefixDotNode->m_oper == OpPlusPlus) ? n + 1 : n - 1;
                JSValue *n2 = jsNumber(newValue);
                base->put(exec, ident, n2);
                
                RETURN_VALUE(n2);
            }
            
            // FIXME: This can just be removed, or?
            case UnaryPlusNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<UnaryPlusNode*>(currentNode)->expr.get(), UnaryPlusNodeEvaluateState1);
            }
            case UnaryPlusNodeEvaluateState1:
            {
                double value = valueReturn()->toNumber(exec);
                RETURN_VALUE(jsNumber(value));
            }

            case NegateNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<NegateNode*>(currentNode)->expr.get(), NegateNodeEvaluateState1);
            }
            case NegateNodeEvaluateState1:
            {
                double value = valueReturn()->toNumber(exec);
                RETURN_VALUE(jsNumber(-value));
            }
            
            case BitwiseNotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<BitwiseNotNode*>(currentNode)->expr.get(), BitwiseNotNodeEvaluateState1);
            }
            case BitwiseNotNodeEvaluateState1:
            {
                int32_t value = valueReturn()->toInt32(exec);
                RETURN_VALUE(jsNumber(~value));
            }
                
            case LogicalNotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<LogicalNotNode*>(currentNode)->expr.get(), LogicalNotNodeEvaluateState1);
            }
            case LogicalNotNodeEvaluateState1:
            {
                bool value = valueReturn()->toBoolean(exec);
                RETURN_VALUE(jsBoolean(!value));
            }
            
            case MultNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<MultNode*>(currentNode)->term1.get(), MultNodeEvaluateState1);
            }
            case MultNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<MultNode*>(currentNode)->term2.get(), MultNodeEvaluateState2);
            }
            case MultNodeEvaluateState2:
            {
                RETURN_VALUE(mult(exec, m_valueStack.pop(), valueReturn(), static_cast<MultNode*>(currentNode)->oper));
            }
            
            case AddNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<AddNode*>(currentNode)->term1.get(), AddNodeEvaluateState1);
            }
            case AddNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<AddNode*>(currentNode)->term2.get(), AddNodeEvaluateState2);
            }
            case AddNodeEvaluateState2:
            {
                RETURN_VALUE(add(exec, m_valueStack.pop(), valueReturn(), static_cast<AddNode*>(currentNode)->oper));
            }
            
            case ShiftNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<ShiftNode*>(currentNode)->term1.get(), ShiftNodeEvaluateState1);
            }
            case ShiftNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<ShiftNode*>(currentNode)->term2.get(), ShiftNodeEvaluateState2);
            }
            case ShiftNodeEvaluateState2:
            {
                JSValue *v2 = valueReturn();
                JSValue *v1 = m_valueStack.pop();
                unsigned int i2 = v2->toUInt32(exec);
                i2 &= 0x1f;
                
                JSValue* result;
                switch (static_cast<ShiftNode*>(currentNode)->oper) {
                    case OpLShift:
                        result = jsNumber(v1->toInt32(exec) << i2);
                        break;
                    case OpRShift:
                        result = jsNumber(v1->toInt32(exec) >> i2);
                        break;
                    case OpURShift:
                        result = jsNumber(v1->toUInt32(exec) >> i2);
                        break;
                    default:
                        assert(!"ShiftNode: unhandled switch case");
                        result = jsUndefined();
                }
                RETURN_VALUE(result);
            }
            
            case RelationalNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<RelationalNode*>(currentNode)->expr1.get(), RelationalNodeEvaluateState1);
            }
            case RelationalNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<RelationalNode*>(currentNode)->expr2.get(), RelationalNodeEvaluateState2);
            }
            case RelationalNodeEvaluateState2:
            {
                RelationalNode* relationalNode = static_cast<RelationalNode*>(currentNode);
                JSValue *v2 = valueReturn();
                JSValue *v1 = m_valueStack.pop();
                Operator oper = relationalNode->oper;
                
                bool b;
                if (oper == OpLess || oper == OpGreaterEq) {
                    int r = relation(exec, v1, v2);
                    if (r < 0)
                        b = false;
                    else
                        b = (oper == OpLess) ? (r == 1) : (r == 0);
                } else if (oper == OpGreater || oper == OpLessEq) {
                    int r = relation(exec, v2, v1);
                    if (r < 0)
                        b = false;
                    else
                        b = (oper == OpGreater) ? (r == 1) : (r == 0);
                } else if (oper == OpIn) {
                    // Is all of this OK for host objects?
                    if (!v2->isObject())
                        RETURN_ERROR(relationalNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with IN expression.", v2, relationalNode->expr2.get()));
                    JSObject *o2(static_cast<JSObject*>(v2));
                    b = o2->hasProperty(exec, Identifier(v1->toString(exec)));
                } else {
                    if (!v2->isObject())
                        RETURN_ERROR(relationalNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with instanceof operator.", v2, relationalNode->expr2.get()));
                    
                    JSObject *o2(static_cast<JSObject*>(v2));
                    if (!o2->implementsHasInstance()) {
                        // According to the spec, only some types of objects "implement" the [[HasInstance]] property.
                        // But we are supposed to throw an exception where the object does not "have" the [[HasInstance]]
                        // property. It seems that all object have the property, but not all implement it, so in this
                        // case we return false (consistent with mozilla)
                        RETURN_VALUE(jsBoolean(false));
                        //      return throwError(exec, TypeError,
                        //                      "Object does not implement the [[HasInstance]] method." );
                    }
                    RETURN_VALUE(jsBoolean(o2->hasInstance(exec, v1)));
                }
                
                RETURN_VALUE(jsBoolean(b));
            }
            

            case EqualNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<EqualNode*>(currentNode)->expr1.get(), EqualNodeEvaluateState1);
            }
            case EqualNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<EqualNode*>(currentNode)->expr2.get(), EqualNodeEvaluateState2);
            }
            case EqualNodeEvaluateState2:
            {
                JSValue* v2 = valueReturn();
                JSValue* v1 = m_valueStack.pop();
                Operator oper = static_cast<EqualNode*>(currentNode)->oper;
                
                bool result;
                if (oper == OpEqEq || oper == OpNotEq) {
                    // == and !=
                    bool eq = equal(exec, v1, v2);
                    result = oper == OpEqEq ? eq : !eq;
                } else {
                    // === and !==
                    bool eq = strictEqual(exec, v1, v2);
                    result = oper == OpStrEq ? eq : !eq;
                }
                RETURN_VALUE(jsBoolean(result));
            }


            case BitOperNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<BitOperNode*>(currentNode)->expr1.get(), BitOperNodeEvaluateState1);
            }
            case BitOperNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<BitOperNode*>(currentNode)->expr2.get(), BitOperNodeEvaluateState2);
            }
            case BitOperNodeEvaluateState2:
            {
                JSValue* v2 = valueReturn();
                JSValue* v1 = m_valueStack.pop();
                Operator oper = static_cast<BitOperNode*>(currentNode)->oper;

                int i1 = v1->toInt32(exec);
                int i2 = v2->toInt32(exec);
                int result;
                if (oper == OpBitAnd)
                    result = i1 & i2;
                else if (oper == OpBitXOr)
                    result = i1 ^ i2;
                else
                    result = i1 | i2;
                
                RETURN_VALUE(jsNumber(result));
            }
                
            case BinaryLogicalNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<BinaryLogicalNode*>(currentNode)->expr1.get(), BinaryLogicalNodeEvaluateState1);
            }
            case BinaryLogicalNodeEvaluateState1:
            {
                JSValue* v1 = valueReturn();
                BinaryLogicalNode* binaryLogicalNode = static_cast<BinaryLogicalNode*>(currentNode);
                Operator oper = binaryLogicalNode->oper;
                bool b1 = v1->toBoolean(exec);
                if ((!b1 && oper == OpAnd) || (b1 && oper == OpOr))
                    RETURN_VALUE(v1);
                pushEvaluate(interpreter, binaryLogicalNode->expr2.get());
                break;
            }
            
            case ConditionalNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<ConditionalNode*>(currentNode)->logical.get(), ConditionalNodeEvaluateState1);
            }
            case ConditionalNodeEvaluateState1:
            {
                JSValue *v = valueReturn();
                ConditionalNode* conditionalNode = static_cast<ConditionalNode*>(currentNode);
                RefPtr<Node> node = v->toBoolean(exec) ? conditionalNode->expr1 : conditionalNode->expr2;
                pushEvaluate(interpreter, node.get());
                break;
            }
            
            case AssignResolveNodeEvaluateState:
            {
                AssignResolveNode *assignResolveNode = static_cast<AssignResolveNode*>(currentNode);
                const ScopeChain& chain = exec->context().imp()->scopeChain();
                ScopeChainIterator iter = chain.begin();
                ScopeChainIterator end = chain.end();
                Operator oper = assignResolveNode->m_oper;
                const Identifier& ident = assignResolveNode->m_ident;
                
                // we must always have something in the scope chain
                assert(iter != end);
                
                PropertySlot slot;
                JSObject *base;
                bool foundSlot = false;
                do { 
                    base = *iter;
                    if (base->getPropertySlot(exec, ident, slot)) {
                        foundSlot = true;
                        break;
                    }
                    
                    ++iter;
                } while (iter != end);
                
                if (!foundSlot && (oper != OpEqual))
                    RETURN_ERROR(assignResolveNode->throwUndefinedVariableError(exec, ident));
                
                m_valueStack.push(base);
                EVALUATE_AND_CONTINUE(assignResolveNode->m_right.get(), AssignResolveNodeEvaluateState1);
            }
            case AssignResolveNodeEvaluateState1:
            {
                AssignResolveNode *assignResolveNode = static_cast<AssignResolveNode*>(currentNode);
                Operator oper = assignResolveNode->m_oper;
                const Identifier& ident = assignResolveNode->m_ident;
                JSValue* v = valueReturn();
                JSObject* base = static_cast<JSObject*>(m_valueStack.pop());
                // FIXME: We could avoid this slot double-lookup by having a slot stack.
                PropertySlot slot;
                base->getPropertySlot(exec, ident, slot);
                if (oper != OpEqual) {
                    JSValue* v1 = slot.getValue(exec, base, ident);
                    KJS_CHECKEXCEPTION();
                    v = valueForReadModifyAssignment(exec, v1, v, oper);
                }
                KJS_CHECKEXCEPTION();
                
                base->put(exec, ident, v);
                RETURN_VALUE(v);
            }
            
            
            case AssignDotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<AssignDotNode*>(currentNode)->m_base.get(), AssignDotNodeEvaluateState1);
            }
            case AssignDotNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                AssignDotNode* assignDotNode = static_cast<AssignDotNode*>(currentNode);
                if (assignDotNode->m_oper == OpEqual)
                    EVALUATE_AND_JUMP(assignDotNode->m_right.get(), AssignDotNodeEvaluateState3);
                else
                    EVALUATE_AND_CONTINUE(assignDotNode->m_right.get(), AssignDotNodeEvaluateState2);
            }
            case AssignDotNodeEvaluateState2:
            {
                AssignDotNode* assignDotNode = static_cast<AssignDotNode*>(currentNode);
                JSValue* v2 = valueReturn();
                JSObject* base = m_valueStack.peek()->toObject(exec);
                KJS_CHECKEXCEPTION();
                PropertySlot slot;
                JSValue *v1 = base->getPropertySlot(exec, assignDotNode->m_ident, slot) ? slot.getValue(exec, base, assignDotNode->m_ident) : jsUndefined();
                KJS_CHECKEXCEPTION();
                
                setValueReturn(valueForReadModifyAssignment(exec, v1, v2, assignDotNode->m_oper));
                FALL_THROUGH();
            }
            case AssignDotNodeEvaluateState3:
            {
                AssignDotNode* assignDotNode = static_cast<AssignDotNode*>(currentNode);
                JSValue* v = valueReturn();
                JSObject* base = m_valueStack.pop()->toObject(exec);
                KJS_CHECKEXCEPTION();
                base->put(exec, assignDotNode->m_ident, v);
                RETURN_VALUE(v);
            }
                
            
            case AssignBracketNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<AssignBracketNode*>(currentNode)->m_base.get(), AssignBracketNodeEvaluateState1);
            }
            case AssignBracketNodeEvaluateState1:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<AssignBracketNode*>(currentNode)->m_subscript.get(), AssignBracketNodeEvaluateState2);
            }
            case AssignBracketNodeEvaluateState2:
            {
                m_valueStack.push(valueReturn());
                EVALUATE_AND_CONTINUE(static_cast<AssignBracketNode*>(currentNode)->m_right.get(), AssignBracketNodeEvaluateState3);
            }
            case AssignBracketNodeEvaluateState3:
            {
                AssignBracketNode* assignBracketNode = static_cast<AssignBracketNode*>(currentNode);
                JSValue* v = valueReturn();
                JSValue* subscript = m_valueStack.pop();            
                JSObject* base = m_valueStack.pop()->toObject(exec);
                KJS_CHECKEXCEPTION();
                Operator oper = assignBracketNode->m_oper;
                
                uint32_t propertyIndex;
                if (subscript->getUInt32(propertyIndex)) {
                    if (oper != OpEqual) {
                        PropertySlot slot;
                        JSValue *v1 = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
                        KJS_CHECKEXCEPTION();
                        v = valueForReadModifyAssignment(exec, v1, v, oper);
                    }
                    KJS_CHECKEXCEPTION();
                    
                    base->put(exec, propertyIndex, v);
                    RETURN_VALUE(v);
                }
                
                Identifier propertyName(subscript->toString(exec));
                
                if (oper != OpEqual) {
                    PropertySlot slot;
                    JSValue *v1 = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
                    KJS_CHECKEXCEPTION();
                    v = valueForReadModifyAssignment(exec, v1, v, oper);
                }
                KJS_CHECKEXCEPTION();
                
                base->put(exec, propertyName, v);
                RETURN_VALUE(v);
            }
                
            case CommaNodeEvaluateState:
            {
                pushEvaluate(interpreter, static_cast<CommaNode*>(currentNode)->expr2.get());
                pushEvaluate(interpreter, static_cast<CommaNode*>(currentNode)->expr1.get()); // ignore the first return value
                break;
            }
            
            case AssignExprNodeEvaluateState:
            {
                pushEvaluate(interpreter, static_cast<AssignExprNode*>(currentNode)->expr.get());
                break;
            }
                
            case VarDeclNodeEvaluateState:
            {
                VarDeclNode* varDecl = static_cast<VarDeclNode*>(currentNode);
                JSObject* variable = exec->context().imp()->variableObject();

                if (varDecl->init)
                    EVALUATE_AND_CONTINUE(varDecl->init.get(), VarDeclNodeEvaluateState1);
                else {
                    // already declared? - check with getDirect so you can override
                    // built-in properties of the global object with var declarations.
                    if (variable->getDirect(varDecl->ident))
                        RETURN_VALUE((JSValue*)0); // FIXME: this is WRONG
                    setValueReturn(jsUndefined());
                }
                FALL_THROUGH();
            }
            case VarDeclNodeEvaluateState1:
            {
                VarDeclNode* varDecl = static_cast<VarDeclNode*>(currentNode);
                JSValue* val = valueReturn();
                JSObject* variable = exec->context().imp()->variableObject();
#ifdef KJS_VERBOSE
                printInfo(exec, (UString("new variable ") + ident.ustring()).cstring().c_str(), val);
#endif
                // We use Internal to bypass all checks in derived objects, e.g. so that
                // "var location" creates a dynamic property instead of activating window.location.
                int flags = Internal;
                if (exec->context().imp()->codeType() != EvalCode)
                    flags |= DontDelete;
                if (varDecl->varType == VarDeclNode::Constant)
                    flags |= ReadOnly;
                variable->put(exec, varDecl->ident, val, flags);
                
                RETURN_VALUE(jsString(varDecl->ident.ustring()));
            }
                
            case VarDeclListNodeEvaluateState:
            {
                // FIXME: This could be optimized to push these all at once and return (once we reverse the list).
                EVALUATE_AND_CONTINUE(static_cast<VarDeclListNode*>(currentNode)->var.get(), VarDeclListNodeEvaluateState1);
            }
            case VarDeclListNodeEvaluateState1:
            {
                VarDeclListNode* n = static_cast<VarDeclListNode*>(currentNode);
                n = n->next.get();
                if (n) {
                    // FIXME: pushing "p" as the current node is dangerous
                    // If this function had to continue after this loop, currentNode would be the wrong node.
                    SET_JUMP_STATE(VarDeclListNodeEvaluateState, n);
                    break;
                }
                
                RETURN_VALUE(jsUndefined());
            }
                
            case CaseClauseNodeEvaluateState:
            {
                pushEvaluate(interpreter, static_cast<CaseClauseNode*>(currentNode)->expr.get());
                break;
            }
                
            case FuncExprNodeEvaluateState:
            {
                FuncExprNode* funcExprNode = static_cast<FuncExprNode*>(currentNode);
                ContextImp *context = exec->context().imp();
                bool named = !funcExprNode->ident.isNull();
                JSObject *functionScopeObject = 0;
                
                if (named) {
                    // named FunctionExpressions can recursively call themselves,
                    // but they won't register with the current scope chain and should
                    // be contained as single property in an anonymous object.
                    functionScopeObject = new JSObject;
                    context->pushScope(functionScopeObject);
                }
                
                FunctionImp* func = new DeclaredFunctionImp(exec, funcExprNode->ident, funcExprNode->body.get(), context->scopeChain());
                JSObject* proto = exec->lexicalInterpreter()->builtinObject()->construct(exec, List::empty());
                proto->put(exec, constructorPropertyName, func, ReadOnly|DontDelete|DontEnum);
                func->put(exec, prototypePropertyName, proto, Internal|DontDelete);
                
                int plen = 0;
                for (ParameterNode *p = funcExprNode->param.get(); p != 0L; p = p->nextParam(), plen++)
                    func->addParameter(p->ident());
                
                if (named) {
                    functionScopeObject->put(exec, funcExprNode->ident, func, Internal | ReadOnly | (context->codeType() == EvalCode ? 0 : DontDelete));
                    context->popScope();
                }
                
                RETURN_VALUE(func);
            }
            case ArgumentListNodeEvaluateListState:
            {
                m_listStack.push(List());
                FALL_THROUGH();
            }
            case ArgumentListNodeEvaluateListState1:
            {
                EVALUATE_AND_CONTINUE(static_cast<ArgumentListNode*>(currentNode)->expr.get(), ArgumentListNodeEvaluateListState2);
            }
            case ArgumentListNodeEvaluateListState2:
            {
                ArgumentListNode* n = static_cast<ArgumentListNode*>(currentNode);
                JSValue* v = valueReturn();
                List& l = m_listStack.peek();
                l.append(v);
                
                n = n->next.get();
                if (n) {
                    SET_JUMP_STATE(ArgumentListNodeEvaluateListState1, n);
                }
                // No return, list already pushed in ArgumentListNodeEvaluateListState
                break;
            }
            case ArgumentsNodeEvaluateListState:
            {
                ArgumentsNode* argumentsNode = static_cast<ArgumentsNode*>(currentNode);
                if (argumentsNode->list)
                    pushEvaluateList(interpreter, argumentsNode->list.get());
                else
                    RETURN_LIST(List());
                break;
            }
        
            case StatListNodeExecuteState:
            {
                EXECUTE_AND_CONTINUE(static_cast<StatListNode*>(currentNode)->statement.get(), StatListNodeExecuteState1);
            }
            case StatListNodeExecuteState1:
            {
                StatListNode* n = static_cast<StatListNode*>(currentNode);                                
                if ((n = n->next.get())) {
                    SET_JUMP_STATE(StatListNodeExecuteState, n);
                    break;
                }
                RETURN_NORMAL_COMPLETION();
            }

            case VarStatementNodeExecuteState:
            {
                pushEvaluate(interpreter, static_cast<VarStatementNode*>(currentNode)->next.get());
                break;
            }

            case BlockNodeExecuteState:
            {
                BlockNode* blockNode = static_cast<BlockNode*>(currentNode);
                if (blockNode->sourceHead) {
                    blockNode->sourceHead->processFuncDecl(exec);
                    pushExecute(interpreter, blockNode->sourceTail);
                    break;
                }
                RETURN_NORMAL_COMPLETION();
            }

            case EmptyStatementNodeExecuteState:
            {
                RETURN_NORMAL_COMPLETION();
            }

            case ExprStatementNodeExecuteState:
            {                
                EVALUATE_AND_CONTINUE(static_cast<ExprStatementNode*>(currentNode)->expr.get(), ExprStatementNodeExecuteState1);
            }
            case ExprStatementNodeExecuteState1:
            {
                RETURN_COMPLETION(Completion(Normal, valueReturn()));
            }

            case IfNodeExecuteState:
            {                
                EVALUATE_AND_CONTINUE(static_cast<IfNode*>(currentNode)->expr.get(), IfNodeExecuteState1);
            }
            case IfNodeExecuteState1:
            {
                IfNode* ifNode = static_cast<IfNode*>(currentNode);
                if (valueReturn()->toBoolean(exec)) {
                    pushExecute(interpreter, ifNode->statement1.get());
                    break;
                }
                
                if (ifNode->statement2) {
                    pushExecute(interpreter, ifNode->statement2.get());
                    break;
                }
                RETURN_NORMAL_COMPLETION();
            }


            case DoWhileNodeExecuteState:
            {
                PUSH_UNWIND_BARRIER(Break, DoWhileNodeExecuteEndState);
                PUSH_UNWIND_BARRIER(Continue, DoWhileNodeExecuteContinueState);
                setValueReturn(jsBoolean(true));
                SET_JUMP_STATE(DoWhileNodeExecuteBodyState, currentNode);
                break;
            }
            case DoWhileNodeExecuteContinueState:
            {
                ASSERT(completionReturn().complType() == Continue);
                resetCompletionToNormal();
                FALL_THROUGH();
            }
            case DoWhileNodeExecuteTestState:
            {
                EVALUATE_AND_CONTINUE(static_cast<DoWhileNode*>(currentNode)->expr.get(), DoWhileNodeExecuteBodyState);
            }
            case DoWhileNodeExecuteBodyState:
            {
                if (valueReturn()->toBoolean(exec))
                    EXECUTE_AND_JUMP(static_cast<DoWhileNode*>(currentNode)->statement.get(), DoWhileNodeExecuteTestState);

                // fall through if the test returns false
                POP_UNWIND_BARRIER(Continue);
                FALL_THROUGH();
            }
            case DoWhileNodeExecuteEndState:
            {
                POP_UNWIND_BARRIER(Break);
                
                ComplType complType = completionReturn().complType();
                if (complType != Normal) {
                    ASSERT(complType == Break);
                    resetCompletionToNormal();
                }
                break;
            }
            
            
            case WhileNodeExecuteState:
            {
                PUSH_UNWIND_BARRIER(Break, WhileNodeExecuteEndState);
                PUSH_UNWIND_BARRIER(Continue, WhileNodeExecuteContinueState);
                FALL_THROUGH();
            }
            case WhileNodeExecuteContinueState:
            {
                ASSERT(completionReturn().complType() == Normal || completionReturn().complType() == Continue);
                resetCompletionToNormal();
                FALL_THROUGH();
            }
            case WhileNodeExecuteTestState:
            {
                EVALUATE_AND_CONTINUE(static_cast<WhileNode*>(currentNode)->expr.get(), WhileNodeExecuteBodyState);
            }
            case WhileNodeExecuteBodyState:
            {
                if (valueReturn()->toBoolean(exec))
                    EXECUTE_AND_JUMP(static_cast<WhileNode*>(currentNode)->statement.get(), WhileNodeExecuteTestState);

                // fall through if the test returns false
                POP_UNWIND_BARRIER(Continue);
                FALL_THROUGH();
            }
            case WhileNodeExecuteEndState:
            {
                POP_UNWIND_BARRIER(Break);

                ComplType complType = completionReturn().complType();
                if (complType != Normal) {
                    ASSERT(complType == Break);
                    resetCompletionToNormal();
                }
                
                break;
            }
                
            case ForNodeExecuteState:
            {
                PUSH_UNWIND_BARRIER(Break, ForNodeExecuteEndState);
                PUSH_UNWIND_BARRIER(Continue, ForNodeExecuteContinueState);
                
                ForNode* forNode = static_cast<ForNode*>(currentNode);
                if (forNode->expr1)
                    EVALUATE_AND_CONTINUE(forNode->expr1.get(), ForNodeExecuteTestState);

                // fall through if there's no initilization statement
                FALL_THROUGH();
            }
            case ForNodeExecuteTestState:
            {
                ForNode* forNode = static_cast<ForNode*>(currentNode);
                if (forNode->expr2)
                    EVALUATE_AND_CONTINUE(forNode->expr2.get(), ForNodeExecuteBodyState);
                    
                // fall through if there's no test statement
                FALL_THROUGH();
            }
            case ForNodeExecuteBodyState:
            {
                ForNode* forNode = static_cast<ForNode*>(currentNode);

                if (forNode->expr2 && !valueReturn()->toBoolean(exec)) {
                    POP_UNWIND_BARRIER(Continue); // we're done, no more continues possible
                    SET_JUMP_STATE(ForNodeExecuteEndState, forNode);
                    break;
                }
                
                EXECUTE_AND_JUMP(forNode->statement.get(), ForNodeExecutePostBodyState);
            }
            case ForNodeExecuteContinueState:
            {
                ASSERT(completionReturn().complType() == Normal || completionReturn().complType() == Continue);
                resetCompletionToNormal();
                FALL_THROUGH();
            }
            case ForNodeExecutePostBodyState:
            {
                ForNode* forNode = static_cast<ForNode*>(currentNode);

                SET_JUMP_STATE(ForNodeExecuteTestState, forNode);
                if (forNode->expr3)
                    pushEvaluate(interpreter, forNode->expr3.get());
                break;
            }
            case ForNodeExecuteEndState:
            {
                POP_UNWIND_BARRIER(Break);
                
                ComplType complType = completionReturn().complType();
                if (complType != Normal) {
                    ASSERT(complType == Break);
                    resetCompletionToNormal();
                }
                
                break;
            }
            
            
            case ForInNodeExecuteState:
            {
                PUSH_UNWIND_BARRIER(Break, ForInNodeExecutePopBreakUnwindBarrierState);

                SET_CONTINUE_STATE(ForInNodeExecuteState2);
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                pushEvaluate(interpreter, forInNode->expr.get());
                if (VarDeclNode* varDeclNode = forInNode->varDecl.get())
                    pushEvaluate(interpreter, varDeclNode); // return is ignored.
                break;
            }
            case ForInNodeExecuteState2:
            {
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                JSValue* e = valueReturn();
                
                // Null and Undefined will throw if you call toObject on them
                if (e->isUndefinedOrNull()) {
                    POP_UNWIND_BARRIER(Break);
                    RETURN_NORMAL_COMPLETION();
                }
                    
                InterpreterState interpreterState;
                if (forInNode->lexpr->isResolveNode())
                    interpreterState = ForInNodeResolveNodeExecuteState;
                else if (forInNode->lexpr->isDotAccessorNode())
                    interpreterState = ForInNodeDotAccessorNodeExecuteState;
                else {
                    ASSERT(forInNode->lexpr->isBracketAccessorNode());
                    interpreterState = ForInNodeBracketAccessorNodeExecuteState;
                }
                    
                SET_JUMP_STATE(ForInNodeExecutePopBreakUnwindBarrierState, currentNode);
                
                JSObject *o = e->toObject(exec);
                KJS_CHECKEXCEPTION();
                Stack<Identifier, 256> propertyStack;
                ReferenceList propList = o->propList(exec);
                ReferenceListIterator propIt = propList.begin();
                while (propIt != propList.end()) {
                    Identifier name = propIt->getPropertyName(exec);
                    if (o->hasProperty(exec, name))
                        propertyStack.push(name);

                    propIt++;
                }

                // FIXME: Store these in the right order, so we don't have to reverse
                // them to push them onto the stack
                size_t numProperties = propertyStack.size();
                for (size_t i = 0; i < numProperties; i++) {
                    m_valueStack.push(jsString(propertyStack.pop().ustring()));
                    SET_JUMP_STATE(interpreterState, forInNode);
                }
                    
                break;
            }
            case ForInNodeResolveNodeExecuteState:
            {
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                const Identifier &lexprIdent = static_cast<ResolveNode*>(forInNode->lexpr.get())->identifier();
                
                const ScopeChain& chain = exec->context().imp()->scopeChain();
                ScopeChainIterator iter = chain.begin();
                ScopeChainIterator end = chain.end();
                
                // we must always have something in the scope chain
                assert(iter != end);
                
                JSValue* str = m_valueStack.pop();
                PropertySlot slot;
                JSObject* o;
                do { 
                    o = *iter;
                    if (o->getPropertySlot(exec, lexprIdent, slot)) {
                        o->put(exec, lexprIdent, str);
                        break;
                    }
                    ++iter;
                } while (iter != end);
                    
                if (iter == end)
                    o->put(exec, lexprIdent, str);
                        
                SET_JUMP_STATE(ForInNodeExecuteBodyState, currentNode);
                break;
            }
            case ForInNodeDotAccessorNodeExecuteState:
            {
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                EVALUATE_AND_JUMP(static_cast<DotAccessorNode*>(forInNode->lexpr.get())->base(), ForInNodeDotAccessorNodeExecuteState1);
            }
            case ForInNodeDotAccessorNodeExecuteState1:
            {
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                JSObject* base = valueReturn()->toObject(exec);
                KJS_CHECKEXCEPTION();
                JSValue* str = m_valueStack.pop();
                
                const Identifier& lexprIdent = static_cast<DotAccessorNode*>(forInNode->lexpr.get())->identifier();
                base->put(exec, lexprIdent, str);
                
                SET_JUMP_STATE(ForInNodeExecuteBodyState, currentNode);
                break;
            }
            
            case ForInNodeBracketAccessorNodeExecuteState:
            {
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                EVALUATE_AND_CONTINUE(static_cast<BracketAccessorNode*>(forInNode->lexpr.get())->base(), ForInNodeBracketAccessorNodeExecuteState1);
            }
            case ForInNodeBracketAccessorNodeExecuteState1:
            {
                m_valueStack.push(valueReturn());
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                EVALUATE_AND_CONTINUE(static_cast<BracketAccessorNode*>(forInNode->lexpr.get())->subscript(), ForInNodeBracketAccessorNodeExecuteState2);
            }
            case ForInNodeBracketAccessorNodeExecuteState2:
            {
                JSObject* base = m_valueStack.pop()->toObject(exec);
                KJS_CHECKEXCEPTION();
                JSValue* subscript = valueReturn();
                JSValue* str = m_valueStack.pop();
                
                uint32_t i;
                if (subscript->getUInt32(i))
                    base->put(exec, i, str);
                base->put(exec, Identifier(subscript->toString(exec)), str);
                
                FALL_THROUGH();
            }            
            case ForInNodeExecuteBodyState:
            {
                PUSH_UNWIND_BARRIER(Continue, ForInNodeExecutePopContinueUnwindBarrierState);
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                EXECUTE_AND_CONTINUE(forInNode->statement.get(), ForInNodeExecutePopContinueUnwindBarrierState);
                break;
            }

            case ForInNodeExecutePopContinueUnwindBarrierState:
            {
                if (completionReturn().complType() == Continue)
                    resetCompletionToNormal();
                ASSERT(completionReturn().complType() == Normal);
                
                POP_UNWIND_BARRIER(Continue);
                break;
            }
            case ForInNodeExecutePopBreakUnwindBarrierState:
            {
                if (completionReturn().complType() == Break)
                    resetCompletionToNormal();
                ASSERT(completionReturn().complType() == Normal);
                
                POP_UNWIND_BARRIER(Break);
                break;
            }
            
            
            case ContinueNodeExecuteState:
            {
                RETURN_COMPLETION(Completion(Continue, 0, static_cast<ContinueNode*>(currentNode)->ident));
            }

            case BreakNodeExecuteState:
            {
                RETURN_COMPLETION(Completion(Break, 0, static_cast<BreakNode*>(currentNode)->ident));
            }

            case ReturnNodeExecuteState:
            {
                ReturnNode* returnNode = static_cast<ReturnNode*>(currentNode);

                CodeType codeType = exec->context().imp()->codeType();
                if (codeType == FunctionCode || codeType == AnonymousCode) {
                    if (!returnNode->value)
                        RETURN_COMPLETION(Completion(ReturnValue, jsUndefined()));
                    EVALUATE_AND_CONTINUE(returnNode->value.get(), ReturnNodeExecuteState1);
                }
                
                RETURN_COMPLETION(returnNode->createErrorCompletion(exec, SyntaxError, "Invalid return statement."));
            }
            case ReturnNodeExecuteState1:
            {
                RETURN_COMPLETION(Completion(ReturnValue, valueReturn()));
            }

            case WithNodeExecuteState:
            {
                EVALUATE_AND_CONTINUE(static_cast<WithNode*>(currentNode)->expr.get(), WithNodeExecuteState1);
            }
            case WithNodeExecuteState1:
            {
                WithNode* withNode = static_cast<WithNode*>(currentNode);
                JSValue *v = valueReturn();
                JSObject *o = v->toObject(exec);
                KJS_CHECKEXCEPTION();
                PUSH_UNWIND_BARRIER(Scope, InternalErrorState); // scope marker
                exec->context().imp()->pushScope(o);
                EXECUTE_AND_CONTINUE(withNode->statement.get(), WithNodeExecuteState2);
            }
            case WithNodeExecuteState2:
            {
                exec->context().imp()->popScope();
                POP_UNWIND_BARRIER(Scope); // scope marker
                RETURN_NORMAL_COMPLETION();
            }

            case SwitchNodeExecuteState:
            {
                EVALUATE_AND_CONTINUE(static_cast<SwitchNode*>(currentNode)->expr.get(), SwitchNodeExecuteState1);
            }
            case SwitchNodeExecuteState1:
            {
                PUSH_UNWIND_BARRIER(Break, SwitchNodeExecuteState2);
                SET_CONTINUE_STATE(SwitchNodeExecuteState2);
                // CaseBlockNodeExecuteBlockWithInputValue uses the last return value as an "argument"
                ASSERT(static_cast<SwitchNode*>(currentNode)->block->interpreterState() == CaseBlockNodeExecuteBlockWithInputValue);
                pushExecute(interpreter, static_cast<SwitchNode*>(currentNode)->block.get());
                break;
            }
            case SwitchNodeExecuteState2:
            {
                POP_UNWIND_BARRIER(Break);
                const Completion& res = completionReturn();
                
                if ((res.complType() == Break) && static_cast<SwitchNode*>(currentNode)->ls.contains(res.target())) {
                    Completion c(Normal, res.value());
                    resetCompletionToNormal();
                    RETURN_COMPLETION(c);
                }
                break;
            }

            case LabelNodeExecuteState:
            {                
                PUSH_UNWIND_BARRIER(Break, LabelNodeExecuteState1);
                SET_CONTINUE_STATE(LabelNodeExecuteState1);
                pushExecute(interpreter, static_cast<LabelNode*>(currentNode)->statement.get());
                break;
            }
            case LabelNodeExecuteState1:
            {
                POP_UNWIND_BARRIER(Break);
                const Completion& e = completionReturn();
                
                if ((e.complType() == Break) && (e.target() == static_cast<LabelNode*>(currentNode)->label)) {
                    Completion c(Normal, e.value());
                    resetCompletionToNormal();
                    RETURN_COMPLETION(c);
                }
                break;
            }

            case ThrowNodeExecuteState:
            {
                EVALUATE_AND_CONTINUE(static_cast<ThrowNode*>(currentNode)->expr.get(), ThrowNodeExecuteState1);
            }
            case ThrowNodeExecuteState1:
            {
                RETURN_COMPLETION(Completion(Throw, valueReturn()));
            }

            case TryNodeExecuteState:
            {
                PUSH_UNWIND_BARRIER(Throw, TryNodeExecuteExceptionThrownState);
                SET_JUMP_STATE(TryNodeExecuteNoExceptionThrownState, currentNode);
                pushExecute(interpreter, static_cast<TryNode*>(currentNode)->tryBlock.get());
                break;
            }
            case TryNodeExecuteExceptionThrownState:
            {
                TryNode* tryNode = static_cast<TryNode*>(currentNode);
                POP_UNWIND_BARRIER(Throw);
                const Completion& c = completionReturn();
                if (tryNode->catchBlock && c.complType() == Throw) {
                    JSObject *obj = new JSObject;
                    obj->put(exec, tryNode->exceptionIdent, c.value(), DontDelete);
                    resetCompletionToNormal();
                    PUSH_UNWIND_BARRIER(Scope, InternalErrorState);
                    exec->context().imp()->pushScope(obj);
                    EXECUTE_AND_CONTINUE(tryNode->catchBlock.get(), TryNodeExecutePopScopeAfterCatchBlockState);
                }
                
                if (tryNode->finallyBlock) {
                    // We will have to re-throw after finally if we didn't catch this exception
                    if (completionReturn().complType() == Throw) {
                        m_valueStack.push(c.value());
                        resetCompletionToNormal();
                        SET_JUMP_STATE(TryNodeExecuteRethrowAfterFinallyState, currentNode);
                    }
                    pushExecute(interpreter, tryNode->finallyBlock.get());
                }
                break;
            }
            case TryNodeExecutePopScopeAfterCatchBlockState:
            {
                // This case is only hit after successfully executing a catch block
                exec->context().imp()->popScope();
                POP_UNWIND_BARRIER(Scope);
                
                TryNode* tryNode = static_cast<TryNode*>(currentNode);
                if (tryNode->finallyBlock)
                    pushExecute(interpreter, tryNode->finallyBlock.get());
                break;
            }
            case TryNodeExecuteNoExceptionThrownState:
            {
                // This case is hit when nothing is thrown
                TryNode* tryNode = static_cast<TryNode*>(currentNode);
                POP_UNWIND_BARRIER(Throw);
                if (tryNode->finallyBlock)
                    pushExecute(interpreter, tryNode->finallyBlock.get());
                break;
            }
            case TryNodeExecuteRethrowAfterFinallyState:
            {
                RETURN_COMPLETION(Completion(Throw, m_valueStack.pop()));
            }

            case FuncDeclNodeExecuteState:
            {
                RETURN_NORMAL_COMPLETION();
            }

            case SourceElementsNodeExecuteState:
            {
                SourceElementsNode* n = static_cast<SourceElementsNode*>(currentNode);
                do {
                    pushExecute(interpreter, n->node.get());
                    n = n->prev;
                } while (n);
                
                break;
            }
                
            case CaseBlockNodeExecuteBlockWithInputValue:
            {
                // FIXME: This is the only execute function which takes an argument!
                // This was originally called CaseBlockNode::evalBlock but was renamed
                // CaseBlockNodeExecuteBlock to indicate the completion return
                m_valueStack.push(valueReturn());
                
                CaseBlockNode* caseBlockNode = static_cast<CaseBlockNode*>(currentNode);
                // Skip over "a" loops to "b" loops if possible
                if (!caseBlockNode->list1) {
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue5, currentNode);
                    break;
                }
                
                m_nodeStack.push(caseBlockNode->list1.get()); // push "a"
                FALL_THROUGH();
            }
            case CaseBlockNodeExecuteBlockWithInputValue1: // top of first "a" loop
            {
                ClauseListNode* a = static_cast<ClauseListNode*>(m_nodeStack.peek());
                EVALUATE_AND_CONTINUE(a->getClause(), CaseBlockNodeExecuteBlockWithInputValue2);
            }
            case CaseBlockNodeExecuteBlockWithInputValue2:
            {
                Node*& a = m_nodeStack.peek();
                JSValue* v = valueReturn();
                JSValue* input = m_valueStack.peek();
                
                if (strictEqual(exec, input, v)) {
                    // more "a" pointers to evaluate, jump to second while.
                    SET_CONTINUE_STATE(CaseBlockNodeExecuteBlockWithInputValue3);
                    break;
                }
                
                a = static_cast<ClauseListNode*>(a)->getNext();
                // jump to top of first while(a)
                if (a) {
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue1, currentNode);
                    break;
                }
                
                m_nodeStack.pop(); // we're done with "a"
                SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue5, currentNode); // jump to top of first "b" loop
                break;
            }
            case CaseBlockNodeExecuteBlockWithInputValue3: // top of second while(a)
            {
                ClauseListNode* a = static_cast<ClauseListNode*>(m_nodeStack.peek());
                // CaseClauseNode::evalStatements()
                if (a->getClause()->next) {
                    EXECUTE_AND_CONTINUE(a->getClause()->next.get(), CaseBlockNodeExecuteBlockWithInputValue4);
                    break;
                }
                FALL_THROUGH();
            }
            case CaseBlockNodeExecuteBlockWithInputValue4:
            {
                Node*& a = m_nodeStack.peek();
                a = static_cast<ClauseListNode*>(a)->getNext();
                if (a) { // still more "a" pointers to evaluate, jump to top of second while
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue3, currentNode);
                    break;
                }
                
                m_nodeStack.pop(); // we're done with "a"
                FALL_THROUGH();
            }
            case CaseBlockNodeExecuteBlockWithInputValue5: // staging area for "b" loops
            {
                CaseBlockNode* caseBlockNode = static_cast<CaseBlockNode*>(currentNode);
                if (!caseBlockNode->list2) { // no "b", jump to default
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue8, currentNode);
                    break;
                }
                m_nodeStack.push(caseBlockNode->list2.get()); // push "b"
                FALL_THROUGH();
            }
            case CaseBlockNodeExecuteBlockWithInputValue6: // top of first "b" loop
            {
                ClauseListNode* b = static_cast<ClauseListNode*>(m_nodeStack.peek());
                EVALUATE_AND_CONTINUE(b->getClause(), CaseBlockNodeExecuteBlockWithInputValue7);
            }
            case CaseBlockNodeExecuteBlockWithInputValue7:
            {
                Node*& b = m_nodeStack.peek();
                JSValue* v = valueReturn();
                JSValue* input = m_valueStack.peek();
                if (strictEqual(exec, input, v)) {
                    // skip over default clause, evaluate statement block if possible.
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue10, currentNode);
                    if (static_cast<ClauseListNode*>(b)->getClause()->next)
                        pushExecute(interpreter, static_cast<ClauseListNode*>(b)->getClause()->next.get());
                    break;
                }
                
                b = static_cast<ClauseListNode*>(b)->getNext();
                if (b) { // jump to top of first "b" loop
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue6, currentNode);
                    break;
                }
                
                m_nodeStack.pop(); // pop "b"
                FALL_THROUGH();
            }
            case CaseBlockNodeExecuteBlockWithInputValue8: // default clause
            {
                CaseBlockNode* caseBlockNode = static_cast<CaseBlockNode*>(currentNode);
                if (caseBlockNode->list2) {
                    m_nodeStack.push(caseBlockNode->list2.get()); // push "b"
                    SET_CONTINUE_STATE(CaseBlockNodeExecuteBlockWithInputValue9);
                } else 
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue11, currentNode);

                if (caseBlockNode->def && caseBlockNode->def->next)
                    pushExecute(interpreter, caseBlockNode->def->next.get());

                break;
            }
            case CaseBlockNodeExecuteBlockWithInputValue9: // top of second "b" loop
            {
                ClauseListNode* b = static_cast<ClauseListNode*>(m_nodeStack.peek());
                if (b->getClause()->next) {
                    EXECUTE_AND_JUMP(b->getClause()->next.get(), CaseBlockNodeExecuteBlockWithInputValue10);
                    break;
                }
                
                FALL_THROUGH();
            }
            case CaseBlockNodeExecuteBlockWithInputValue10:
            {
                Node*& b = m_nodeStack.peek();
                b = static_cast<ClauseListNode*>(b)->getNext();
                if (b) { // jump to top of second "b" loop
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue9, currentNode);
                    break;
                }
                m_nodeStack.pop(); // done with "b"
            }    
            case CaseBlockNodeExecuteBlockWithInputValue11:
            {
                m_valueStack.pop(); // done with "input"
                RETURN_NORMAL_COMPLETION();
            }
            
            case JSObjectCallState:
            {
                // we ignore the current node!
                
                /* JSObjectCallState has a unique calling convention: 
                   Arguments are pushed on the stack in the same order
                   you might expect to pass them to JSObject::call()
                   i.e. func->call(exec, thisObj, args)
                   Thus they are popped here in reverse order. */
                
                const List& args = m_listStack.peek();
                JSObject* thisObj = static_cast<JSObject*>(m_valueStack.peek());
                JSObject* functionObj = static_cast<JSObject*>(m_valueStack.peek(1)); // left on the stack for use during final cleanup state
                
                if (functionObj->inherits(&GlobalFuncImp::info) && static_cast<GlobalFuncImp*>(functionObj)->m_functionId == GlobalFuncImp::Eval) {
                    SET_JUMP_STATE(GlobalFuncCallEvalState, currentNode);
                    break;
                } else if (!functionObj->inherits(&DeclaredFunctionImp::info)) {
                    JSValue* val = functionObj->call(exec, thisObj, args);
                    // we're done, pop the input args
                    m_listStack.pop();
                    m_valueStack.pop();
                    m_valueStack.pop();
                    RETURN_VALUE(val);
                }
                FALL_THROUGH();
            }
            
            
            case DeclaredFunctionCallState:
            {
                // See JSObjectCallState for notes on calling convention
                List args = m_listStack.pop();
                JSObject* thisObj = static_cast<JSObject*>(m_valueStack.pop());
                DeclaredFunctionImp* declaredFunction = static_cast<DeclaredFunctionImp*>(m_valueStack.peek()); // left on the stack for use during final cleanup state
                
#if KJS_MAX_STACK > 0
                if (m_execStateStack.size() > KJS_MAX_STACK)
                    RETURN_ERROR(throwError(exec, RangeError, "Maximum call stack size exceeded."));
#endif
                
#if JAVASCRIPT_CALL_TRACING
                static bool tracing = false;
                if (traceJavaScript() && !tracing) {
                    unsigned depth = executionContextPairStackDepth();
                    tracing = true;
                    for (int i = 0; i < depth; i++)
                        putchar(' ');
                    printf("*** calling:  %s\n", toString(exec).ascii());
                    for (int j = 0; j < args.size(); j++) {
                        for (int i = 0; i < depth; i++)
                            putchar(' ');
                        printf("*** arg[%d] = %s\n", j, args[j]->toString(exec).ascii());
                    }
                    tracing = false;
                }
#endif
                
                FunctionBodyNode* functionBodyNode = declaredFunction->body.get();                
                ContextImp* ctx = new ContextImp(globalObject(), interpreter, thisObj, functionBodyNode, declaredFunction->codeType(), exec->context().imp(), declaredFunction, &args);
                ExecState* newExec = new ExecState(exec->dynamicInterpreter(), ctx);
                m_execStateStack.push(newExec);
                
                // assign user supplied arguments to parameters
                declaredFunction->processParameters(newExec, args);
                // add variable declarations (initialized to undefined)
                declaredFunction->processVarDecls(newExec);
                
                if (m_debugger) {
                    bool shouldContinue = m_debugger->callEvent(newExec, functionBodyNode->sourceId(), functionBodyNode->firstLine(), declaredFunction, args);
                    if (!shouldContinue) {
                        m_debugger->imp()->abort();
                        RETURN_VALUE(jsUndefined()); // FIXME: This won't actualy exit the function.
                    }
                }
                
                PUSH_UNWIND_BARRIER(ReturnValue, DeclaredFunctionCallExplicitReturnState);
                PUSH_UNWIND_BARRIER(Throw, DeclaredFunctionCallEndState);
                exec = newExec;
                EXECUTE_AND_JUMP(functionBodyNode, DeclaredFunctionCallEndState);
            }
            case DeclaredFunctionCallEndState:
            {
                ASSERT(completionReturn().complType() == Throw || completionReturn().complType() == Normal);
                POP_UNWIND_BARRIER(Throw);
                FALL_THROUGH();
            }
            case DeclaredFunctionCallExplicitReturnState:
            {
                ASSERT(completionReturn().complType() == Throw || completionReturn().complType() == ReturnValue || completionReturn().complType() == Normal);
                POP_UNWIND_BARRIER(ReturnValue);
                
                DeclaredFunctionImp* declaredFunction = static_cast<DeclaredFunctionImp*>(m_valueStack.pop());
                const Completion& comp = completionReturn();
                FunctionBodyNode* functionBodyNode = declaredFunction->body.get();
                
                ExecState* newExec = m_execStateStack.pop();
                exec = m_execStateStack.peek();
                
                if (m_debugger) {
                    if (comp.complType() == Throw)
                        newExec->setException(comp.value());
                    
                    int shouldContinue = m_debugger->returnEvent(newExec, functionBodyNode->sourceId(), functionBodyNode->lastLine(), declaredFunction);
                    
                    delete newExec->context().imp();
                    delete newExec;
                    if (!shouldContinue) {
                        m_debugger->imp()->abort();
                        RETURN_VALUE(jsUndefined());
                    }
                }
                
                delete newExec->context().imp();
                delete newExec;
                newExec = 0;
                
                JSValue* val;
                if (comp.complType() == Throw) {
                    exec->setException(comp.value()); // FIXME: this should be removed when we revise the interpreter to not use exceptions
                    val = comp.value();
                    resetCompletionToNormal();
                } else if (comp.complType() == ReturnValue) {
                    val = comp.value();
                    resetCompletionToNormal();
                } else
                    val = jsUndefined();
                    
#if JAVASCRIPT_CALL_TRACING
                if (traceJavaScript() && !tracing) {
                    tracing = true;
                    for (int i = 0; i < depth; i++)
                        putchar(' ');
                    printf("*** returning:  %s\n", val->toString(exec).ascii());
                    tracing = false;
                }
#endif
                RETURN_VALUE(val);
            }
            
            
            case GlobalFuncCallEvalState:
            {
                // See JSObjectCallState for notes on calling convention
                List args = m_listStack.pop();
                m_valueStack.pop(); // ignore passed "thisObj"
                m_valueStack.pop(); // ignore passed "this"
            
                JSValue *x = args[0];
                if (!x->isString())
                    RETURN_VALUE(x);

                UString s = x->toString(exec);
                
                int sid;
                int errLine;
                UString errMsg;
                RefPtr<ProgramNode> programNode(Parser::parse(UString(), 0, s.data(), s.size(), &sid, &errLine, &errMsg));
                
                if (m_debugger) {
                    bool shouldContinue = m_debugger->sourceParsed(exec, sid, UString(), s, errLine);
                    if (!shouldContinue)
                        RETURN_VALUE(jsUndefined());
                }
                
                // no program node means a syntax occurred
                if (!programNode)
                    RETURN_ERROR(throwError(exec, SyntaxError, errMsg, errLine, sid, NULL));
                
                // enter a new execution context
                JSObject *thisObj = static_cast<JSObject *>(exec->context().thisValue());                    
                ContextImp* ctx = new ContextImp(globalObject(), interpreter, thisObj, programNode.get(), EvalCode, exec->context().imp());
                ExecState* newExec = new ExecState(exec->dynamicInterpreter(), ctx);
                m_execStateStack.push(newExec);
                
                // execute the code
                programNode->processVarDecls(newExec);
                m_nodeStack.push(programNode.release().release());
                PUSH_UNWIND_BARRIER(All, GlobalFuncCallEvalEndState);
                exec = newExec;
                resetCompletionToNormal();
                EXECUTE_AND_CONTINUE(m_nodeStack.peek(), GlobalFuncCallEvalEndState);
            }
            case GlobalFuncCallEvalEndState:
            {
                POP_UNWIND_BARRIER(All);
                RefPtr<ProgramNode> programNode = adoptRef(static_cast<ProgramNode*>(m_nodeStack.pop()));
                const Completion& comp = completionReturn();
                
                ExecState* newExec = m_execStateStack.pop();
                exec = m_execStateStack.peek();
                
                delete newExec->context().imp();
                delete newExec;
                newExec = 0;
                
                RETURN_VALUE(comp.value() ? comp.value() : jsUndefined());
            }
            
        } // end switch
        
        // This label is used by RETURN_VALUE to break out of nested while loops in case statements
interpreter_state_switch_end:
        if (exec->hadException()) {
            resetCompletionToNormal();
            setCompletionReturn(Completion(Throw, exec->exception()));
        }
        if (completionReturn().complType() != Normal)
            unwindToNextBarrier(exec, currentNode);
    }
    
}

// Legacy execution support -- clients should treecode-ify themselves
Completion callExecuteOnNode(StatementNode* node, ExecState* exec)
{
    InterpreterImp* interpreter = exec->dynamicInterpreter()->imp();

    interpreter->m_execStateStack.push(exec);
    
    // FIXME: It's a bit of a hack to use InternalErrorState to represent the stack frame barrier
    interpreter->pushUnwindBarrier(All, InterpreterImp::State(InternalErrorState, 0));

    pushExecute(interpreter, node);
    interpreter->runInterpreterLoop(exec);

    POP_UNWIND_BARRIER(All);
    interpreter->m_execStateStack.pop();

    Completion c = interpreter->completionReturn();
    interpreter->resetCompletionToNormal();
    return c;
}

} // namespace KJS
