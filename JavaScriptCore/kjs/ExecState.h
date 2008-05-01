// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef ExecState_h
#define ExecState_h

#include "LabelStack.h"
#include "LocalStorage.h"
#include "completion.h"
#include "list.h"
#include "scope_chain.h"

namespace KJS  {

    class CommonIdentifiers;
    class EvalNode;
    class FunctionBodyNode;
    class FunctionImp;
    class GlobalFuncImp;
    class Interpreter;
    class JSGlobalObject;
    class JSVariableObject;
    class ProgramNode;
    class ScopeNode;

    struct Instruction;
    
    // Passed as the first argument to most functions.
    class ExecState : Noncopyable {
    public:
        ExecState(JSGlobalObject*, JSObject* globalThisValue, ScopeChain& globalScopeChain);

        // Global object in which execution began.
        JSGlobalObject* dynamicGlobalObject() const { return m_globalObject; }
        
        // Global object in which the current script was defined. (Can differ
        // from dynamicGlobalObject() during function calls across frames.)
        JSGlobalObject* ExecState::lexicalGlobalObject() const
        {
            JSObject* object = m_scopeChain->bottom();
            ASSERT(isGlobalObject(object));
            return (JSGlobalObject*)object;
        }
        
        JSObject* globalThisValue() const { return m_globalThisValue; }
        
        // Exception propogation.
        void setExceptionSource(Instruction* source) { m_exceptionSource = source; }
        Instruction* exceptionSource() { return m_exceptionSource; }
        void clearExceptionSource() { m_exceptionSource = 0; }
        void setException(JSValue* exception) { m_exception = exception; }
        void clearException() { m_exception = 0; }
        JSValue* exception() const { return m_exception; }
        JSValue** exceptionSlot() { return &m_exception; }
        bool hadException() const { return !!m_exception; }
        
        // Fast access to commonly used data.
        const CommonIdentifiers& propertyNames() const { return *m_propertyNames; }
        const List& emptyList() const { return *m_emptyList; }

    private:
        bool isGlobalObject(JSObject*) const;
    
        JSGlobalObject* m_globalObject;
        JSObject* m_globalThisValue;

        JSValue* m_exception;
        Instruction* m_exceptionSource;

        const CommonIdentifiers* m_propertyNames;
        const List* m_emptyList;

        const ScopeChainNode* m_scopeChain;
    };

    // This code is now defunct:

    enum CodeType { GlobalCode, EvalCode, FunctionCode };
    class OldInterpreterExecState : public ExecState {
    public:
        void pushSwitch() { m_switchDepth++; }
        void popSwitch() { m_switchDepth--; }
        bool inSwitch() const { return (m_switchDepth > 0); }

        // These are only valid right after calling execute().
        ComplType completionType() const { return m_completionType; }
        const Identifier& breakOrContinueTarget() const
        {
            ASSERT(m_completionType == Break || m_completionType == Continue);
            return *m_breakOrContinueTarget;
        }

        // Only for use in the implementation of execute().
        void setCompletionType(ComplType type)
        {
            ASSERT(type != Break);
            ASSERT(type != Continue);
            m_completionType = type;
        }
        JSValue* setNormalCompletion()
        {
            ASSERT(!hadException());
            m_completionType = Normal;
            return 0;
        }
        JSValue* setNormalCompletion(JSValue* value)
        {
            ASSERT(!hadException());
            m_completionType = Normal;
            return value;
        }
        JSValue* setBreakCompletion(const Identifier* target)
        {
            ASSERT(!hadException());
            m_completionType = Break;
            m_breakOrContinueTarget = target;
            return 0;
        }
        JSValue* setContinueCompletion(const Identifier* target)
        {
            ASSERT(!hadException());
            m_completionType = Continue;
            m_breakOrContinueTarget = target;
            return 0;
        }
        JSValue* setReturnValueCompletion(JSValue* returnValue)
        {
            ASSERT(!hadException());
            ASSERT(returnValue);
            m_completionType = ReturnValue;
            return returnValue;
        }
        JSValue* setThrowCompletion(JSValue* exception)
        {
            ASSERT(!hadException());
            ASSERT(exception);
            m_completionType = Throw;
            return exception;
        }
        JSValue* setInterruptedCompletion()
        {
            ASSERT(!hadException());
            m_completionType = Interrupted;
            return 0;
        }
        CodeType codeType() { return m_codeType; }
        void pushIteration() { m_iterationDepth++; }
        void popIteration() { m_iterationDepth--; }
        bool inIteration() const { return (m_iterationDepth > 0); }
        LabelStack& seenLabels() { return m_labelStack; }
        void pushScope(JSObject* s) { m_scopeChain.push(s); }
        void popScope() { m_scopeChain.pop(); }
        JSVariableObject* variableObject() const { ASSERT_NOT_REACHED(); return m_variableObject; }
        void setVariableObject(JSVariableObject* v) { m_variableObject = v; }
        ExecState* callingExecState() { return m_callingExec; }
        ScopeNode* scopeNode() { return m_scopeNode; }
        const List* arguments() const { return m_arguments; }
        FunctionImp* function() const { return m_function; }
        LocalStorage& localStorage() { ASSERT_NOT_REACHED(); return *(LocalStorage*)0; }
        void setLocalStorage(LocalStorage*) { ASSERT_NOT_REACHED(); }
        ScopeChain& scopeChain() { return m_scopeChain; }
        JSObject* thisValue() const { return m_thisValue; }
        
        ComplType m_completionType;
        const Identifier* m_breakOrContinueTarget;
        int m_switchDepth;
        CodeType m_codeType;
        int m_iterationDepth;
        LabelStack m_labelStack;
        ScopeChainNode m_inlineScopeChainNode;
        ScopeChain m_scopeChain;
        JSVariableObject* m_variableObject;
        ScopeNode* m_scopeNode;
        const List* m_arguments;
        FunctionImp* m_function;
        ExecState* m_callingExec;
        JSObject* m_thisValue;
    };

} // namespace KJS

#endif // ExecState_h
