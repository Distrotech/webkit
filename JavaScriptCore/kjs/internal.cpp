/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004 Apple Computer, Inc.
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
#include "internal.h"

#include "array_object.h"
#include "bool_object.h"
#include "collector.h"
#include "context.h"
#include "date_object.h"
#include "debugger.h"
#include "error_object.h"
#include "function_object.h"
#include "lexer.h"
#include "math_object.h"
#include "nodes.h"
#include "number_object.h"
#include "object.h"
#include "object_object.h"
#include "operations.h"
#include "regexp_object.h"
#include "string_object.h"
#include "TreeCode.h"
#include <assert.h>
#include <kxmlcore/HashMap.h>
#include <kxmlcore/HashSet.h>
#include <kxmlcore/Vector.h>
#include <math.h>
#include <stdio.h>

namespace KJS {

#if PLATFORM(WIN_OS)
#define copysign _copysign
#endif

// ------------------------------ StringImp ------------------------------------

JSValue *StringImp::toPrimitive(ExecState *, JSType) const
{
  return const_cast<StringImp *>(this);
}

bool StringImp::toBoolean(ExecState *) const
{
  return (val.size() > 0);
}

double StringImp::toNumber(ExecState *) const
{
  return val.toDouble();
}

UString StringImp::toString(ExecState *) const
{
  return val;
}

JSObject *StringImp::toObject(ExecState *exec) const
{
    return new StringInstance(exec->lexicalInterpreter()->builtinStringPrototype(), val);
}

// ------------------------------ NumberImp ------------------------------------

JSValue *NumberImp::toPrimitive(ExecState *, JSType) const
{
  return const_cast<NumberImp *>(this);
}

bool NumberImp::toBoolean(ExecState *) const
{
  return val < 0.0 || val > 0.0; // false for NaN
}

double NumberImp::toNumber(ExecState *) const
{
  return val;
}

UString NumberImp::toString(ExecState *) const
{
  if (val == 0.0) // +0.0 or -0.0
    return "0";
  return UString::from(val);
}

JSObject *NumberImp::toObject(ExecState *exec) const
{
  List args;
  args.append(const_cast<NumberImp*>(this));
  return static_cast<JSObject *>(exec->lexicalInterpreter()->builtinNumber()->construct(exec,args));
}

// FIXME: We can optimize this to work like JSValue::getUInt32. I'm ignoring it for now
// because it never shows up on profiles.
bool NumberImp::getUInt32(uint32_t& uint32) const
{
  uint32 = (uint32_t)val;
  return (double)uint32 == val;
}

// --------------------------- GetterSetterImp ---------------------------------
void GetterSetterImp::mark()
{
    JSCell::mark();
    
    if (getter && !getter->marked())
        getter->mark();
    if (setter && !setter->marked())
        setter->mark();
}

JSValue *GetterSetterImp::toPrimitive(ExecState*, JSType) const
{
    assert(false);
    return jsNull();
}

bool GetterSetterImp::toBoolean(ExecState*) const
{
    assert(false);
    return false;
}

double GetterSetterImp::toNumber(ExecState *) const
{
    assert(false);
    return 0.0;
}

UString GetterSetterImp::toString(ExecState *) const
{
    assert(false);
    return UString::null();
}

JSObject *GetterSetterImp::toObject(ExecState *exec) const
{
    assert(false);
    return jsNull()->toObject(exec);
}

// ------------------------------ LabelStack -----------------------------------

bool LabelStack::push(const Identifier &id)
{
  if (contains(id))
    return false;

  StackElem *newtos = new StackElem;
  newtos->id = id;
  newtos->prev = tos;
  tos = newtos;
  return true;
}

bool LabelStack::contains(const Identifier &id) const
{
  if (id.isEmpty())
    return true;

  for (StackElem *curr = tos; curr; curr = curr->prev)
    if (curr->id == id)
      return true;

  return false;
}

// ------------------------------ ContextImp -----------------------------------

// ECMA 10.2
ContextImp::ContextImp(JSObject *glob, InterpreterImp *interpreter, JSObject *thisV, FunctionBodyNode* currentBody,
                       
                       CodeType type, ContextImp *callingCon, FunctionImp *func, const List *args)
    : _interpreter(interpreter)
    , m_currentBody(currentBody)
    , _function(func)
    , _arguments(args)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
{
  m_codeType = type;
  _callingContext = callingCon;

  // create and initialize activation object (ECMA 10.1.6)
  if (type == FunctionCode || type == AnonymousCode ) {
    activation = new ActivationImp(func, *args);
    variable = activation;
  } else {
    activation = NULL;
    variable = glob;
  }

  // ECMA 10.2
  switch(type) {
    case EvalCode:
      if (_callingContext) {
        scope = _callingContext->scopeChain();
        variable = _callingContext->variableObject();
        thisVal = _callingContext->thisValue();
        break;
      } // else same as GlobalCode
    case GlobalCode:
      scope.clear();
      scope.push(glob);
      thisVal = static_cast<JSObject*>(glob);
      break;
    case FunctionCode:
    case AnonymousCode:
      if (type == FunctionCode) {
        scope = func->scope();
        scope.push(activation);
      } else {
        scope.clear();
        scope.push(glob);
        scope.push(activation);
      }
      variable = activation; // TODO: DontDelete ? (ECMA 10.2.3)
      thisVal = thisV;
      break;
    }

  _interpreter->setContext(this);
}

ContextImp::~ContextImp()
{
  _interpreter->setContext(_callingContext);
}

void ContextImp::mark()
{
    // FIXME: This context walk should go away once all Contexts are on the interpreter context stack
    for (ContextImp *context = this; context; context = context->_callingContext)
        context->scope.mark();
    
    if (_function && !_function->marked())
        _function->mark();
    if (activation && !activation->marked())
        activation->mark();
    if (!variable->marked())
        variable->mark();
    if (!thisVal->marked())
        thisVal->mark();
}

// ------------------------------ InterpreterImp -------------------------------

InterpreterImp::InterpreterMap* InterpreterImp::s_interpreterMap = 0L;

InterpreterImp::InterpreterMap& InterpreterImp::interpreterMap()
{
    if (!s_interpreterMap)
        s_interpreterMap = new InterpreterMap;
    return *s_interpreterMap;
}

InterpreterImp::InterpreterImp(Interpreter *interpeter, JSObject *globalObject)
    : m_globalObject(globalObject)
    , m_interpreter(interpeter)
    , m_debugger(0)
    , m_globalExecState(interpeter, 0)
    , m_compatMode(Interpreter::NativeMode)
    , m_context(0)
    , m_recursion(0)
{
    JSLock lock;
    interpreterMap().set(globalObject, this);
    initGlobalObject();
}

void InterpreterImp::initGlobalObject()
{
  Identifier::init();
  
  // Contructor prototype objects (Object.prototype, Array.prototype etc)
  m_builtIns.initBuiltIns(&m_globalExecState, m_globalObject);
}

void BuiltInTypes::initBuiltIns(ExecState* globExec, JSObject* global)
{
    FunctionPrototype *funcProto = new FunctionPrototype(globExec);
    b_FunctionPrototype = funcProto;
    ObjectPrototype *objProto = new ObjectPrototype(globExec, funcProto);
    b_ObjectPrototype = objProto;
    funcProto->setPrototype(b_ObjectPrototype);

    ArrayPrototype *arrayProto = new ArrayPrototype(globExec, objProto);
    b_ArrayPrototype = arrayProto;
    StringPrototype *stringProto = new StringPrototype(globExec, objProto);
    b_StringPrototype = stringProto;
    BooleanPrototype *booleanProto = new BooleanPrototype(globExec, objProto, funcProto);
    b_BooleanPrototype = booleanProto;
    NumberPrototype *numberProto = new NumberPrototype(globExec, objProto, funcProto);
    b_NumberPrototype = numberProto;
    DatePrototype *dateProto = new DatePrototype(globExec, objProto);
    b_DatePrototype = dateProto;
    RegExpPrototype *regexpProto = new RegExpPrototype(globExec, objProto, funcProto);
    b_RegExpPrototype = regexpProto;
    ErrorPrototype *errorProto = new ErrorPrototype(globExec, objProto, funcProto);
    b_ErrorPrototype = errorProto;

    static_cast<JSObject*>(global)->setPrototype(b_ObjectPrototype);

    // Constructors (Object, Array, etc.)
    b_Object = new ObjectObjectImp(globExec, objProto, funcProto);
    b_Function = new FunctionObjectImp(globExec, funcProto);
    b_Array = new ArrayObjectImp(globExec, funcProto, arrayProto);
    b_String = new StringObjectImp(globExec, funcProto, stringProto);
    b_Boolean = new BooleanObjectImp(globExec, funcProto, booleanProto);
    b_Number = new NumberObjectImp(globExec, funcProto, numberProto);
    b_Date = new DateObjectImp(globExec, funcProto, dateProto);
    b_RegExp = new RegExpObjectImp(globExec, funcProto, regexpProto);
    b_Error = new ErrorObjectImp(globExec, funcProto, errorProto);

    // Error object prototypes
    b_evalErrorPrototype = new NativeErrorPrototype(globExec, errorProto, EvalError, "EvalError", "EvalError");
    b_rangeErrorPrototype = new NativeErrorPrototype(globExec, errorProto, RangeError, "RangeError", "RangeError");
    b_referenceErrorPrototype = new NativeErrorPrototype(globExec, errorProto, ReferenceError, "ReferenceError", "ReferenceError");
    b_syntaxErrorPrototype = new NativeErrorPrototype(globExec, errorProto, SyntaxError, "SyntaxError", "SyntaxError");
    b_typeErrorPrototype = new NativeErrorPrototype(globExec, errorProto, TypeError, "TypeError", "TypeError");
    b_uriErrorPrototype = new NativeErrorPrototype(globExec, errorProto, URIError, "URIError", "URIError");

    // Error objects
    b_evalError = new NativeErrorImp(globExec, funcProto, b_evalErrorPrototype);
    b_rangeError = new NativeErrorImp(globExec, funcProto, b_rangeErrorPrototype);
    b_referenceError = new NativeErrorImp(globExec, funcProto, b_referenceErrorPrototype);
    b_syntaxError = new NativeErrorImp(globExec, funcProto, b_syntaxErrorPrototype);
    b_typeError = new NativeErrorImp(globExec, funcProto, b_typeErrorPrototype);
    b_uriError = new NativeErrorImp(globExec, funcProto, b_uriErrorPrototype);

    // ECMA 15.3.4.1
    funcProto->put(globExec, constructorPropertyName, b_Function, DontEnum);

    global->put(globExec, "Object", b_Object, DontEnum);
    global->put(globExec, "Function", b_Function, DontEnum);
    global->put(globExec, "Array", b_Array, DontEnum);
    global->put(globExec, "Boolean", b_Boolean, DontEnum);
    global->put(globExec, "String", b_String, DontEnum);
    global->put(globExec, "Number", b_Number, DontEnum);
    global->put(globExec, "Date", b_Date, DontEnum);
    global->put(globExec, "RegExp", b_RegExp, DontEnum);
    global->put(globExec, "Error", b_Error, DontEnum);
    // Using Internal for those to have something != 0
    // (see kjs_window). Maybe DontEnum would be ok too ?
    global->put(globExec, "EvalError",b_evalError, Internal);
    global->put(globExec, "RangeError",b_rangeError, Internal);
    global->put(globExec, "ReferenceError",b_referenceError, Internal);
    global->put(globExec, "SyntaxError",b_syntaxError, Internal);
    global->put(globExec, "TypeError",b_typeError, Internal);
    global->put(globExec, "URIError",b_uriError, Internal);

    // Set the constructorPropertyName property of all builtin constructors
    objProto->put(globExec, constructorPropertyName, b_Object, DontEnum | DontDelete | ReadOnly);
    funcProto->put(globExec, constructorPropertyName, b_Function, DontEnum | DontDelete | ReadOnly);
    arrayProto->put(globExec, constructorPropertyName, b_Array, DontEnum | DontDelete | ReadOnly);
    booleanProto->put(globExec, constructorPropertyName, b_Boolean, DontEnum | DontDelete | ReadOnly);
    stringProto->put(globExec, constructorPropertyName, b_String, DontEnum | DontDelete | ReadOnly);
    numberProto->put(globExec, constructorPropertyName, b_Number, DontEnum | DontDelete | ReadOnly);
    dateProto->put(globExec, constructorPropertyName, b_Date, DontEnum | DontDelete | ReadOnly);
    regexpProto->put(globExec, constructorPropertyName, b_RegExp, DontEnum | DontDelete | ReadOnly);
    errorProto->put(globExec, constructorPropertyName, b_Error, DontEnum | DontDelete | ReadOnly);
    b_evalErrorPrototype->put(globExec, constructorPropertyName, b_evalError, DontEnum | DontDelete | ReadOnly);
    b_rangeErrorPrototype->put(globExec, constructorPropertyName, b_rangeError, DontEnum | DontDelete | ReadOnly);
    b_referenceErrorPrototype->put(globExec, constructorPropertyName, b_referenceError, DontEnum | DontDelete | ReadOnly);
    b_syntaxErrorPrototype->put(globExec, constructorPropertyName, b_syntaxError, DontEnum | DontDelete | ReadOnly);
    b_typeErrorPrototype->put(globExec, constructorPropertyName, b_typeError, DontEnum | DontDelete | ReadOnly);
    b_uriErrorPrototype->put(globExec, constructorPropertyName, b_uriError, DontEnum | DontDelete | ReadOnly);

    // built-in values
    global->put(globExec, "NaN",        jsNaN(), DontEnum|DontDelete);
    global->put(globExec, "Infinity",   jsNumber(Inf), DontEnum|DontDelete);
    global->put(globExec, "undefined",  jsUndefined(), DontEnum|DontDelete);

    // built-in functions
    global->putDirectFunction(new GlobalFuncImp(globExec, funcProto, GlobalFuncImp::Eval, 1, "eval"), DontEnum);
    global->putDirectFunction(new GlobalFuncImp(globExec, funcProto, GlobalFuncImp::ParseInt, 2, "parseInt"), DontEnum);
    global->putDirectFunction(new GlobalFuncImp(globExec, funcProto, GlobalFuncImp::ParseFloat, 1, "parseFloat"), DontEnum);
    global->putDirectFunction(new GlobalFuncImp(globExec, funcProto, GlobalFuncImp::IsNaN, 1, "isNaN"), DontEnum);
    global->putDirectFunction(new GlobalFuncImp(globExec, funcProto, GlobalFuncImp::IsFinite, 1, "isFinite"), DontEnum);
    global->putDirectFunction(new GlobalFuncImp(globExec, funcProto, GlobalFuncImp::Escape, 1, "escape"), DontEnum);
    global->putDirectFunction(new GlobalFuncImp(globExec, funcProto, GlobalFuncImp::UnEscape, 1, "unescape"), DontEnum);
    global->putDirectFunction(new GlobalFuncImp(globExec, funcProto, GlobalFuncImp::DecodeURI, 1, "decodeURI"), DontEnum);
    global->putDirectFunction(new GlobalFuncImp(globExec, funcProto, GlobalFuncImp::DecodeURIComponent, 1, "decodeURIComponent"), DontEnum);
    global->putDirectFunction(new GlobalFuncImp(globExec, funcProto, GlobalFuncImp::EncodeURI, 1, "encodeURI"), DontEnum);
    global->putDirectFunction(new GlobalFuncImp(globExec, funcProto, GlobalFuncImp::EncodeURIComponent, 1, "encodeURIComponent"), DontEnum);
    #ifndef NDEBUG
    global->putDirectFunction(new GlobalFuncImp(globExec, funcProto, GlobalFuncImp::KJSPrint, 1, "kjsprint"), DontEnum);
    #endif

    // built-in objects
    global->put(globExec, "Math", new MathObjectImp(globExec, objProto), DontEnum);
}

InterpreterImp::~InterpreterImp()
{
    if (m_debugger)
        m_debugger->detach(m_interpreter);
    clear();
}

void InterpreterImp::clear()
{
    // remove from global chain (see init())
    JSLock lock;
    interpreterMap().remove(m_globalObject);
}

void InterpreterImp::mark()
{
    if (m_interpreter)
        m_interpreter->mark();
    if (m_context)
        m_context->mark();
    if (m_globalObject)
        m_globalObject->mark();
    if (m_globalExecState.exception())
        m_globalExecState.exception()->mark();
    unsigned size = m_valueStack.size();
    for (unsigned x = 0; x < size; x++) {
        JSValue* val = m_valueStack[x];
        if (!val->marked())
            val->mark();
    }
    if (m_valueReturn && !m_valueReturn->marked())
        m_valueReturn->mark();
    if (m_completionReturn.value() && !m_completionReturn.value()->marked())
        m_completionReturn.value()->mark();
    size = m_listStack.size();
    for (unsigned x = 0; x < size; x++)
        m_listStack[x].mark();
    size = m_execStateStack.size();
    for (unsigned x = 0; x < size; x++)
        m_execStateStack[x]->context().imp()->mark();
}

bool InterpreterImp::checkSyntax(const UString &code)
{
  JSLock lock;

  // Parser::parse() returns 0 in a syntax error occurs, so we just check for that
  RefPtr<ProgramNode> progNode = Parser::parse(UString(), 0, code.data(), code.size(), 0, 0, 0);
  return progNode;
}

Completion InterpreterImp::evaluate(const UChar* code, int codeLength, JSValue* thisV, const UString& sourceURL, int startingLineNumber)
{
  JSLock lock;

  // prevent against infinite recursion
  if (m_recursion >= 20)
    return Completion(Throw, Error::create(&m_globalExecState, GeneralError, "Recursion too deep"));

  // parse the source code
  int sid;
  int errLine;
  UString errMsg;
  RefPtr<ProgramNode> progNode = Parser::parse(sourceURL, startingLineNumber, code, codeLength, &sid, &errLine, &errMsg);

  // notify debugger that source has been parsed
  if (m_debugger) {
    bool shouldContinue = m_debugger->sourceParsed(&m_globalExecState, sid, sourceURL, UString(code, codeLength), errLine);
    if (!shouldContinue)
      return Completion(Break);
  }
  
  // no program node means a syntax error occurred
  if (!progNode)
    return Completion(Throw, Error::create(&m_globalExecState, SyntaxError, errMsg, errLine, sid, &sourceURL));

  ASSERT(!m_globalExecState.hadException());
  ASSERT(m_completionReturn.complType() == Normal && m_completionReturn.value() == 0);

  m_recursion++;

  JSObject* globalObj = globalObject();
  JSObject* thisObj = globalObj;

  // "this" must be an object... use same rules as Function.prototype.apply()
  if (thisV && !thisV->isUndefinedOrNull())
      thisObj = thisV->toObject(&m_globalExecState);

  Completion res;
  if (m_globalExecState.hadException()) {
    // the thisV->toObject() conversion above might have thrown an exception - if so, propagate it
    res = Completion(Throw, m_globalExecState.exception());
      
    m_globalExecState.clearException();
    resetCompletionToNormal();
  } else {
    // execute the code
    ContextImp ctx(globalObj, this, thisObj, progNode.get());
    ExecState newExec(m_interpreter, &ctx);
    progNode->processVarDecls(&newExec);
    res = callExecuteOnNode(progNode.get(), &newExec);
  }

  m_recursion--;

  ASSERT(!m_globalExecState.hadException());
  ASSERT(m_completionReturn.complType() == Normal && m_completionReturn.value() == 0);
  
  return res;
}

void BuiltInTypes::saveBuiltins(SavedBuiltins& builtins) const
{
  if (!builtins._internal)
    builtins._internal = new SavedBuiltinsInternal;

  builtins._internal->b_Object = b_Object;
  builtins._internal->b_Function = b_Function;
  builtins._internal->b_Array = b_Array;
  builtins._internal->b_Boolean = b_Boolean;
  builtins._internal->b_String = b_String;
  builtins._internal->b_Number = b_Number;
  builtins._internal->b_Date = b_Date;
  builtins._internal->b_RegExp = b_RegExp;
  builtins._internal->b_Error = b_Error;
  
  builtins._internal->b_ObjectPrototype = b_ObjectPrototype;
  builtins._internal->b_FunctionPrototype = b_FunctionPrototype;
  builtins._internal->b_ArrayPrototype = b_ArrayPrototype;
  builtins._internal->b_BooleanPrototype = b_BooleanPrototype;
  builtins._internal->b_StringPrototype = b_StringPrototype;
  builtins._internal->b_NumberPrototype = b_NumberPrototype;
  builtins._internal->b_DatePrototype = b_DatePrototype;
  builtins._internal->b_RegExpPrototype = b_RegExpPrototype;
  builtins._internal->b_ErrorPrototype = b_ErrorPrototype;
  
  builtins._internal->b_evalError = b_evalError;
  builtins._internal->b_rangeError = b_rangeError;
  builtins._internal->b_referenceError = b_referenceError;
  builtins._internal->b_syntaxError = b_syntaxError;
  builtins._internal->b_typeError = b_typeError;
  builtins._internal->b_uriError = b_uriError;
  
  builtins._internal->b_evalErrorPrototype = b_evalErrorPrototype;
  builtins._internal->b_rangeErrorPrototype = b_rangeErrorPrototype;
  builtins._internal->b_referenceErrorPrototype = b_referenceErrorPrototype;
  builtins._internal->b_syntaxErrorPrototype = b_syntaxErrorPrototype;
  builtins._internal->b_typeErrorPrototype = b_typeErrorPrototype;
  builtins._internal->b_uriErrorPrototype = b_uriErrorPrototype;
}

void BuiltInTypes::restoreBuiltins(const SavedBuiltins& builtins)
{
  if (!builtins._internal)
    return;

  b_Object = builtins._internal->b_Object;
  b_Function = builtins._internal->b_Function;
  b_Array = builtins._internal->b_Array;
  b_Boolean = builtins._internal->b_Boolean;
  b_String = builtins._internal->b_String;
  b_Number = builtins._internal->b_Number;
  b_Date = builtins._internal->b_Date;
  b_RegExp = builtins._internal->b_RegExp;
  b_Error = builtins._internal->b_Error;
  
  b_ObjectPrototype = builtins._internal->b_ObjectPrototype;
  b_FunctionPrototype = builtins._internal->b_FunctionPrototype;
  b_ArrayPrototype = builtins._internal->b_ArrayPrototype;
  b_BooleanPrototype = builtins._internal->b_BooleanPrototype;
  b_StringPrototype = builtins._internal->b_StringPrototype;
  b_NumberPrototype = builtins._internal->b_NumberPrototype;
  b_DatePrototype = builtins._internal->b_DatePrototype;
  b_RegExpPrototype = builtins._internal->b_RegExpPrototype;
  b_ErrorPrototype = builtins._internal->b_ErrorPrototype;
  
  b_evalError = builtins._internal->b_evalError;
  b_rangeError = builtins._internal->b_rangeError;
  b_referenceError = builtins._internal->b_referenceError;
  b_syntaxError = builtins._internal->b_syntaxError;
  b_typeError = builtins._internal->b_typeError;
  b_uriError = builtins._internal->b_uriError;
  
  b_evalErrorPrototype = builtins._internal->b_evalErrorPrototype;
  b_rangeErrorPrototype = builtins._internal->b_rangeErrorPrototype;
  b_referenceErrorPrototype = builtins._internal->b_referenceErrorPrototype;
  b_syntaxErrorPrototype = builtins._internal->b_syntaxErrorPrototype;
  b_typeErrorPrototype = builtins._internal->b_typeErrorPrototype;
  b_uriErrorPrototype = builtins._internal->b_uriErrorPrototype;
}

InterpreterImp *InterpreterImp::interpreterWithGlobalObject(JSObject *global)
{
    return interpreterMap().get(global);
}

enum StackType {
    ValueStack,
    StateStack,
    ListStack
};

static unsigned sizeMarkerForStack(const InterpreterImp::UnwindBarrier& unwindBarrier, StackType stackType)
{
    switch(stackType) {
    case ValueStack:
        return unwindBarrier.valueStackSize;
    case StateStack:
        return unwindBarrier.stateStackSize;
    case ListStack:
        return unwindBarrier.listStackSize;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static void printUnwindBarrier(const InterpreterImp::UnwindBarrier& unwindBarrier, bool printStackStates = false)
{
    printf("=== unwind barrier type(s):");
    short barrierType = unwindBarrier.barrierType;
    if (barrierType == All)
        printf(" All");
    else {
        if (barrierType == Break)
            printf(" Break");
        if (barrierType == Continue)
            printf(" Continue");
        if (barrierType == ReturnValue)
            printf(" ReturnValue");
        if (barrierType == Throw)
            printf(" Throw");
        if (barrierType == Scope)
            printf(" Scope");
    }
    printf(", state: %s (%i) node: %p", nameForOpcode[unwindBarrier.continueState.opcode], unwindBarrier.continueState.opcode, unwindBarrier.continueState.node);
    if (printStackStates) {
        printf(", values: %lu", unwindBarrier.valueStackSize);
        printf(", states: %lu", unwindBarrier.stateStackSize);
        printf(", lists: %lu", unwindBarrier.listStackSize);
        printf(", nodes: %lu", unwindBarrier.nodeStackSize);
    }
    printf(" ===\n");
}

static void printUnwindBarriersIfNecessary(const Stack<InterpreterImp::UnwindBarrier, KJS_MAX_STACK>& unwindStack, int& currentMarker, unsigned stackLocation, StackType stackType)
{
    if (currentMarker < 0)
        return;
    InterpreterImp::UnwindBarrier unwindBarrier = unwindStack[currentMarker];
    while ((currentMarker >= 0) && (sizeMarkerForStack(unwindBarrier, stackType) - 1) == stackLocation) {
        printUnwindBarrier(unwindBarrier);
        // keep searching in case there are multiple unwind markers at this position.
        currentMarker--;
        unwindBarrier = unwindStack[currentMarker];
    }
}

void InterpreterImp::printUnwindBarrierStack()
{
    printf("UnwindBarrier Stack:\n");
    unsigned size = m_unwindBarrierStack.size();
    if (size == 0)
        printf("<empty>\n");
    
    for (int x = size-1; x >= 0; x--) {
        printf("%i: ", x);
        printUnwindBarrier(m_unwindBarrierStack[x], true);
    }
}

void InterpreterImp::printStacks()
{
    printStateStack();
    printValueStack();
    printListStack();
}

void InterpreterImp::printStateStack()
{
    printf("State Stack:\n");
    unsigned size = m_stateStack.size();
    if (size == 0)
        printf("<empty>\n");
    
    int unwindIter = m_unwindBarrierStack.size() - 1;
    for (int x = size - 1; x >= 0; x--) {
        printUnwindBarriersIfNecessary(m_unwindBarrierStack, unwindIter, x, StateStack); 
        Opcode opcode = m_stateStack[x].opcode;
        printf("%i: %s (%i), %p\n", x, nameForOpcode[opcode], opcode, m_stateStack[x].node);
    }
    printUnwindBarriersIfNecessary(m_unwindBarrierStack, unwindIter, 0, StateStack); 
}

void InterpreterImp::printValueStack()
{
    printf("Value Stack:\n");
    unsigned size = m_valueStack.size();
    if (size == 0)
        printf("<empty>\n");
    
    int unwindIter = m_unwindBarrierStack.size() - 1;
    for (int x = size - 1; x >= 0; x--) {
        printUnwindBarriersIfNecessary(m_unwindBarrierStack, unwindIter, x, ValueStack);
        JSValue* v = m_valueStack[x];
        printf("%i: %p [type: %s] [value: %s]\n"
               , x
               , v
               , v ? typeStringForValue(v)->toString(&m_globalExecState).cstring().c_str() : "NULL stack marker"
               , v ? v->toString(&m_globalExecState).cstring().c_str() : "0"
               );
    }
    printUnwindBarriersIfNecessary(m_unwindBarrierStack, unwindIter, 0, ValueStack);
}

void InterpreterImp::printListStack()
{
    printf("List Stack:\n");
    unsigned size = m_listStack.size();
    if (size == 0)
        printf("<empty>\n");
    
    int unwindIter = m_unwindBarrierStack.size() - 1;
    for (int x = size-1; x >= 0; x--) {
        printUnwindBarriersIfNecessary(m_unwindBarrierStack, unwindIter, x, ListStack);
        printf("%i: (", x);
        const List& list = m_listStack[x];
        ListIterator itr = list.begin();
        ListIterator end = list.end();
        while (itr != end) {
             printf("%p", *itr);
             if (++itr)
                printf(", ");
        }
        printf(")\n");
    }
    printUnwindBarriersIfNecessary(m_unwindBarrierStack, unwindIter, 0, ListStack);
}

static inline const UString& currentSourceURL(ExecState* exec)
{
    return exec->context().imp()->currentBody()->sourceURL();
}

void InterpreterImp::unwindToNextBarrier(ExecState* exec, Node* currentNode)
{
    ASSERT(m_completionReturn.complType() != Normal);
    
    UnwindBarrier* barrier = 0;
    int barrierIndex = m_unwindBarrierStack.size() - 1;
    while (barrierIndex >= 0) {
        barrier = &m_unwindBarrierStack[barrierIndex];
        // FIXME: Hack for recognizing function call frame barriers
        if (barrier->barrierType == All)
            break;
        
        // FIXME: Hack for popping the scope chain when unwinding past a scope push
        if (barrier->barrierType == Scope)
            exec->context().imp()->popScope();
        
        if (barrier->barrierType == m_completionReturn.complType()) {
            StatementNode* barrierNode = static_cast<StatementNode*>(barrier->continueState.node);
            if (barrierNode->ls.contains(m_completionReturn.target()) || barrierNode->isLabelNode())
                break;
        }
        barrierIndex--;
    }
    
    if (barrier->barrierType == All) { // unwinding past a function call frame
        switch (m_completionReturn.complType()) {
        default:
        case Normal:
            ASSERT_NOT_REACHED();
            break;
        // You can't "break" out of a function
        case Break:
        {
            // FIXME: This check should happen at parse time. Then it can change to an ASSERT.
            const Identifier& target = m_completionReturn.target();
            if (target.isEmpty())
                m_completionReturn = currentNode->createErrorCompletion(exec, SyntaxError, "Invalid break statement.");
            else
                m_completionReturn = currentNode->createErrorCompletion(exec, SyntaxError, "Label %s not found.", target);
            break;
        }
        // You can't "continue" out of a function
        case Continue:
        {
            // FIXME: This check should happen at parse time. Then it can change to an ASSERT.
            const Identifier& target = m_completionReturn.target();
            if (target.isEmpty())
                m_completionReturn = currentNode->createErrorCompletion(exec, SyntaxError, "Invalid continue statement.");
            else
                m_completionReturn = currentNode->createErrorCompletion(exec, SyntaxError, "Label %s not found.", target);
            break;
        }
        // You can "return" and "throw" out of a function, so we propogate the completion to the calling frame
        case ReturnValue:
        case Throw:
            break;
        }
    }

    // If a throw comes from an internal function or an eval statement, it doesn't include a line number,
    // so we walk the state stack until we find a node with a valid line number, which should be the caller.
    if (exec->hadException()) {    
        JSValue *exceptionValue = exec->exception();
        if (exceptionValue->isObject()) {
            JSObject *exception = static_cast<JSObject *>(exceptionValue);
            if (!exception->hasProperty(exec, "sourceURL"))
                exception->put(exec, "sourceURL", jsString(currentSourceURL(exec)));
            if (!exception->hasProperty(exec, "line")) {
                // Walk the state stack until we find a valid line number
                int stateStackUnwindLimit = barrier->stateStackSize;
                for (int stateStackIndex = m_stateStack.size() - 1; stateStackIndex >= stateStackUnwindLimit; stateStackIndex--) {
                    const State& state = m_stateStack[stateStackIndex];
                    if (state.node->m_line != -1) {
                        exception->put(exec, "line", jsNumber(state.node->m_line));
                        break;
                    }
                }
            }
        }
        exec->clearException();
    }
    
    // Reset the unwind barrier stack
    m_unwindBarrierStack.shrinkTo(barrierIndex + 1);

    // ASSERT that unwinding travels down the stack, not up
    ASSERT(m_valueStack.size() >= barrier->valueStackSize);
    ASSERT(m_stateStack.size() >= barrier->stateStackSize);
    ASSERT(m_listStack.size() >= barrier->listStackSize);
    ASSERT(m_nodeStack.size() >= barrier->nodeStackSize);

    // Unwind the stacks
    m_stateStack.shrinkTo(barrier->stateStackSize);
    m_valueStack.shrinkTo(barrier->valueStackSize);
    m_listStack.shrinkTo(barrier->listStackSize);
    m_nodeStack.shrinkTo(barrier->nodeStackSize);
    
    // FIXME: We really should have a more elegant ExecState cleanup mechanism, after ExecState and Context merge
    for (unsigned x = m_execStateStack.size(); x > barrier->execStateStackSize; --x) {
        ExecState* exec = m_execStateStack[x-1];
        delete exec->context().imp();
        delete exec;
    }
    m_execStateStack.shrinkTo(barrier->execStateStackSize);
    
    // FIXME: Hack for recognizing function call frame barriers and scope chain barriers
    if (barrier->continueState.opcode != InternalErrorState)
        m_stateStack.push(barrier->continueState);
}

// ------------------------------ InternalFunctionImp --------------------------

const ClassInfo InternalFunctionImp::info = {"Function", 0, 0, 0};

InternalFunctionImp::InternalFunctionImp()
{
}

InternalFunctionImp::InternalFunctionImp(FunctionPrototype* funcProto)
  : JSObject(funcProto)
{
}

InternalFunctionImp::InternalFunctionImp(FunctionPrototype* funcProto, const Identifier& name)
  : JSObject(funcProto)
  , m_name(name)
{
}

bool InternalFunctionImp::implementsCall() const
{
  return true;
}

bool InternalFunctionImp::implementsHasInstance() const
{
  return true;
}

bool InternalFunctionImp::hasInstance(ExecState *exec, JSValue *value)
{
  if (!value->isObject())
    return false;

  JSValue *prot = get(exec,prototypePropertyName);
  if (!prot->isObject() && !prot->isNull()) {
    throwError(exec, TypeError, "Invalid prototype encountered in instanceof operation.");
    return false;
  }

  JSObject *v = static_cast<JSObject *>(value);
  while ((v = v->prototype()->getObject())) {
    if (v == prot)
      return true;
  }
  return false;
}

// ------------------------------ global functions -----------------------------

double roundValue(ExecState *exec, JSValue *v)
{
  double d = v->toNumber(exec);
  double ad = fabs(d);
  if (ad == 0 || isNaN(d) || isInf(d))
    return d;
  return copysign(floor(ad), d);
}

#ifndef NDEBUG
#include <stdio.h>
void printInfo(ExecState *exec, const char *s, JSValue *o, int lineno)
{
  if (!o)
    fprintf(stderr, "KJS: %s: (null)", s);
  else {
    JSValue *v = o;

    UString name;
    switch (v->type()) {
    case UnspecifiedType:
      name = "Unspecified";
      break;
    case UndefinedType:
      name = "Undefined";
      break;
    case NullType:
      name = "Null";
      break;
    case BooleanType:
      name = "Boolean";
      break;
    case StringType:
      name = "String";
      break;
    case NumberType:
      name = "Number";
      break;
    case ObjectType:
      name = static_cast<JSObject *>(v)->className();
      if (name.isNull())
        name = "(unknown class)";
      break;
    case GetterSetterType:
      name = "GetterSetter";
      break;
    }
    UString vString = v->toString(exec);
    if ( vString.size() > 50 )
      vString = vString.substr( 0, 50 ) + "...";
    // Can't use two UString::ascii() in the same fprintf call
    CString tempString( vString.cstring() );

    fprintf(stderr, "KJS: %s: %s : %s (%p)",
            s, tempString.c_str(), name.ascii(), (void*)v);

    if (lineno >= 0)
      fprintf(stderr, ", line %d\n",lineno);
    else
      fprintf(stderr, "\n");
  }
}
#endif

}
