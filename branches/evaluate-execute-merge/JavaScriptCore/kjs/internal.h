// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef INTERNAL_H
#define INTERNAL_H

#include "JSType.h"
#include "interpreter.h"
#include "object.h"
#include "protect.h"
#include "scope_chain.h"
#include "types.h"
#include "ustring.h"
#include "InterpreterState.h"

#include <kxmlcore/Noncopyable.h>
#include <kxmlcore/RefPtr.h>
#include <kxmlcore/Vector.h>

#define I18N_NOOP(s) s

namespace KJS {

  class Debugger;
  class FunctionPrototype;
  class Node;
    
  // ---------------------------------------------------------------------------
  //                            Primitive impls
  // ---------------------------------------------------------------------------

  class StringImp : public JSCell {
  public:
    StringImp(const UString& v) : val(v) { }
    UString value() const { return val; }

    JSType type() const { return StringType; }

    JSValue *toPrimitive(ExecState *exec, JSType preferred = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    UString toString(ExecState *exec) const;
    JSObject *toObject(ExecState *exec) const;

  private:
    UString val;
  };

  class NumberImp : public JSCell {
    friend class ConstantValues;
    friend class InterpreterImp;
    friend JSValue *jsNumberCell(double);
  public:
    double value() const { return val; }

    JSType type() const { return NumberType; }

    JSValue *toPrimitive(ExecState *exec, JSType preferred = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    UString toString(ExecState *exec) const;
    JSObject *toObject(ExecState *exec) const;

  private:
    NumberImp(double v) : val(v) { }

    virtual bool getUInt32(uint32_t&) const;

    double val;
  };
  

  /**
   * @short The "label set" in Ecma-262 spec
   */
  class LabelStack : Noncopyable {
  public:
    LabelStack()
      : tos(0)
    {
    }
    ~LabelStack();

    /**
     * If id is not empty and is not in the stack already, puts it on top of
     * the stack and returns true, otherwise returns false
     */
    bool push(const Identifier &id);
    /**
     * Is the id in the stack?
     */
    bool contains(const Identifier &id) const;
    /**
     * Removes from the stack the last pushed id (what else?)
     */
    void pop();
    
  private:
    struct StackElem {
      Identifier id;
      StackElem *prev;
    };

    StackElem *tos;
  };


  // ---------------------------------------------------------------------------
  //                            Evaluation
  // ---------------------------------------------------------------------------

  enum CodeType { GlobalCode,
                  EvalCode,
                  FunctionCode,
                  AnonymousCode };

  class SavedBuiltinsInternal {
    friend class InterpreterImp;
  private:
    ProtectedPtr<JSObject> b_Object;
    ProtectedPtr<JSObject> b_Function;
    ProtectedPtr<JSObject> b_Array;
    ProtectedPtr<JSObject> b_Boolean;
    ProtectedPtr<JSObject> b_String;
    ProtectedPtr<JSObject> b_Number;
    ProtectedPtr<JSObject> b_Date;
    ProtectedPtr<JSObject> b_RegExp;
    ProtectedPtr<JSObject> b_Error;

    ProtectedPtr<JSObject> b_ObjectPrototype;
    ProtectedPtr<JSObject> b_FunctionPrototype;
    ProtectedPtr<JSObject> b_ArrayPrototype;
    ProtectedPtr<JSObject> b_BooleanPrototype;
    ProtectedPtr<JSObject> b_StringPrototype;
    ProtectedPtr<JSObject> b_NumberPrototype;
    ProtectedPtr<JSObject> b_DatePrototype;
    ProtectedPtr<JSObject> b_RegExpPrototype;
    ProtectedPtr<JSObject> b_ErrorPrototype;

    ProtectedPtr<JSObject> b_evalError;
    ProtectedPtr<JSObject> b_rangeError;
    ProtectedPtr<JSObject> b_referenceError;
    ProtectedPtr<JSObject> b_syntaxError;
    ProtectedPtr<JSObject> b_typeError;
    ProtectedPtr<JSObject> b_uriError;

    ProtectedPtr<JSObject> b_evalErrorPrototype;
    ProtectedPtr<JSObject> b_rangeErrorPrototype;
    ProtectedPtr<JSObject> b_referenceErrorPrototype;
    ProtectedPtr<JSObject> b_syntaxErrorPrototype;
    ProtectedPtr<JSObject> b_typeErrorPrototype;
    ProtectedPtr<JSObject> b_uriErrorPrototype;
  };
  
#define STACK_USES_ARRAY 0

  template <typename T, size_t inlineCapacity = 0>
      class Stack {
    public:
#if STACK_USES_ARRAY
          Stack() : m_stackTop(m_stackBase - 1) { }
          T& peek() { ASSERT(!isEmpty()); return *m_stackTop;}
          void push(const T& value) {  ASSERT(size() != inlineCapacity); *(++m_stackTop) = value; }
          T pop() { ASSERT(!isEmpty()); return *m_stackTop--; }
          size_t size() const { return m_stackTop - m_stackBase + 1; }
          bool isEmpty() const { return (m_stackTop < m_stackBase); }
          void shrinkTo(size_t newSize) { m_stackTop = m_stackBase + newSize - 1; }
          T& operator[](unsigned i) { ASSERT(i < inlineCapacity); return m_stackBase[i]; }
          const T& operator[](unsigned i) const { ASSERT(i < inlineCapacity); return m_stackBase[i]; }
    private:
        T m_stackBase[inlineCapacity];
        T* m_stackTop; // pointer to the last pushed value
#else
          T& peek() { return m_vector.last(); }
          void push(const T& value) { m_vector.append(value); }
          T pop();
          size_t size() const { return m_vector.size(); }
          bool isEmpty() const { return m_vector.isEmpty(); }
          // FIXME: Resize should be removed, replace with a multi-pop/shrink operation
          void shrinkTo(size_t newSize) { m_vector.resize(newSize); }
          T& operator[](unsigned i) { return m_vector.at(i); }
          const T& operator[](unsigned i) const { return m_vector.at(i); }
    private:
        Vector<T, inlineCapacity> m_vector;
#endif
      };

#if !STACK_USES_ARRAY
  template <typename T, size_t inlineCapacity>
      inline T Stack<T, inlineCapacity>::pop()
  {
          T value = m_vector.last();
          m_vector.removeLast();
          return value;
  }
#endif

  class InterpreterImp {
    friend class Collector;
  public:
    InterpreterImp(Interpreter *interp, JSObject *glob);
    ~InterpreterImp();

    JSObject *globalObject() { return global; }
    Interpreter *interpreter() const { return m_interpreter; }

    void initGlobalObject();

    void mark();

    ExecState *globalExec() { return &globExec; }
    bool checkSyntax(const UString &code);
    Completion evaluate(const UChar* code, int codeLength, JSValue* thisV, const UString& sourceURL, int startingLineNumber);
    Debugger *debugger() const { return dbg; }
    void setDebugger(Debugger *d) { dbg = d; }

    JSObject *builtinObject() const { return b_Object; }
    JSObject *builtinFunction() const { return b_Function; }
    JSObject *builtinArray() const { return b_Array; }
    JSObject *builtinBoolean() const { return b_Boolean; }
    JSObject *builtinString() const { return b_String; }
    JSObject *builtinNumber() const { return b_Number; }
    JSObject *builtinDate() const { return b_Date; }
    JSObject *builtinRegExp() const { return b_RegExp; }
    JSObject *builtinError() const { return b_Error; }

    JSObject *builtinObjectPrototype() const { return b_ObjectPrototype; }
    JSObject *builtinFunctionPrototype() const { return b_FunctionPrototype; }
    JSObject *builtinArrayPrototype() const { return b_ArrayPrototype; }
    JSObject *builtinBooleanPrototype() const { return b_BooleanPrototype; }
    JSObject *builtinStringPrototype() const { return b_StringPrototype; }
    JSObject *builtinNumberPrototype() const { return b_NumberPrototype; }
    JSObject *builtinDatePrototype() const { return b_DatePrototype; }
    JSObject *builtinRegExpPrototype() const { return b_RegExpPrototype; }
    JSObject *builtinErrorPrototype() const { return b_ErrorPrototype; }

    JSObject *builtinEvalError() const { return b_evalError; }
    JSObject *builtinRangeError() const { return b_rangeError; }
    JSObject *builtinReferenceError() const { return b_referenceError; }
    JSObject *builtinSyntaxError() const { return b_syntaxError; }
    JSObject *builtinTypeError() const { return b_typeError; }
    JSObject *builtinURIError() const { return b_uriError; }

    JSObject *builtinEvalErrorPrototype() const { return b_evalErrorPrototype; }
    JSObject *builtinRangeErrorPrototype() const { return b_rangeErrorPrototype; }
    JSObject *builtinReferenceErrorPrototype() const { return b_referenceErrorPrototype; }
    JSObject *builtinSyntaxErrorPrototype() const { return b_syntaxErrorPrototype; }
    JSObject *builtinTypeErrorPrototype() const { return b_typeErrorPrototype; }
    JSObject *builtinURIErrorPrototype() const { return b_uriErrorPrototype; }

    void setCompatMode(Interpreter::CompatMode mode) { m_compatMode = mode; }
    Interpreter::CompatMode compatMode() const { return m_compatMode; }

    // Chained list of interpreters (ring)
    static InterpreterImp* firstInterpreter() { return s_hook; }
    InterpreterImp *nextInterpreter() const { return next; }
    InterpreterImp *prevInterpreter() const { return prev; }

    static InterpreterImp *interpreterWithGlobalObject(JSObject *);
    
    void setContext(ContextImp *c) { _context = c; }
    ContextImp *context() const { return _context; }

    void saveBuiltins (SavedBuiltins &builtins) const;
    void restoreBuiltins (const SavedBuiltins &builtins);
            
    JSValue*& peekValueReturn() { return m_valueReturnStack.peek(); }
    JSValue* popValueReturn() { return m_valueReturnStack.pop(); }
    void pushValueReturn(JSValue* value) { m_valueReturnStack.push(value); }
    unsigned valueStackDepth() { return m_valueReturnStack.size(); }
    
    struct State {
        State() { } // Allow Stack<T> array-based allocation
        State(InterpreterState s, Node* n) : state(s), node(n) { }
        InterpreterState state;
        Node* node;
    };

    const Completion& peekCompletionReturn() { return m_completionReturn; }
    Completion popCompletionReturn() { return m_completionReturn; }
    void pushCompletionReturn(const Completion& c) { m_completionReturn = c; }
        
    const State& peekNextState() { return m_stateStack.peek(); }
    State popNextState() { return m_stateStack.pop(); }
    void pushNextState(const State& state) { m_stateStack.push(state); }
    unsigned stateStackDepth() { return m_stateStack.size(); }
    
    struct UnwindBarrier {
        UnwindBarrier() { } // Allow Stack<T> array-based allocation
        UnwindBarrier(short type, size_t valueBase, size_t stateBase, size_t listBase, size_t nodeBase)
            : barrierType(type)
            , valueStackSize(valueBase)
            , stateStackSize(stateBase)
            , listStackSize(listBase)
            , nodeStackSize(nodeBase) { }
        short barrierType;
        size_t valueStackSize;
        size_t stateStackSize;
        size_t listStackSize;
        size_t nodeStackSize;
    };
    
    const UnwindBarrier& peekUnwindBarrier()
    {
        return m_unwindBarrierStack.peek();
    }
    
    void pushUnwindBarrier(short barrierType)
    {
        UnwindBarrier unwindBarrier(barrierType, valueStackDepth(), stateStackDepth(), listStackDepth(), nodeStackDepth());
        m_unwindBarrierStack.push(unwindBarrier);
    }
    void popUnwindBarrier()
    {
#ifndef NDEBUG
        const UnwindBarrier& unwindBarrier = m_unwindBarrierStack.peek();
        ASSERT(valueStackDepth() == unwindBarrier.valueStackSize);
        ASSERT(stateStackDepth() == unwindBarrier.stateStackSize);
        ASSERT(listStackDepth() == unwindBarrier.listStackSize);
        ASSERT(nodeStackDepth() == unwindBarrier.nodeStackSize);
#endif
        m_unwindBarrierStack.pop();
    }
    void unwindToNextBarrier(ExecState* exec, Node* currentNode);
        
    List& peekListReturn() { return m_listReturnStack.peek(); }
    List popListReturn() { return m_listReturnStack.pop(); }
    void pushListReturn(const List& list) { m_listReturnStack.push(list); }
    unsigned listStackDepth() { return m_listReturnStack.size(); }
    
    Node*& peekNodeReturn() { return m_nodeStack.peek(); }
    Node* popNodeReturn() { return m_nodeStack.pop(); }
    void pushNodeReturn(Node* node) { m_nodeStack.push(node); }
    unsigned nodeStackDepth() { return m_nodeStack.size(); }
    
    void printStateStack();
    void printValueStack();
    void printListStack();

  private:
    void clear();
    Interpreter *m_interpreter;
    JSObject *global;
    Debugger *dbg;

    // Built-in properties of the object prototype. These are accessible
    // from here even if they are replaced by js code (e.g. assigning to
    // Array.prototype)

    ProtectedPtr<JSObject> b_Object;
    ProtectedPtr<JSObject> b_Function;
    ProtectedPtr<JSObject> b_Array;
    ProtectedPtr<JSObject> b_Boolean;
    ProtectedPtr<JSObject> b_String;
    ProtectedPtr<JSObject> b_Number;
    ProtectedPtr<JSObject> b_Date;
    ProtectedPtr<JSObject> b_RegExp;
    ProtectedPtr<JSObject> b_Error;

    ProtectedPtr<JSObject> b_ObjectPrototype;
    ProtectedPtr<JSObject> b_FunctionPrototype;
    ProtectedPtr<JSObject> b_ArrayPrototype;
    ProtectedPtr<JSObject> b_BooleanPrototype;
    ProtectedPtr<JSObject> b_StringPrototype;
    ProtectedPtr<JSObject> b_NumberPrototype;
    ProtectedPtr<JSObject> b_DatePrototype;
    ProtectedPtr<JSObject> b_RegExpPrototype;
    ProtectedPtr<JSObject> b_ErrorPrototype;

    ProtectedPtr<JSObject> b_evalError;
    ProtectedPtr<JSObject> b_rangeError;
    ProtectedPtr<JSObject> b_referenceError;
    ProtectedPtr<JSObject> b_syntaxError;
    ProtectedPtr<JSObject> b_typeError;
    ProtectedPtr<JSObject> b_uriError;

    ProtectedPtr<JSObject> b_evalErrorPrototype;
    ProtectedPtr<JSObject> b_rangeErrorPrototype;
    ProtectedPtr<JSObject> b_referenceErrorPrototype;
    ProtectedPtr<JSObject> b_syntaxErrorPrototype;
    ProtectedPtr<JSObject> b_typeErrorPrototype;
    ProtectedPtr<JSObject> b_uriErrorPrototype;

    ExecState globExec;
    Interpreter::CompatMode m_compatMode;

    // Chained list of interpreters (ring) - for collector
    static InterpreterImp* s_hook;
    InterpreterImp *next, *prev;
    
    ContextImp *_context;

    int recursion;
    
    Completion m_completionReturn;
    Stack<UnwindBarrier, KJS_MAX_STACK> m_unwindBarrierStack;
    Stack<State, KJS_MAX_STACK> m_stateStack;
    Stack<JSValue*, KJS_MAX_STACK> m_valueReturnStack;
    Stack<List, KJS_MAX_STACK> m_listReturnStack;
    Stack<Node*, KJS_MAX_STACK> m_nodeStack;
  };

  class AttachedInterpreter;
  class DebuggerImp {
  public:

    DebuggerImp() {
      interps = 0;
      isAborted = false;
    }

    void abort() { isAborted = true; }
    bool aborted() const { return isAborted; }

    AttachedInterpreter *interps;
    bool isAborted;
  };

  class InternalFunctionImp : public JSObject {
  public:
    InternalFunctionImp();
    InternalFunctionImp(FunctionPrototype*);
    InternalFunctionImp(FunctionPrototype*, const Identifier&);

    virtual bool implementsCall() const;
    virtual JSValue* callAsFunction(ExecState*, JSObject* thisObjec, const List& args) = 0;
    virtual bool implementsHasInstance() const;
    virtual bool hasInstance(ExecState*, JSValue*);

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    const Identifier& functionName() const { return m_name; }

  private:
    Identifier m_name;
  };

  // helper function for toInteger, toInt32, toUInt32 and toUInt16
  double roundValue(ExecState *, JSValue *);

#ifndef NDEBUG
  void printInfo(ExecState *exec, const char *s, JSValue *, int lineno = -1);
#endif

inline LabelStack::~LabelStack()
{
    StackElem *prev;
    for (StackElem *e = tos; e; e = prev) {
        prev = e->prev;
        delete e;
    }
}

inline void LabelStack::pop()
{
    if (StackElem *e = tos) {
        tos = e->prev;
        delete e;
    }
}

} // namespace

namespace KXMLCore {
    template<> struct IsPod<KJS::InterpreterImp::State> { static const bool value = true; };
    template<> struct IsPod<KJS::InterpreterImp::UnwindBarrier> { static const bool value = true; };
}

#endif //  INTERNAL_H
