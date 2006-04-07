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
    "LastInterpreterState"
};

#define RETURN_COMPLETION(c) do { \
    interpreter->setCompletionReturn(c); \
    goto interpreter_state_switch_end; \
} while (0)

#define RETURN_NORMAL_COMPLETION() do { \
    ASSERT(interpreter->getCompletionReturn().complType() == Normal); \
    goto interpreter_state_switch_end; \
} while (0)

#define KJS_CHECKEXCEPTION() \
if (exec->hadException()) { \
    static_cast<StatementNode*>(currentNode)->setExceptionDetailsIfNeeded(exec); \
    JSValue *ex = exec->exception(); \
    exec->clearException(); \
    RETURN_COMPLETION(Completion(Throw, ex)); \
}

#define KJS_CHECKEXCEPTIONVALUE() \
if (exec->hadException()) { \
    currentNode->setExceptionDetailsIfNeeded(exec); \
        RETURN_VALUE(jsUndefined()); \
} \
if (Collector::isOutOfMemory()) { \
    RETURN_VALUE(jsUndefined()); \
} // will be picked up by KJS_CHECKEXCEPTION() 

#define KJS_CHECKEXCEPTIONLIST() \
if (exec->hadException()) { \
    setExceptionDetailsIfNeeded(exec); \
        return List(); \
} \
if (Collector::isOutOfMemory()) \
    return List(); // will be picked up by KJS_CHECKEXCEPTION()

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

#define SET_VALUE_RETURN(v) \
    interpreter->setValueReturn(v)

// Set this to 1 to see logging.
#define TRACE_EXECUTION 0

#if TRACE_EXECUTION
#define PUSH_LOCAL_VALUE(value) \
    (printf("PUSH %s (%i) called on %p, pushing %p, pre-push depth: %i\n", nameForInterpreterState[statePair.state], statePair.state, statePair.node, value, interpreter->valueStackDepth()), interpreter->pushValueLocal(value))

#define POP_LOCAL_VALUE() (printf("POP %s (%i) called on %p, popping, pre-pop depth: %i\n", nameForInterpreterState[statePair.state], statePair.state, statePair.node, interpreter->valueStackDepth()), assert(stackBase.valueStackSize <= interpreter->valueStackDepth()), interpreter->popValueLocal())

#define RETURN_VALUE(value) \
{\
    JSValue* val = value; \
    printf("RETURN %s (%i) called on %p, returning %p\n", nameForInterpreterState[statePair.state], statePair.state, statePair.node, val); \
    SET_VALUE_RETURN(val); \
    goto interpreter_state_switch_end; \
}

#define SET_JUMP_STATE(nextState, nextNode) \
{ \
    printf("JUMP %s (%i) called on %p, jumping to %s (%i) on %p\n", nameForInterpreterState[statePair.state], statePair.state, statePair.node, nameForInterpreterState[nextState], nextState, nextNode); \
    ASSERT(nextState != statePair.state); \
    interpreter->pushNextState(InterpreterImp::State(nextState, nextNode)); \
}

#else

#define PUSH_LOCAL_VALUE(value) interpreter->pushValueLocal(value)
#define POP_LOCAL_VALUE() (assert(stackBase.valueStackSize <= interpreter->valueStackDepth()), interpreter->popValueLocal())

#define RETURN_VALUE(value) \
{\
    SET_VALUE_RETURN(value); \
    goto interpreter_state_switch_end; \
}

// All code should use SET_CONTINUE_STATE when possible, as it performs additional sanity checks.
#define SET_JUMP_STATE(nextState, nextNode) \
{ \
    ASSERT(nextState != statePair.state); \
    interpreter->pushNextState(InterpreterImp::State(nextState, nextNode)); \
}

#endif

// This can only be used for calling yourself.
#define SET_LOOP_STATE(nextState) \
{ \
    ASSERT(nextState == statePair.state); \
    interpreter->pushNextState(InterpreterImp::State(nextState, currentNode)); \
}


#define PEEK_LOCAL_VALUE() (assert(stackBase.valueStackSize <= interpreter->valueStackDepth()), interpreter->peekValueLocal())

#define PUSH_LIST(list) interpreter->pushListReturn(list)
#define PEEK_LIST() (assert(stackBase.listStackSize <= interpreter->listStackDepth()), interpreter->peekListReturn())
#define POP_LIST() (assert(stackBase.listStackSize <= interpreter->listStackDepth()), interpreter->popListReturn())

#define PUSH_NODE(node) interpreter->pushNodeLocal(node)
#define PEEK_NODE() (assert(stackBase.nodeStackSize <= interpreter->nodeStackDepth()), interpreter->peekNodeLocal())
#define POP_NODE() (assert(stackBase.nodeStackSize <= interpreter->nodeStackDepth()), interpreter->popNodeLocal())

#define RETURN_LIST(list) \
do {\
    PUSH_LIST(list); \
    goto interpreter_state_switch_end; \
} while (0)

#define IS_EVALUATE_STATE(state) \
    (state > InternalErrorState && state < Evaluate_EvaluteList_Boundary)

#define IS_EVALUATE_LIST_STATE(state) \
    (state > Evaluate_EvaluteList_Boundary && state < EvaluateList_Execute_Boundary)

#define IS_EXECUTE_STATE(state) \
    (state > EvaluateList_Execute_Boundary && state < LastInterpreterState)

#define PUSH_EVALUATE(node) \
do { \
    InterpreterState nextState = node->interpreterState(); \
    ASSERT(IS_EVALUATE_STATE(nextState)); \
    interpreter->pushNextState(InterpreterImp::State(nextState, node)); \
} while (0)
    
#define PUSH_EVALUATE_LIST(node) \
do { \
    InterpreterState nextState = node->interpreterState(); \
    ASSERT(IS_EVALUATE_LIST_STATE(nextState)); \
    interpreter->pushNextState(InterpreterImp::State(nextState, node)); \
} while (0)

#define PUSH_EXECUTE(node) \
do { \
    InterpreterState nextState = node->interpreterState(); \
    ASSERT(IS_EXECUTE_STATE(nextState)); \
    interpreter->pushNextState(InterpreterImp::State(nextState, node)); \
} while (0)

// This explicity checks for continue state bugs
// FIXME: This should not take an argument once we have a way to check if currentState+1 == a substate
#define SET_CONTINUE_STATE(nextState) \
do { \
    ASSERT(nextState == statePair.state + 1); \
    SET_JUMP_STATE(nextState, currentNode); \
} while (0)


#define GET_LAST_COMPLETION() \
    interpreter->getCompletionReturn()
    
#define GET_VALUE_RETURN() \
    interpreter->getValueReturn()
    
#define RESET_COMPLETION_TO_NORMAL() \
    interpreter->resetCompletionToNormal()

#define PUSH_UNWIND_BARRIER(barrierType, state) \
    interpreter->pushUnwindBarrier(barrierType, InterpreterImp::State(state, currentNode));

#define POP_UNWIND_BARRIER(type) \
do { \
    ASSERT(interpreter->peekUnwindBarrier().barrierType == type); \
    interpreter->popUnwindBarrier(); \
} while (0)
    
#define EXECUTE_AND_CONTINUE(node, nextState) \
do { \
    SET_CONTINUE_STATE(nextState); \
    PUSH_EXECUTE(node); \
    goto interpreter_state_switch_end; \
} while (0)

#define EXECUTE_AND_JUMP(node, nextState) \
do { \
    SET_JUMP_STATE(nextState, currentNode); \
    PUSH_EXECUTE(node); \
    goto interpreter_state_switch_end; \
} while (0)

// This call can only be used for continuing with the next state
// Use other calls to jump or loop.
#define EVALUATE_AND_CONTINUE(node, nextState) \
do { \
    SET_CONTINUE_STATE(nextState); \
    PUSH_EVALUATE(node); \
    goto interpreter_state_switch_end; \
} while (0)

// All code should use EVALUATE_AND_CONTINUE when possible, as it performs additional sanity checks.
#define EVALUATE_AND_JUMP(node, nextState) \
do { \
    SET_JUMP_STATE(nextState, currentNode); \
    PUSH_EVALUATE(node); \
    goto interpreter_state_switch_end; \
} while (0)

// This call can only be used for continuing with the next state
// Use other calls to jump or loop.
#define EVALUATE_LIST_AND_CONTINUE(node, nextState) \
do { \
    SET_CONTINUE_STATE(nextState); \
    PUSH_EVALUATE_LIST(node); \
    goto interpreter_state_switch_end; \
} while (0)

void runInterpreterLoop(ExecState* exec)
{
    ASSERT(!exec->hadException());
    
    bool debuggerAttached = Debugger::debuggersPresent > 0;

    InterpreterImp* interpreter = exec->dynamicInterpreter()->imp();
    InterpreterImp::UnwindBarrier stackBase = interpreter->peekUnwindBarrier();
    
    while (interpreter->stateStackDepth() > stackBase.stateStackSize) {
    
        // FIXME: As a loop optimization, we should just peek, instead of pop here
        // Then every non-loop function would pop itself.
        InterpreterImp::State statePair = interpreter->popNextState();
        Node* currentNode = statePair.node;
        
#if TRACE_EXECUTION
        printf("EXEC %s (%i) on %p (state state: %i, value stack: %i)\n", nameForInterpreterState[statePair.state], statePair.state, statePair.node, interpreter->stateStackDepth(), interpreter->valueStackDepth());
#endif

        // FIXME: This is wrong, this won't actually return.
        if (debuggerAttached) {
            if (IS_EXECUTE_STATE(statePair.state) && !static_cast<StatementNode*>(currentNode)->hitStatement(exec))
                RETURN_NORMAL_COMPLETION(); // FIXME: Do something the debugger can use
            if (interpreter->debugger() && interpreter->debugger()->imp()->aborted())
                RETURN_NORMAL_COMPLETION(); // FIXME: Do something the debugger can use
        }
        
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
                 
                RETURN_VALUE(resolveNode->throwUndefinedVariableError(exec, resolveNode->ident));
            }
                
            case GroupNodeEvaluateState:
            {
                PUSH_EVALUATE(static_cast<GroupNode*>(currentNode)->group.get());
                break;
            }
                
            case ElementNodeEvaluateState:
            {
                PUSH_LOCAL_VALUE(exec->lexicalInterpreter()->builtinArray()->construct(exec, List::empty()));
                PUSH_LOCAL_VALUE(jsNumber(0));
                // fall through
            }
            case ElementNodeEvaluateState1:
            {
                // This can't use continue, since we fell through above
                EVALUATE_AND_JUMP(static_cast<ElementNode*>(currentNode)->node.get(), ElementNodeEvaluateState2);
            }
            case ElementNodeEvaluateState2:
            {
                ElementNode* n = static_cast<ElementNode*>(currentNode);
                JSValue* val = GET_VALUE_RETURN();
                int l = POP_LOCAL_VALUE()->toInt32(exec);
                JSObject* array = static_cast<JSObject*>(PEEK_LOCAL_VALUE());
                
                l += n->elision;
                array->put(exec, l++, val);
                
                n = n->next.get();
                if (n) {
                    PUSH_LOCAL_VALUE(jsNumber(l));
                    // FIXME: pushing "p" as the current node is dangerous
                    // If this function used current node after this call, currentNode would be wrong.
                    SET_JUMP_STATE(ElementNodeEvaluateState1, n);
                    break;
                }
                RETURN_VALUE(POP_LOCAL_VALUE());
            }
                
            case ArrayNodeEvaluateState:
            {
                ArrayNode* arrayNode = static_cast<ArrayNode*>(currentNode);
                if (arrayNode->element)
                    EVALUATE_AND_CONTINUE(arrayNode->element.get(), ArrayNodeEvaluateState1);
                // fall through
            }
            case ArrayNodeEvaluateState1:
            {
                ArrayNode* arrayNode = static_cast<ArrayNode*>(currentNode);
                JSObject *array;
                int length;
                
                if (arrayNode->element) {
                    array = static_cast<JSObject*>(GET_VALUE_RETURN());
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
                    PUSH_EVALUATE(objectLiteralNode->list.get());
                    break;
                }
                
                RETURN_VALUE(exec->lexicalInterpreter()->builtinObject()->construct(exec, List::empty()));
            }
                
            case PropertyListNodeEvaluateState:
            {
                JSObject* obj = exec->lexicalInterpreter()->builtinObject()->construct(exec, List::empty());
                PUSH_LOCAL_VALUE(obj);
                // fall through
            }
            case PropertyListNodeEvaluateState1:
            {
                // This has to use jump since we fell through above.
                EVALUATE_AND_JUMP(static_cast<PropertyListNode*>(currentNode)->node->name.get(), PropertyListNodeEvaluateState2);
            }
            case PropertyListNodeEvaluateState2:
            {
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<PropertyListNode*>(currentNode)->node->assign.get(), PropertyListNodeEvaluateState3);
            }
            case PropertyListNodeEvaluateState3:
            {
                PropertyListNode* p = static_cast<PropertyListNode*>(currentNode);
                JSValue* v = GET_VALUE_RETURN();
                JSValue* name = POP_LOCAL_VALUE();
                JSObject* obj = static_cast<JSObject*>(PEEK_LOCAL_VALUE());
                
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
                }
                
                RETURN_VALUE(POP_LOCAL_VALUE());
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
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<BracketAccessorNode*>(currentNode)->expr2.get(), BracketAccessorNodeEvaluateState2);
            }
            case BracketAccessorNodeEvaluateState2:
            {
                JSValue *v1 = POP_LOCAL_VALUE();
                JSValue *v2 = GET_VALUE_RETURN();
                JSObject *o = v1->toObject(exec);
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
                RETURN_VALUE(GET_VALUE_RETURN()->toObject(exec)->get(exec, static_cast<DotAccessorNode*>(currentNode)->ident));
            }
                
            case NewExprNodeEvaluateState:
            {
                NewExprNode* newExprNode = static_cast<NewExprNode*>(currentNode);
                SET_CONTINUE_STATE(NewExprNodeEvaluateState1);
                if (newExprNode->args)
                    PUSH_EVALUATE_LIST(newExprNode->args.get());
                
                PUSH_EVALUATE(newExprNode->expr.get());
                break;
            }
            case NewExprNodeEvaluateState1:
            {
                NewExprNode* newExprNode = static_cast<NewExprNode*>(currentNode);
                JSValue* v = GET_VALUE_RETURN();
                
                List argList;
                if (newExprNode->args)
                    argList = POP_LIST();
                
                if (!v->isObject())
                    RETURN_VALUE(newExprNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with new.", v, newExprNode->expr.get()));
                
                JSObject *constr = static_cast<JSObject*>(v);
                if (!constr->implementsConstruct())
                    RETURN_VALUE(newExprNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not a constructor. Cannot be used with new.", v, newExprNode->expr.get()));
                
                RETURN_VALUE(constr->construct(exec, argList));
            }
                
            case FunctionCallValueNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<FunctionCallValueNode*>(currentNode)->expr.get(), FunctionCallValueNodeEvaluateState1);
            }
            case FunctionCallValueNodeEvaluateState1:
            {
                FunctionCallValueNode* functionCallValueNode = static_cast<FunctionCallValueNode*>(currentNode);
                JSValue* v = GET_VALUE_RETURN();
                if (!v->isObject())
                    RETURN_VALUE(functionCallValueNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, functionCallValueNode->expr.get()));
                
                JSObject *func = static_cast<JSObject*>(v);
                if (!func->implementsCall())
                    RETURN_VALUE(functionCallValueNode->throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, functionCallValueNode->expr.get()));
                PUSH_LOCAL_VALUE(func);
                EVALUATE_LIST_AND_CONTINUE(functionCallValueNode->args.get(), FunctionCallValueNodeEvaluateState2);
            }
            case FunctionCallValueNodeEvaluateState2:
            {
                List argList = POP_LIST();
                JSObject *func = static_cast<JSObject*>(POP_LOCAL_VALUE());
                JSObject *thisObj =  exec->dynamicInterpreter()->globalObject();
                RETURN_VALUE(func->call(exec, thisObj, argList));
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
                        KJS_CHECKEXCEPTIONVALUE();
                        
                        if (!v->isObject())
                            RETURN_VALUE(functionCallResolveNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, ident));
                        
                        resolvedFunction = static_cast<JSObject*>(v);
                        
                        if (!resolvedFunction->implementsCall())
                            RETURN_VALUE(functionCallResolveNode->throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, ident));
                        break;
                    }
                    ++iter;
                } while (iter != end);
                
                if (!resolvedFunction)
                    RETURN_VALUE(functionCallResolveNode->throwUndefinedVariableError(exec, ident));
                
                PUSH_LOCAL_VALUE(base);
                PUSH_LOCAL_VALUE(resolvedFunction);
                EVALUATE_LIST_AND_CONTINUE(functionCallResolveNode->args.get(), FunctionCallResolveNodeEvaluateState1);
            }
            case FunctionCallResolveNodeEvaluateState1:
            {
                List argList = POP_LIST();
                JSObject* func = static_cast<JSObject*>(POP_LOCAL_VALUE());
                JSObject* thisObj = static_cast<JSObject*>(POP_LOCAL_VALUE());
                // ECMA 11.2.3 says that in this situation the this value should be null.
                // However, section 10.2.3 says that in the case where the value provided
                // by the caller is null, the global object should be used. It also says
                // that the section does not apply to interal functions, but for simplicity
                // of implementation we use the global object anyway here. This guarantees
                // that in host objects you always get a valid object for this.
                if (thisObj->isActivation())
                    thisObj = exec->dynamicInterpreter()->globalObject();
                
                RETURN_VALUE(func->call(exec, thisObj, argList));
            }
                
            case FunctionCallBracketNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<FunctionCallBracketNode*>(currentNode)->base.get(), FunctionCallBracketNodeEvaluateState1);
            }
            case FunctionCallBracketNodeEvaluateState1:
            {
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<FunctionCallBracketNode*>(currentNode)->subscript.get(), FunctionCallBracketNodeEvaluateState2);
            }
            case FunctionCallBracketNodeEvaluateState2:
            {
                FunctionCallBracketNode* functionCallBracketNode = static_cast<FunctionCallBracketNode*>(currentNode);
                
                JSValue *subscriptVal = GET_VALUE_RETURN();
                JSObject *baseObj = PEEK_LOCAL_VALUE()->toObject(exec);
                
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
                
                KJS_CHECKEXCEPTIONVALUE();
                
                if (!funcVal->isObject())
                    RETURN_VALUE(functionCallBracketNode->throwError(exec, TypeError, "Value %s (result of expression %s[%s]) is not object.", funcVal, functionCallBracketNode->base.get(), functionCallBracketNode->subscript.get()));
                
                JSObject *func = static_cast<JSObject*>(funcVal);
                
                if (!func->implementsCall())
                    RETURN_VALUE(functionCallBracketNode->throwError(exec, TypeError, "Object %s (result of expression %s[%s]) does not allow calls.", funcVal, functionCallBracketNode->base.get(), functionCallBracketNode->subscript.get()));
            
                PUSH_LOCAL_VALUE(func);
                EVALUATE_LIST_AND_CONTINUE(functionCallBracketNode->args.get(), FunctionCallBracketNodeEvaluateState3);
            }
            case FunctionCallBracketNodeEvaluateState3:
            {
                List argList = POP_LIST();
                JSObject *func = static_cast<JSObject*>(POP_LOCAL_VALUE());
                JSObject *thisObj = POP_LOCAL_VALUE()->toObject(exec);
                assert(thisObj);
                assert(thisObj->isObject());
                assert(!thisObj->isActivation());
                
                RETURN_VALUE(func->call(exec, thisObj, argList));
            }
            
            case FunctionCallDotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<FunctionCallDotNode*>(currentNode)->base.get(), FunctionCallDotNodeEvaluateState1);
            }
            case FunctionCallDotNodeEvaluateState1:
            {
                FunctionCallDotNode* functionCallDotNode = static_cast<FunctionCallDotNode*>(currentNode);
                JSObject* baseObj = GET_VALUE_RETURN()->toObject(exec);
                PropertySlot slot;
                const Identifier& ident = functionCallDotNode->ident;
                JSValue* funcVal = baseObj->getPropertySlot(exec, ident, slot) ? slot.getValue(exec, baseObj, ident) : jsUndefined();
                KJS_CHECKEXCEPTIONVALUE();
                
                if (!funcVal->isObject())
                    RETURN_VALUE(functionCallDotNode->throwError(exec, TypeError, dotExprNotAnObjectString(), funcVal, functionCallDotNode->base.get(), ident));
                
                JSObject* func = static_cast<JSObject*>(funcVal);
                
                if (!func->implementsCall())
                    RETURN_VALUE(functionCallDotNode->throwError(exec, TypeError, dotExprDoesNotAllowCallsString(), funcVal, functionCallDotNode->base.get(), ident));
                PUSH_LOCAL_VALUE(baseObj);
                PUSH_LOCAL_VALUE(func);
                EVALUATE_LIST_AND_CONTINUE(functionCallDotNode->args.get(), FunctionCallDotNodeEvaluateState2);
            }
            case FunctionCallDotNodeEvaluateState2:
            {
                List argList = POP_LIST();
                JSObject* func = static_cast<JSObject*>(POP_LOCAL_VALUE());
                JSObject *thisObj = POP_LOCAL_VALUE()->toObject(exec);
                assert(thisObj);
                assert(thisObj->isObject());
                assert(!thisObj->isActivation());
                
                RETURN_VALUE(func->call(exec, thisObj, argList));
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
                        
                        double newValue = (postfixResolveNode->m_oper == OpPlusPlus) ? n + 1 : n - 1;
                        base->put(exec, ident, jsNumber(newValue));
                        
                        RETURN_VALUE(jsNumber(n));
                    }
                    
                    ++iter;
                } while (iter != end);
                         
                RETURN_VALUE(postfixResolveNode->throwUndefinedVariableError(exec, ident));
            }
                

            case PostfixBracketNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<PostfixBracketNode*>(currentNode)->m_base.get(), PostfixBracketNodeEvaluateState1);
            }
            case PostfixBracketNodeEvaluateState1:
            {
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<PostfixBracketNode*>(currentNode)->m_subscript.get(), PostfixBracketNodeEvaluateState2);
            }
            case PostfixBracketNodeEvaluateState2:
            {
                PostfixBracketNode* postfixBracketNode = static_cast<PostfixBracketNode*>(currentNode);
                
                JSValue *subscript = GET_VALUE_RETURN();
                JSObject *base = POP_LOCAL_VALUE()->toObject(exec);
                
                uint32_t propertyIndex;
                if (subscript->getUInt32(propertyIndex)) {
                    PropertySlot slot;
                    JSValue *v = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
                    KJS_CHECKEXCEPTIONVALUE();
                    
                    double n = v->toNumber(exec);
                    
                    double newValue = (postfixBracketNode->m_oper == OpPlusPlus) ? n + 1 : n - 1;
                    base->put(exec, propertyIndex, jsNumber(newValue));
                    
                    RETURN_VALUE(jsNumber(n));
                }
                
                Identifier propertyName(subscript->toString(exec));
                PropertySlot slot;
                JSValue *v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
                KJS_CHECKEXCEPTIONVALUE();
                
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
                JSObject* base = GET_VALUE_RETURN()->toObject(exec);
                
                PropertySlot slot;
                JSValue *v = base->getPropertySlot(exec, postFixDotNode->m_ident, slot) ? slot.getValue(exec, base, postFixDotNode->m_ident) : jsUndefined();
                KJS_CHECKEXCEPTIONVALUE();
                
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
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<DeleteBracketNode*>(currentNode)->m_subscript.get(), DeleteBracketNodeEvaluateState2);
            }
            case DeleteBracketNodeEvaluateState2:
            {
                JSValue *subscript = GET_VALUE_RETURN();
                JSObject *base = POP_LOCAL_VALUE()->toObject(exec);
                
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
                JSObject *base = GET_VALUE_RETURN()->toObject(exec);                
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
                RETURN_VALUE(typeStringForValue(GET_VALUE_RETURN()));
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
                
                RETURN_VALUE(prefixResolveNode->throwUndefinedVariableError(exec, ident));
            }
                
                
            case PrefixBracketNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<PrefixBracketNode*>(currentNode)->m_base.get(), PrefixBracketNodeEvaluateState1);
            }
            case PrefixBracketNodeEvaluateState1:
            {
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<PrefixBracketNode*>(currentNode)->m_subscript.get(), PrefixBracketNodeEvaluateState2);
            }
            case PrefixBracketNodeEvaluateState2:
            {
                PrefixBracketNode* prefixBracketNode = static_cast<PrefixBracketNode*>(currentNode);
                Operator oper = prefixBracketNode->m_oper;
                JSValue *subscript = GET_VALUE_RETURN();
                JSObject *base = POP_LOCAL_VALUE()->toObject(exec);
                
                uint32_t propertyIndex;
                if (subscript->getUInt32(propertyIndex)) {
                    PropertySlot slot;
                    JSValue *v = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
                    KJS_CHECKEXCEPTIONVALUE();
                    
                    double n = v->toNumber(exec);
                    
                    double newValue = (oper == OpPlusPlus) ? n + 1 : n - 1;
                    JSValue *n2 = jsNumber(newValue);
                    base->put(exec, propertyIndex, n2);
                    
                    RETURN_VALUE(n2);
                }
                
                Identifier propertyName(subscript->toString(exec));
                PropertySlot slot;
                JSValue *v = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
                KJS_CHECKEXCEPTIONVALUE();
                
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
                JSObject *base = GET_VALUE_RETURN()->toObject(exec);
                PrefixDotNode* prefixDotNode = static_cast<PrefixDotNode*>(currentNode);
                const Identifier& ident = prefixDotNode->m_ident;
                
                PropertySlot slot;
                JSValue *v = base->getPropertySlot(exec, ident, slot) ? slot.getValue(exec, base, ident) : jsUndefined();
                KJS_CHECKEXCEPTIONVALUE();
                
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
                double value = GET_VALUE_RETURN()->toNumber(exec);
                RETURN_VALUE(jsNumber(value));
            }

            case NegateNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<NegateNode*>(currentNode)->expr.get(), NegateNodeEvaluateState1);
            }
            case NegateNodeEvaluateState1:
            {
                double value = GET_VALUE_RETURN()->toNumber(exec);
                RETURN_VALUE(jsNumber(-value));
            }
            
            case BitwiseNotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<BitwiseNotNode*>(currentNode)->expr.get(), BitwiseNotNodeEvaluateState1);
            }
            case BitwiseNotNodeEvaluateState1:
            {
                int32_t value = GET_VALUE_RETURN()->toInt32(exec);
                RETURN_VALUE(jsNumber(~value));
            }
                
            case LogicalNotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<LogicalNotNode*>(currentNode)->expr.get(), LogicalNotNodeEvaluateState1);
            }
            case LogicalNotNodeEvaluateState1:
            {
                bool value = GET_VALUE_RETURN()->toBoolean(exec);
                RETURN_VALUE(jsBoolean(!value));
            }
            
            case MultNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<MultNode*>(currentNode)->term1.get(), MultNodeEvaluateState1);
            }
            case MultNodeEvaluateState1:
            {
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<MultNode*>(currentNode)->term2.get(), MultNodeEvaluateState2);
            }
            case MultNodeEvaluateState2:
            {
                RETURN_VALUE(mult(exec, POP_LOCAL_VALUE(), GET_VALUE_RETURN(), static_cast<MultNode*>(currentNode)->oper));
            }
            
            case AddNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<AddNode*>(currentNode)->term1.get(), AddNodeEvaluateState1);
            }
            case AddNodeEvaluateState1:
            {
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<AddNode*>(currentNode)->term2.get(), AddNodeEvaluateState2);
            }
            case AddNodeEvaluateState2:
            {
                RETURN_VALUE(add(exec, POP_LOCAL_VALUE(), GET_VALUE_RETURN(), static_cast<AddNode*>(currentNode)->oper));
            }
            
            case ShiftNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<ShiftNode*>(currentNode)->term1.get(), ShiftNodeEvaluateState1);
            }
            case ShiftNodeEvaluateState1:
            {
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<ShiftNode*>(currentNode)->term2.get(), ShiftNodeEvaluateState2);
            }
            case ShiftNodeEvaluateState2:
            {
                JSValue *v2 = GET_VALUE_RETURN();
                JSValue *v1 = POP_LOCAL_VALUE();
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
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<RelationalNode*>(currentNode)->expr2.get(), RelationalNodeEvaluateState2);
            }
            case RelationalNodeEvaluateState2:
            {
                RelationalNode* relationalNode = static_cast<RelationalNode*>(currentNode);
                JSValue *v2 = GET_VALUE_RETURN();
                JSValue *v1 = POP_LOCAL_VALUE();
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
                        RETURN_VALUE(relationalNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with IN expression.", v2, relationalNode->expr2.get()));
                    JSObject *o2(static_cast<JSObject*>(v2));
                    b = o2->hasProperty(exec, Identifier(v1->toString(exec)));
                } else {
                    if (!v2->isObject())
                        RETURN_VALUE(relationalNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not an object. Cannot be used with instanceof operator.", v2, relationalNode->expr2.get()));
                    
                    JSObject *o2(static_cast<JSObject*>(v2));
                    if (!o2->implementsHasInstance()) {
                        // According to the spec, only some types of objects "implement" the [[HasInstance]] property.
                        // But we are supposed to throw an exception where the object does not "have" the [[HasInstance]]
                        // property. It seems that all object have the property, but not all implement it, so in this
                        // case we return false (consistent with mozilla)
                        RETURN_VALUE(jsBoolean(false));
                        //      return throwError(exec, TypeError,
                        //			"Object does not implement the [[HasInstance]] method." );
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
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<EqualNode*>(currentNode)->expr2.get(), EqualNodeEvaluateState2);
            }
            case EqualNodeEvaluateState2:
            {
                JSValue *v2 = GET_VALUE_RETURN();
                JSValue *v1 = POP_LOCAL_VALUE();
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
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<BitOperNode*>(currentNode)->expr2.get(), BitOperNodeEvaluateState2);
            }
            case BitOperNodeEvaluateState2:
            {
                JSValue *v2 = GET_VALUE_RETURN();
                JSValue *v1 = POP_LOCAL_VALUE();
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
                JSValue* v1 = GET_VALUE_RETURN();
                BinaryLogicalNode* binaryLogicalNode = static_cast<BinaryLogicalNode*>(currentNode);
                Operator oper = binaryLogicalNode->oper;
                bool b1 = v1->toBoolean(exec);
                if ((!b1 && oper == OpAnd) || (b1 && oper == OpOr))
                    RETURN_VALUE(v1);
                PUSH_EVALUATE(binaryLogicalNode->expr2.get());
                break;
            }
            
            case ConditionalNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<ConditionalNode*>(currentNode)->logical.get(), ConditionalNodeEvaluateState1);
            }
            case ConditionalNodeEvaluateState1:
            {
                JSValue *v = GET_VALUE_RETURN();
                ConditionalNode* conditionalNode = static_cast<ConditionalNode*>(currentNode);
                RefPtr<Node> node = v->toBoolean(exec) ? conditionalNode->expr1 : conditionalNode->expr2;
                PUSH_EVALUATE(node.get());
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
                    RETURN_VALUE(assignResolveNode->throwUndefinedVariableError(exec, ident));
                
                PUSH_LOCAL_VALUE(base);
                EVALUATE_AND_CONTINUE(assignResolveNode->m_right.get(), AssignResolveNodeEvaluateState1);
            }
            case AssignResolveNodeEvaluateState1:
            {
                AssignResolveNode *assignResolveNode = static_cast<AssignResolveNode*>(currentNode);
                Operator oper = assignResolveNode->m_oper;
                const Identifier& ident = assignResolveNode->m_ident;
                JSValue* v = GET_VALUE_RETURN();
                JSObject* base = static_cast<JSObject*>(POP_LOCAL_VALUE());
                // FIXME: We could avoid this slot double-lookup by having a slot stack.
                PropertySlot slot;
                base->getPropertySlot(exec, ident, slot);
                if (oper != OpEqual) {
                    JSValue* v1 = slot.getValue(exec, base, ident);
                    KJS_CHECKEXCEPTIONVALUE();
                    v = valueForReadModifyAssignment(exec, v1, v, oper);
                }
                KJS_CHECKEXCEPTIONVALUE();
                
                base->put(exec, ident, v);
                RETURN_VALUE(v);
            }
            
            
            case AssignDotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<AssignDotNode*>(currentNode)->m_base.get(), AssignDotNodeEvaluateState1);
            }
            case AssignDotNodeEvaluateState1:
            {
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                AssignDotNode* assignDotNode = static_cast<AssignDotNode*>(currentNode);
                if (assignDotNode->m_oper == OpEqual)
                    EVALUATE_AND_JUMP(assignDotNode->m_right.get(), AssignDotNodeEvaluateState3);
                else
                    EVALUATE_AND_CONTINUE(assignDotNode->m_right.get(), AssignDotNodeEvaluateState2);
            }
            case AssignDotNodeEvaluateState2:
            {
                AssignDotNode* assignDotNode = static_cast<AssignDotNode*>(currentNode);
                JSValue* v2 = GET_VALUE_RETURN();
                JSObject* base = PEEK_LOCAL_VALUE()->toObject(exec);
                PropertySlot slot;
                JSValue *v1 = base->getPropertySlot(exec, assignDotNode->m_ident, slot) ? slot.getValue(exec, base, assignDotNode->m_ident) : jsUndefined();
                KJS_CHECKEXCEPTIONVALUE();
                
                SET_VALUE_RETURN(valueForReadModifyAssignment(exec, v1, v2, assignDotNode->m_oper));
                // fall through
            }
            case AssignDotNodeEvaluateState3:
            {
                AssignDotNode* assignDotNode = static_cast<AssignDotNode*>(currentNode);
                JSValue* v = GET_VALUE_RETURN();
                JSObject* base = POP_LOCAL_VALUE()->toObject(exec);
                base->put(exec, assignDotNode->m_ident, v);
                RETURN_VALUE(v);
            }
                
            
            case AssignBracketNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<AssignBracketNode*>(currentNode)->m_base.get(), AssignBracketNodeEvaluateState1);
            }
            case AssignBracketNodeEvaluateState1:
            {
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<AssignBracketNode*>(currentNode)->m_subscript.get(), AssignBracketNodeEvaluateState2);
            }
            case AssignBracketNodeEvaluateState2:
            {
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                EVALUATE_AND_CONTINUE(static_cast<AssignBracketNode*>(currentNode)->m_right.get(), AssignBracketNodeEvaluateState3);
            }
            case AssignBracketNodeEvaluateState3:
            {
                AssignBracketNode* assignBracketNode = static_cast<AssignBracketNode*>(currentNode);
                JSValue* v = GET_VALUE_RETURN();
                JSValue* subscript = POP_LOCAL_VALUE();            
                JSObject* base = POP_LOCAL_VALUE()->toObject(exec);
                Operator oper = assignBracketNode->m_oper;
                
                uint32_t propertyIndex;
                if (subscript->getUInt32(propertyIndex)) {
                    if (oper != OpEqual) {
                        PropertySlot slot;
                        JSValue *v1 = base->getPropertySlot(exec, propertyIndex, slot) ? slot.getValue(exec, base, propertyIndex) : jsUndefined();
                        KJS_CHECKEXCEPTIONVALUE();
                        v = valueForReadModifyAssignment(exec, v1, v, oper);
                    }
                    KJS_CHECKEXCEPTIONVALUE();
                    
                    base->put(exec, propertyIndex, v);
                    RETURN_VALUE(v);
                }
                
                Identifier propertyName(subscript->toString(exec));
                
                if (oper != OpEqual) {
                    PropertySlot slot;
                    JSValue *v1 = base->getPropertySlot(exec, propertyName, slot) ? slot.getValue(exec, base, propertyName) : jsUndefined();
                    KJS_CHECKEXCEPTIONVALUE();
                    v = valueForReadModifyAssignment(exec, v1, v, oper);
                }
                KJS_CHECKEXCEPTIONVALUE();
                
                base->put(exec, propertyName, v);
                RETURN_VALUE(v);
            }
                
                
            case CommaNodeEvaluateState:
            {
                PUSH_EVALUATE(static_cast<CommaNode*>(currentNode)->expr2.get());
                PUSH_EVALUATE(static_cast<CommaNode*>(currentNode)->expr1.get()); // ignore the first return value
            }
            
            case AssignExprNodeEvaluateState:
            {
                PUSH_EVALUATE(static_cast<AssignExprNode*>(currentNode)->expr.get());
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
                        RETURN_VALUE((JSValue*)0);
                    SET_VALUE_RETURN(jsUndefined());
                }
                // fall through
            }
            case VarDeclNodeEvaluateState1:
            {
                VarDeclNode* varDecl = static_cast<VarDeclNode*>(currentNode);
                JSValue* val = GET_VALUE_RETURN();
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
                PUSH_EVALUATE(static_cast<CaseClauseNode*>(currentNode)->expr.get());
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
                
                FunctionImp *func = new DeclaredFunctionImp(exec, funcExprNode->ident, funcExprNode->body.get(), context->scopeChain());
                JSObject *proto = exec->lexicalInterpreter()->builtinObject()->construct(exec, List::empty());
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
                PUSH_LIST(List());
                // fall through
            }
            case ArgumentListNodeEvaluateListState1:
            {
                // This has to jump due to the fall through above.
                EVALUATE_AND_JUMP(static_cast<ArgumentListNode*>(currentNode)->expr.get(), ArgumentListNodeEvaluateListState2);
            }
            case ArgumentListNodeEvaluateListState2:
            {
                ArgumentListNode* n = static_cast<ArgumentListNode*>(currentNode);
                JSValue* v = GET_VALUE_RETURN();
                List& l = PEEK_LIST();
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
                    PUSH_EVALUATE_LIST(argumentsNode->list.get());
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
                PUSH_EVALUATE(static_cast<VarStatementNode*>(currentNode)->next.get());
                break;
            }

            case BlockNodeExecuteState:
            {
                BlockNode* blockNode = static_cast<BlockNode*>(currentNode);
                if (!blockNode->source)
                    RETURN_NORMAL_COMPLETION();
                
                blockNode->source->processFuncDecl(exec);
                PUSH_EXECUTE(blockNode->source.get());
                break;
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
                RETURN_COMPLETION(Completion(Normal, GET_VALUE_RETURN()));
            }

            case IfNodeExecuteState:
            {                
                EVALUATE_AND_CONTINUE(static_cast<IfNode*>(currentNode)->expr.get(), IfNodeExecuteState1);
            }
            case IfNodeExecuteState1:
            {
                IfNode* ifNode = static_cast<IfNode*>(currentNode);
                if (GET_VALUE_RETURN()->toBoolean(exec)) {
                    PUSH_EXECUTE(ifNode->statement1.get());
                    break;
                }
                
                // no else
                if (!ifNode->statement2)
                    RETURN_NORMAL_COMPLETION();
                
                PUSH_EXECUTE(ifNode->statement2.get());
                break;
            }

            case DoWhileNodeExecuteState:
            {
                PUSH_UNWIND_BARRIER(Break, DoWhileNodeExecuteEndState);
                PUSH_UNWIND_BARRIER(Continue, DoWhileNodeExecuteContinueState);
                EXECUTE_AND_CONTINUE(static_cast<DoWhileNode*>(currentNode)->statement.get(), DoWhileNodeExecuteBodyState);
            }
                
            case DoWhileNodeExecuteContinueState:
            {
                ASSERT(GET_LAST_COMPLETION().complType() == Continue);
                RESET_COMPLETION_TO_NORMAL();
                
                // fall through
            }
                
            case DoWhileNodeExecuteTestState:
            {
                EVALUATE_AND_JUMP(static_cast<DoWhileNode*>(currentNode)->expr.get(), DoWhileNodeExecuteBodyState);
            }
                
            case DoWhileNodeExecuteBodyState:
            {
                if (GET_VALUE_RETURN()->toBoolean(exec))
                    EXECUTE_AND_CONTINUE(static_cast<DoWhileNode*>(currentNode)->statement.get(), DoWhileNodeExecuteTestState);

                // fall through if the test returns false
            }
                
            case DoWhileNodeExecuteEndState:
            {
                POP_UNWIND_BARRIER(Continue);
                POP_UNWIND_BARRIER(Break);
                
                ComplType complType = GET_LAST_COMPLETION().complType();
                if (complType != Normal) {
                    ASSERT(complType == Break);
                    RESET_COMPLETION_TO_NORMAL();
                }
                
                break;
            }
                
            case WhileNodeExecuteState:
            {
                PUSH_UNWIND_BARRIER(Break, WhileNodeExecuteEndState);
                PUSH_UNWIND_BARRIER(Continue, WhileNodeExecuteContinueState);

                // fall through
            }
                
            case WhileNodeExecuteContinueState:
            {
                ASSERT(GET_LAST_COMPLETION().complType() == Normal || GET_LAST_COMPLETION().complType() == Continue);
                RESET_COMPLETION_TO_NORMAL();
                
                // fall through
            }
                
            case WhileNodeExecuteTestState:
            {
                EVALUATE_AND_JUMP(static_cast<WhileNode*>(currentNode)->expr.get(), WhileNodeExecuteBodyState);
            }
                
            case WhileNodeExecuteBodyState:
            {
                if (GET_VALUE_RETURN()->toBoolean(exec))
                    EXECUTE_AND_JUMP(static_cast<WhileNode*>(currentNode)->statement.get(), WhileNodeExecuteTestState);

                // fall through if the test returns false
            }
                
            case WhileNodeExecuteEndState:
            {
                POP_UNWIND_BARRIER(Continue);
                POP_UNWIND_BARRIER(Break);

                ComplType complType = GET_LAST_COMPLETION().complType();
                if (complType != Normal) {
                    ASSERT(complType == Break);
                    RESET_COMPLETION_TO_NORMAL();
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
            }
                
            case ForNodeExecuteTestState:
            {
                ForNode* forNode = static_cast<ForNode*>(currentNode);
                if (forNode->expr2)
                    EVALUATE_AND_JUMP(forNode->expr2.get(), ForNodeExecuteBodyState);
                    
                // fall through if there's no test statement
            }
            
            case ForNodeExecuteBodyState:
            {
                ForNode* forNode = static_cast<ForNode*>(currentNode);

                if (forNode->expr2 && !GET_VALUE_RETURN()->toBoolean(exec)) {
                    SET_JUMP_STATE(ForNodeExecuteEndState, forNode);
                    break;
                }
                
                EXECUTE_AND_JUMP(forNode->statement.get(), ForNodeExecutePostBodyState);
            }
                
            case ForNodeExecuteContinueState:
            {
                ASSERT(GET_LAST_COMPLETION().complType() == Normal || GET_LAST_COMPLETION().complType() == Continue);
                RESET_COMPLETION_TO_NORMAL();
                
                // fall through
            }
                
            case ForNodeExecutePostBodyState:
            {
                ForNode* forNode = static_cast<ForNode*>(currentNode);

                SET_JUMP_STATE(ForNodeExecuteTestState, forNode);
                if (forNode->expr3)
                    PUSH_EVALUATE(forNode->expr3.get());
                break;
            }
                
            case ForNodeExecuteEndState:
            {
                POP_UNWIND_BARRIER(Continue);
                POP_UNWIND_BARRIER(Break);
                
                ComplType complType = GET_LAST_COMPLETION().complType();
                if (complType != Normal) {
                    ASSERT(complType == Break);
                    RESET_COMPLETION_TO_NORMAL();
                }
                
                break;
            }
        
            case ForInNodeExecuteState:
            {
                PUSH_UNWIND_BARRIER(Break, ForInNodeExecuteBreakState);

                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                PUSH_EVALUATE(forInNode->expr.get());
                if (VarDeclNode* varDeclNode = forInNode->varDecl.get())
                    PUSH_EVALUATE(varDeclNode);
                SET_CONTINUE_STATE(ForInNodeExecuteState2);
                break;
            }

            case ForInNodeExecuteState2:
            {
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                JSValue* e = GET_VALUE_RETURN();
                
                // Null and Undefined will throw if you call toObject on them
                if (e->isUndefinedOrNull())
                    RETURN_NORMAL_COMPLETION();
                    
                InterpreterState interpreterState;
                if (forInNode->lexpr->isResolveNode())
                    interpreterState = ForInNodeResolveNodeExecuteState;
                else if (forInNode->lexpr->isDotAccessorNode())
                    interpreterState = ForInNodeDotAccessorNodeExecuteState;
                else {
                    ASSERT(forInNode->lexpr->isBracketAccessorNode());
                    interpreterState = ForInNodeBracketAccessorNodeExecuteState;
                }
                    
                // FIXME: Continue state somewhere around here
                
                JSObject *o = e->toObject(exec);
                ReferenceList propList = o->propList(exec);
                ReferenceListIterator propIt = propList.begin();
                while (propIt != propList.end()) {
                    Identifier name = propIt->getPropertyName(exec);
                    if (o->hasProperty(exec, name)) {
                        PUSH_LOCAL_VALUE(jsString(name.ustring()));
                        SET_JUMP_STATE(interpreterState, forInNode);
                    }
                    
                    propIt++;
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
                
                JSValue* str = POP_LOCAL_VALUE();
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
                JSObject* base = GET_VALUE_RETURN()->toObject(exec);
                JSValue* str = POP_LOCAL_VALUE();
                
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
                PUSH_LOCAL_VALUE(GET_VALUE_RETURN());
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                EVALUATE_AND_CONTINUE(static_cast<BracketAccessorNode*>(forInNode->lexpr.get())->subscript(), ForInNodeBracketAccessorNodeExecuteState2);
            }
            case ForInNodeBracketAccessorNodeExecuteState2:
            {
                JSObject* base = POP_LOCAL_VALUE()->toObject(exec);
                JSValue* subscript = GET_VALUE_RETURN();
                JSValue* str = POP_LOCAL_VALUE();
                
                uint32_t i;
                if (subscript->getUInt32(i)) {
                    base->put(exec, i, str);
                    base->put(exec, Identifier(subscript->toString(exec)), str);
                }

                SET_JUMP_STATE(ForInNodeExecuteBodyState, currentNode);
                break; // could just fall through here, but better to be explicit and consistent
            }
            
            case ForInNodeExecuteBodyState:
            {
                PUSH_UNWIND_BARRIER(Continue, ForInNodeExecuteContinueState);
                ForInNode* forInNode = static_cast<ForInNode*>(currentNode);
                EXECUTE_AND_CONTINUE(forInNode->statement.get(), ForInNodeExecuteContinueState);
                break;
            }

            case ForInNodeExecuteContinueState:
            {
                ASSERT(GET_LAST_COMPLETION().complType() == Normal || GET_LAST_COMPLETION().complType() == Continue);

                RESET_COMPLETION_TO_NORMAL();
                POP_UNWIND_BARRIER(Continue);
                break;
            }
            
            case ForInNodeExecuteBreakState:
            {
                ASSERT(GET_LAST_COMPLETION().complType() == Break);
                
                RESET_COMPLETION_TO_NORMAL();
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
                if (codeType != FunctionCode && codeType != AnonymousCode)
                    RETURN_COMPLETION(returnNode->createErrorCompletion(exec, SyntaxError, "Invalid return statement."));
                
                if (!returnNode->value)
                    RETURN_COMPLETION(Completion(ReturnValue, jsUndefined()));
                
                EVALUATE_AND_CONTINUE(returnNode->value.get(), ReturnNodeExecuteState1);
            }
            case ReturnNodeExecuteState1:
            {
                RETURN_COMPLETION(Completion(ReturnValue, GET_VALUE_RETURN()));
            }

            case WithNodeExecuteState:
            {
                EVALUATE_AND_CONTINUE(static_cast<WithNode*>(currentNode)->expr.get(), WithNodeExecuteState1);
            }
            case WithNodeExecuteState1:
            {
                WithNode* withNode = static_cast<WithNode*>(currentNode);
                JSValue *v = GET_VALUE_RETURN();
                JSObject *o = v->toObject(exec);
                KJS_CHECKEXCEPTIONVALUE();
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
                // no need to push argument, block will use the one returned from the original evaluate
                ASSERT(static_cast<SwitchNode*>(currentNode)->block->interpreterState() == CaseBlockNodeExecuteBlockWithInputValue);
                PUSH_EXECUTE(static_cast<SwitchNode*>(currentNode)->block.get());
                break;
            }
            case SwitchNodeExecuteState2:
            {
                POP_UNWIND_BARRIER(Break);
                Completion res = GET_LAST_COMPLETION();
                
                if ((res.complType() == Break) && static_cast<SwitchNode*>(currentNode)->ls.contains(res.target())) {
                    RESET_COMPLETION_TO_NORMAL();
                    RETURN_COMPLETION(Completion(Normal, res.value()));
                }
                RETURN_COMPLETION(res);
            }

            case LabelNodeExecuteState:
            {                
                PUSH_UNWIND_BARRIER(Break, LabelNodeExecuteState1);
                SET_CONTINUE_STATE(LabelNodeExecuteState1);
                PUSH_EXECUTE(static_cast<LabelNode*>(currentNode)->statement.get());
                break;
            }
            case LabelNodeExecuteState1:
            {
                POP_UNWIND_BARRIER(Break);
                Completion e = GET_LAST_COMPLETION();
                
                if ((e.complType() == Break) && (e.target() == static_cast<LabelNode*>(currentNode)->label)) {
                    RESET_COMPLETION_TO_NORMAL();
                    RETURN_COMPLETION(Completion(Normal, e.value()));
                }
                RETURN_COMPLETION(e);
            }

            case ThrowNodeExecuteState:
            {
                EVALUATE_AND_CONTINUE(static_cast<ThrowNode*>(currentNode)->expr.get(), ThrowNodeExecuteState1);
            }
            case ThrowNodeExecuteState1:
            {
                RETURN_COMPLETION(Completion(Throw, GET_VALUE_RETURN()));
            }

            case TryNodeExecuteState:
            {
                SET_CONTINUE_STATE(TryNodeExecuteState1);
                PUSH_UNWIND_BARRIER(Throw, TryNodeExecuteState1);
                PUSH_EXECUTE(static_cast<TryNode*>(currentNode)->tryBlock.get());
                break;
            }

            case TryNodeExecuteState1:
            {
                TryNode* tryNode = static_cast<TryNode*>(currentNode);
                POP_UNWIND_BARRIER(Throw);
                Completion c = GET_LAST_COMPLETION();
                if (tryNode->catchBlock && c.complType() == Throw) {
                    RESET_COMPLETION_TO_NORMAL();
                    JSObject *obj = new JSObject;
                    obj->put(exec, tryNode->exceptionIdent, c.value(), DontDelete);
                    SET_CONTINUE_STATE(TryNodeExecuteState2);
                    PUSH_UNWIND_BARRIER(Scope, InternalErrorState);
                    exec->context().imp()->pushScope(obj);
                    PUSH_EXECUTE(tryNode->catchBlock.get());
                    break;
                }
                if (tryNode->finallyBlock)
                    PUSH_EXECUTE(tryNode->finallyBlock.get());
                break;
            }
            case TryNodeExecuteState2:
            {
                // This case is only hit after successfully executing a catch block
                exec->context().imp()->popScope();
                POP_UNWIND_BARRIER(Scope);
                
                TryNode* tryNode = static_cast<TryNode*>(currentNode);
                if (tryNode->finallyBlock)
                    PUSH_EXECUTE(tryNode->finallyBlock.get());
                break;
            }

            case FuncDeclNodeExecuteState:
            {
                RETURN_NORMAL_COMPLETION();
            }

            case SourceElementsNodeExecuteState:
            {
                // Have to use jump because of fall through above.
                EXECUTE_AND_JUMP(static_cast<SourceElementsNode*>(currentNode)->node.get(), SourceElementsNodeExecuteState2);
            }
            case SourceElementsNodeExecuteState2:
            {
                Completion c = GET_LAST_COMPLETION();
                SourceElementsNode* n = static_cast<SourceElementsNode*>(currentNode);
                if ((n = n->next.get())) {
                    // FIXME, with a little more trickery this "method" could be only 1 state.
                    SET_JUMP_STATE(SourceElementsNodeExecuteState, n);
                    break;
                }
                
                RETURN_NORMAL_COMPLETION();
            }
                
            case CaseBlockNodeExecuteBlockWithInputValue:
            {
                // FIXME: This is the only execute function which takes an argument!
                // This was originally called CaseBlockNode::evalBlock but was renamed
                // CaseBlockNodeExecuteBlock to indicate the completion return
                
                CaseBlockNode* caseBlockNode = static_cast<CaseBlockNode*>(currentNode);
                // Skip over "a" loops to "b" loops if possible
                if (!caseBlockNode->list1) {
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue5, currentNode);
                    break;
                }
                
                PUSH_NODE(caseBlockNode->list1.get());
            }
            case CaseBlockNodeExecuteBlockWithInputValue1: // top of first "a" loop
            {
                ClauseListNode* a = static_cast<ClauseListNode*>(PEEK_NODE());
                EVALUATE_AND_CONTINUE(a->getClause(), CaseBlockNodeExecuteBlockWithInputValue2);
            }
            case CaseBlockNodeExecuteBlockWithInputValue2:
            {
                ClauseListNode* a = static_cast<ClauseListNode*>(PEEK_NODE());
                JSValue* v = GET_VALUE_RETURN();
                JSValue* input = PEEK_LOCAL_VALUE();
                
                if (strictEqual(exec, input, v)) {
                    // more "a" pointers to evaluate, jump to second while.
                    SET_CONTINUE_STATE(CaseBlockNodeExecuteBlockWithInputValue3);
                    break;
                }
                
                // jump to top of first while(a)
                if (a) {
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue1, currentNode);
                    break;
                }
                
                POP_NODE(); // we're done with "a"
                SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue5, currentNode); // jump to top of first "b" loop
                break;
            }
            case CaseBlockNodeExecuteBlockWithInputValue3: // top of second while(a)
            {
                ClauseListNode* a = static_cast<ClauseListNode*>(PEEK_NODE());
                // CaseClauseNode::evalStatements()
                if (a->getClause()->next) {
                    EXECUTE_AND_CONTINUE(a->getClause()->next.get(), CaseBlockNodeExecuteBlockWithInputValue4);
                    break;
                }
                // fall through
            }
            case CaseBlockNodeExecuteBlockWithInputValue4:
            {
                ClauseListNode*& a = reinterpret_cast<ClauseListNode*&>(PEEK_NODE());
                a = a->getNext();
                if (a) { // still more "a" pointers to evaluate, jump to top of second while
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue3, currentNode);
                    break;
                }
                
                POP_NODE(); // we're done with "a"
                // fall through to first "b" loop
            }
            case CaseBlockNodeExecuteBlockWithInputValue5: // staging area for "b" loops
            {
                CaseBlockNode* caseBlockNode = static_cast<CaseBlockNode*>(currentNode);
                if (!caseBlockNode->list2) { // no "b", jump to default
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue8, currentNode);
                    break;
                }
                PUSH_NODE(caseBlockNode->list2.get()); // push "b"
                // fall through
            }
            case CaseBlockNodeExecuteBlockWithInputValue6: // top of first "b" loop
            {
                ClauseListNode* b = static_cast<ClauseListNode*>(PEEK_NODE());
                EVALUATE_AND_CONTINUE(b->getClause(), CaseBlockNodeExecuteBlockWithInputValue7);
            }
            case CaseBlockNodeExecuteBlockWithInputValue7:
            {
                ClauseListNode*& b = reinterpret_cast<ClauseListNode*&>(PEEK_NODE());
                JSValue* v = GET_VALUE_RETURN();
                JSValue* input = PEEK_LOCAL_VALUE();
                if (strictEqual(exec, input, v)) {
                    // skip over default clause, evaluate statement block if possible.
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue9, currentNode);
                    if (b->getClause()->next)
                        PUSH_EXECUTE(b->getClause()->next.get());
                    break;
                }
                
                b = b->getNext();
                if (b) { // jump to top of first "b" loop
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue6, currentNode);
                    break;
                }
                
                b = static_cast<CaseBlockNode*>(currentNode)->list2.get();
                // reset "b" and fall through
            }
            case CaseBlockNodeExecuteBlockWithInputValue8: // default clause
            {
                CaseBlockNode* caseBlockNode = static_cast<CaseBlockNode*>(currentNode);
                if (caseBlockNode->def) {
                    if (caseBlockNode->def->next) {
                        EXECUTE_AND_CONTINUE(caseBlockNode->def->next.get(), CaseBlockNodeExecuteBlockWithInputValue8);
                        break;
                    }
                }
                if (!caseBlockNode->list2) // never had a "b", we're done.
                    RETURN_NORMAL_COMPLETION();
                
                // fall through
            }
            case CaseBlockNodeExecuteBlockWithInputValue9: // top of second "b" loop
            {
                ClauseListNode* b = static_cast<ClauseListNode*>(PEEK_NODE());
                if (b->getClause()->next) {
                    EXECUTE_AND_CONTINUE(b->getClause()->next.get(), CaseBlockNodeExecuteBlockWithInputValue10);
                    break;
                }
            }
            case CaseBlockNodeExecuteBlockWithInputValue10:
            {
                ClauseListNode*& b = reinterpret_cast<ClauseListNode*&>(PEEK_NODE());
                b = b->getNext();
                if (b) { // jump to top of second "b" loop
                    SET_JUMP_STATE(CaseBlockNodeExecuteBlockWithInputValue9, currentNode);
                    break;
                }
                POP_NODE(); // done with "b"
                POP_LOCAL_VALUE(); // done with "input"
                RETURN_NORMAL_COMPLETION();
            }
            
        } // end switch
        
        // This label is used by RETURN_VALUE to break out of nested while loops in case statements
interpreter_state_switch_end:
        if (interpreter->getCompletionReturn().complType() != Normal)
            interpreter->unwindToNextBarrier(exec, currentNode);
    }
}

// Legacy execution support -- clients should treecode-ify themselves
Completion callExecuteOnNode(StatementNode* node, ExecState* exec)
{
    InterpreterImp* interpreter = exec->dynamicInterpreter()->imp();
    interpreter->pushUnwindBarrier(All, InterpreterImp::State(InternalErrorState, 0));
    PUSH_EXECUTE(node);
    runInterpreterLoop(exec);
    Completion c = GET_LAST_COMPLETION();
    RESET_COMPLETION_TO_NORMAL();
    POP_UNWIND_BARRIER(All);
    return c;
}

} // namespace KJS
