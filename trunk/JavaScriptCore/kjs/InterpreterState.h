// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * This file is part of the KDE libraries
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef InterpreterState_h
#define InterpreterState_h

namespace KJS {

// FIXME: this should probably just be a generated source file from a perl script.

#define EVALUATE_MACRO_FOR_EACH_EVALUATE_STATE(macro) \
    macro(InternalErrorState) \
    macro(NullNodeEvaluateState) \
    macro(BooleanNodeEvaluateState) \
    macro(NumberNodeEvaluateState) \
    macro(StringNodeEvaluateState) \
    macro(RegExpNodeEvaluateState) \
    macro(ThisNodeEvaluateState) \
    macro(ResolveNodeEvaluateState) \
    macro(GroupNodeEvaluateState) \
    macro(ElementNodeEvaluateState) \
    macro(ElementNodeEvaluateState1) \
    macro(ElementNodeEvaluateState2) \
    macro(ArrayNodeEvaluateState) \
    macro(ArrayNodeEvaluateState1) \
    macro(ObjectLiteralNodeEvaluateState) \
    macro(PropertyListNodeEvaluateState) \
    macro(PropertyListNodeEvaluateState1) \
    macro(PropertyListNodeEvaluateState2) \
    macro(PropertyNameNodeEvaluateState) \
    macro(BracketAccessorNodeEvaluateState) \
    macro(BracketAccessorNodeEvaluateState1) \
    macro(DotAccessorNodeEvaluateState) \
    macro(DotAccessorNodeEvaluateState1) \
    macro(NewExprNodeEvaluateState) \
    macro(NewExprNodeEvaluateState1) \
    macro(FunctionCallValueNodeEvaluateState) \
    macro(FunctionCallValueNodeEvaluateState1) \
    macro(FunctionCallValueNodeEvaluateState2) \
    macro(FunctionCallResolveNodeEvaluateState) \
    macro(FunctionCallResolveNodeEvaluateState1) \
    macro(FunctionCallBracketNodeEvaluateState) \
    macro(FunctionCallBracketNodeEvaluateState1) \
    macro(FunctionCallBracketNodeEvaluateState2) \
    macro(FunctionCallDotNodeEvaluateState) \
    macro(FunctionCallDotNodeEvaluateState1) \
    macro(FunctionCallDotNodeEvaluateState2) \
    macro(PostfixResolveNodeEvaluateState) \
    macro(PostfixBracketNodeEvaluateState) \
    macro(PostfixBracketNodeEvaluateState1) \
    macro(PostfixDotNodeEvaluateState) \
    macro(PostfixDotNodeEvaluateState1) \
    macro(DeleteResolveNodeEvaluateState) \
    macro(DeleteBracketNodeEvaluateState) \
    macro(DeleteBracketNodeEvaluateState1) \
    macro(DeleteDotNodeEvaluateState) \
    macro(DeleteDotNodeEvaluateState1) \
    macro(DeleteValueNodeEvaluateState) \
    macro(DeleteValueNodeEvaluateState1) \
    macro(VoidNodeEvaluateState) \
    macro(VoidNodeEvaluateState1) \
    macro(TypeOfResolveNodeEvaluateState) \
    macro(TypeOfValueNodeEvaluateState) \
    macro(TypeOfValueNodeEvaluateState1) \
    macro(PrefixResolveNodeEvaluateState) \
    macro(PrefixBracketNodeEvaluateState) \
    macro(PrefixBracketNodeEvaluateState1) \
    macro(PrefixDotNodeEvaluateState) \
    macro(PrefixDotNodeEvaluateState1) \
    macro(UnaryPlusNodeEvaluateState) \
    macro(UnaryPlusNodeEvaluateState1) \
    macro(NegateNodeEvaluateState) \
    macro(NegateNodeEvaluateState1) \
    macro(BitwiseNotNodeEvaluateState) \
    macro(BitwiseNotNodeEvaluateState1) \
    macro(LogicalNotNodeEvaluateState) \
    macro(LogicalNotNodeEvaluateState1) \
    macro(MultNodeEvaluateState) \
    macro(MultNodeEvaluateState1) \
    macro(AddNodeEvaluateState) \
    macro(AddNodeEvaluateState1) \
    macro(ShiftNodeEvaluateState) \
    macro(ShiftNodeEvaluateState1) \
    macro(RelationalNodeEvaluateState) \
    macro(RelationalNodeEvaluateState1) \
    macro(EqualNodeEvaluateState) \
    macro(EqualNodeEvaluateState1) \
    macro(BitOperNodeEvaluateState) \
    macro(BitOperNodeEvaluateState1) \
    macro(BinaryLogicalNodeEvaluateState) \
    macro(BinaryLogicalNodeEvaluateState1) \
    macro(ConditionalNodeEvaluateState) \
    macro(ConditionalNodeEvaluateState1) \
    macro(AssignResolveNodeEvaluateState) \
    macro(AssignResolveNodeEvaluateState1) \
    macro(AssignDotNodeEvaluateState) \
    macro(AssignDotNodeEvaluateState1) \
    macro(AssignDotNodeEvaluateState2) \
    macro(AssignDotNodeEvaluateState3) \
    macro(AssignBracketNodeEvaluateState) \
    macro(AssignBracketNodeEvaluateState1) \
    macro(CommaNodeEvaluateState) \
    macro(CommaNodeEvaluateState1) \
    macro(AssignExprNodeEvaluateState) \
    macro(VarDeclNodeEvaluateState) \
    macro(VarDeclNodeEvaluateState1) \
    macro(VarDeclListNodeEvaluateState) \
    macro(VarDeclListNodeEvaluateState1) \
    macro(CaseClauseNodeEvaluateState) \
    macro(FuncExprNodeEvaluateState)
// end of macro

#define EVALUATE_MACRO_FOR_EACH_EVALUATE_LIST_STATE(macro) \
    macro(ArgumentListNodeEvaluateListState) \
    macro(ArgumentListNodeEvaluateListState1) \
    macro(ArgumentListNodeEvaluateListState2) \
    macro(ArgumentsNodeEvaluateListState)
// end of macro
    
#define EVALUATE_MACRO_FOR_EACH_EXECUTE_STATE(macro) \
    macro(InvalidNodeExecuteState) \
    macro(StatListNodeExecuteState) \
    macro(VarStatementNodeExecuteState) \
    macro(BlockNodeExecuteState) \
    macro(EmptyStatementNodeExecuteState) \
    macro(ExprStatementNodeExecuteState) \
    macro(IfNodeExecuteState) \
    macro(DoWhileNodeExecuteState) \
    macro(WhileNodeExecuteState) \
    macro(ForNodeExecuteState) \
    macro(ForInNodeExecuteState) \
    macro(ContinueNodeExecuteState) \
    macro(BreakNodeExecuteState) \
    macro(ReturnNodeExecuteState) \
    macro(WithNodeExecuteState) \
    macro(SwitchNodeExecuteState) \
    macro(LabelNodeExecuteState) \
    macro(ThrowNodeExecuteState) \
    macro(TryNodeExecuteState) \
    macro(FuncDeclNodeExecuteState) \
    macro(SourceElementsNodeExecuteState)
// end of macro
    
enum InterpreterState {
#define PRINT_AS_ENUM(name)    name,
    EVALUATE_MACRO_FOR_EACH_EVALUATE_STATE(PRINT_AS_ENUM)
    
    Evaluate_EvaluteList_Boundary,
    
    EVALUATE_MACRO_FOR_EACH_EVALUATE_LIST_STATE(PRINT_AS_ENUM)
    
    EvaluateList_Execute_Boundary,

    EVALUATE_MACRO_FOR_EACH_EXECUTE_STATE(PRINT_AS_ENUM)
#undef PRINT_AS_ENUM

    LastInterpreterState
}; 

extern const char* nameForInterpreterState[LastInterpreterState+1];

}

#endif
