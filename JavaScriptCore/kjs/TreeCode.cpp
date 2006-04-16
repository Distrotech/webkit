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
#include "Opcode.h"

namespace KJS {

const char* nameForOpcode[LastOpcode+1] = {
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

    "LastOpcode"
};

static inline int currentSourceId(ExecState* exec)
{
    return exec->context().imp()->currentBody()->sourceId();
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

#define RETURN() \
{\
    goto interpreter_state_switch_end; \
}

// Use SET_CONTINUE_STATE instead whenever possible
#define SET_JUMP_STATE(nextState, node) \
do { \
    ASSERT(nextState != statePair.opcode); \
    m_stateStack.push(InterpreterImp::State(nextState, node)); \
} while (0)

// This can only be used for calling yourself.
#define SET_LOOP_STATE(nextState) \
do { \
    ASSERT(nextState == statePair.opcode); \
    m_stateStack.push(InterpreterImp::State(nextState, currentNode)); \
} while (0)

// FALL_THROUGH is used in debug builds to allow proper assert checking even
// when some states fall through to another (thus not changing the current state)
#ifndef NDEBUG
#define FALL_THROUGH() \
    statePair.opcode = (Opcode)(statePair.opcode + 1)
#else
#define FALL_THROUGH()
#endif

#define RETURN_LIST(list) \
do {\
    m_listStack.push(list); \
    goto interpreter_state_switch_end; \
} while (0)

#ifndef NDEBUG
static inline bool isEvaluateOpcode(Opcode opcode)
{
    return (opcode > InternalErrorState && opcode < Evaluate_EvaluteList_Boundary);
}

static inline bool isEvaluateListOpcode(Opcode opcode)
{
    return (opcode > Evaluate_EvaluteList_Boundary && opcode < EvaluateList_Execute_Boundary);
}
#endif

static inline bool isExecuteState(Opcode opcode)
{
    return (opcode > EvaluateList_Execute_Boundary && opcode < LastOpcode);
}

static inline void pushEvaluate(InterpreterImp* interpreter, Node* node)
{
    Opcode nextOpcode = node->opcode();
    ASSERT(isEvaluateOpcode(nextOpcode));
    interpreter->m_stateStack.push(InterpreterImp::State(nextOpcode, node));
}

static inline void pushEvaluateList(InterpreterImp* interpreter, Node* node)
{
    Opcode nextOpcode = node->opcode();
    ASSERT(isEvaluateListOpcode(nextOpcode));
    interpreter->m_stateStack.push(InterpreterImp::State(nextOpcode, node));
}

static inline void pushExecute(InterpreterImp* interpreter, Node* node)
{
    Opcode nextOpcode = node->opcode();
    ASSERT(isExecuteOpcode(nextOpcode));
    interpreter->m_stateStack.push(InterpreterImp::State(nextOpcode, node));
}

// This explicitly checks for continue state bugs
// FIXME: This should not take an argument once we have a way to check if currentState+1 == a substate
#define SET_CONTINUE_STATE(nextOpcode) \
do { \
    ASSERT(nextOpcode == statePair.opcode + 1); \
    SET_JUMP_STATE(nextOpcode, currentNode); \
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

#define EVALUATE_NEXT(n) \
do { \
    currentNode = n; \
    statePair.node = currentNode; \
    statePair.opcode = currentNode->opcode(); \
    ASSERT(isEvaluateState(statePair.opcode) || statePair.opcode == WhileTestEndNodeExecuteState || statePair.opcode == DoWhileTestEndNodeExecuteState || statePair.opcode == JSObjectCallState); \
    goto evaluate_next_continue; \
} while (0)

#define PUSH_RESULT_AND_EVAL_NEXT(val) \
do { \
    m_valueStack.push(val); \
    EVALUATE_NEXT(static_cast<ExprNode*>(currentNode)->next()); \
} while (0)

#define REPLACE_RESULT_AND_EVAL_NEXT(val) \
do { \
    m_valueStack.peek() = val; \
    EVALUATE_NEXT(static_cast<ExprNode*>(currentNode)->next()); \
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
    SET_JUMP_STATE(JSObjectCallState, currentNode); \
    pushEvaluateList(interpreter, argsNode); \
    goto interpreter_state_switch_end; \
}

void InterpreterImp::runInterpreterLoop(ExecState* exec)
{
    ASSERT(!exec->hadException());
    ASSERT(exec->dynamicInterpreter()->imp() == this);
    
    InterpreterImp* interpreter = this;
    InterpreterImp::UnwindBarrier stackBase = m_unwindBarrierStack.peek();
        
    // FIXME: As a loop optimization, we should just peek, instead of pop here
    // Then every non-loop function would pop itself.
    InterpreterImp::State statePair = m_stateStack.pop();
    Node* currentNode = statePair.node;

    while (true) {

#if TRACE_EXECUTION
        printf("EXEC %s (%i) on %p (state state: %i, value stack: %i)\n", nameForOpcode[statePair.opcode], statePair.opcode, statePair.node, m_stateStack.size(), interpreter->valueStackDepth());
#endif

        // FIXME: This is wrong, this won't actually return.
        if (m_debugger) {
            if (isExecuteState(statePair.opcode)) {
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
            RETURN_ERROR(Error::create(exec, GeneralError, "Out of memory"));

        switch (statePair.opcode) {
            case InternalErrorState:
            default:
            {
                // should never be called
                ASSERT_NOT_REACHED();
                RETURN_VALUE((JSValue*)0);
            }
            
            case NullNodeEvaluateState:
            {
                PUSH_RESULT_AND_EVAL_NEXT(jsNull());
            }
            
            case BooleanNodeEvaluateState:
            {
                PUSH_RESULT_AND_EVAL_NEXT(jsBoolean(static_cast<BooleanNode*>(currentNode)->value));
            }
            
            case NumberNodeEvaluateState:
            {
                PUSH_RESULT_AND_EVAL_NEXT(jsNumber(static_cast<NumberNode*>(currentNode)->value));
            }
            
            case StringNodeEvaluateState:
            {
                PUSH_RESULT_AND_EVAL_NEXT(jsString(static_cast<StringNode*>(currentNode)->value));
            }
            
            case RegExpNodeEvaluateState:
            {
                RegExpNode* regexpNode = static_cast<RegExpNode*>(currentNode);
                List list;
                list.append(jsString(regexpNode->pattern));
                list.append(jsString(regexpNode->flags));
                
                JSObject *reg = exec->lexicalInterpreter()->imp()->builtinRegExp();
                PUSH_RESULT_AND_EVAL_NEXT(reg->construct(exec, list));
            }
            
            case ThisNodeEvaluateState:
            {
                PUSH_RESULT_AND_EVAL_NEXT(exec->context().imp()->thisValue());
            }
                
            case ResolveValueEvaluateState:
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
                        PUSH_RESULT_AND_EVAL_NEXT(slot.getValue(exec, o, resolveNode->ident));
                    
                    ++iter;
                } while (iter != end);
                 
                RETURN_ERROR(resolveNode->throwUndefinedVariableError(exec, resolveNode->ident));
            }

            case ResolveBaseEvaluateState:
            {
                ResolveNode* resolveNode = static_cast<ResolveNode*>(currentNode);
                const ScopeChain& chain = exec->context().imp()->scopeChain();
                ScopeChainIterator iter = chain.begin();
                ScopeChainIterator end = chain.end();
                
                // we must always have something in the scope chain
                assert(iter != end);
                
                PropertySlot slot;
                JSObject* o;
                do { 
                    o = *iter;
                    
                    if (o->getPropertySlot(exec, resolveNode->ident, slot))
                        PUSH_RESULT_AND_EVAL_NEXT(o);
                    
                    ++iter;
                } while (iter != end);

                PUSH_RESULT_AND_EVAL_NEXT(o);
            }

            case ResolveBaseAndValueEvaluateState:
            {
                ResolveNode* resolveNode = static_cast<ResolveNode*>(currentNode);
                const ScopeChain& chain = exec->context().imp()->scopeChain();
                ScopeChainIterator iter = chain.begin();
                ScopeChainIterator end = chain.end();
                
                // we must always have something in the scope chain
                assert(iter != end);
                
                PropertySlot slot;
                do { 
                    JSObject* o = *iter;
                    
                    if (o->getPropertySlot(exec, resolveNode->ident, slot)) {
                        m_valueStack.push(o);
                        PUSH_RESULT_AND_EVAL_NEXT(slot.getValue(exec, o, resolveNode->ident));
                    }

                    ++iter;
                } while (iter != end);

                RETURN_ERROR(resolveNode->throwUndefinedVariableError(exec, resolveNode->ident));
            }
                
            case ElementNodeEvaluateState:
            {
                m_valueStack.push(exec->lexicalInterpreter()->builtinArray()->construct(exec, List::empty()));
                m_valueStack.push(jsNumber(0));
                FALL_THROUGH();
            }
            case ElementNodeEvaluateState1:
            {
                EVALUATE_NEXT(static_cast<ElementNode*>(currentNode)->node.get());
            }
            case ElementEndNodeEvaluateState:
            {
                ElementNode* n = static_cast<ElementEndNode*>(currentNode)->m_element;
                JSValue* val = m_valueStack.pop();
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
                
                PUSH_RESULT_AND_EVAL_NEXT(array);
            }
                
            case ObjectLiteralNodeEvaluateState:
            {
                ObjectLiteralNode* objectLiteralNode = static_cast<ObjectLiteralNode*>(currentNode);
                if (objectLiteralNode->list) {
                    EVALUATE_AND_CONTINUE(objectLiteralNode->list.get(), ObjectLiteralNodeEvaluateState1);
                    break;
                }
                
                PUSH_RESULT_AND_EVAL_NEXT(exec->lexicalInterpreter()->builtinObject()->construct(exec, List::empty()));
            }
            case ObjectLiteralNodeEvaluateState1:
            {
                EVALUATE_NEXT(static_cast<ExprNode*>(currentNode)->m_next.get());
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
                EVALUATE_NEXT(static_cast<PropertyListNode*>(currentNode)->node->assign.get());
            }
            case PropertyEndNodeEvaluateState:
            {
                PropertyListNode* p = static_cast<PropertyEndNode*>(currentNode)->m_propertyList;
                JSValue* v = m_valueStack.pop();
                JSValue* name = m_valueStack.pop();
                JSObject* obj = static_cast<JSObject*>(m_valueStack.peek());
                
                Identifier propertyName = Identifier(name->toString(exec));
                switch (p->node->type) {
                case PropertyNode::Getter:
                    assert(v->isObject());
                    obj->defineGetter(exec, propertyName, static_cast<JSObject*>(v));
                    break;
                case PropertyNode::Setter:
                    assert(v->isObject());
                    obj->defineSetter(exec, propertyName, static_cast<JSObject*>(v));
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
                
                RETURN_VALUE(m_valueStack.peek());
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
            
            case BracketAccessorValueEvaluateState:
            {
                // stack in: [ ... base subscript ]

                JSValue* subscript = m_valueStack.pop();
                JSObject* base = m_valueStack.peek()->toObject(exec);
                KJS_CHECKEXCEPTION();

                uint32_t i;
                JSValue* result = subscript->getUInt32(i) ? base->get(exec, i) : base->get(exec, Identifier(subscript->toString(exec)));
                REPLACE_RESULT_AND_EVAL_NEXT(result);

                // stack out: [ ... value ]
            }
            
            case BracketAccessorBaseAndValueEvaluateState:
            {
                // stack in: [ ... base subscript ]

                JSValue* subscript = m_valueStack.peek();
                JSObject* base = m_valueStack.fromLast(1)->toObject(exec);
                KJS_CHECKEXCEPTION();
                m_valueStack.fromLast(1) = base;

                uint32_t i;
                JSValue* result = subscript->getUInt32(i) ? base->get(exec, i) : base->get(exec, Identifier(subscript->toString(exec)));
                REPLACE_RESULT_AND_EVAL_NEXT(result);

                // stack out: [ ... base value ]
            }

            case BracketAccessorBaseAndSubscriptEvaluateState:
            {
                // stack in: [ ... base subscript ]

                JSObject* base = m_valueStack.fromLast(1)->toObject(exec);
                KJS_CHECKEXCEPTION();
                m_valueStack.fromLast(1) = base;

                EVALUATE_NEXT(static_cast<ExprNode*>(currentNode)->next());

                // stack out: [ ... base subscript ]
            }
            
            case BracketAccessorBaseSubscriptAndValueEvaluateState:
            {
                // stack in: [ ... base subscript ]

                JSValue* subscript = m_valueStack.peek();
                JSObject* base = m_valueStack.fromLast(1)->toObject(exec);
                KJS_CHECKEXCEPTION();
                m_valueStack.fromLast(1) = base;

                uint32_t i;

                JSValue* result = subscript->getUInt32(i) ? base->get(exec, i) : base->get(exec, Identifier(subscript->toString(exec)));
                PUSH_RESULT_AND_EVAL_NEXT(result);

                // stack out: [ ... base subscript value ]
            }

            case DotAccessorValueEvaluateState:
            {
                JSObject* base = m_valueStack.peek()->toObject(exec);
                KJS_CHECKEXCEPTION();
                REPLACE_RESULT_AND_EVAL_NEXT(base->get(exec, static_cast<DotAccessorNode*>(currentNode)->ident));
            }
                
            case DotAccessorBaseEvaluateState:
            {
                JSObject* base = m_valueStack.peek()->toObject(exec);
                KJS_CHECKEXCEPTION();
                m_valueStack.peek() = base;

                EVALUATE_NEXT(static_cast<ExprNode*>(currentNode)->next());
            }
                
            case DotAccessorBaseAndValueEvaluateState:
            {
                JSObject* base = m_valueStack.peek()->toObject(exec);
                KJS_CHECKEXCEPTION();
                m_valueStack.peek() = base;

                PUSH_RESULT_AND_EVAL_NEXT(base->get(exec, static_cast<DotAccessorNode*>(currentNode)->ident));
            }
                
            case NewExprNodeEvaluateState:
            {
                NewExprNode* newExprNode = static_cast<NewExprNode*>(currentNode);
                if (newExprNode->args)
                    EVALUATE_LIST_AND_CONTINUE(newExprNode->args.get(), NewExprNodeEvaluateState1);
                FALL_THROUGH();
            }
            case NewExprNodeEvaluateState1:
            {
                NewExprNode* newExprNode = static_cast<NewExprNode*>(currentNode);
                JSValue* v = m_valueStack.peek();
                List argList;
                if (newExprNode->args)
                    argList = m_listStack.pop();
                
                if (!v->isObject())
                    // FIXME: made error message worse
                    /* RETURN_ERROR(newExprNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with new.", v, newExprNode->expr.get())); */
                    RETURN_ERROR(newExprNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with new.", v));
                JSObject *constr = static_cast<JSObject*>(v);
                if (!constr->implementsConstruct())
                    /* RETURN_ERROR(newExprNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not a constructor. Cannot be used with new.", v, newExprNode->expr.get())); */
                    RETURN_ERROR(newExprNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not a constructor. Cannot be used with new.", v));

                REPLACE_RESULT_AND_EVAL_NEXT(constr->construct(exec, argList));
            }
                
            case FunctionCallNoBaseNodeEvaluateState:
            {
                m_valueStack.push(interpreter->globalObject()); // push "thisObj"
                m_valueStack.swapLastTwo();
                FALL_THROUGH();
            }
            case FunctionCallNodeEvaluateState:
            {
                JSValue* v = m_valueStack.peek();
                JSObject* base = static_cast<JSObject*>(m_valueStack.fromLast(1));

                // FIXME: check for objectness and callability should only be done *after* evaluating arguments!
                FunctionCallNode* functionCallNode = static_cast<FunctionCallNode*>(currentNode);
                if (!v->isObject())
                    // FIXME: suboptimal error message
                    RETURN_ERROR(functionCallNode->throwError(exec, TypeError, "Value %s is not object.", v));
                /* RETURN_ERROR(functionCallNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, ident)); */
                        
                JSObject* func = static_cast<JSObject*>(v);
                        
                if (!func->implementsCall())
                    // FIXME: suboptimal error message
                    RETURN_ERROR(functionCallNode->throwError(exec, TypeError, "Object %s does not allow calls.", v));
                /* RETURN_ERROR(functionCallNode->throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, ident)); */
                
                // ECMA 11.2.3 says that in this situation the this value should be null.
                // However, section 10.2.3 says that in the case where the value provided
                // by the caller is null, the global object should be used. It also says
                // that the section does not apply to interal functions, but for simplicity
                // of implementation we use the global object anyway here. This guarantees
                // that in host objects you always get a valid object for this.

                // FIXME: this check is only needed in the Resolve case, could optimize out for a dot call
                if (base->isActivation())
                    base = interpreter->globalObject();
                
                m_valueStack.swapLastTwo();

                EVALUATE_ARGUMENT_LIST_AND_JUMP_TO_CALL(functionCallNode->args.get()); // argsList is pushed as part of call
            }
                
            case PostfixNodeEvaluateState:
            {
                // stack in: [ ... base oldValue ]

                JSValue* oldValue = m_valueStack.pop();
                JSObject* base = static_cast<JSObject*>(m_valueStack.peek());

                PostfixNode* postfixNode = static_cast<PostfixNode*>(currentNode);
                Identifier& ident = postfixNode->m_ident;
                double oldNumber = oldValue->toNumber(exec);
                double newNumber = (postfixNode->m_oper == OpPlusPlus) ? oldNumber + 1.0 : oldNumber - 1.0;
                base->put(exec, ident, jsNumber(newNumber));

                REPLACE_RESULT_AND_EVAL_NEXT(jsNumber(oldNumber));

                // stack out: [ ... oldValue.toNumber() ]
            }
                
            case PostfixBracketNodeEvaluateState:
            {
                // stack in: [ ... base subscript oldValue ]

                JSValue* oldValue = m_valueStack.pop();
                JSValue* subscript = m_valueStack.pop();
                JSObject* base = static_cast<JSObject*>(m_valueStack.peek());

                PostfixBracketNode* postfixBracketNode = static_cast<PostfixBracketNode*>(currentNode);

                double oldNumber = oldValue->toNumber(exec);
                double newNumber = (postfixBracketNode->m_oper == OpPlusPlus) ? oldNumber + 1 : oldNumber - 1;
                JSValue* newValue = jsNumber(newNumber);

                uint32_t propertyIndex;
                if (subscript->getUInt32(propertyIndex))
                    base->put(exec, propertyIndex, newValue);
                else {
                    Identifier propertyName(subscript->toString(exec));
                    base->put(exec, propertyName, newValue);
                }
                
                REPLACE_RESULT_AND_EVAL_NEXT(jsNumber(oldNumber));

                // stack out: [ ... oldValue.toNumber() ]
            }
                
            case DeleteNodeEvaluateState:
            {
                // stack in: [ ... base ]
                
                JSObject* base = static_cast<JSObject*>(m_valueStack.peek());
                KJS_CHECKEXCEPTION();

                DeleteNode* deleteNode = static_cast<DeleteNode*>(currentNode);
                REPLACE_RESULT_AND_EVAL_NEXT(jsBoolean(base->deleteProperty(exec, deleteNode->m_ident)));
                    
                // stack out: [ ... succeeded? ]
            }
                
            case DeleteBracketNodeEvaluateState:
            {
                // stack in: [ ... base subscript ]
                
                JSValue *subscript = m_valueStack.pop();
                JSObject *base = static_cast<JSObject*>(m_valueStack.peek());
                KJS_CHECKEXCEPTION();
                
                uint32_t propertyIndex;
                bool succeeded;
                if (subscript->getUInt32(propertyIndex))
                    succeeded = base->deleteProperty(exec, propertyIndex);
                else {
                    Identifier propertyName(subscript->toString(exec));
                    succeeded = base->deleteProperty(exec, propertyName);
                }

                REPLACE_RESULT_AND_EVAL_NEXT(jsBoolean(succeeded));

                // stack out: [ ... succeeded? ]
            }
            
            case DeleteValueNodeEvaluateState:
            {
                // stack in: [ ... value ]

                REPLACE_RESULT_AND_EVAL_NEXT(jsBoolean(true));

                // stack out: [ ... true]
            }

            case VoidNodeEvaluateState:
            {
                REPLACE_RESULT_AND_EVAL_NEXT(jsUndefined());
            }

            case TypeOfResolveNodeEvaluateState:
            {
                // stack in: [ ... base ]

                JSObject* base = static_cast<JSObject*>(m_valueStack.peek());

                JSValue* type;
                Identifier& ident = static_cast<TypeOfResolveNode*>(currentNode)->m_ident;
                PropertySlot slot;
                if (base->getPropertySlot(exec, ident, slot)) {
                    JSValue* v = slot.getValue(exec, base, ident);
                    type = typeStringForValue(v);
                } else
                    type = jsString("undefined");
                
                REPLACE_RESULT_AND_EVAL_NEXT(type);

                // stack out: [ ... type ]
            }
                
            case TypeOfValueNodeEvaluateState:
            {
                // stack in: [ ... value ]

                REPLACE_RESULT_AND_EVAL_NEXT(typeStringForValue(m_valueStack.peek()));

                // stack out: [ ... type ]
            }
            
            case PrefixNodeEvaluateState:
            {
                // stack in: [ ... base value ]
                
                JSValue* value = m_valueStack.pop();
                JSObject* base = static_cast<JSObject*>(m_valueStack.peek());

                PrefixNode* prefixNode = static_cast<PrefixNode*>(currentNode);
                double n = value->toNumber(exec);
                double newNumber = (prefixNode->m_oper == OpPlusPlus) ? n + 1 : n - 1;
                JSValue* newValue = jsNumber(newNumber);
                base->put(exec, prefixNode->m_ident, newValue);

                REPLACE_RESULT_AND_EVAL_NEXT(newValue);

                // stack out: [ ... newValue ]
            }
                
            case PrefixBracketNodeEvaluateState:
            {
                // stack in: [ ... base subscript value ]

                JSValue* value = m_valueStack.pop();
                JSValue* subscript = m_valueStack.pop();
                JSObject* base = static_cast<JSObject*>(m_valueStack.peek());
                KJS_CHECKEXCEPTION();

                PrefixBracketNode* prefixBracketNode = static_cast<PrefixBracketNode*>(currentNode);
                Operator oper = prefixBracketNode->m_oper;
                
                double n = value->toNumber(exec);
                double newNumber = (oper == OpPlusPlus) ? n + 1 : n - 1;
                JSValue* newValue = jsNumber(newNumber);
                
                uint32_t propertyIndex;
                if (subscript->getUInt32(propertyIndex))
                    base->put(exec, propertyIndex, newValue);
                else {
                    Identifier propertyName(subscript->toString(exec));
                    base->put(exec, propertyName, newValue);
                }

                REPLACE_RESULT_AND_EVAL_NEXT(newValue);

                // stack out: [ ... newValue ]
            }
                
            case UnaryPlusNodeEvaluateState:
            {
                double value = m_valueStack.peek()->toNumber(exec);
                REPLACE_RESULT_AND_EVAL_NEXT(jsNumber(value));
            }

            case NegateNodeEvaluateState:
            {
                double value = m_valueStack.peek()->toNumber(exec);
                REPLACE_RESULT_AND_EVAL_NEXT(jsNumber(-value));
            }
            
            case BitwiseNotNodeEvaluateState:
            {
                int32_t value = m_valueStack.peek()->toInt32(exec);
                REPLACE_RESULT_AND_EVAL_NEXT(jsNumber(~value));
            }
                
            case LogicalNotNodeEvaluateState:
            {
                bool value = m_valueStack.peek()->toBoolean(exec);
                REPLACE_RESULT_AND_EVAL_NEXT(jsBoolean(!value));
            }
            
            case MultNodeEvaluateState:
            {
                JSValue* v2 = m_valueStack.pop();
                JSValue* v1 = m_valueStack.peek();
                REPLACE_RESULT_AND_EVAL_NEXT(mult(exec, v1, v2, static_cast<MultNode*>(currentNode)->oper));
            }
            
            case AddNodeEvaluateState:
            {
                JSValue* v2 = m_valueStack.pop();
                JSValue* v1 = m_valueStack.peek();
                REPLACE_RESULT_AND_EVAL_NEXT(add(exec, v1, v2, static_cast<AddNode*>(currentNode)->oper));
            }
            
            case ShiftNodeEvaluateState:
            {
                JSValue* v2 = m_valueStack.pop();
                JSValue* v1 = m_valueStack.peek();
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
                REPLACE_RESULT_AND_EVAL_NEXT(result);
            }
            
            case RelationalNodeEvaluateState:
            {
                RelationalNode* relationalNode = static_cast<RelationalNode*>(currentNode);
                JSValue* v2 = m_valueStack.pop();
                JSValue* v1 = m_valueStack.peek();
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
                        RETURN_ERROR(relationalNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with IN expression.", v2, currentNode));
                    JSObject *o2(static_cast<JSObject*>(v2));
                    b = o2->hasProperty(exec, Identifier(v1->toString(exec)));
                } else {
                    if (!v2->isObject())
                        RETURN_ERROR(relationalNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with instanceof operator.", v2, currentNode));
                    
                    JSObject *o2(static_cast<JSObject*>(v2));
                    if (!o2->implementsHasInstance()) {
                        // According to the spec, only some types of objects "implement" the [[HasInstance]] property.
                        // But we are supposed to throw an exception where the object does not "have" the [[HasInstance]]
                        // property. It seems that all object have the property, but not all implement it, so in this
                        // case we return false (consistent with mozilla)
                        b = false;
                        //      return throwError(exec, TypeError,
                        //                      "Object does not implement the [[HasInstance]] method." );
                    } else
                        b = o2->hasInstance(exec, v1);
                }
                
                REPLACE_RESULT_AND_EVAL_NEXT(jsBoolean(b));
            }
            
            case EqualNodeEvaluateState:
            {
                JSValue* v2 = m_valueStack.pop();
                JSValue* v1 = m_valueStack.peek();
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

                REPLACE_RESULT_AND_EVAL_NEXT(jsBoolean(result));
            }

            case BitOperNodeEvaluateState:
            {
                JSValue* v2 = m_valueStack.pop();
                JSValue* v1 = m_valueStack.peek();
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

                REPLACE_RESULT_AND_EVAL_NEXT(jsNumber(result));
            }
                
            case NoOpEvaluateState:
            {
                // stack in: [ ... ]
                EVALUATE_NEXT(static_cast<ExprNode*>(currentNode)->next());
                // stack out [ ... ]
            }

            case JumpEvaluateState:
            {
                // stack in: [ ... ]
                EVALUATE_NEXT(static_cast<JumpNode*>(currentNode)->m_target);
                // stack out: [ ... ]
            }

            case JumpIfFalseEvaluateState:
            {
                // stack in: [ ... conditionVal ]

                if (!m_valueStack.pop()->toBoolean(exec))
                    EVALUATE_NEXT(static_cast<JumpNode*>(currentNode)->m_target);

                EVALUATE_NEXT(static_cast<ExprNode*>(currentNode)->next());

                // stack out: [ ... ]
            }

            case JumpAndSaveIfFalseEvaluateState:
            {
                // stack in: [ ... conditionVal ]

                if (!m_valueStack.peek()->toBoolean(exec))
                    EVALUATE_NEXT(static_cast<JumpNode*>(currentNode)->m_target);

                m_valueStack.pop();
                EVALUATE_NEXT(static_cast<ExprNode*>(currentNode)->next());

                // stack out if taken: [ ... conditionVal ]
                // stack out if not taken: [ ... ]
            }

            case JumpAndSaveIfTrueEvaluateState:
            {
                // stack in: [ ... conditionVal ]

                if (m_valueStack.peek()->toBoolean(exec))
                    EVALUATE_NEXT(static_cast<JumpNode*>(currentNode)->m_target);

                m_valueStack.pop();
                EVALUATE_NEXT(static_cast<ExprNode*>(currentNode)->next());

                // stack out if taken: [ ... conditionVal ]
                // stack out if not taken: [ ... ]
            }
    
            case AssignNodeEvaluateState:
            {
                JSValue* v = m_valueStack.pop();
                // FIXME: in resolve case we know already that it's an object, should just
                // cast. For dot accessor, sadly we need this check
                JSObject* base = static_cast<JSObject*>(m_valueStack.peek());
                KJS_CHECKEXCEPTION();

                AssignNode *assignNode = static_cast<AssignNode*>(currentNode);
                const Identifier& ident = assignNode->m_ident;
                base->put(exec, ident, v);
                REPLACE_RESULT_AND_EVAL_NEXT(v);
            }

            case ReadModifyAssignNodeEvaluateState:
            {
                JSValue* v = m_valueStack.pop();
                JSValue* oldValue = m_valueStack.pop();
                JSObject* base = static_cast<JSObject*>(m_valueStack.peek());

                ReadModifyAssignNode *assignNode = static_cast<ReadModifyAssignNode*>(currentNode);
                Operator oper = assignNode->m_oper;
                const Identifier& ident = assignNode->m_ident;
                
                v = valueForReadModifyAssignment(exec, oldValue, v, oper);
                KJS_CHECKEXCEPTION();
                
                base->put(exec, ident, v);
                REPLACE_RESULT_AND_EVAL_NEXT(v);
            }

            case ReadModifyAssignBracketNodeEvaluateState:
            {
                // stack in: [ ... base subscript oldValue valueChange ]

                ReadModifyAssignBracketNode* assignBracketNode = static_cast<ReadModifyAssignBracketNode*>(currentNode);
                JSValue* valueChange = m_valueStack.pop();
                JSValue* oldValue = m_valueStack.pop();
                JSValue* subscript = m_valueStack.pop();            
                JSObject* base = static_cast<JSObject*>(m_valueStack.peek());
                KJS_CHECKEXCEPTION();
                Operator oper = assignBracketNode->m_oper;
                
                JSValue* newValue;
                uint32_t propertyIndex;
                if (subscript->getUInt32(propertyIndex)) {
                    newValue = valueForReadModifyAssignment(exec, valueChange, oldValue, oper);
                    KJS_CHECKEXCEPTION();
                    base->put(exec, propertyIndex, newValue);
                } else {
                    Identifier propertyName(subscript->toString(exec));
                    newValue = valueForReadModifyAssignment(exec, valueChange, oldValue, oper);
                    KJS_CHECKEXCEPTION();
                    base->put(exec, propertyName, newValue);
                }

                REPLACE_RESULT_AND_EVAL_NEXT(newValue);

                // stack out: [ ... newValue ]
            }
            
            case AssignBracketNodeEvaluateState:
            {
                // stack in: [ ... base subscript newValue ]

                JSValue* newValue = m_valueStack.pop();
                JSValue* subscript = m_valueStack.pop();            
                JSObject* base = static_cast<JSObject*>(m_valueStack.peek());
                KJS_CHECKEXCEPTION();
                
                uint32_t propertyIndex;
                if (subscript->getUInt32(propertyIndex))
                    base->put(exec, propertyIndex, newValue);
                else {
                    Identifier propertyName(subscript->toString(exec));
                    base->put(exec, propertyName, newValue);
                }

                REPLACE_RESULT_AND_EVAL_NEXT(newValue);

                // stack out: [ ... newValue ]
            }
                
            case CommaNodeEvaluateState:
            {
                // FIXME: this (and maybe other stuff) would do better with a
                // way to swap the top two stack values, then it would just be swap, pop
                JSValue* v2 = m_valueStack.pop();
                m_valueStack.peek(); /* ignore first value */

                REPLACE_RESULT_AND_EVAL_NEXT(v2);
            }
            
            case VarDeclNodeEvaluateState:
            {
                VarDeclNode* varDecl = static_cast<VarDeclNode*>(currentNode);
                JSObject* variable = exec->context().imp()->variableObject();

                if (varDecl->init)
                    EVALUATE_NEXT(varDecl->init.get());

                // already declared? - check with getDirect so you can override
                // built-in properties of the global object with var declarations.
                if (variable->getDirect(varDecl->ident))
                    RETURN();

                m_valueStack.push(jsUndefined());
                FALL_THROUGH();
            }
            case VarDeclEndNodeEvaluateState:
            {
                VarDeclNode* varDecl = static_cast<VarDeclEndNode*>(currentNode)->m_varDecl;
                JSValue* val = m_valueStack.pop();
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
                
                // FIXME: why does this return a string? no one seems to care...
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
                
                PUSH_RESULT_AND_EVAL_NEXT(func);
            }
            case ArgumentListNodeEvaluateListState:
            {
                m_listStack.push(List());
                FALL_THROUGH();
            }
            case ArgumentListNodeEvaluateListState1:
            {
                EVALUATE_NEXT(static_cast<ArgumentListNode*>(currentNode)->expr.get());
            }

            case ArgumentEndNodeEvaluateState:
            {
                ArgumentListNode* n = static_cast<ArgumentEndNode*>(currentNode)->m_argumentList;
                JSValue* v = m_valueStack.pop();
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
                EVALUATE_NEXT(static_cast<ExprStatementNode*>(currentNode)->expr.get());
            }
            case ExprStatementEndNodeExecuteState:
            {
                RETURN_COMPLETION(Completion(Normal, m_valueStack.pop()));
            }

            case IfNodeExecuteState:
            {           
                EVALUATE_NEXT(static_cast<IfNode*>(currentNode)->expr.get());
            }
            case IfConditionEndNodeExecuteState:
            {
                IfNode* ifNode = static_cast<IfConditionEndNode*>(currentNode)->m_if;
                if (m_valueStack.pop()->toBoolean(exec)) {
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
                FALL_THROUGH();
            }
            case DoWhileNodeExecuteBodyState:
            {
                EXECUTE_AND_JUMP(static_cast<DoWhileNode*>(currentNode)->statement.get(), DoWhileNodeExecuteTestState);
            }
            case DoWhileNodeExecuteContinueState:
            {
                ASSERT(completionReturn().complType() == Continue);
                resetCompletionToNormal();
                FALL_THROUGH();
            }
            case DoWhileNodeExecuteTestState:
            {
                EVALUATE_NEXT(static_cast<DoWhileNode*>(currentNode)->expr.get());
            }
            case DoWhileTestEndNodeExecuteState:
            {
                currentNode = static_cast<DoWhileTestExprEndNode*>(currentNode)->m_doWhile;
                if (m_valueStack.pop()->toBoolean(exec)) {
                    SET_JUMP_STATE(DoWhileNodeExecuteBodyState, currentNode);
                    goto interpreter_state_switch_end;
                }

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
                EVALUATE_NEXT(static_cast<WhileNode*>(currentNode)->expr.get());
            }
            case WhileTestEndNodeExecuteState:
            {
                currentNode = static_cast<WhileTestExprEndNode*>(currentNode)->m_while;
                if (m_valueStack.pop()->toBoolean(exec))
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
                

            case ForNodeVarDeclExecuteState:
            {
                PUSH_UNWIND_BARRIER(Break, ForNodeExecuteEndState);
                PUSH_UNWIND_BARRIER(Continue, ForNodeExecuteContinueState);
                
                ForNode* forNode = static_cast<ForNode*>(currentNode);
                
                EVALUATE_AND_JUMP(forNode->decls.get(), ForNodeVarDeclEndState);
            }
            case ForNodeExecuteState:
            {
                PUSH_UNWIND_BARRIER(Break, ForNodeExecuteEndState);
                PUSH_UNWIND_BARRIER(Continue, ForNodeExecuteContinueState);
                
                ForNode* forNode = static_cast<ForNode*>(currentNode);
                EVALUATE_NEXT(forNode->expr1.get());
            }
            case ForNodeInitOrIncrementEndState:
            {
                m_valueStack.pop();
                currentNode = static_cast<ForExprEndNode*>(currentNode)->m_for;
                FALL_THROUGH();
            }
            case ForNodeVarDeclEndState:
            {
                ForNode* forNode = static_cast<ForNode*>(currentNode);
                EVALUATE_NEXT(forNode->expr2.get());
            }
            case ForNodeExecuteTestState:
            {
                ForNode* forNode = static_cast<ForNode*>(currentNode);
                EVALUATE_NEXT(forNode->expr2.get());
            }
            case ForNodeTestEndState:
            {
                ForNode* forNode = static_cast<ForExprEndNode*>(currentNode)->m_for;
                currentNode = forNode;

                if (!m_valueStack.pop()->toBoolean(exec)) {
                    POP_UNWIND_BARRIER(Continue); // we're done, no more continues possible
                    SET_JUMP_STATE(ForNodeExecuteEndState, forNode);
                    RETURN();
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

                if (forNode->expr3)
                    EVALUATE_NEXT(forNode->expr3.get());
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
                    
                Opcode opcode;
                if (forInNode->lexpr->isResolveNode())
                    opcode = ForInNodeResolveNodeExecuteState;
                else if (forInNode->lexpr->isDotAccessorNode())
                    opcode = ForInNodeDotAccessorNodeExecuteState;
                else {
                    ASSERT(forInNode->lexpr->isBracketAccessorNode());
                    opcode = ForInNodeBracketAccessorNodeExecuteState;
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
                    SET_JUMP_STATE(opcode, forInNode);
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
                // FIXME
                ASSERT_NOT_REACHED();
#if 0
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                EVALUATE_AND_JUMP(static_cast<DotAccessorNode*>(forInNode->lexpr.get())->base(), ForInNodeDotAccessorNodeExecuteState1);
#endif
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
                ASSERT_NOT_REACHED();
                // FIXME
#if 0
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                EVALUATE_AND_CONTINUE(static_cast<BracketAccessorNode*>(forInNode->lexpr.get())->base(), ForInNodeBracketAccessorNodeExecuteState1);
#endif
            }
            case ForInNodeBracketAccessorNodeExecuteState1:
            {
                ASSERT_NOT_REACHED();
                // FIXME
#if 0
                m_valueStack.push(valueReturn());
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                EVALUATE_AND_CONTINUE(static_cast<BracketAccessorNode*>(forInNode->lexpr.get())->subscript(), ForInNodeBracketAccessorNodeExecuteState2);
#endif
            }
            case ForInNodeBracketAccessorNodeExecuteState2:
            {
                ASSERT_NOT_REACHED();
                // FIXME
#if 0
                JSObject* base = m_valueStack.pop()->toObject(exec);
                KJS_CHECKEXCEPTION();
                JSValue* subscript = valueReturn();
                JSValue* str = m_valueStack.pop();
                
                uint32_t i;
                if (subscript->getUInt32(i))
                    base->put(exec, i, str);
                base->put(exec, Identifier(subscript->toString(exec)), str);
                
                FALL_THROUGH();
#endif
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
                if (codeType == FunctionCode || codeType == AnonymousCode)
                    RETURN_COMPLETION(Completion(ReturnValue, jsUndefined()));
                
                RETURN_COMPLETION(returnNode->createErrorCompletion(exec, SyntaxError, "Invalid return statement."));
            }

            case ValueReturnEndNodeExecuteState:
            {
                ValueReturnEndNode* returnNode = static_cast<ValueReturnEndNode*>(currentNode);

                CodeType codeType = exec->context().imp()->codeType();
                if (codeType == FunctionCode || codeType == AnonymousCode)
                    RETURN_COMPLETION(Completion(ReturnValue, m_valueStack.pop()));

                RETURN_COMPLETION(returnNode->createErrorCompletion(exec, SyntaxError, "Invalid return statement."));
            }

            case WithNodeExecuteState:
            {
                EVALUATE_NEXT(static_cast<WithNode*>(currentNode)->expr.get());
            }
            case WithExprEndNodeExecuteState:
            {
                WithNode* withNode = static_cast<WithExprEndNode*>(currentNode)->m_with;
                JSValue *v = m_valueStack.pop();
                JSObject *o = v->toObject(exec);
                KJS_CHECKEXCEPTION();
                PUSH_UNWIND_BARRIER(Scope, InternalErrorState); // scope marker
                exec->context().imp()->pushScope(o);
                currentNode = withNode;
                EXECUTE_AND_JUMP(withNode->statement.get(), WithNodeExecuteState2);
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
                ASSERT(static_cast<SwitchNode*>(currentNode)->block->opcode() == CaseBlockNodeExecuteBlockWithInputValue);
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

            case ThrowEndNodeExecuteState:
            {
                ;
                RETURN_COMPLETION(Completion(Throw, m_valueStack.pop()));
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
                ClauseListNode*& a = reinterpret_cast<ClauseListNode*&>(m_nodeStack.peek());
                JSValue* v = valueReturn();
                JSValue* input = m_valueStack.peek();
                
                if (strictEqual(exec, input, v)) {
                    // more "a" pointers to evaluate, jump to second while.
                    SET_CONTINUE_STATE(CaseBlockNodeExecuteBlockWithInputValue3);
                    break;
                }
                
                a = a->getNext();
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
                ClauseListNode*& a = reinterpret_cast<ClauseListNode*&>(m_nodeStack.peek());
                a = a->getNext();
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
                ClauseListNode*& b = reinterpret_cast<ClauseListNode*&>(m_nodeStack.peek());
                JSValue* v = valueReturn();
                JSValue* input = m_valueStack.peek();
                if (strictEqual(exec, input, v)) {
                    // skip over default clause, evaluate statement block if possible.
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue10, currentNode);
                    if (b->getClause()->next)
                        pushExecute(interpreter, b->getClause()->next.get());
                    break;
                }
                
                b = b->getNext();
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
                ClauseListNode*& b = reinterpret_cast<ClauseListNode*&>(m_nodeStack.peek());
                b = b->getNext();
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
                
                /* JSObjectCallState expects  has a unique calling convention: 
                   Arguments are pushed on the stack in the same order
                   you might expect to pass them to JSObject::call()
                   i.e. func->call(exec, thisObj, args)
                   Thus they are popped here in reverse order. */
                
                JSObject* functionObj = static_cast<JSObject*>(m_valueStack.peek(1)); // left on the stack for use during final cleanup state

                if (functionObj->inherits(&GlobalFuncImp::info) && static_cast<GlobalFuncImp*>(functionObj)->m_functionId == GlobalFuncImp::Eval) {
                    SET_JUMP_STATE(GlobalFuncCallEvalState, currentNode);
                    break;
                } else if (!functionObj->inherits(&DeclaredFunctionImp::info)) {
                    const List& args = m_listStack.pop();
                    JSObject* thisObj = static_cast<JSObject*>(m_valueStack.pop());
                    JSValue* val = functionObj->call(exec, thisObj, args);
                    // we're done, replace the input args
                    REPLACE_RESULT_AND_EVAL_NEXT(val);
                }
                    
                DeclaredFunctionImp* declaredFunction = static_cast<DeclaredFunctionImp*>(functionObj);
                
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

                List args = m_listStack.pop();
                JSObject* thisObj = static_cast<JSObject*>(m_valueStack.pop());

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
                        PUSH_RESULT_AND_EVAL_NEXT(jsUndefined()); // FIXME: This won't actualy exit the function.
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
                
                DeclaredFunctionImp* declaredFunction = static_cast<DeclaredFunctionImp*>(m_valueStack.peek());
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
                        REPLACE_RESULT_AND_EVAL_NEXT(jsUndefined());
                    }
                }
                
                delete newExec->context().imp();
                delete newExec;
                newExec = 0;
                
                JSValue* val;
                bool threw = comp.complType() == Throw;
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
                if (threw)
                    EVALUATE_NEXT(static_cast<ExprNode*>(currentNode)->next());

                REPLACE_RESULT_AND_EVAL_NEXT(val);
            }
                
            case GlobalFuncCallEvalState:
            {
                // See JSObjectCallState for notes on calling convention
                List args = m_listStack.pop();
                m_valueStack.pop(); // ignore passed "thisObj"

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
                        REPLACE_RESULT_AND_EVAL_NEXT(jsUndefined());
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

                REPLACE_RESULT_AND_EVAL_NEXT(comp.value() ? comp.value() : jsUndefined());
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

        if (m_stateStack.size() <= stackBase.stateStackSize)
            break;

        // FIXME: As a loop optimization, we should just peek, instead of pop here
        // Then every non-loop function would pop itself.
        statePair = m_stateStack.pop();
        currentNode = statePair.node;
    
        continue;

    evaluate_next_continue:

        if (exec->hadException()) {
            resetCompletionToNormal();
            setCompletionReturn(Completion(Throw, exec->exception()));
        }
        if (completionReturn().complType() != Normal) {
            unwindToNextBarrier(exec, currentNode);
            if (m_stateStack.size() <= stackBase.stateStackSize)
                break;
        }
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
