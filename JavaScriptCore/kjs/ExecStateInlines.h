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

#ifndef ExecStateInlines_h
#define ExecStateInlines_h

#include "ExecState.h"
#include "function.h"
#include "JSGlobalObject.h"

namespace KJS  {

    inline ExecState::ExecState(JSGlobalObject* globalObject, JSObject* thisObject, JSObject* globalThisValue,
                                FunctionBodyNode* functionBodyNode, ExecState* callingExec,
                                FunctionImp* func, const List& args)
        : m_globalObject(globalObject)
        , m_exception(0)
        , m_callingExec(callingExec)
        , m_perThreadData(callingExec->m_perThreadData)
        , m_scopeNode(functionBodyNode)
        , m_function(func)
        , m_arguments(&args)
        , m_scopeChain(func->scope())
        , m_inlineScopeChainNode(0, 0)
        , m_thisValue(thisObject)
        , m_globalThisValue(globalThisValue)
        , m_iterationDepth(0)
        , m_switchDepth(0) 
        , m_codeType(FunctionCode)
    {
        ASSERT(m_scopeNode);
        
        m_activation = 0;
        m_variableObject = 0;
    }

    inline ExecState::~ExecState()
    {
    }

    inline FunctionExecState::FunctionExecState(JSGlobalObject* globalObject, JSObject* thisObject, JSObject* globalThisValue,
                                                FunctionBodyNode* functionBodyNode, ExecState* callingExec,
                                                FunctionImp* func, const List& args)
        : ExecState(globalObject, thisObject, globalThisValue, functionBodyNode, callingExec, func, args)
    {
        m_globalObject->activeExecStates().append(this);
    }

    inline FunctionExecState::~FunctionExecState()
    {
        ASSERT(m_globalObject->activeExecStates().last() == this);
        m_globalObject->activeExecStates().removeLast();
    }

} // namespace KJS

#endif // ExecStateInlines_h
