// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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

#ifndef NODES_H_
#define NODES_H_

#include "InterpreterState.h"
#include "Parser.h"
#include "internal.h"
#include <kxmlcore/ListRefPtr.h>

namespace KJS {

  struct ProgramNode;
  struct PropertyNameNode;
  struct PropertyListNode;
  struct StatementNode;
  class Reference;
  class RegExp;
  class SourceElementsNode;
  class SourceStream;
  
  enum Operator { OpEqual,
                  OpEqEq,
                  OpNotEq,
                  OpStrEq,
                  OpStrNEq,
                  OpPlusEq,
                  OpMinusEq,
                  OpMultEq,
                  OpDivEq,
                  OpPlusPlus,
                  OpMinusMinus,
                  OpLess,
                  OpLessEq,
                  OpGreater,
                  OpGreaterEq,
                  OpAndEq,
                  OpXOrEq,
                  OpOrEq,
                  OpModEq,
                  OpAnd,
                  OpOr,
                  OpBitAnd,
                  OpBitXOr,
                  OpBitOr,
                  OpLShift,
                  OpRShift,
                  OpURShift,
                  OpIn,
                  OpInstanceOf
  };

  struct Node {
    explicit Node(InterpreterState);
    virtual ~Node();

    UString toString() const;
    virtual void streamTo(SourceStream&) const = 0;
    virtual void processVarDecls(ExecState*) {}
    int lineNo() const { return m_line; }

    void ref();
    void deref();
    unsigned refcount();
    static void clearNewNodes();

    InterpreterState interpreterState() { return (InterpreterState)m_interpreterState; }

    virtual bool isLocation() const { return false; }
    virtual bool isResolveNode() const { return false; }
    virtual bool isBracketAccessorNode() const { return false; }
    virtual bool isDotAccessorNode() const { return false; }
    virtual bool isGroupNode() const { return false; }
    virtual bool isLabelNode() const { return false; }

    virtual void breakCycle() { }

    Completion createErrorCompletion(ExecState*, ErrorType, const char *msg);
    Completion createErrorCompletion(ExecState*, ErrorType, const char *msg, const Identifier &);

    JSValue *throwError(ExecState*, ErrorType, const char *msg);
    JSValue *throwError(ExecState*, ErrorType, const char *msg, JSValue *, Node *);
    JSValue *throwError(ExecState*, ErrorType, const char *msg, const Identifier &);
    JSValue *throwError(ExecState*, ErrorType, const char *msg, JSValue *);
    JSValue *throwError(ExecState*, ErrorType, const char *msg, JSValue *, const Identifier &);
    JSValue *throwError(ExecState*, ErrorType, const char *msg, JSValue *, Node *, Node *);
    JSValue *throwError(ExecState*, ErrorType, const char *msg, JSValue *, Node *, const Identifier &);

    JSValue *throwUndefinedVariableError(ExecState*, const Identifier &);

    void setExceptionDetailsIfNeeded(ExecState*);

    short m_line;
    short m_interpreterState;
    
  private:
    // prohibit these operations
    Node& operator=(const Node&);
    Node(const Node &other);
  };

  struct ExprNode : public Node {
    ExprNode(InterpreterState state) : Node(state) {}
    ExprNode* next() { return m_next.get(); }
    virtual void streamTo(SourceStream&) const;

    RefPtr<ExprNode> m_next;
  };

  struct StatementNode : public Node {
    explicit StatementNode(InterpreterState state) : Node(state), m_lastLine(-1) { m_line = -1; }
    void setLoc(int line0, int line1);
    int firstLine() const { return lineNo(); }
    int lastLine() const { return m_lastLine; }

    void pushLabel(const Identifier &id) { ls.push(id); }
    virtual void processFuncDecl(ExecState*);
    LabelStack ls;
    int m_lastLine;
  };

  struct ExprStatementEndNode : public ExprNode {
    ExprStatementEndNode() : ExprNode(ExprStatementEndNodeExecuteState) {}
    virtual void streamTo(SourceStream&) const {}
  };

  struct ArgumentListNode;

  struct ArgumentEndNode : public ExprNode {
    ArgumentEndNode(ArgumentListNode* a) : ExprNode(ArgumentEndNodeEvaluateState), m_argumentList(a) {}
    virtual void streamTo(SourceStream&) const {}
    ArgumentListNode* m_argumentList;
  };

  struct PropertyEndNode : public ExprNode {
    PropertyEndNode(PropertyListNode* p) : ExprNode(PropertyEndNodeEvaluateState), m_propertyList(p) {}
    virtual void streamTo(SourceStream&) const {}
    PropertyListNode* m_propertyList;
  };

  struct ElementNode;

  struct ElementEndNode : public ExprNode {
    ElementEndNode(ElementNode* e) : ExprNode(ElementEndNodeEvaluateState), m_element(e) {}
    virtual void streamTo(SourceStream&) const {}
    ElementNode* m_element;
  };
  
  struct NullNode : public ExprNode {
    NullNode() : ExprNode(NullNodeEvaluateState) {}
    virtual void streamTo(SourceStream&) const;
  };

  struct BooleanNode : public ExprNode {
    BooleanNode(bool v) : ExprNode(BooleanNodeEvaluateState), value(v) {}
    virtual void streamTo(SourceStream&) const;
    bool value;
  };

  struct NumberNode : public ExprNode {
    NumberNode(double v) : ExprNode(NumberNodeEvaluateState), value(v) {}
    virtual void streamTo(SourceStream&) const;
    double value;
  };

  struct StringNode : public ExprNode {
    StringNode(const UString *v) : ExprNode(StringNodeEvaluateState) { value = *v; }
    virtual void streamTo(SourceStream&) const;
    UString value;
  };

  struct RegExpNode : public ExprNode {
    RegExpNode(const UString &p, const UString &f)
      : ExprNode(RegExpNodeEvaluateState), pattern(p), flags(f) { }
    virtual void streamTo(SourceStream&) const;
    UString pattern, flags;
  };

  struct ThisNode : public ExprNode {
    ThisNode() : ExprNode(ThisNodeEvaluateState) {}
    virtual void streamTo(SourceStream&) const;
  };

  struct ResolveNode : public ExprNode {
    ResolveNode(const Identifier &s) : ExprNode(ResolveValueEvaluateState), ident(s) { }
    virtual void streamTo(SourceStream&) const;

    virtual bool isLocation() const { return true; }
    virtual bool isResolveNode() const { return true; }
    const Identifier& identifier() const { return ident; }

    Identifier ident;
  };

  struct ElementNode : public Node {
    // list pointer is tail of a circular list, cracked in the ArrayNode ctor
    ElementNode(int e, Node *n) : Node(ElementNodeEvaluateState), next(this), elision(e), node(n) { Parser::noteNodeCycle(this); }
    ElementNode(ElementNode *l, int e, Node *n)
      : Node(ElementNodeEvaluateState), next(l->next), elision(e), node(n) { l->next = this; }
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<ElementNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    ListRefPtr<ElementNode> next;
    int elision;
    RefPtr<Node> node;
  };

  struct ArrayNode : public ExprNode {
    ArrayNode(int e) : ExprNode(ArrayNodeEvaluateState), elision(e), opt(true) { }
    ArrayNode(ElementNode* ele)
      : ExprNode(ArrayNodeEvaluateState), element(ele->next.release()), elision(0), opt(false) { Parser::removeNodeCycle(element.get()); }
    ArrayNode(int eli, ElementNode* ele)
      : ExprNode(ArrayNodeEvaluateState), element(ele->next.release()), elision(eli), opt(true) { Parser::removeNodeCycle(element.get()); }
    virtual void streamTo(SourceStream&) const;
    RefPtr<ElementNode> element;
    int elision;
    bool opt;
  };

  struct PropertyNameNode : public Node {
    PropertyNameNode(double d) : Node(PropertyNameNodeEvaluateState), numeric(d) { }
    PropertyNameNode(const Identifier &s) : Node(PropertyNameNodeEvaluateState), str(s) { }
    virtual void streamTo(SourceStream&) const;
    double numeric;
    Identifier str;
  };
  
  struct PropertyNode : public Node {
    enum Type { Constant, Getter, Setter };
    PropertyNode(PropertyNameNode* n, Node* a, ExprNode* tail, Type t) 
      : Node(InternalErrorState), name(n), assign(a), m_tail(tail), type(t) { }
    virtual void streamTo(SourceStream&) const;
    RefPtr<PropertyNameNode> name;
    RefPtr<Node> assign;
    ExprNode* m_tail;
    Type type;
  };
  
  struct PropertyListNode : public Node {
    // list pointer is tail of a circular list, cracked in the ObjectLiteralNode ctor
    PropertyListNode(PropertyNode *n)
      : Node(PropertyListNodeEvaluateState), node(n), next(this) { Parser::noteNodeCycle(this); }
    PropertyListNode(PropertyNode *n, PropertyListNode *l)
      : Node(PropertyListNodeEvaluateState), node(n), next(l->next) { l->next = this; }
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<PropertyListNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    RefPtr<PropertyNode> node;
    ListRefPtr<PropertyListNode> next;
  };

  struct ObjectLiteralNode : public ExprNode {
    ObjectLiteralNode() : ExprNode(ObjectLiteralNodeEvaluateState) { }
    ObjectLiteralNode(PropertyListNode *l) : ExprNode(ObjectLiteralNodeEvaluateState), list(l->next.release()) { Parser::removeNodeCycle(list.get()); }
    virtual void streamTo(SourceStream&) const;
    RefPtr<PropertyListNode> list;
  };

  struct BracketAccessorNode : public ExprNode {
    BracketAccessorNode() : ExprNode(BracketAccessorValueEvaluateState) {}

    virtual bool isLocation() const { return true; }
    virtual bool isBracketAccessorNode() const { return true; }
  };

  struct DotAccessorNode : public ExprNode {
    DotAccessorNode(const Identifier& s) : ExprNode(DotAccessorValueEvaluateState), ident(s) { }

    virtual bool isLocation() const { return true; }
    virtual bool isDotAccessorNode() const { return true; }
    const Identifier& identifier() const { return ident; }

    Identifier ident;
  };

  struct ArgumentListNode : public Node {
    // list pointer is tail of a circular list, cracked in the ArgumentsNode ctor
    ArgumentListNode(ExprNode* e) : Node(ArgumentListNodeEvaluateListState), next(this), expr(e) { Parser::noteNodeCycle(this); }
    ArgumentListNode(ArgumentListNode *l, ExprNode* e)
      : Node(ArgumentListNodeEvaluateListState), next(l->next), expr(e) { l->next = this; }
    PassRefPtr<ArgumentListNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    virtual void streamTo(SourceStream&) const {}
    ListRefPtr<ArgumentListNode> next;
    RefPtr<Node> expr;
  };

  struct ArgumentsNode : public Node {
    ArgumentsNode() : Node(ArgumentsNodeEvaluateListState) { }
    ArgumentsNode(ArgumentListNode *l)
      : Node(ArgumentsNodeEvaluateListState), list(l->next.release()) { Parser::removeNodeCycle(list.get()); }
    virtual void streamTo(SourceStream&) const {}
    RefPtr<ArgumentListNode> list;
  };

  struct NewExprNode : public ExprNode {
    NewExprNode() : ExprNode(NewExprNodeEvaluateState) {}
    NewExprNode(ArgumentsNode* a) : ExprNode(NewExprNodeEvaluateState), args(a) {}
    RefPtr<ArgumentsNode> args;
  };

  struct FunctionCallNode : public ExprNode {
    FunctionCallNode(ArgumentsNode* a, bool expectBase = true) : ExprNode(expectBase ?  FunctionCallNodeEvaluateState : FunctionCallNoBaseNodeEvaluateState), args(a) {}
    RefPtr<ArgumentsNode> args;
  };

  struct PostfixNode : public ExprNode {
    PostfixNode(const Identifier& i, Operator o) : ExprNode(PostfixNodeEvaluateState), m_ident(i), m_oper(o) {}
    Identifier m_ident;
    Operator m_oper;
  };

  struct PostfixBracketNode : public ExprNode {
    PostfixBracketNode(Operator o) : ExprNode(PostfixBracketNodeEvaluateState), m_oper(o) {}
    Operator m_oper;
  };

  struct DeleteNode : public ExprNode {
    DeleteNode(const Identifier& i) : ExprNode(DeleteNodeEvaluateState), m_ident(i) {}
    Identifier m_ident;
  };

  struct DeleteBracketNode : public ExprNode {
    DeleteBracketNode() : ExprNode(DeleteBracketNodeEvaluateState) {}
  };

  struct DeleteValueNode : public ExprNode {
    DeleteValueNode() : ExprNode(DeleteValueNodeEvaluateState) {}
  };

  struct VoidNode : public ExprNode {
    VoidNode() : ExprNode(VoidNodeEvaluateState) {}
  };

  struct TypeOfResolveNode : public ExprNode {
    TypeOfResolveNode(const Identifier& i) : ExprNode(TypeOfResolveNodeEvaluateState), m_ident(i) {}
    Identifier m_ident;
  };

  struct TypeOfValueNode : public ExprNode {
    TypeOfValueNode() : ExprNode(TypeOfValueNodeEvaluateState) {}
  };

  struct PrefixNode : public ExprNode {
    PrefixNode(const Identifier& i, Operator o) : ExprNode(PrefixNodeEvaluateState), m_ident(i), m_oper(o) {}
    Identifier m_ident;
    Operator m_oper;
  };

  struct PrefixBracketNode : public ExprNode {
    PrefixBracketNode(Operator o) : ExprNode(PrefixBracketNodeEvaluateState), m_oper(o) {}
    Operator m_oper;
  };

  struct UnaryPlusNode : public ExprNode {
    UnaryPlusNode() : ExprNode(UnaryPlusNodeEvaluateState) {}
  };

  struct NegateNode : public ExprNode {
    NegateNode() : ExprNode(NegateNodeEvaluateState) {}
  };

  struct BitwiseNotNode : public ExprNode {
    BitwiseNotNode() : ExprNode(BitwiseNotNodeEvaluateState) {}
  };

  struct LogicalNotNode : public ExprNode {
    LogicalNotNode() : ExprNode(LogicalNotNodeEvaluateState) {}
  };

  struct MultNode : public ExprNode {
    MultNode(char op) : ExprNode(MultNodeEvaluateState), oper(op) {}
    char oper;
  };

  struct AddNode : public ExprNode {
    AddNode(char op) : ExprNode(AddNodeEvaluateState), oper(op) {}
    char oper;
  };

  struct ShiftNode : public ExprNode {
    ShiftNode(Operator o) : ExprNode(ShiftNodeEvaluateState), oper(o) {}
    Operator oper;
  };

  struct RelationalNode : public ExprNode {
    RelationalNode(Operator o)
      : ExprNode(RelationalNodeEvaluateState), oper(o) {}
    Operator oper;
  };

  struct EqualNode : public ExprNode {
    EqualNode(Operator o)
      : ExprNode(EqualNodeEvaluateState), oper(o) {}
    Operator oper;
  };

  struct BitOperNode : public ExprNode {
    BitOperNode(Operator o) :
      ExprNode(BitOperNodeEvaluateState), oper(o) {}
    Operator oper;
  };

  struct JumpNode : public ExprNode {
    JumpNode(InterpreterState opcode, ExprNode* target) 
      : ExprNode(opcode), m_target(target) {}
    ExprNode* m_target;
  };

  struct NoOpNode : public ExprNode {
    NoOpNode() : ExprNode(NoOpEvaluateState) {}
  };

  struct ReadModifyAssignNode : public ExprNode {
    ReadModifyAssignNode(Operator oper, Identifier ident) 
      : ExprNode(ReadModifyAssignNodeEvaluateState), m_ident(ident), m_oper(oper) {}
    Identifier m_ident;
    Operator m_oper;
  };

  struct AssignNode : public ExprNode {
    AssignNode(Identifier ident) 
      : ExprNode(AssignNodeEvaluateState), m_ident(ident) {}
    Identifier m_ident;
  };

  struct ReadModifyAssignBracketNode : public ExprNode {
    ReadModifyAssignBracketNode(Operator oper)
      : ExprNode(ReadModifyAssignBracketNodeEvaluateState), m_oper(oper) {}
    Operator m_oper;
  };

  struct AssignBracketNode : public ExprNode {
    AssignBracketNode() : ExprNode(AssignBracketNodeEvaluateState) {}
  };

  struct CommaNode : public ExprNode {
    CommaNode() : ExprNode(CommaNodeEvaluateState) {}
  };

  struct StatListNode : public StatementNode {
    // list pointer is tail of a circular list, cracked in the CaseClauseNode ctor
    StatListNode(StatementNode *s);
    StatListNode(StatListNode *l, StatementNode *s);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<StatListNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    RefPtr<StatementNode> statement;
    ListRefPtr<StatListNode> next;
  };

  struct VarDeclNode : public Node {
    enum Type { Variable, Constant };
    VarDeclNode::VarDeclNode(const Identifier &id, ExprNode* in, Type t)
      : Node(VarDeclNodeEvaluateState), varType(t), ident(id), init(in) { }
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    Type varType;
    Identifier ident;
    RefPtr<ExprNode> init;
  };

  struct VarDeclEndNode : public ExprNode {
    VarDeclEndNode(VarDeclNode* v) : ExprNode(VarDeclEndNodeEvaluateState), m_varDecl(v) {}
    virtual void streamTo(SourceStream&) const {}
    VarDeclNode* m_varDecl;
  };

  struct VarDeclListNode : public Node {
    // list pointer is tail of a circular list, cracked in the ForNode/VarStatementNode ctor
    VarDeclListNode(VarDeclNode *v) : Node(VarDeclListNodeEvaluateState), next(this), var(v) { Parser::noteNodeCycle(this); }
    VarDeclListNode(VarDeclListNode *l, VarDeclNode *v)
      : Node(VarDeclListNodeEvaluateState), next(l->next), var(v) { l->next = this; }
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<VarDeclListNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    ListRefPtr<VarDeclListNode> next;
    RefPtr<VarDeclNode> var;
  };

  struct VarStatementNode : public StatementNode {
    VarStatementNode(VarDeclListNode *l) 
        : StatementNode(VarStatementNodeExecuteState), next(l->next.release()) 
    {
        Parser::removeNodeCycle(next.get());
    }
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<VarDeclListNode> next;
  };

  struct BlockNode : public StatementNode {
    BlockNode(SourceElementsNode *s);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;

    // FIXME: Once all execution is stack-based, we'll only need a tail pointer, and no head

    // Our ref to the list head implicitly refs the list tail
    SourceElementsNode* sourceTail;
    RefPtr<SourceElementsNode> sourceHead;
  };

  struct EmptyStatementNode : public StatementNode {
    EmptyStatementNode() : StatementNode(EmptyStatementNodeExecuteState) { }
    virtual void streamTo(SourceStream&) const;
  };

  struct ExprStatementNode : public StatementNode {
    ExprStatementNode(ExprNode* e) : StatementNode(ExprStatementNodeExecuteState), expr(e) { }

    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
  };

  struct IfNode : public StatementNode {
    IfNode(Node* e, StatementNode *s1, StatementNode *s2)
        : StatementNode(IfNodeExecuteState), expr(e), statement1(s1), statement2(s2)  { }
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
    RefPtr<StatementNode> statement1;
    RefPtr<StatementNode> statement2;
  };

  struct IfConditionEndNode : public ExprNode {
    IfConditionEndNode(IfNode* i) : ExprNode(IfConditionEndNodeExecuteState), m_if(i) {}
    virtual void streamTo(SourceStream&) const {}
    IfNode* m_if;
  };

  struct DoWhileNode : public StatementNode {
    DoWhileNode(StatementNode *s, Node* e) 
        : StatementNode(DoWhileNodeExecuteState), statement(s), expr(e) { }
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<StatementNode> statement;
    RefPtr<Node> expr;
  };

  struct DoWhileTestExprEndNode : public ExprNode {
    DoWhileTestExprEndNode(DoWhileNode* dw) : ExprNode(DoWhileTestEndNodeExecuteState), m_doWhile(dw) {}
    virtual void streamTo(SourceStream&) const {}
    DoWhileNode* m_doWhile;
  };

  struct WhileNode : public StatementNode {
    WhileNode(Node* e, StatementNode *s) 
        : StatementNode(WhileNodeExecuteState), expr(e), statement(s) { }
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
    RefPtr<StatementNode> statement;
  };

  struct WhileTestExprEndNode : public ExprNode {
    WhileTestExprEndNode(WhileNode* w) : ExprNode(WhileTestEndNodeExecuteState), m_while(w) {}
    virtual void streamTo(SourceStream&) const {}
    WhileNode* m_while;
  };

  struct ForNode : public StatementNode {
    ForNode(ExprNode* e1, ExprNode* e2, ExprNode* e3, StatementNode *s)
        : StatementNode(ForNodeExecuteState), expr1(e1), expr2(e2), expr3(e3), statement(s) { }
    ForNode(VarDeclListNode* e1, ExprNode* e2, ExprNode* e3, StatementNode *s) 
        : StatementNode(ForNodeVarDeclExecuteState), decls(e1->next.release()), expr2(e2), expr3(e3), statement(s) 
    { 
        Parser::removeNodeCycle(decls.get()); 
    }

    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<VarDeclListNode> decls;
    RefPtr<ExprNode> expr1;
    RefPtr<ExprNode> expr2;
    RefPtr<ExprNode> expr3;
    RefPtr<StatementNode> statement;
  };

  struct ForExprEndNode : public ExprNode {
    ForExprEndNode(ForNode* f, InterpreterState opcode) : ExprNode(opcode), m_for(f) {}
    virtual void streamTo(SourceStream&) const {}
    ForNode* m_for;
  };

  struct ForInNode : public StatementNode {
    ForInNode(Node *l, Node* e, StatementNode *s);
    ForInNode(const Identifier &i, ExprNode *in, Node* e, StatementNode *s);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    Identifier ident;
    // used to be AssignExprNode
    RefPtr<ExprNode> init;
    RefPtr<Node> lexpr;
    RefPtr<Node> expr;
    RefPtr<VarDeclNode> varDecl;
    RefPtr<StatementNode> statement;
  };

  struct ContinueNode : public StatementNode {
    ContinueNode() : StatementNode(ContinueNodeExecuteState) { }
    ContinueNode(const Identifier &i) : StatementNode(ContinueNodeExecuteState), ident(i) { }
    virtual void streamTo(SourceStream&) const;
    Identifier ident;
  };

  struct BreakNode : public StatementNode {
    BreakNode() : StatementNode(BreakNodeExecuteState) { }
    BreakNode(const Identifier &i) : StatementNode(BreakNodeExecuteState), ident(i) { }
    virtual void streamTo(SourceStream&) const;
    Identifier ident;
  };

  struct ReturnNode : public StatementNode {
    ReturnNode() : StatementNode(ReturnNodeExecuteState) { }
    virtual void streamTo(SourceStream&) const;
  };

  struct ValueReturnEndNode : public ExprNode {
    ValueReturnEndNode() : ExprNode(ValueReturnEndNodeExecuteState) { }
  };

  struct WithNode : public StatementNode {
    WithNode(Node* e, StatementNode *s) : StatementNode(WithNodeExecuteState), expr(e), statement(s) { }
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
    RefPtr<StatementNode> statement;
  };

  struct WithExprEndNode : public ExprNode {
    WithExprEndNode(WithNode* w) : ExprNode(WithExprEndNodeExecuteState), m_with(w) {}
    virtual void streamTo(SourceStream&) const {}
    WithNode* m_with;
  };

  struct CaseClauseNode : public Node {
    CaseClauseNode(Node* e) : Node(CaseClauseNodeEvaluateState), expr(e) {} 
    CaseClauseNode(Node* e, StatListNode *l) : Node(CaseClauseNodeEvaluateState), expr(e), next(l->next.release()) { Parser::removeNodeCycle(next.get());}
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
    RefPtr<StatListNode> next;
  };

  struct ClauseListNode : public Node {
    // list pointer is tail of a circular list, cracked in the CaseBlockNode ctor
    ClauseListNode(CaseClauseNode *c) : Node(InternalErrorState), clause(c), next(this) { Parser::noteNodeCycle(this); }
    ClauseListNode(ClauseListNode *n, CaseClauseNode *c)
      : Node(InternalErrorState), clause(c), next(n->next) { n->next = this; }
    CaseClauseNode *getClause() const { return clause.get(); }
    ClauseListNode *getNext() const { return next.get(); }
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<ClauseListNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    RefPtr<CaseClauseNode> clause;
    ListRefPtr<ClauseListNode> next;
  };

  struct CaseBlockNode : public Node {
    CaseBlockNode(ClauseListNode *l1, CaseClauseNode *d, ClauseListNode *l2);
    Completion evalBlock(ExecState*, JSValue *input);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<ClauseListNode> list1;
    RefPtr<CaseClauseNode> def;
    RefPtr<ClauseListNode> list2;
  };

  struct SwitchNode : public StatementNode {
    SwitchNode(Node* e, CaseBlockNode *b) : StatementNode(SwitchNodeExecuteState), expr(e), block(b) { }
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
    RefPtr<CaseBlockNode> block;
  };

  struct LabelNode : public StatementNode {
    LabelNode(const Identifier &l, StatementNode *s) 
        : StatementNode(LabelNodeExecuteState), label(l), statement(s) { }
    virtual bool isLabelNode() const { return true; }
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    Identifier label;
    RefPtr<StatementNode> statement;
  };

  struct ThrowEndNode : public ExprNode {
    ThrowEndNode() : ExprNode(ThrowEndNodeExecuteState) { }
  };

  struct TryNode : public StatementNode {
    TryNode(StatementNode *b, const Identifier &e, StatementNode *c, StatementNode *f)
        : StatementNode(TryNodeExecuteState), tryBlock(b), exceptionIdent(e), catchBlock(c), finallyBlock(f) { }
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<StatementNode> tryBlock;
    Identifier exceptionIdent;
    RefPtr<StatementNode> catchBlock;
    RefPtr<StatementNode> finallyBlock;
  };

  struct ParameterNode : public Node {
    // list pointer is tail of a circular list, cracked in the FuncDeclNode/FuncExprNode ctor
    ParameterNode(const Identifier &i) : Node(InternalErrorState), id(i), next(this) { Parser::noteNodeCycle(this); }
    ParameterNode(ParameterNode *next, const Identifier &i)
      : Node(InternalErrorState), id(i), next(next->next) { next->next = this; }
    Identifier ident() { return id; }
    ParameterNode *nextParam() { return next.get(); }
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<ParameterNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    Identifier id;
    ListRefPtr<ParameterNode> next;
  };

  // inherited by ProgramNode
  struct FunctionBodyNode : public BlockNode {
    FunctionBodyNode(SourceElementsNode *);
    virtual void processFuncDecl(ExecState*);
    int sourceId() { return m_sourceId; }
    const UString& sourceURL() { return m_sourceURL; }
    UString m_sourceURL;
    int m_sourceId;
  };

  struct FuncExprNode : public ExprNode {
    FuncExprNode(const Identifier &i, FunctionBodyNode *b, ParameterNode *p = 0)
      : ExprNode(FuncExprNodeEvaluateState), ident(i), param(p ? p->next.release() : 0), body(b) { if (p) { Parser::removeNodeCycle(param.get()); } }
    virtual void streamTo(SourceStream&) const;
    // Used for streamTo
    Identifier ident;
    RefPtr<ParameterNode> param;
    RefPtr<FunctionBodyNode> body;
  };

  struct FuncDeclNode : public StatementNode {
    FuncDeclNode(const Identifier &i, FunctionBodyNode *b)
        : StatementNode(FuncDeclNodeExecuteState), ident(i), body(b) { }
    FuncDeclNode(const Identifier &i, ParameterNode *p, FunctionBodyNode *b)
        : StatementNode(FuncDeclNodeExecuteState), ident(i), param(p->next.release()), body(b) 
    { 
        Parser::removeNodeCycle(param.get());
    }
    virtual void processFuncDecl(ExecState*);
    virtual void streamTo(SourceStream&) const;
    Identifier ident;
    RefPtr<ParameterNode> param;
    RefPtr<FunctionBodyNode> body;
  };

  // A linked list of source element nodes
  struct SourceElementsNode : public StatementNode {
      static int count;
    // list pointer is tail of a circular list, cracked in the BlockNode (or subclass) ctor
    SourceElementsNode(StatementNode *s1);
    SourceElementsNode(SourceElementsNode *s1, StatementNode *s2);
    
    void processFuncDecl(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<SourceElementsNode> releaseNext() { return next.release(); }
    virtual void breakCycle();

    // FIXME: Once all execution is stack-based, we'll only need a prev pointer, and no next
    RefPtr<StatementNode> node;
    SourceElementsNode* prev; // the list is ref'd from head to tail to avoid a ref cycle
    ListRefPtr<SourceElementsNode> next;
  };

  struct ProgramNode : public FunctionBodyNode {
    ProgramNode(SourceElementsNode *s);
  };

} // namespace

#endif
