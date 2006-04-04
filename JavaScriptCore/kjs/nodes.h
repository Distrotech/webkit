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

#include "Parser.h"
#include "internal.h"
#include <kxmlcore/ListRefPtr.h>
#include <kxmlcore/RefPtr.h>
#include "InterpreterState.h"

namespace KJS {

  class ProgramNode;
  class PropertyNameNode;
  class PropertyListNode;
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
    Node();
    virtual ~Node();

    UString toString() const;
    virtual void streamTo(SourceStream&) const = 0;
    virtual void processVarDecls(ExecState*) {}
    virtual InterpreterState evaluateState() const = 0;
    int lineNo() const { return m_line; }

    void ref();
    void deref();
    unsigned refcount();
    static void clearNewNodes();

    virtual Node *nodeInsideAllParens();

    virtual bool isLocation() const { return false; }
    virtual bool isResolveNode() const { return false; }
    virtual bool isBracketAccessorNode() const { return false; }
    virtual bool isDotAccessorNode() const { return false; }
    virtual bool isGroupNode() const { return false; }

    virtual void breakCycle() { }

    Completion createErrorCompletion(ExecState *, ErrorType, const char *msg);
    Completion createErrorCompletion(ExecState *, ErrorType, const char *msg, const Identifier &);

    JSValue *throwError(ExecState *, ErrorType, const char *msg);
    JSValue *throwError(ExecState *, ErrorType, const char *msg, JSValue *, Node *);
    JSValue *throwError(ExecState *, ErrorType, const char *msg, const Identifier &);
    JSValue *throwError(ExecState *, ErrorType, const char *msg, JSValue *, const Identifier &);
    JSValue *throwError(ExecState *, ErrorType, const char *msg, JSValue *, Node *, Node *);
    JSValue *throwError(ExecState *, ErrorType, const char *msg, JSValue *, Node *, const Identifier &);

    JSValue *throwUndefinedVariableError(ExecState *, const Identifier &);

    void setExceptionDetailsIfNeeded(ExecState*);

    int m_line;
    // disallow assignment
    Node& operator=(const Node&);
    Node(const Node &other);
  };

  struct StatementNode : public Node {
    StatementNode();
    void setLoc(int line0, int line1);
    int firstLine() const { return lineNo(); }
    int lastLine() const { return m_lastLine; }
    bool hitStatement(ExecState*);
    virtual Completion execute(ExecState *exec) = 0;
    virtual InterpreterState evaluateState() const { return StatementNodeEvaluateState; }
    void pushLabel(const Identifier &id) { ls.push(id); }
    virtual void processFuncDecl(ExecState*);
    LabelStack ls;
    int m_lastLine;
  };

  struct NullNode : public Node {
    NullNode() {}
    virtual void streamTo(SourceStream&) const;
    virtual InterpreterState evaluateState() const { return NullNodeEvaluateState; }
  };

  struct BooleanNode : public Node {
    BooleanNode(bool v) : value(v) {}
    JSValue* jsValue() const { return jsBoolean(value); }
    virtual void streamTo(SourceStream&) const;
    virtual InterpreterState evaluateState() const { return BooleanNodeEvaluateState; }
    bool value;
  };

  struct NumberNode : public Node {
    NumberNode(double v) : value(v) {}
    JSValue* jsValue() const { return jsNumber(value); }
    virtual void streamTo(SourceStream&) const;
    virtual InterpreterState evaluateState() const { return NumberNodeEvaluateState; }
    double value;
  };

  struct StringNode : public Node {
    StringNode(const UString *v) { value = *v; }
    JSValue* jsValue() const { return jsString(value); }
    virtual void streamTo(SourceStream&) const;
    virtual InterpreterState evaluateState() const { return StringNodeEvaluateState; }
    UString value;
  };

  struct RegExpNode : public Node {
    RegExpNode(const UString &p, const UString &f)
      : pattern(p), flags(f) { }
    virtual void streamTo(SourceStream&) const;
    virtual InterpreterState evaluateState() const { return RegExpNodeEvaluateState; }
    UString pattern, flags;
  };

  struct ThisNode : public Node {
    ThisNode() {}
    virtual InterpreterState evaluateState() const { return ThisNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
  };

  struct ResolveNode : public Node {
    ResolveNode(const Identifier &s) : ident(s) { }
    virtual void streamTo(SourceStream&) const;
    virtual InterpreterState evaluateState() const { return ResolveNodeEvaluateState; }

    virtual bool isLocation() const { return true; }
    virtual bool isResolveNode() const { return true; }
    const Identifier& identifier() const { return ident; }

    Identifier ident;
  };

  struct GroupNode : public Node {
    GroupNode(Node *g) : group(g) { }
    virtual Node *nodeInsideAllParens();
    virtual void streamTo(SourceStream&) const;
    virtual InterpreterState evaluateState() const { return GroupNodeEvaluateState; }
    virtual bool isGroupNode() const { return true; }
    RefPtr<Node> group;
  };

  struct ElementNode : public Node {
    // list pointer is tail of a circular list, cracked in the ArrayNode ctor
    ElementNode(int e, Node *n) : next(this), elision(e), node(n) { Parser::noteNodeCycle(this); }
    ElementNode(ElementNode *l, int e, Node *n)
      : next(l->next), elision(e), node(n) { l->next = this; }
    virtual void streamTo(SourceStream&) const;
    virtual InterpreterState evaluateState() const { return ElementNodeEvaluateState; }
    PassRefPtr<ElementNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    ListRefPtr<ElementNode> next;
    int elision;
    RefPtr<Node> node;
  };

  struct ArrayNode : public Node {
    ArrayNode(int e) : elision(e), opt(true) { }
    ArrayNode(ElementNode *ele)
      : element(ele->next), elision(0), opt(false) { Parser::removeNodeCycle(element.get()); ele->next = 0; }
    ArrayNode(int eli, ElementNode *ele)
      : element(ele->next), elision(eli), opt(true) { Parser::removeNodeCycle(element.get()); ele->next = 0; }
    virtual InterpreterState evaluateState() const { return ArrayNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<ElementNode> element;
    int elision;
    bool opt;
  };

  struct PropertyNameNode : public Node {
    PropertyNameNode(double d) : numeric(d) { }
    PropertyNameNode(const Identifier &s) : str(s) { }
    virtual InterpreterState evaluateState() const { return PropertyNameNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    double numeric;
    Identifier str;
  };
  
  struct PropertyNode : public Node {
    enum Type { Constant, Getter, Setter };
    PropertyNode(PropertyNameNode *n, Node *a, Type t) 
      : name(n), assign(a), type(t) { }
    virtual InterpreterState evaluateState() const { return PropertyNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<PropertyNameNode> name;
    RefPtr<Node> assign;
    Type type;
  };
  
  struct PropertyListNode : public Node {
    // list pointer is tail of a circular list, cracked in the ObjectLiteralNode ctor
    PropertyListNode(PropertyNode *n)
      : node(n), next(this) { Parser::noteNodeCycle(this); }
    PropertyListNode(PropertyNode *n, PropertyListNode *l)
      : node(n), next(l->next) { l->next = this; }
    virtual InterpreterState evaluateState() const { return PropertyListNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<PropertyListNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    RefPtr<PropertyNode> node;
    ListRefPtr<PropertyListNode> next;
  };

  struct ObjectLiteralNode : public Node {
    ObjectLiteralNode() { }
    ObjectLiteralNode(PropertyListNode *l) : list(l->next) { Parser::removeNodeCycle(list.get()); l->next = 0; }
    virtual InterpreterState evaluateState() const { return ObjectLiteralNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<PropertyListNode> list;
  };

  struct BracketAccessorNode : public Node {
    BracketAccessorNode(Node *e1, Node *e2) : expr1(e1), expr2(e2) {}
    virtual InterpreterState evaluateState() const { return BracketAccessorNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;

    virtual bool isLocation() const { return true; }
    virtual bool isBracketAccessorNode() const { return true; }
    Node *base() { return expr1.get(); }
    Node *subscript() { return expr2.get(); }

    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  struct DotAccessorNode : public Node {
    DotAccessorNode(Node *e, const Identifier &s) : expr(e), ident(s) { }
    virtual InterpreterState evaluateState() const { return DotAccessorNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;

    virtual bool isLocation() const { return true; }
    virtual bool isDotAccessorNode() const { return true; }
    Node *base() const { return expr.get(); }
    const Identifier& identifier() const { return ident; }

    RefPtr<Node> expr;
    Identifier ident;
  };

  struct ArgumentListNode : public Node {
    // list pointer is tail of a circular list, cracked in the ArgumentsNode ctor
    ArgumentListNode(Node *e) : next(this), expr(e) { Parser::noteNodeCycle(this); }
    ArgumentListNode(ArgumentListNode *l, Node *e)
      : next(l->next), expr(e) { l->next = this; }
    List evaluateList(ExecState*);
    virtual InterpreterState evaluateState() const { return ArgumentListNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<ArgumentListNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    ListRefPtr<ArgumentListNode> next;
    RefPtr<Node> expr;
  };

  struct ArgumentsNode : public Node {
    ArgumentsNode() { }
    ArgumentsNode(ArgumentListNode *l)
      : list(l->next) { Parser::removeNodeCycle(list.get()); l->next = 0; }
    List evaluateList(ExecState *exec) { return list ? list->evaluateList(exec) : List(); }
    virtual InterpreterState evaluateState() const { return ArgumentsNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<ArgumentListNode> list;
  };

  struct NewExprNode : public Node {
    NewExprNode(Node *e) : expr(e) {}
    NewExprNode(Node *e, ArgumentsNode *a) : expr(e), args(a) {}
    virtual InterpreterState evaluateState() const { return NewExprNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
    RefPtr<ArgumentsNode> args;
  };

  struct FunctionCallValueNode : public Node {
    FunctionCallValueNode(Node *e, ArgumentsNode *a) : expr(e), args(a) {}
    virtual InterpreterState evaluateState() const { return FunctionCallValueNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
    RefPtr<ArgumentsNode> args;
  };

  struct FunctionCallResolveNode : public Node {
    FunctionCallResolveNode(const Identifier& i, ArgumentsNode *a) : ident(i), args(a) {}
    virtual InterpreterState evaluateState() const { return FunctionCallResolveNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    Identifier ident;
    RefPtr<ArgumentsNode> args;
  };

  struct FunctionCallBracketNode : public Node {
    FunctionCallBracketNode(Node *b, Node *s, ArgumentsNode *a) : base(b), subscript(s), args(a) {}
    virtual InterpreterState evaluateState() const { return FunctionCallBracketNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> base;
    RefPtr<Node> subscript;
    RefPtr<ArgumentsNode> args;
  };

  struct FunctionCallParenBracketNode : public FunctionCallBracketNode {
    FunctionCallParenBracketNode(Node *b, Node *s, ArgumentsNode *a) : FunctionCallBracketNode(b, s, a) {}
    virtual void streamTo(SourceStream&) const;
  };

  struct FunctionCallDotNode : public Node {
    FunctionCallDotNode(Node *b, const Identifier &i, ArgumentsNode *a) : base(b), ident(i), args(a) {}
    virtual InterpreterState evaluateState() const { return FunctionCallDotNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> base;
    Identifier ident;
    RefPtr<ArgumentsNode> args;
  };

  struct FunctionCallParenDotNode : public FunctionCallDotNode {
    FunctionCallParenDotNode(Node *b, const Identifier &i, ArgumentsNode *a) : FunctionCallDotNode(b, i, a) {}
    virtual void streamTo(SourceStream&) const;
  };

  struct PostfixResolveNode : public Node {
    PostfixResolveNode(const Identifier& i, Operator o) : m_ident(i), m_oper(o) {}
    virtual InterpreterState evaluateState() const { return PostfixResolveNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    Identifier m_ident;
    Operator m_oper;
  };

  struct PostfixBracketNode : public Node {
    PostfixBracketNode(Node *b, Node *s, Operator o) : m_base(b), m_subscript(s), m_oper(o) {}
    virtual InterpreterState evaluateState() const { return PostfixBracketNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> m_base;
    RefPtr<Node> m_subscript;
    Operator m_oper;
  };

  struct PostfixDotNode : public Node {
    PostfixDotNode(Node *b, const Identifier& i, Operator o) : m_base(b), m_ident(i), m_oper(o) {}
    virtual InterpreterState evaluateState() const { return PostfixDotNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> m_base;
    Identifier m_ident;
    Operator m_oper;
  };

  struct DeleteResolveNode : public Node {
    DeleteResolveNode(const Identifier& i) : m_ident(i) {}
    virtual InterpreterState evaluateState() const { return DeleteResolveNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    Identifier m_ident;
  };

  struct DeleteBracketNode : public Node {
    DeleteBracketNode(Node *base, Node *subscript) : m_base(base), m_subscript(subscript) {}
    virtual InterpreterState evaluateState() const { return DeleteBracketNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> m_base;
    RefPtr<Node> m_subscript;
  };

  struct DeleteDotNode : public Node {
    DeleteDotNode(Node *base, const Identifier& i) : m_base(base), m_ident(i) {}
    virtual InterpreterState evaluateState() const { return DeleteDotNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> m_base;
    Identifier m_ident;
  };

  struct DeleteValueNode : public Node {
    DeleteValueNode(Node *e) : m_expr(e) {}
    virtual InterpreterState evaluateState() const { return DeleteValueNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> m_expr;
  };

  struct VoidNode : public Node {
    VoidNode(Node *e) : expr(e) {}
    virtual InterpreterState evaluateState() const { return VoidNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
  };

  struct TypeOfResolveNode : public Node {
    TypeOfResolveNode(const Identifier& i) : m_ident(i) {}
    virtual InterpreterState evaluateState() const { return TypeOfResolveNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    Identifier m_ident;
  };

  struct TypeOfValueNode : public Node {
    TypeOfValueNode(Node *e) : m_expr(e) {}
    virtual InterpreterState evaluateState() const { return TypeOfValueNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> m_expr;
  };

  struct PrefixResolveNode : public Node {
    PrefixResolveNode(const Identifier& i, Operator o) : m_ident(i), m_oper(o) {}
    virtual InterpreterState evaluateState() const { return PrefixResolveNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    Identifier m_ident;
    Operator m_oper;
  };

  struct PrefixBracketNode : public Node {
    PrefixBracketNode(Node *b, Node *s, Operator o) : m_base(b), m_subscript(s), m_oper(o) {}
    virtual InterpreterState evaluateState() const { return PrefixBracketNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> m_base;
    RefPtr<Node> m_subscript;
    Operator m_oper;
  };

  struct PrefixDotNode : public Node {
    PrefixDotNode(Node *b, const Identifier& i, Operator o) : m_base(b), m_ident(i), m_oper(o) {}
    virtual InterpreterState evaluateState() const { return PrefixDotNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> m_base;
    Identifier m_ident;
    Operator m_oper;
  };

  struct UnaryPlusNode : public Node {
    UnaryPlusNode(Node *e) : expr(e) {}
    virtual InterpreterState evaluateState() const { return UnaryPlusNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
  };

  struct NegateNode : public Node {
    NegateNode(Node *e) : expr(e) {}
    virtual InterpreterState evaluateState() const { return NegateNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
  };

  struct BitwiseNotNode : public Node {
    BitwiseNotNode(Node *e) : expr(e) {}
    virtual InterpreterState evaluateState() const { return BitwiseNotNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
  };

  struct LogicalNotNode : public Node {
    LogicalNotNode(Node *e) : expr(e) {}
    virtual InterpreterState evaluateState() const { return LogicalNotNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
  };

  struct MultNode : public Node {
    MultNode(Node *t1, Node *t2, char op) : term1(t1), term2(t2), oper(op) {}
    virtual InterpreterState evaluateState() const { return MultNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> term1;
    RefPtr<Node> term2;
    char oper;
  };

  struct AddNode : public Node {
    AddNode(Node *t1, Node *t2, char op) : term1(t1), term2(t2), oper(op) {}
    virtual InterpreterState evaluateState() const { return AddNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> term1;
    RefPtr<Node> term2;
    char oper;
  };

  struct ShiftNode : public Node {
    ShiftNode(Node *t1, Operator o, Node *t2)
      : term1(t1), term2(t2), oper(o) {}
    virtual InterpreterState evaluateState() const { return ShiftNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> term1;
    RefPtr<Node> term2;
    Operator oper;
  };

  struct RelationalNode : public Node {
    RelationalNode(Node *e1, Operator o, Node *e2) :
      expr1(e1), expr2(e2), oper(o) {}
    virtual InterpreterState evaluateState() const { return RelationalNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
    Operator oper;
  };

  struct EqualNode : public Node {
    EqualNode(Node *e1, Operator o, Node *e2)
      : expr1(e1), expr2(e2), oper(o) {}
    virtual InterpreterState evaluateState() const { return EqualNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
    Operator oper;
  };

  struct BitOperNode : public Node {
    BitOperNode(Node *e1, Operator o, Node *e2) :
      expr1(e1), expr2(e2), oper(o) {}
    virtual InterpreterState evaluateState() const { return BitOperNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
    Operator oper;
  };

  /**
   * expr1 && expr2, expr1 || expr2
   */
  struct BinaryLogicalNode : public Node {
    BinaryLogicalNode(Node *e1, Operator o, Node *e2) :
      expr1(e1), expr2(e2), oper(o) {}
    virtual InterpreterState evaluateState() const { return BinaryLogicalNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
    Operator oper;
  };

  /**
   * The ternary operator, "logical ? expr1 : expr2"
   */
  struct ConditionalNode : public Node {
    ConditionalNode(Node *l, Node *e1, Node *e2) :
      logical(l), expr1(e1), expr2(e2) {}
    virtual InterpreterState evaluateState() const { return ConditionalNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> logical;
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  struct AssignResolveNode : public Node {
    AssignResolveNode(const Identifier &ident, Operator oper, Node *right) 
      : m_ident(ident), m_oper(oper), m_right(right) {}
    virtual InterpreterState evaluateState() const { return AssignResolveNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    Identifier m_ident;
    Operator m_oper;
    RefPtr<Node> m_right;
  };

  struct AssignBracketNode : public Node {
    AssignBracketNode(Node *base, Node *subscript, Operator oper, Node *right) 
      : m_base(base), m_subscript(subscript), m_oper(oper), m_right(right) {}
    virtual InterpreterState evaluateState() const { return AssignBracketNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> m_base;
    RefPtr<Node> m_subscript;
    Operator m_oper;
    RefPtr<Node> m_right;
  };

  struct AssignDotNode : public Node {
    AssignDotNode(Node *base, const Identifier& ident, Operator oper, Node *right)
      : m_base(base), m_ident(ident), m_oper(oper), m_right(right) {}
    virtual InterpreterState evaluateState() const { return AssignDotNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> m_base;
    Identifier m_ident;
    Operator m_oper;
    RefPtr<Node> m_right;
  };

  struct CommaNode : public Node {
    CommaNode(Node *e1, Node *e2) : expr1(e1), expr2(e2) {}
    virtual InterpreterState evaluateState() const { return CommaNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
  };

  struct StatListNode : public StatementNode {
    // list pointer is tail of a circular list, cracked in the CaseClauseNode ctor
    StatListNode(StatementNode *s);
    StatListNode(StatListNode *l, StatementNode *s);
    virtual Completion execute(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<StatListNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    RefPtr<StatementNode> statement;
    ListRefPtr<StatListNode> next;
  };

  struct AssignExprNode : public Node {
    AssignExprNode(Node *e) : expr(e) {}
    virtual InterpreterState evaluateState() const { return AssignExprNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
  };

  struct VarDeclNode : public Node {
    enum Type { Variable, Constant };
    VarDeclNode(const Identifier &id, AssignExprNode *in, Type t);
    virtual void processVarDecls(ExecState*);
    virtual InterpreterState evaluateState() const { return VarDeclNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    Type varType;
    Identifier ident;
    RefPtr<AssignExprNode> init;
  };

  struct VarDeclListNode : public Node {
    // list pointer is tail of a circular list, cracked in the ForNode/VarStatementNode ctor
    VarDeclListNode(VarDeclNode *v) : next(this), var(v) { Parser::noteNodeCycle(this); }
    VarDeclListNode(VarDeclListNode *l, VarDeclNode *v)
      : next(l->next), var(v) { l->next = this; }
    virtual void processVarDecls(ExecState*);
    virtual InterpreterState evaluateState() const { return VarDeclListNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<VarDeclListNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    ListRefPtr<VarDeclListNode> next;
    RefPtr<VarDeclNode> var;
  };

  struct VarStatementNode : public StatementNode {
    VarStatementNode(VarDeclListNode *l) : next(l->next) { Parser::removeNodeCycle(next.get()); l->next = 0; }
    virtual Completion execute(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<VarDeclListNode> next;
  };

  struct BlockNode : public StatementNode {
    BlockNode(SourceElementsNode *s);
    virtual Completion execute(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<SourceElementsNode> source;
  };

  struct EmptyStatementNode : public StatementNode {
    EmptyStatementNode() { } // debug
    virtual Completion execute(ExecState*);
    virtual void streamTo(SourceStream&) const;
  };

  struct ExprStatementNode : public StatementNode {
    ExprStatementNode(Node *e) : expr(e) { }
    virtual Completion execute(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
  };

  struct IfNode : public StatementNode {
    IfNode(Node *e, StatementNode *s1, StatementNode *s2)
      : expr(e), statement1(s1), statement2(s2) {}
    virtual Completion execute(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
    RefPtr<StatementNode> statement1;
    RefPtr<StatementNode> statement2;
  };

  struct DoWhileNode : public StatementNode {
    DoWhileNode(StatementNode *s, Node *e) : statement(s), expr(e) {}
    virtual Completion execute(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<StatementNode> statement;
    RefPtr<Node> expr;
  };

  struct WhileNode : public StatementNode {
    WhileNode(Node *e, StatementNode *s) : expr(e), statement(s) {}
    virtual Completion execute(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
    RefPtr<StatementNode> statement;
  };

  struct ForNode : public StatementNode {
    ForNode(Node *e1, Node *e2, Node *e3, StatementNode *s) :
      expr1(e1), expr2(e2), expr3(e3), statement(s) {}
    ForNode(VarDeclListNode *e1, Node *e2, Node *e3, StatementNode *s) :
      expr1(e1->next), expr2(e2), expr3(e3), statement(s) { Parser::removeNodeCycle(expr1.get()); e1->next = 0; }
    virtual Completion execute(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr1;
    RefPtr<Node> expr2;
    RefPtr<Node> expr3;
    RefPtr<StatementNode> statement;
  };

  struct ForInNode : public StatementNode {
    ForInNode(Node *l, Node *e, StatementNode *s);
    ForInNode(const Identifier &i, AssignExprNode *in, Node *e, StatementNode *s);
    virtual Completion execute(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    Identifier ident;
    RefPtr<AssignExprNode> init;
    RefPtr<Node> lexpr;
    RefPtr<Node> expr;
    RefPtr<VarDeclNode> varDecl;
    RefPtr<StatementNode> statement;
  };

  struct ContinueNode : public StatementNode {
    ContinueNode() { }
    ContinueNode(const Identifier &i) : ident(i) { }
    virtual Completion execute(ExecState*);
    virtual void streamTo(SourceStream&) const;
    Identifier ident;
  };

  struct BreakNode : public StatementNode {
    BreakNode() { }
    BreakNode(const Identifier &i) : ident(i) { }
    virtual Completion execute(ExecState*);
    virtual void streamTo(SourceStream&) const;
    Identifier ident;
  };

  struct ReturnNode : public StatementNode {
    ReturnNode(Node *v) : value(v) {}
    virtual Completion execute(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> value;
  };

  struct WithNode : public StatementNode {
    WithNode(Node *e, StatementNode *s) : expr(e), statement(s) {}
    virtual Completion execute(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
    RefPtr<StatementNode> statement;
  };

  struct CaseClauseNode : public Node {
    CaseClauseNode(Node *e) : expr(e) { }
    CaseClauseNode(Node *e, StatListNode *l)
      : expr(e), next(l->next) { Parser::removeNodeCycle(next.get()); l->next = 0; }
    Completion evalStatements(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual InterpreterState evaluateState() const { return CaseClauseNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
    RefPtr<StatListNode> next;
  };

  struct ClauseListNode : public Node {
    // list pointer is tail of a circular list, cracked in the CaseBlockNode ctor
    ClauseListNode(CaseClauseNode *c) : clause(c), next(this) { Parser::noteNodeCycle(this); }
    ClauseListNode(ClauseListNode *n, CaseClauseNode *c)
      : clause(c), next(n->next) { n->next = this; }
    CaseClauseNode *getClause() const { return clause.get(); }
    ClauseListNode *getNext() const { return next.get(); }
    virtual void processVarDecls(ExecState*);
    virtual InterpreterState evaluateState() const { return ClauseListNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<ClauseListNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    RefPtr<CaseClauseNode> clause;
    ListRefPtr<ClauseListNode> next;
  };

  struct CaseBlockNode : public Node {
    CaseBlockNode(ClauseListNode *l1, CaseClauseNode *d, ClauseListNode *l2);
    Completion evalBlock(ExecState *exec, JSValue *input);
    virtual void processVarDecls(ExecState*);
    virtual InterpreterState evaluateState() const { return CaseBlockNodeEvaluateState; }
    virtual void streamTo(SourceStream&) const;
    RefPtr<ClauseListNode> list1;
    RefPtr<CaseClauseNode> def;
    RefPtr<ClauseListNode> list2;
  };

  struct SwitchNode : public StatementNode {
    SwitchNode(Node *e, CaseBlockNode *b) : expr(e), block(b) { }
    virtual Completion execute(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
    RefPtr<CaseBlockNode> block;
  };

  struct LabelNode : public StatementNode {
    LabelNode(const Identifier &l, StatementNode *s) : label(l), statement(s) { }
    virtual Completion execute(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    Identifier label;
    RefPtr<StatementNode> statement;
  };

  struct ThrowNode : public StatementNode {
    ThrowNode(Node *e) : expr(e) {}
    virtual Completion execute(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<Node> expr;
  };

  struct TryNode : public StatementNode {
    TryNode(StatementNode *b, const Identifier &e, StatementNode *c, StatementNode *f)
      : tryBlock(b), exceptionIdent(e), catchBlock(c), finallyBlock(f) { }
    virtual Completion execute(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    RefPtr<StatementNode> tryBlock;
    Identifier exceptionIdent;
    RefPtr<StatementNode> catchBlock;
    RefPtr<StatementNode> finallyBlock;
  };

  struct ParameterNode : public Node {
    // list pointer is tail of a circular list, cracked in the FuncDeclNode/FuncExprNode ctor
    ParameterNode(const Identifier &i) : id(i), next(this) { Parser::noteNodeCycle(this); }
    ParameterNode(ParameterNode *next, const Identifier &i)
      : id(i), next(next->next) { next->next = this; }
    Identifier ident() { return id; }
    ParameterNode *nextParam() { return next.get(); }
    virtual InterpreterState evaluateState() const { return ParameterNodeEvaluateState; }
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

  struct FuncExprNode : public Node {
    FuncExprNode(const Identifier &i, FunctionBodyNode *b, ParameterNode *p = 0)
      : ident(i), param(p ? p->next : 0), body(b) { if (p) { Parser::removeNodeCycle(param.get()); p->next = 0; } }
    virtual void streamTo(SourceStream&) const;
    // Used for streamTo
    Identifier ident;
    virtual InterpreterState evaluateState() const { return FuncExprNodeEvaluateState; }
    RefPtr<ParameterNode> param;
    RefPtr<FunctionBodyNode> body;
  };

  struct FuncDeclNode : public StatementNode {
    FuncDeclNode(const Identifier &i, FunctionBodyNode *b)
      : ident(i), body(b) { }
    FuncDeclNode(const Identifier &i, ParameterNode *p, FunctionBodyNode *b)
      : ident(i), param(p->next), body(b) { Parser::removeNodeCycle(param.get()); p->next = 0; }
    virtual Completion execute(ExecState*);
    virtual void processFuncDecl(ExecState*);
    virtual void streamTo(SourceStream&) const;
    Identifier ident;
    RefPtr<ParameterNode> param;
    RefPtr<FunctionBodyNode> body;
  };

  // A linked list of source element nodes
  struct SourceElementsNode : public StatementNode {
      static int count;
    // list pointer is tail of a circular list, cracked in the BlockNode (or substruct) ctor
    SourceElementsNode(StatementNode*);
    SourceElementsNode(SourceElementsNode *s1, StatementNode *s2);
    
    Completion execute(ExecState*);
    void processFuncDecl(ExecState*);
    virtual void processVarDecls(ExecState*);
    virtual void streamTo(SourceStream&) const;
    PassRefPtr<SourceElementsNode> releaseNext() { return next.release(); }
    virtual void breakCycle();
    RefPtr<StatementNode> node;
    ListRefPtr<SourceElementsNode> next;
  };

  struct ProgramNode : public FunctionBodyNode {
    ProgramNode(SourceElementsNode *s);
  };

} // namespace

#endif
