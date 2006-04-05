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
#define INTERPRETER_STATE_DEFINE_NAME_ARRAY(name) #name ,
    INTERPRETER_STATE_FOR_EACH(INTERPRETER_STATE_DEFINE_NAME_ARRAY)
#undef INTERPRETER_STATE_DEFINE_NAME_ARRAY
    "LastInterpreterState"
};

#define KJS_BREAKPOINT() \
if (Debugger::debuggersPresent > 0 && !hitStatement(exec)) \
return Completion(Normal);

#define KJS_ABORTPOINT() \
if (Debugger::debuggersPresent > 0 && \
    exec->dynamicInterpreter()->imp()->debugger() && \
    exec->dynamicInterpreter()->imp()->debugger()->imp()->aborted()) \
return Completion(Normal);

#define KJS_CHECKEXCEPTION() \
if (exec->hadException()) { \
    setExceptionDetailsIfNeeded(exec); \
        JSValue *ex = exec->exception(); \
            exec->clearException(); \
                return Completion(Throw, ex); \
} \
if (Collector::isOutOfMemory()) \
    return Completion(Throw, Error::create(exec, GeneralError, "Out of memory"));

#define KJS_CHECKEXCEPTIONVALUE() \
if (exec->hadException()) { \
    currentNode->setExceptionDetailsIfNeeded(exec); \
        RETURN_VALUE(jsUndefined()); \
} \
if (Collector::isOutOfMemory()) { \
    RETURN_VALUE(jsUndefined()); \
} // will be picked up by KJS_CHECKEXCEPTION 

#define KJS_CHECKEXCEPTIONLIST() \
if (exec->hadException()) { \
    setExceptionDetailsIfNeeded(exec); \
        return List(); \
} \
if (Collector::isOutOfMemory()) \
    return List(); // will be picked up by KJS_CHECKEXCEPTION

static inline int currentSourceId(ExecState* exec)
{
    return exec->context().imp()->currentBody()->sourceId();
}

static inline const UString& currentSourceURL(ExecState* exec)
{
    return exec->context().imp()->currentBody()->sourceURL();
}

static const char *dotExprNotAnObjectString()
{
    return "Value %s (result of expression %s.%s) is not object.";
}

static const char *dotExprDoesNotAllowCallsString() 
{
    return "Object %s (result of expression %s.%s) does not allow calls.";
}

static JSValue *typeStringForValue(JSValue *v)
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

// Set this to 1 to see logging.
#define TRACE_EXECUTION 0

#if TRACE_EXECUTION
#define PUSH_VALUE(value) \
    (printf("PUSH %s (%i) called on %p, pushing %p, pre-push depth: %i\n", nameForInterpreterState[statePair.state], statePair.state, statePair.node, value, interpreter->valueStackDepth()), interpreter->pushValueReturn(value))

#define POP_VALUE() (printf("POP %s (%i) called on %p, popping, pre-pop depth: %i\n", nameForInterpreterState[statePair.state], statePair.state, statePair.node, interpreter->valueStackDepth()), assert(stackBase.valueStackSize <= interpreter->valueStackDepth()), interpreter->popValueReturn())

#define RETURN_VALUE(value) \
{\
    JSValue* val = value; \
    printf("RETURN %s (%i) called on %p, returning %p\n", nameForInterpreterState[statePair.state], statePair.state, statePair.node, val); \
    PUSH_VALUE(val); \
    goto interpreter_state_switch_end; \
}

#define SET_JUMP_STATE(nextState, nextNode) \
{ \
    printf("JUMP %s (%i) called on %p, jumping to %s (%i) on %p\n", nameForInterpreterState[statePair.state], statePair.state, statePair.node, nameForInterpreterState[nextState], nextState, nextNode); \
    ASSERT(nextState != statePair.state); \
    interpreter->pushNextState(State(nextState, nextNode)); \
}

#else

#define PUSH_VALUE(value) interpreter->pushValueReturn(value)
#define POP_VALUE() (assert(stackBase.valueStackSize <= interpreter->valueStackDepth()), interpreter->popValueReturn())

// FIXME: Return values don't need to be on the stack (except possibly we pause the interpreter)
#define RETURN_VALUE(value) \
{\
    PUSH_VALUE(value); \
    goto interpreter_state_switch_end; \
}

// All code should use SET_CONTINUE_STATE when possible, as it performs additional sanity checks.
#define SET_JUMP_STATE(nextState, nextNode) \
{ \
    ASSERT(nextState != statePair.state); \
    interpreter->pushNextState(InterpreterImp::State(nextState, nextNode)); \
}

#endif

#define PUSH_EVALUATE(node) \
    interpreter->pushNextState(InterpreterImp::State(node->evaluateState(), node));

// This explicity checks for continue state bugs
// FIXME: This should not take an argument once we have a way to check if currentState+1 == a substate
#define SET_CONTINUE_STATE(nextState) \
{ \
    ASSERT(nextState == statePair.state + 1); \
    SET_JUMP_STATE(nextState, currentNode); \
}

// This call can only be used for continuing with the next state
// Use other calls to jump or loop.
#define EVALUATE_AND_CONTINUE(node, nextState) \
{ \
    SET_CONTINUE_STATE(nextState); \
    PUSH_EVALUATE(node); \
    goto interpreter_state_switch_end; \
}

// All code should use EVALUATE_AND_CONTINUE when possible, as it performs additional sanity checks.
#define EVALUATE_AND_JUMP(node, nextState) \
{ \
    SET_JUMP_STATE(nextState, currentNode); \
    PUSH_EVALUATE(node); \
    break; \
}

JSValue* callEvaluateOnNode(Node* node, ExecState* exec)
{
    InterpreterImp* interpreter = exec->dynamicInterpreter()->imp();
    ASSERT(!exec->hadException());

    InterpreterImp::UnwindMarker stackBase = interpreter->pushUnwindMarker();
    PUSH_EVALUATE(node);
    
    while (!exec->hadException() && interpreter->stateStackDepth() > stackBase.stateStackSize) {
    
        // FIXME: As a loop optimization, we should just peek, instead of pop here
        // Then every non-loop function would pop itself.
        InterpreterImp::State statePair = interpreter->popNextState();
        Node* currentNode = statePair.node;
        
#if TRACE_EXECUTION
        printf("EXEC %s (%i) on %p (state state: %i, value stack: %i)\n", nameForInterpreterState[statePair.state], statePair.state, statePair.node, interpreter->stateStackDepth(), interpreter->valueStackDepth());
#endif

        switch (statePair.state) {
            case EmptyStackState:
            case InternalErrorState:
            case ArgumentListNodeEvaluateState:
            case ArgumentsNodeEvaluateState:
            case ClauseListNodeEvaluateState:
            case CaseBlockNodeEvaluateState:
            case PropertyNodeEvaluateState:
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
                PUSH_VALUE(exec->lexicalInterpreter()->builtinArray()->construct(exec, List::empty()));
                PUSH_VALUE(jsNumber(0));
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
                JSValue* val = POP_VALUE();
                int l = POP_VALUE()->toInt32(exec);
                JSObject* array = POP_VALUE()->toObject(exec);
                
                l += n->elision;
                array->put(exec, l++, val);
                
                n = n->next.get();
                if (n) {
                    PUSH_VALUE(array);
                    PUSH_VALUE(jsNumber(l));
                    // FIXME: pushing "p" as the current node is dangerous
                    // If this function used current node after this call, currentNode would be wrong.
                    SET_JUMP_STATE(ElementNodeEvaluateState1, n);
                    break;
                }
                
                RETURN_VALUE(array);
            }
                
            case ArrayNodeEvaluateState:
            {
                ArrayNode* arrayNode = static_cast<ArrayNode*>(currentNode);
                if (arrayNode->element) {
                    EVALUATE_AND_CONTINUE(arrayNode->element.get(), ArrayNodeEvaluateState1);
                }
                // fall through
            }
            case ArrayNodeEvaluateState1:
            {
                ArrayNode* arrayNode = static_cast<ArrayNode*>(currentNode);
                JSObject *array;
                int length;
                
                if (arrayNode->element) {
                    array = static_cast<JSObject*>(POP_VALUE());
                    length = arrayNode->opt ? array->get(exec, lengthPropertyName)->toInt32(exec) : 0;
                } else {
                    JSValue *newArr = exec->lexicalInterpreter()->builtinArray()->construct(exec, List::empty());
                    array = static_cast<JSObject*>(newArr);
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
                PUSH_VALUE(obj);
                // fall through
            }
            case PropertyListNodeEvaluateState1:
            {
                PropertyListNode* propertyListNode = static_cast<PropertyListNode*>(currentNode);
                // This has to use jump since we fell through above.
                SET_JUMP_STATE(PropertyListNodeEvaluateState2, propertyListNode);
                PUSH_EVALUATE(propertyListNode->node->assign.get());
                PUSH_EVALUATE(propertyListNode->node->name.get());
                break;
            }
            case PropertyListNodeEvaluateState2:
            {
                PropertyListNode *p = static_cast<PropertyListNode*>(currentNode);
                JSValue* v = POP_VALUE();
                JSValue* name = POP_VALUE();
                JSObject* obj = static_cast<JSObject*>(interpreter->peekValueReturn());
                
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
                // No return need, "obj" was already pushed originally.
                break;
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
                BracketAccessorNode* braketAccessorNode = static_cast<BracketAccessorNode*>(currentNode);
                SET_CONTINUE_STATE(BracketAccessorNodeEvaluateState1);
                PUSH_EVALUATE(braketAccessorNode->expr2.get());
                PUSH_EVALUATE(braketAccessorNode->expr1.get());
                break;
            }
            case BracketAccessorNodeEvaluateState1:
            {
                JSValue *v2 = POP_VALUE();
                JSValue *v1 = POP_VALUE();
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
                JSValue *v = POP_VALUE();
                RETURN_VALUE(v->toObject(exec)->get(exec, static_cast<DotAccessorNode*>(currentNode)->ident));
            }
                
            case NewExprNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<NewExprNode*>(currentNode)->expr.get(), NewExprNodeEvaluateState1)
            }
            case NewExprNodeEvaluateState1:
            {
                JSValue* v = POP_VALUE();
                NewExprNode* newExprNode = static_cast<NewExprNode*>(currentNode);
                List argList;
                if (newExprNode->args) {
                    argList = newExprNode->args->evaluateList(exec);
                    KJS_CHECKEXCEPTIONVALUE();
                }
                
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
                JSValue* v = POP_VALUE();
                if (!v->isObject())
                    RETURN_VALUE(functionCallValueNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, functionCallValueNode->expr.get()));
                
                JSObject *func = static_cast<JSObject*>(v);
                
                if (!func->implementsCall())
                    RETURN_VALUE(functionCallValueNode->throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, functionCallValueNode->expr.get()));
                
                List argList = functionCallValueNode->args->evaluateList(exec);
                KJS_CHECKEXCEPTIONVALUE();
                
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
                
                PropertySlot slot;
                JSObject *base;
                do {
                    base = *iter;
                    if (base->getPropertySlot(exec, ident, slot)) {
                        JSValue *v = slot.getValue(exec, base, ident);
                        KJS_CHECKEXCEPTIONVALUE();
                        
                        if (!v->isObject())
                            RETURN_VALUE(functionCallResolveNode->throwError(exec, TypeError, "Value %s (result of expression %s) is not object.", v, ident));
                        
                        JSObject *func = static_cast<JSObject*>(v);
                        
                        if (!func->implementsCall())
                            RETURN_VALUE(functionCallResolveNode->throwError(exec, TypeError, "Object %s (result of expression %s) does not allow calls.", v, ident));
                        
                        List argList = functionCallResolveNode->args->evaluateList(exec);
                        KJS_CHECKEXCEPTIONVALUE();
                        
                        JSObject *thisObj = base;
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
                    ++iter;
                } while (iter != end);
                             
                RETURN_VALUE(functionCallResolveNode->throwUndefinedVariableError(exec, ident));
            }
                
            case FunctionCallBracketNodeEvaluateState:
            {
                FunctionCallBracketNode* functionCallBracketNode = static_cast<FunctionCallBracketNode*>(currentNode);
                SET_CONTINUE_STATE(FunctionCallBracketNodeEvaluateState1);
                PUSH_EVALUATE(functionCallBracketNode->subscript.get());
                PUSH_EVALUATE(functionCallBracketNode->base.get());
                break;
            }
            case FunctionCallBracketNodeEvaluateState1:
            {
                FunctionCallBracketNode* functionCallBracketNode = static_cast<FunctionCallBracketNode*>(currentNode);
                
                JSValue *subscriptVal = POP_VALUE();
                JSObject *baseObj = POP_VALUE()->toObject(exec);
                
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
                
                List argList = functionCallBracketNode->args->evaluateList(exec);
                KJS_CHECKEXCEPTIONVALUE();
                
                JSObject *thisObj = baseObj;
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
                JSObject* baseObj = POP_VALUE()->toObject(exec);
                PropertySlot slot;
                const Identifier& ident = functionCallDotNode->ident;
                JSValue* funcVal = baseObj->getPropertySlot(exec, ident, slot) ? slot.getValue(exec, baseObj, ident) : jsUndefined();
                KJS_CHECKEXCEPTIONVALUE();
                
                if (!funcVal->isObject())
                    RETURN_VALUE(functionCallDotNode->throwError(exec, TypeError, dotExprNotAnObjectString(), funcVal, functionCallDotNode->base.get(), ident));
                
                JSObject *func = static_cast<JSObject*>(funcVal);
                
                if (!func->implementsCall())
                    RETURN_VALUE(functionCallDotNode->throwError(exec, TypeError, dotExprDoesNotAllowCallsString(), funcVal, functionCallDotNode->base.get(), ident));
                
                List argList = functionCallDotNode->args->evaluateList(exec);
                KJS_CHECKEXCEPTIONVALUE();
                
                JSObject *thisObj = baseObj;
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
                PostfixBracketNode* postfixBracketNode = static_cast<PostfixBracketNode*>(currentNode);
                SET_CONTINUE_STATE(PostfixBracketNodeEvaluateState1);
                PUSH_EVALUATE(postfixBracketNode->m_subscript.get());
                PUSH_EVALUATE(postfixBracketNode->m_base.get());
                break;
            }
            case PostfixBracketNodeEvaluateState1:
            {
                PostfixBracketNode* postfixBracketNode = static_cast<PostfixBracketNode*>(currentNode);
                
                JSValue *subscript = POP_VALUE();
                JSObject *base = POP_VALUE()->toObject(exec);
                
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
                JSValue* baseValue = POP_VALUE();
                JSObject* base = baseValue->toObject(exec);
                
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
                DeleteBracketNode* deleteBracketNode = static_cast<DeleteBracketNode*>(currentNode);
                SET_CONTINUE_STATE(DeleteBracketNodeEvaluateState1);
                PUSH_EVALUATE(deleteBracketNode->m_subscript.get());
                PUSH_EVALUATE(deleteBracketNode->m_base.get());
                break;
            }
            case DeleteBracketNodeEvaluateState1:
            {
                JSValue *subscript = POP_VALUE();
                JSObject *base = POP_VALUE()->toObject(exec);
                
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
                JSValue *baseValue = POP_VALUE();
                JSObject *base = baseValue->toObject(exec);                
                RETURN_VALUE(jsBoolean(base->deleteProperty(exec, static_cast<DeleteDotNode*>(currentNode)->m_ident)));
            }

            case DeleteValueNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<DeleteValueNode*>(currentNode)->m_expr.get(), DeleteValueNodeEvaluateState1);
            }
            case DeleteValueNodeEvaluateState1:
            {
                // delete on a non-location expression ignores the value and returns true
                POP_VALUE();
                RETURN_VALUE(jsBoolean(true));
            }

            case VoidNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<VoidNode*>(currentNode)->expr.get(), VoidNodeEvaluateState1);
            }
            case VoidNodeEvaluateState1:
            {
                POP_VALUE();
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
                RETURN_VALUE(typeStringForValue(POP_VALUE()));
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
                PrefixBracketNode* prefixBracketNode = static_cast<PrefixBracketNode*>(currentNode);
                SET_CONTINUE_STATE(PrefixBracketNodeEvaluateState1);
                PUSH_EVALUATE(prefixBracketNode->m_subscript.get());
                PUSH_EVALUATE(prefixBracketNode->m_base.get());
                break;
            }
            case PrefixBracketNodeEvaluateState1:
            {
                PrefixBracketNode* prefixBracketNode = static_cast<PrefixBracketNode*>(currentNode);
                Operator oper = prefixBracketNode->m_oper;
                JSValue *subscript = POP_VALUE();
                JSObject *base = POP_VALUE()->toObject(exec);
                
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
                JSValue* baseValue = POP_VALUE();
                JSObject *base = baseValue->toObject(exec);
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
                double value = POP_VALUE()->toNumber(exec);
                RETURN_VALUE(jsNumber(value));
            }

            case NegateNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<NegateNode*>(currentNode)->expr.get(), NegateNodeEvaluateState1);
            }
            case NegateNodeEvaluateState1:
            {
                double value = POP_VALUE()->toNumber(exec);
                RETURN_VALUE(jsNumber(-value));
            }
            
            case BitwiseNotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<BitwiseNotNode*>(currentNode)->expr.get(), BitwiseNotNodeEvaluateState1);
            }
            case BitwiseNotNodeEvaluateState1:
            {
                int32_t value = POP_VALUE()->toInt32(exec);
                RETURN_VALUE(jsNumber(~value));
            }
                
            case LogicalNotNodeEvaluateState:
            {
                EVALUATE_AND_CONTINUE(static_cast<LogicalNotNode*>(currentNode)->expr.get(), LogicalNotNodeEvaluateState1);
            }
            case LogicalNotNodeEvaluateState1:
            {
                bool value = POP_VALUE()->toBoolean(exec);
                RETURN_VALUE(jsBoolean(!value));
            }
            
            case MultNodeEvaluateState:
            {
                MultNode* multNode = static_cast<MultNode*>(currentNode);
                SET_CONTINUE_STATE(MultNodeEvaluateState1);
                PUSH_EVALUATE(multNode->term2.get());
                PUSH_EVALUATE(multNode->term1.get());
                break;
            }
            case MultNodeEvaluateState1:
            {
                JSValue *v2 = POP_VALUE();
                JSValue *v1 = POP_VALUE();
                RETURN_VALUE(mult(exec, v1, v2, static_cast<MultNode*>(currentNode)->oper));
            }
            
            case AddNodeEvaluateState:
            {
                AddNode* addNode = static_cast<AddNode*>(currentNode);
                SET_CONTINUE_STATE(AddNodeEvaluateState1);
                PUSH_EVALUATE(addNode->term2.get());
                PUSH_EVALUATE(addNode->term1.get());
                break;
            }
            case AddNodeEvaluateState1:
            {
                JSValue *v2 = POP_VALUE();
                JSValue *v1 = POP_VALUE();
                RETURN_VALUE(add(exec, v1, v2, static_cast<AddNode*>(currentNode)->oper));
            }
            
            case ShiftNodeEvaluateState:
            {
                ShiftNode* shiftNode = static_cast<ShiftNode*>(currentNode);
                SET_CONTINUE_STATE(ShiftNodeEvaluateState1);
                PUSH_EVALUATE(shiftNode->term2.get());
                PUSH_EVALUATE(shiftNode->term1.get());
                break;
            }
            case ShiftNodeEvaluateState1:
            {
                JSValue *v2 = POP_VALUE();
                JSValue *v1 = POP_VALUE();
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
                RelationalNode* relationalNode = static_cast<RelationalNode*>(currentNode);
                SET_CONTINUE_STATE(RelationalNodeEvaluateState1);
                PUSH_EVALUATE(relationalNode->expr2.get());
                PUSH_EVALUATE(relationalNode->expr1.get());
                break;
            }
            case RelationalNodeEvaluateState1:
            {
                RelationalNode* relationalNode = static_cast<RelationalNode*>(currentNode);
                JSValue *v2 = POP_VALUE();
                JSValue *v1 = POP_VALUE();
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
                EqualNode* equalNode = static_cast<EqualNode*>(currentNode);
                SET_CONTINUE_STATE(EqualNodeEvaluateState1);
                PUSH_EVALUATE(equalNode->expr2.get());
                PUSH_EVALUATE(equalNode->expr1.get());
                break;
            }
            case EqualNodeEvaluateState1:
            {
                JSValue *v2 = POP_VALUE();
                JSValue *v1 = POP_VALUE();
                Operator oper = static_cast<EqualNode*>(currentNode)->oper;
                
                bool result;
                if (oper == OpEqEq || oper == OpNotEq) {
                    // == and !=
                    bool eq = equal(exec,v1, v2);
                    result = oper == OpEqEq ? eq : !eq;
                } else {
                    // === and !==
                    bool eq = strictEqual(exec,v1, v2);
                    result = oper == OpStrEq ? eq : !eq;
                }
                RETURN_VALUE(jsBoolean(result));
            }


            case BitOperNodeEvaluateState:
            {
                BitOperNode* bitOperNode = static_cast<BitOperNode*>(currentNode);
                SET_CONTINUE_STATE(BitOperNodeEvaluateState1);
                PUSH_EVALUATE(bitOperNode->expr2.get());
                PUSH_EVALUATE(bitOperNode->expr1.get());
                break;
            }
            case BitOperNodeEvaluateState1:
            {
                JSValue *v2 = POP_VALUE();
                JSValue *v1 = POP_VALUE();
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
                JSValue* v1 = POP_VALUE();
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
                JSValue *v = POP_VALUE();
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
                
                PUSH_VALUE(base);
                EVALUATE_AND_CONTINUE(assignResolveNode->m_right.get(), AssignResolveNodeEvaluateState1);
            }
            case AssignResolveNodeEvaluateState1:
            {
                AssignResolveNode *assignResolveNode = static_cast<AssignResolveNode*>(currentNode);
                Operator oper = assignResolveNode->m_oper;
                const Identifier& ident = assignResolveNode->m_ident;
                JSValue* v = POP_VALUE();
                JSObject* base = static_cast<JSObject*>(POP_VALUE());
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
                AssignDotNode* assignDotNode = static_cast<AssignDotNode*>(currentNode);
                if (assignDotNode->m_oper == OpEqual) {
                    EVALUATE_AND_JUMP(assignDotNode->m_right.get(), AssignDotNodeEvaluateState3);
                } else {
                    EVALUATE_AND_CONTINUE(assignDotNode->m_right.get(), AssignDotNodeEvaluateState2);
                }
            }
            case AssignDotNodeEvaluateState2:
            {
                AssignDotNode* assignDotNode = static_cast<AssignDotNode*>(currentNode);
                JSValue* v2 = POP_VALUE();
                JSObject* base = interpreter->peekValueReturn()->toObject(exec);
                PropertySlot slot;
                JSValue *v1 = base->getPropertySlot(exec, assignDotNode->m_ident, slot) ? slot.getValue(exec, base, assignDotNode->m_ident) : jsUndefined();
                KJS_CHECKEXCEPTIONVALUE();
                
                PUSH_VALUE(valueForReadModifyAssignment(exec, v1, v2, assignDotNode->m_oper));
                // fall through
            }
            case AssignDotNodeEvaluateState3:
            {
                AssignDotNode* assignDotNode = static_cast<AssignDotNode*>(currentNode);
                JSValue* v = POP_VALUE();
                JSObject* base = POP_VALUE()->toObject(exec);
                base->put(exec, assignDotNode->m_ident, v);
                RETURN_VALUE(v);
            }
                
                
            case AssignBracketNodeEvaluateState:
            {
                AssignBracketNode* assignBracketNode = static_cast<AssignBracketNode*>(currentNode);
                SET_CONTINUE_STATE(AssignBracketNodeEvaluateState1);
                PUSH_EVALUATE(assignBracketNode->m_right.get());
                PUSH_EVALUATE(assignBracketNode->m_subscript.get());
                PUSH_EVALUATE(assignBracketNode->m_base.get());
                break;
            }
            case AssignBracketNodeEvaluateState1:
            {
                AssignBracketNode* assignBracketNode = static_cast<AssignBracketNode*>(currentNode);
                JSValue* v = POP_VALUE();
                JSValue* subscript = POP_VALUE();            
                JSObject* base = POP_VALUE()->toObject(exec);
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
                EVALUATE_AND_CONTINUE(static_cast<CommaNode*>(currentNode)->expr1.get(), CommaNodeEvaluateState1);
            }
            case CommaNodeEvaluateState1:
            {
                POP_VALUE(); // Ignore the first return value
                PUSH_EVALUATE(static_cast<CommaNode*>(currentNode)->expr2.get());
                break;
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

                if (varDecl->init) {
                    EVALUATE_AND_CONTINUE(varDecl->init.get(), VarDeclNodeEvaluateState1);
                } else {
                    // already declared? - check with getDirect so you can override
                    // built-in properties of the global object with var declarations.
                    if (variable->getDirect(varDecl->ident))
                        RETURN_VALUE((JSValue*)0);
                    PUSH_VALUE(jsUndefined());
                }
                // fall through
            }
            case VarDeclNodeEvaluateState1:
            {
                VarDeclNode* varDecl = static_cast<VarDeclNode*>(currentNode);
                JSValue* val = POP_VALUE();
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
                POP_VALUE(); // ignore value from last evaluate
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
                        
            case StatementNodeEvaluateState:
            case ParameterNodeEvaluateState:
            {
                RETURN_VALUE(jsUndefined());
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
        }
        
        // This lable is used by RETURN_VALUE to break out of nested while loops in case statements
interpreter_state_switch_end:
        // FIXME: need proper exception unrolling and "detail setting" here.
        continue;
    }
    
    if (exec->hadException()) {
        interpreter->unwindToNextMarker();
        return jsUndefined();
    }
    
    JSValue* result = interpreter->popValueReturn();
    interpreter->popUnwindMarker();
    return result;
}

// FIXME: This should be moved into the interpreter loop
List ArgumentListNode::evaluateList(ExecState *exec)
{
    List l;
    
    for (ArgumentListNode *n = this; n; n = n->next.get()) {
        JSValue *v = callEvaluateOnNode(n->expr.get(), exec);
        KJS_CHECKEXCEPTIONLIST();
        l.append(v);
    }
    
    return l;
}

} // namespace KJS
