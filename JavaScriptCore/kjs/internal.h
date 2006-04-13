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

#include <kxmlcore/HashMap.h>
#include <kxmlcore/Noncopyable.h>
#include <kxmlcore/Vector.h>

#define I18N_NOOP(s) s

namespace KJS {

  class Debugger;
  class FunctionPrototype;
  class Node;
  class StatementNode;
    
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
    friend class BuiltInTypes;
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

    template <typename T, size_t inlineCapacity = 0>
    class Stack {
    public:
        T& peek() { return m_vector.last(); }
        T& peek(unsigned n) { return m_vector.at(size() - n - 1); }
        void push(const T& value) { m_vector.append(value); }
        T pop();
        size_t size() const { return m_vector.size(); }
        bool isEmpty() const { return m_vector.isEmpty(); }
        void shrinkTo(size_t newSize) { m_vector.shrinkTo(newSize); }
        T& operator[](unsigned i) { return m_vector.at(i); }
        const T& operator[](unsigned i) const { return m_vector.at(i); }
    private:
        Vector<T, inlineCapacity> m_vector;
    };

    template <typename T, size_t inlineCapacity>
    inline T Stack<T, inlineCapacity>::pop()
    {
        T value = m_vector.last();
        m_vector.removeLast();
        return value;
    }
    
    class BuiltInTypes {
    friend class InterpreterImp;
    private:
    
        void initBuiltIns(ExecState* globExec, JSObject* global);
        void saveBuiltins(SavedBuiltins&) const;
        void restoreBuiltins(const SavedBuiltins&);
        
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
    };

  class InterpreterImp {
    friend class Collector;
    friend Completion callExecuteOnNode(StatementNode*, ExecState*); // FIXME: remove once all callers are rolled into the interpreter
  public:
    InterpreterImp(Interpreter *interp, JSObject *glob);
    ~InterpreterImp();

    JSObject *globalObject() { return m_globalObject; }
    Interpreter *interpreter() const { return m_interpreter; }

    void initGlobalObject();

    void mark();

    ExecState* globalExec() { return &m_globalExecState; }
    bool checkSyntax(const UString &code);
    Completion evaluate(const UChar* code, int codeLength, JSValue* thisV, const UString& sourceURL, int startingLineNumber);
    Debugger* debugger() const { return m_debugger; }
    void setDebugger(Debugger* d) { m_debugger = d; }

    JSObject* builtinObject() const { return m_builtIns.b_Object; }
    JSObject* builtinFunction() const { return m_builtIns.b_Function; }
    JSObject* builtinArray() const { return m_builtIns.b_Array; }
    JSObject* builtinBoolean() const { return m_builtIns.b_Boolean; }
    JSObject* builtinString() const { return m_builtIns.b_String; }
    JSObject* builtinNumber() const { return m_builtIns.b_Number; }
    JSObject* builtinDate() const { return m_builtIns.b_Date; }
    JSObject* builtinRegExp() const { return m_builtIns.b_RegExp; }
    JSObject* builtinError() const { return m_builtIns.b_Error; }

    JSObject* builtinObjectPrototype() const { return m_builtIns.b_ObjectPrototype; }
    JSObject* builtinFunctionPrototype() const { return m_builtIns.b_FunctionPrototype; }
    JSObject* builtinArrayPrototype() const { return m_builtIns.b_ArrayPrototype; }
    JSObject* builtinBooleanPrototype() const { return m_builtIns.b_BooleanPrototype; }
    JSObject* builtinStringPrototype() const { return m_builtIns.b_StringPrototype; }
    JSObject* builtinNumberPrototype() const { return m_builtIns.b_NumberPrototype; }
    JSObject* builtinDatePrototype() const { return m_builtIns.b_DatePrototype; }
    JSObject* builtinRegExpPrototype() const { return m_builtIns.b_RegExpPrototype; }
    JSObject* builtinErrorPrototype() const { return m_builtIns.b_ErrorPrototype; }

    JSObject* builtinEvalError() const { return m_builtIns.b_evalError; }
    JSObject* builtinRangeError() const { return m_builtIns.b_rangeError; }
    JSObject* builtinReferenceError() const { return m_builtIns.b_referenceError; }
    JSObject* builtinSyntaxError() const { return m_builtIns.b_syntaxError; }
    JSObject* builtinTypeError() const { return m_builtIns.b_typeError; }
    JSObject* builtinURIError() const { return m_builtIns.b_uriError; }

    JSObject* builtinEvalErrorPrototype() const { return m_builtIns.b_evalErrorPrototype; }
    JSObject* builtinRangeErrorPrototype() const { return m_builtIns.b_rangeErrorPrototype; }
    JSObject* builtinReferenceErrorPrototype() const { return m_builtIns.b_referenceErrorPrototype; }
    JSObject* builtinSyntaxErrorPrototype() const { return m_builtIns.b_syntaxErrorPrototype; }
    JSObject* builtinTypeErrorPrototype() const { return m_builtIns.b_typeErrorPrototype; }
    JSObject* builtinURIErrorPrototype() const { return m_builtIns.b_uriErrorPrototype; }

    void setCompatMode(Interpreter::CompatMode mode) { m_compatMode = mode; }
    Interpreter::CompatMode compatMode() const { return m_compatMode; }

    static InterpreterImp *interpreterWithGlobalObject(JSObject*);
    
    void setContext(ContextImp* c) { m_context = c; }
    ContextImp* context() const { return m_context; }
    
    void saveBuiltins(SavedBuiltins& builtins) const { m_builtIns.saveBuiltins(builtins); }
    void restoreBuiltins(const SavedBuiltins& builtins) { m_builtIns.restoreBuiltins(builtins); }

    /* TreeCode Support */
    
    void runInterpreterLoop(ExecState*);
    
    // FIXME: this should be private once all callers are moved into the interpreter loop
    void resetCompletionToNormal() { m_completionReturn = Normal; }
    
    struct State {
        State() { } // Allow Stack<T> array-based allocation
        State(InterpreterState s, Node* n) : state(s), node(n) { }
        InterpreterState state;
        Node* node;
    };
    
    struct UnwindBarrier {
        UnwindBarrier() { } // Allow Stack<T> array-based allocation
        UnwindBarrier(short type, const State& state, size_t valueBase, size_t stateBase, size_t listBase, size_t nodeBase, size_t execBase)
            : barrierType(type)
            , continueState(state)
            , valueStackSize(valueBase)
            , stateStackSize(stateBase)
            , listStackSize(listBase)
            , nodeStackSize(nodeBase)
            , execStateStackSize(execBase)
        {
        }

        short barrierType;
        State continueState;
        size_t valueStackSize;
        size_t stateStackSize;
        size_t listStackSize;
        size_t nodeStackSize;
        size_t execStateStackSize;
    };

    typedef HashMap<JSObject*, InterpreterImp*> InterpreterMap;
    static InterpreterMap& interpreterMap();

  private:
    static InterpreterMap* s_interpreterMap;

    const Completion& completionReturn() { return m_completionReturn; }
    void setCompletionReturn(const Completion& c) { ASSERT(m_completionReturn.complType() == Normal); if (c.complType() != Normal || c.value()) m_completionReturn = c; }
    
    JSValue* valueReturn() { return m_valueReturn; }
    void setValueReturn(JSValue* v) { m_valueReturn = v; }
    
    // FIXME: ASSERT that enum/short conversion doesn't lose data
    void pushUnwindBarrier(short barrierType, State continueState)
    {
        UnwindBarrier unwindBarrier(barrierType, continueState, m_valueStack.size(), m_stateStack.size(), m_listStack.size(), m_nodeStack.size(), m_execStateStack.size());
        m_unwindBarrierStack.push(unwindBarrier);
    }
    void popUnwindBarrier()
    {
#ifndef NDEBUG
        const UnwindBarrier& unwindBarrier = m_unwindBarrierStack.peek();
        ASSERT(m_valueStack.size() == unwindBarrier.valueStackSize);
        ASSERT(m_stateStack.size() == unwindBarrier.stateStackSize);
        ASSERT(m_listStack.size() == unwindBarrier.listStackSize);
        ASSERT(m_nodeStack.size() == unwindBarrier.nodeStackSize);
        ASSERT(m_execStateStack.size() == unwindBarrier.execStateStackSize);
#endif
        m_unwindBarrierStack.pop();
    }
    void unwindToNextBarrier(ExecState*, Node* currentNode);
    
    void printStacks();
    void printStateStack();
    void printValueStack();
    void printListStack();
    void printUnwindBarrierStack();

    void clear();
    JSObject* m_globalObject;
    Interpreter* m_interpreter;
    Debugger* m_debugger;

    ExecState m_globalExecState;
    Interpreter::CompatMode m_compatMode;
    
    ContextImp* m_context;

    int m_recursion;
    
    Completion m_completionReturn;
    JSValue* m_valueReturn;
    Stack<UnwindBarrier, KJS_MAX_STACK> m_unwindBarrierStack;
public:
    Stack<State, KJS_MAX_STACK> m_stateStack;
private:
    Stack<JSValue*, KJS_MAX_STACK> m_valueStack;
    Stack<List, KJS_MAX_STACK> m_listStack;
    Stack<Node*, KJS_MAX_STACK> m_nodeStack;
    
    // FIXME: This stack does not yet contain all exec states in the call chain
    // Eventually globalObject.eval() as well as any other creators of ExecStates
    // should push thier ExecStates onto this stack.
    Stack<ExecState*> m_execStateStack;
    
    BuiltInTypes m_builtIns;
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
