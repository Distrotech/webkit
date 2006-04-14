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
    macro(NullNodeEvaluateState) \
    macro(BooleanNodeEvaluateState) \
    macro(NumberNodeEvaluateState) \
    macro(StringNodeEvaluateState) \
    macro(RegExpNodeEvaluateState) \
    macro(ThisNodeEvaluateState) \
    macro(ResolveValueEvaluateState) \
    macro(ResolveBaseEvaluateState) \
    macro(ResolveBaseAndValueEvaluateState) \
    macro(ArgumentEndNodeEvaluateState) \
    macro(ElementNodeEvaluateState) \
    macro(ElementNodeEvaluateState1) \
    macro(ElementNodeEvaluateState2) \
    macro(ElementEndNodeEvaluateState) \
    macro(ArrayNodeEvaluateState) \
    macro(ArrayNodeEvaluateState1) \
    macro(ObjectLiteralNodeEvaluateState) \
    macro(ObjectLiteralNodeEvaluateState1) \
    macro(PropertyListNodeEvaluateState) \
    macro(PropertyListNodeEvaluateState1) \
    macro(PropertyListNodeEvaluateState2) \
    macro(PropertyEndNodeEvaluateState) \
    macro(PropertyNameNodeEvaluateState) \
    macro(BracketAccessorValueEvaluateState) \
    macro(BracketAccessorBaseAndValueEvaluateState) \
    macro(BracketAccessorBaseSubscriptAndValueEvaluateState) \
    macro(BracketAccessorBaseAndSubscriptEvaluateState) \
    macro(DotAccessorValueEvaluateState) \
    macro(DotAccessorBaseEvaluateState) \
    macro(DotAccessorBaseAndValueEvaluateState) \
    macro(NewExprNodeEvaluateState) \
    macro(NewExprNodeEvaluateState1) \
    macro(FunctionCallNoBaseNodeEvaluateState) \
    macro(FunctionCallNodeEvaluateState) \
    macro(PostfixNodeEvaluateState) \
    macro(PostfixBracketNodeEvaluateState) \
    macro(DeleteNodeEvaluateState) \
    macro(DeleteBracketNodeEvaluateState) \
    macro(DeleteValueNodeEvaluateState) \
    macro(VoidNodeEvaluateState) \
    macro(TypeOfResolveNodeEvaluateState) \
    macro(TypeOfValueNodeEvaluateState) \
    macro(PrefixNodeEvaluateState) \
    macro(PrefixBracketNodeEvaluateState) \
    macro(UnaryPlusNodeEvaluateState) \
    macro(NegateNodeEvaluateState) \
    macro(BitwiseNotNodeEvaluateState) \
    macro(LogicalNotNodeEvaluateState) \
    macro(MultNodeEvaluateState) \
    macro(AddNodeEvaluateState) \
    macro(ShiftNodeEvaluateState) \
    macro(RelationalNodeEvaluateState) \
    macro(EqualNodeEvaluateState) \
    macro(BitOperNodeEvaluateState) \
    macro(BinaryLogicalNodeEvaluateState) \
    macro(BinaryLogicalNodeEvaluateState1) \
    macro(NoOpEvaluateState) \
    macro(JumpEvaluateState) \
    macro(JumpIfFalseEvaluateState) \
    macro(JumpAndSaveIfFalseEvaluateState) \
    macro(JumpAndSaveIfTrueEvaluateState) \
    macro(ReadModifyAssignNodeEvaluateState) \
    macro(AssignNodeEvaluateState) \
    macro(AssignBracketNodeEvaluateState) \
    macro(ReadModifyAssignBracketNodeEvaluateState) \
    macro(CommaNodeEvaluateState) \
    macro(AssignExprNodeEvaluateState) \
    macro(VarDeclNodeEvaluateState) \
    macro(VarDeclEndNodeEvaluateState) \
    macro(VarDeclListNodeEvaluateState) \
    macro(VarDeclListNodeEvaluateState1) \
    macro(CaseClauseNodeEvaluateState) \
    macro(FuncExprNodeEvaluateState) \
    macro(ExprStatementEndNodeExecuteState) \
    macro(ValueReturnEndNodeExecuteState) \
    macro(ThrowEndNodeExecuteState) \
    macro(IfConditionEndNodeExecuteState) \
    macro(WithExprEndNodeExecuteState) \
    macro(ForNodeInitOrIncrementEndState) \
    macro(ForNodeVarDeclEndState) \
    macro(ForNodeTestEndState)
// end of macro

#define EVALUATE_MACRO_FOR_EACH_EVALUATE_LIST_STATE(macro) \
    macro(ArgumentListNodeEvaluateListState) \
    macro(ArgumentListNodeEvaluateListState1) \
    macro(ArgumentListNodeEvaluateListState2) \
    macro(ArgumentsNodeEvaluateListState)
// end of macro
    
#define EVALUATE_MACRO_FOR_EACH_EXECUTE_STATE(macro) \
    macro(StatListNodeExecuteState) \
    macro(StatListNodeExecuteState1) \
    macro(StatListNodeExecuteState2) \
    macro(VarStatementNodeExecuteState) \
    macro(BlockNodeExecuteState) \
    macro(EmptyStatementNodeExecuteState) \
    macro(ExprStatementNodeExecuteState) \
    macro(IfNodeExecuteState) \
    macro(WhileNodeExecuteState) \
    macro(WhileNodeExecuteContinueState) \
    macro(WhileNodeExecuteTestState) \
    macro(WhileTestEndNodeExecuteState) \
    macro(WhileNodeExecuteEndState) \
    macro(DoWhileNodeExecuteState) \
    macro(DoWhileNodeExecuteBodyState) \
    macro(DoWhileNodeExecuteContinueState) \
    macro(DoWhileNodeExecuteTestState) \
    macro(DoWhileTestEndNodeExecuteState) \
    macro(DoWhileNodeExecuteEndState) \
    macro(ForNodeVarDeclExecuteState) \
    macro(ForNodeExecuteState) \
    macro(ForNodeExecuteTestState) \
    macro(ForNodeExecuteContinueState) \
    macro(ForNodeExecutePostBodyState) \
    macro(ForNodeExecuteEndState) \
    macro(ForInNodeExecuteState) \
    macro(ForInNodeExecuteState2) \
    macro(ForInNodeExecuteState3) \
    macro(ForInNodeResolveNodeExecuteState) \
    macro(ForInNodeDotAccessorNodeExecuteState) \
    macro(ForInNodeDotAccessorNodeExecuteState1) \
    macro(ForInNodeBracketAccessorNodeExecuteState) \
    macro(ForInNodeBracketAccessorNodeExecuteState1) \
    macro(ForInNodeBracketAccessorNodeExecuteState2) \
    macro(ForInNodeExecuteBodyState) \
    macro(ForInNodeExecutePopContinueUnwindBarrierState) \
    macro(ForInNodeExecutePopBreakUnwindBarrierState) \
    macro(ContinueNodeExecuteState) \
    macro(BreakNodeExecuteState) \
    macro(ReturnNodeExecuteState) \
    macro(WithNodeExecuteState) \
    macro(WithNodeExecuteState2) \
    macro(SwitchNodeExecuteState) \
    macro(SwitchNodeExecuteState1) \
    macro(SwitchNodeExecuteState2) \
    macro(LabelNodeExecuteState) \
    macro(LabelNodeExecuteState1) \
    macro(TryNodeExecuteState) \
    macro(TryNodeExecuteExceptionThrownState) \
    macro(TryNodeExecutePopScopeAfterCatchBlockState) \
    macro(TryNodeExecuteNoExceptionThrownState) \
    macro(TryNodeExecuteRethrowAfterFinallyState) \
    macro(FuncDeclNodeExecuteState) \
    macro(SourceElementsNodeExecuteState) \
    macro(SourceElementsNodeExecuteState1) \
    macro(CaseBlockNodeExecuteBlockWithInputValue) \
    macro(CaseBlockNodeExecuteBlockWithInputValue1) \
    macro(CaseBlockNodeExecuteBlockWithInputValue2) \
    macro(CaseBlockNodeExecuteBlockWithInputValue3) \
    macro(CaseBlockNodeExecuteBlockWithInputValue4) \
    macro(CaseBlockNodeExecuteBlockWithInputValue5) \
    macro(CaseBlockNodeExecuteBlockWithInputValue6) \
    macro(CaseBlockNodeExecuteBlockWithInputValue7) \
    macro(CaseBlockNodeExecuteBlockWithInputValue8) \
    macro(CaseBlockNodeExecuteBlockWithInputValue9) \
    macro(CaseBlockNodeExecuteBlockWithInputValue10) \
    macro(CaseBlockNodeExecuteBlockWithInputValue11)
// end of macro
    
enum InterpreterState {
    InternalErrorState,
    
#define PRINT_AS_ENUM(name)    name,
    EVALUATE_MACRO_FOR_EACH_EVALUATE_STATE(PRINT_AS_ENUM)
    
    Evaluate_EvaluteList_Boundary,
    
    EVALUATE_MACRO_FOR_EACH_EVALUATE_LIST_STATE(PRINT_AS_ENUM)
    
    EvaluateList_Execute_Boundary,

    EVALUATE_MACRO_FOR_EACH_EXECUTE_STATE(PRINT_AS_ENUM)
#undef PRINT_AS_ENUM

    JSObjectCallState,
    DeclaredFunctionCallState,
    DeclaredFunctionCallEndState,
    DeclaredFunctionCallExplicitReturnState,
    GlobalFuncCallEvalState,
    GlobalFuncCallEvalEndState,

    LastInterpreterState
}; 

extern const char* nameForInterpreterState[LastInterpreterState+1];

}

#endif
