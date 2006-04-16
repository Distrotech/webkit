%{

/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "nodes.h"
#include "lexer.h"
#include "internal.h"
#include "expr.h"

// Not sure why, but yacc doesn't add this define along with the others.
#define yylloc kjsyylloc

/* default values for bison */
#define YYDEBUG 0
#if !PLATFORM(DARWIN)
    // avoid triggering warnings in older bison
#define YYERROR_VERBOSE
#endif

extern int kjsyylex();
int kjsyyerror(const char *);
static bool allowAutomaticSemicolon();

#define AUTO_SEMICOLON do { if (!allowAutomaticSemicolon()) YYABORT; } while (0)
#define DBG(l, s, e) (l)->setLoc((s).first_line, (e).last_line)

using namespace KJS;

 static bool makeGetterOrSetterPropertyNode(PropertyNode*& result, Identifier &getOrSet, Identifier& name, ParameterNode *params, FunctionBodyNode *body);

 static bool emitPrefixExpression(Expr& e, const Expr& loc, Operator op);
 static bool emitPostfixExpression(Expr& e, const Expr& loc, Operator op);
 static void emitTypeOfExpression(Expr& e, const Expr& loc);
 static void emitDeleteExpression(Expr& e, const Expr& loc);
 static bool emitAssignment(Expr& e, const Expr& loc, const Expr& expr);
 static bool emitReadModifyAssignment(Expr& e, const Expr& loc, Operator op, const Expr& expr);
 static void emitFunctionCall(Expr& e, const Expr& func, ArgumentsNode *args);
 static void emitConditionalExpression(Expr& e, const Expr& condition, const Expr& thenExpr, const Expr& elseExpr);
 static void emitLogicalAND(Expr& e, const Expr& left, const Expr& right);
 static void emitLogicalOR(Expr& e, const Expr& left, const Expr& right);
 static void emitNoExpr(Expr& e);

 static void emitForLoop(StatementNode*& stat, const Expr& init, VarDeclListNode* node, const Expr& test, const Expr& inc, StatementNode* body);

 static inline void emitLeafExpression(Expr& e, ExprNode* leaf) 
 {
     e.head = leaf;
     e.tail = leaf;
 }

 static inline void emitUnaryExpression(Expr& e, ExprNode* unary, const Expr& operandExpr) 
 {
     e.head = operandExpr.head;
     operandExpr.tail->m_next = unary;
     e.tail = unary;
 }

 static inline void emitBinaryExpression(Expr& e, ExprNode* binary, const Expr& aExpr, const Expr& bExpr) 
 {
     e.head = aExpr.head;
     aExpr.tail->m_next = bExpr.head;
     bExpr.tail->m_next = binary;
     e.tail = binary;
 }

 static inline void emitExpressionStatement(StatementNode*& statement, const Expr& expr) 
 {
     expr.tail->m_next = new ExprStatementEndNode;
     statement = new ExprStatementNode(expr.head); 
 }

 static inline void emitValueReturn(StatementNode*& statement, const Expr& expr) 
 {
     expr.tail->m_next = new ValueReturnEndNode;
     statement = new ExprStatementNode(expr.head); 
 }

 static inline void emitThrow(StatementNode*& statement, const Expr& expr) 
 {
     expr.tail->m_next = new ThrowEndNode;
     statement = new ExprStatementNode(expr.head); 
 }

%}

%union {
  int                 ival;
  double              dval;
  UString             *ustr;
  Identifier          *ident;
  Expr                expr;
  Node                *node;
  StatementNode       *stat;
  ParameterNode       *param;
  FunctionBodyNode    *body;
  FuncDeclNode        *func;
  ProgramNode         *prog;
  SourceElementsNode  *srcs;
  StatListNode        *slist;
  ArgumentsNode       *args;
  ArgumentListNode    *alist;
  VarDeclNode         *decl;
  VarDeclListNode     *vlist;
  CaseBlockNode       *cblk;
  ClauseListNode      *clist;
  CaseClauseNode      *ccl;
  ElementNode         *elm;
  Operator            op;
  PropertyListNode   *plist;
  PropertyNode       *pnode;
  PropertyNameNode   *pname;
}

%start Program

/* literals */
%token NULLTOKEN TRUETOKEN FALSETOKEN
%token STRING NUMBER

/* keywords */
%token BREAK CASE DEFAULT FOR NEW VAR CONST CONTINUE
%token FUNCTION RETURN VOID DELETE
%token IF THIS DO WHILE IN INSTANCEOF TYPEOF
%token SWITCH WITH RESERVED
%token THROW TRY CATCH FINALLY

/* give an if without an else higher precedence than an else to resolve the ambiguity */
%nonassoc IF_WITHOUT_ELSE
%nonassoc ELSE

/* punctuators */
%token EQEQ NE                     /* == and != */
%token STREQ STRNEQ                /* === and !== */
%token LE GE                       /* < and > */
%token OR AND                      /* || and && */
%token PLUSPLUS MINUSMINUS         /* ++ and --  */
%token LSHIFT                      /* << */
%token RSHIFT URSHIFT              /* >> and >>> */
%token PLUSEQUAL MINUSEQUAL        /* += and -= */
%token MULTEQUAL DIVEQUAL          /* *= and /= */
%token LSHIFTEQUAL                 /* <<= */
%token RSHIFTEQUAL URSHIFTEQUAL    /* >>= and >>>= */
%token ANDEQUAL MODEQUAL           /* &= and %= */
%token XOREQUAL OREQUAL            /* ^= and |= */

/* terminal types */
%token <dval> NUMBER
%token <ustr> STRING
%token <ident> IDENT

/* automatically inserted semicolon */
%token AUTOPLUSPLUS AUTOMINUSMINUS

/* non-terminal types */
%type <expr>  Literal
%type <op>    ReadModifyAssignmentOperator
%type <expr>  AssignmentExpr AssignmentExprNoIn AssignmentExprNoBF
%type <expr>  ArrayLiteral
%type <expr>  PrimaryExpr PrimaryExprNoBrace
%type <expr>  PostfixExpr PostfixExprNoBF
%type <expr>  NewExpr NewExprNoBF
%type <expr>  MemberExpr MemberExprNoBF
%type <expr>  CallExpr CallExprNoBF
%type <args>  Arguments
%type <alist> ArgumentList
%type <expr>  UnaryExprCommon
%type <expr>  UnaryExpr UnaryExprNoBF
%type <expr>  LeftHandSideExpr LeftHandSideExprNoBF
%type <expr>  MultiplicativeExpr MultiplicativeExprNoBF
%type <expr>  AdditiveExpr AdditiveExprNoBF
%type <expr>  ShiftExpr ShiftExprNoBF
%type <expr>  RelationalExpr RelationalExprNoIn RelationalExprNoBF
%type <expr>  EqualityExpr EqualityExprNoIn EqualityExprNoBF
%type <expr>  BitwiseANDExpr BitwiseANDExprNoIn BitwiseANDExprNoBF
%type <expr>  BitwiseXORExpr BitwiseXORExprNoIn BitwiseXORExprNoBF
%type <expr>  BitwiseORExpr BitwiseORExprNoIn BitwiseORExprNoBF
%type <expr>  ConditionalExpr ConditionalExprNoIn ConditionalExprNoBF
%type <expr>  LogicalORExpr LogicalORExprNoIn LogicalORExprNoBF
%type <expr>  LogicalANDExpr LogicalANDExprNoIn LogicalANDExprNoBF
%type <expr>  FunctionExpr
%type <ival>  Elision ElisionOpt
%type <elm>   ElementList
%type <pname> PropertyName
%type <pnode> Property
%type <plist> PropertyList
%type <expr>  Expr ExprNoIn ExprNoBF

%type <stat>  SourceElement
%type <srcs>  SourceElements
%type <stat>  Statement
%type <stat>  ExprStatement
%type <stat>  Block
%type <stat>  EmptyStatement
%type <stat>  ContinueStatement
%type <stat>  BreakStatement
%type <stat>  LabelledStatement
%type <stat>  ReturnStatement
%type <stat>  ThrowStatement
%type <expr>  Initializer InitializerNoIn
%type <vlist> VariableDeclarationList VariableDeclarationListNoIn  ConstDeclarationList
%type <decl>  VariableDeclaration VariableDeclarationNoIn ConstDeclaration
%type <stat>  VariableStatement
%type <stat>  ConstStatement
%type <stat>  TryStatement
%type <stat>  IfStatement
%type <stat>  WithStatement
%type <stat>  DoWhileStatement
%type <stat>  WhileStatement
%type <stat>  ForStatement
%type <expr>  ExprOpt ExprNoInOpt

%type <param> FormalParameterList
%type <body>  FunctionBody
%type <func>  FunctionDeclaration

%%

Literal:
    NULLTOKEN                           { emitLeafExpression($$, new NullNode()); }
  | TRUETOKEN                           { emitLeafExpression($$, new BooleanNode(true)); }
  | FALSETOKEN                          { emitLeafExpression($$, new BooleanNode(false)); }
  | NUMBER                              { emitLeafExpression($$, new NumberNode($1)); }
  | STRING                              { emitLeafExpression($$, new StringNode($1)); }
  | '/' /* regexp */                    {
                                            Lexer *l = Lexer::curr();
                                            if (!l->scanRegExp()) YYABORT;
                                            emitLeafExpression($$, new RegExpNode(l->pattern, l->flags));
                                        }
  | DIVEQUAL /* regexp with /= */       {
                                            Lexer *l = Lexer::curr();
                                            if (!l->scanRegExp()) YYABORT;
                                            emitLeafExpression($$, new RegExpNode(UString('=') + l->pattern, l->flags));
                                        }
;

PropertyName:
    IDENT                               { $$ = new PropertyNameNode(*$1); }
  | STRING                              { $$ = new PropertyNameNode(Identifier(*$1)); }
  | NUMBER                              { $$ = new PropertyNameNode($1); }
;

Property:
     PropertyName ':' AssignmentExpr     { $$ = new PropertyNode($1, $3.head, $3.tail, PropertyNode::Constant); }
  | IDENT IDENT '(' ')' FunctionBody     { if (!makeGetterOrSetterPropertyNode($$, *$1, *$2, 0, $5)) YYABORT; }
  | IDENT IDENT '(' FormalParameterList ')' FunctionBody
                                         { if (!makeGetterOrSetterPropertyNode($$, *$1, *$2, $4, $6)) YYABORT; }
;

PropertyList:
    Property                            { $$ = new PropertyListNode($1); $1->m_tail->m_next = new PropertyEndNode($$); }
  | PropertyList ',' Property           { $$ = new PropertyListNode($3, $1); $3->m_tail->m_next = new PropertyEndNode($$); }
  ;

PrimaryExpr:
    PrimaryExprNoBrace
  | '{' '}'                             { emitLeafExpression($$, new ObjectLiteralNode); }
  | '{' PropertyList '}'                { emitLeafExpression($$, new ObjectLiteralNode($2)); }
   /* allow extra comma, see http://bugzilla.opendarwin.org/show_bug.cgi?id=5939 */
  | '{' PropertyList ',' '}'            { emitLeafExpression($$, new ObjectLiteralNode($2)); } 
;

PrimaryExprNoBrace:
    THIS                                { emitLeafExpression($$, new ThisNode); }
  | Literal                             
  | ArrayLiteral
  | IDENT                               { emitLeafExpression($$, new ResolveNode(*$1)); }
  | '(' Expr ')'                        { $$ = $2; }
;

ArrayLiteral:
    '[' ElisionOpt ']'                  { emitLeafExpression($$, new ArrayNode($2)); }
  | '[' ElementList ']'                 { emitLeafExpression($$, new ArrayNode($2)); }
  | '[' ElementList ',' ElisionOpt ']'  { emitLeafExpression($$, new ArrayNode($4, $2)); }
;

ElementList:
    ElisionOpt AssignmentExpr           { $$ = new ElementNode($1, $2.head); $2.tail->m_next = new ElementEndNode($$);}
  | ElementList ',' ElisionOpt AssignmentExpr
                                        { $$ = new ElementNode($1, $3, $4.head); $4.tail->m_next = new ElementEndNode($$); }
;

ElisionOpt:
  /* nothing */                         { $$ = 0; }
  | Elision
;

Elision:
    ','                                 { $$ = 1; }
  | Elision ','                         { $$ = $1 + 1; }
;

MemberExpr:
    PrimaryExpr
  | FunctionExpr                        { $$ = $1; }
  | MemberExpr '[' Expr ']'             { emitBinaryExpression($$, new BracketAccessorNode, $1, $3); }
  | MemberExpr '.' IDENT                { emitUnaryExpression($$, new DotAccessorNode(*$3), $1); }
  | NEW MemberExpr Arguments            { emitUnaryExpression($$, new NewExprNode($3), $2); }
;

MemberExprNoBF:
    PrimaryExprNoBrace
  | MemberExprNoBF '[' Expr ']'         { emitBinaryExpression($$, new BracketAccessorNode, $1, $3); }
  | MemberExprNoBF '.' IDENT            { emitUnaryExpression($$, new DotAccessorNode(*$3), $1); }
  | NEW MemberExpr Arguments            { emitUnaryExpression($$, new NewExprNode($3), $2); }
;

NewExpr:
    MemberExpr
  | NEW NewExpr                         { emitUnaryExpression($$, new NewExprNode, $2); }
;

NewExprNoBF:
    MemberExprNoBF
  | NEW NewExpr                         { emitUnaryExpression($$, new NewExprNode, $2); }
;

CallExpr:
    MemberExpr Arguments                { emitFunctionCall($$, $1, $2); }
  | CallExpr Arguments                  { emitFunctionCall($$, $1, $2); }
  | CallExpr '[' Expr ']'               { emitBinaryExpression($$, new BracketAccessorNode, $1, $3); }
  | CallExpr '.' IDENT                  { emitUnaryExpression($$, new DotAccessorNode(*$3), $1); }
;

CallExprNoBF:
    MemberExprNoBF Arguments            { emitFunctionCall($$, $1, $2); }
  | CallExprNoBF Arguments              { emitFunctionCall($$, $1, $2); }
  | CallExprNoBF '[' Expr ']'           { emitBinaryExpression($$, new BracketAccessorNode, $1, $3); }
  | CallExprNoBF '.' IDENT              { emitUnaryExpression($$, new DotAccessorNode(*$3), $1); }
;

Arguments:
    '(' ')'                             { $$ = new ArgumentsNode(); }
  | '(' ArgumentList ')'                { $$ = new ArgumentsNode($2); }
;

ArgumentList:
    AssignmentExpr                      { $$ = new ArgumentListNode($1.head); $1.tail->m_next = new ArgumentEndNode($$); }
  | ArgumentList ',' AssignmentExpr     { $$ = new ArgumentListNode($1, $3.head); $3.tail->m_next = new ArgumentEndNode($$); }
;

LeftHandSideExpr:
    NewExpr
  | CallExpr
;

LeftHandSideExprNoBF:
    NewExprNoBF
  | CallExprNoBF
;

PostfixExpr:
    LeftHandSideExpr
  | LeftHandSideExpr PLUSPLUS           { if (!emitPostfixExpression($$, $1, OpPlusPlus)) YYABORT; }
  | LeftHandSideExpr MINUSMINUS         { if (!emitPostfixExpression($$, $1, OpMinusMinus)) YYABORT; }
;

PostfixExprNoBF:
    LeftHandSideExprNoBF
  | LeftHandSideExprNoBF PLUSPLUS       { if (!emitPostfixExpression($$, $1, OpPlusPlus)) YYABORT; }
  | LeftHandSideExprNoBF MINUSMINUS     { if (!emitPostfixExpression($$, $1, OpMinusMinus)) YYABORT; }
;

UnaryExprCommon:
    DELETE UnaryExpr                    { emitDeleteExpression($$, $2); }
  | VOID UnaryExpr                      { emitUnaryExpression($$, new VoidNode, $2); }
  | TYPEOF UnaryExpr                    { emitTypeOfExpression($$, $2); }
  | PLUSPLUS UnaryExpr                  { if (!emitPrefixExpression($$, $2, OpPlusPlus)) YYABORT; }
  | AUTOPLUSPLUS UnaryExpr              { if (!emitPrefixExpression($$, $2, OpPlusPlus)) YYABORT; }
  | MINUSMINUS UnaryExpr                { if (!emitPrefixExpression($$, $2, OpMinusMinus)) YYABORT; }
  | AUTOMINUSMINUS UnaryExpr            { if (!emitPrefixExpression($$, $2, OpMinusMinus)) YYABORT; }
  | '+' UnaryExpr                       { emitUnaryExpression($$, new UnaryPlusNode, $2); }
  | '-' UnaryExpr                       { emitUnaryExpression($$, new NegateNode, $2); }
  | '~' UnaryExpr                       { emitUnaryExpression($$, new BitwiseNotNode, $2); }
  | '!' UnaryExpr                       { emitUnaryExpression($$, new LogicalNotNode, $2); }

UnaryExpr:
    PostfixExpr
  | UnaryExprCommon
;

UnaryExprNoBF:
    PostfixExprNoBF
  | UnaryExprCommon
;

MultiplicativeExpr:
    UnaryExpr
  | MultiplicativeExpr '*' UnaryExpr    { emitBinaryExpression($$, new MultNode('*'), $1, $3); }
  | MultiplicativeExpr '/' UnaryExpr    { emitBinaryExpression($$, new MultNode('/'), $1, $3); }
  | MultiplicativeExpr '%' UnaryExpr    { emitBinaryExpression($$, new MultNode('%'), $1, $3); }
;

MultiplicativeExprNoBF:
    UnaryExprNoBF
  | MultiplicativeExprNoBF '*' UnaryExpr
                                        { emitBinaryExpression($$, new MultNode('*'), $1, $3); }
  | MultiplicativeExprNoBF '/' UnaryExpr
                                        { emitBinaryExpression($$, new MultNode('/'), $1, $3); }
  | MultiplicativeExprNoBF '%' UnaryExpr
                                        { emitBinaryExpression($$, new MultNode('%'), $1, $3); }
;

AdditiveExpr:
    MultiplicativeExpr
  | AdditiveExpr '+' MultiplicativeExpr { emitBinaryExpression($$, new AddNode('+'), $1, $3); }

  | AdditiveExpr '-' MultiplicativeExpr { emitBinaryExpression($$, new AddNode('-'), $1, $3); }
;

AdditiveExprNoBF:
    MultiplicativeExprNoBF
  | AdditiveExprNoBF '+' MultiplicativeExpr
                                        { emitBinaryExpression($$, new AddNode('+'), $1, $3); }
  | AdditiveExprNoBF '-' MultiplicativeExpr
                                        { emitBinaryExpression($$, new AddNode('-'), $1, $3); }
;

ShiftExpr:
    AdditiveExpr
  | ShiftExpr LSHIFT AdditiveExpr       { emitBinaryExpression($$, new ShiftNode(OpLShift), $1, $3); }
  | ShiftExpr RSHIFT AdditiveExpr       { emitBinaryExpression($$, new ShiftNode(OpRShift), $1, $3); }
  | ShiftExpr URSHIFT AdditiveExpr      { emitBinaryExpression($$, new ShiftNode(OpURShift), $1, $3); }
;

ShiftExprNoBF:
    AdditiveExprNoBF
  | ShiftExprNoBF LSHIFT AdditiveExpr   { emitBinaryExpression($$, new ShiftNode(OpLShift), $1, $3); }
  | ShiftExprNoBF RSHIFT AdditiveExpr   { emitBinaryExpression($$, new ShiftNode(OpRShift), $1, $3); }
  | ShiftExprNoBF URSHIFT AdditiveExpr  { emitBinaryExpression($$, new ShiftNode(OpURShift), $1, $3); }
;

RelationalExpr:
    ShiftExpr
  | RelationalExpr '<' ShiftExpr        { emitBinaryExpression($$, new RelationalNode(OpLess), $1, $3); }
  | RelationalExpr '>' ShiftExpr        { emitBinaryExpression($$, new RelationalNode(OpGreater), $1, $3); }
  | RelationalExpr LE ShiftExpr         { emitBinaryExpression($$, new RelationalNode(OpLessEq), $1, $3); }
  | RelationalExpr GE ShiftExpr         { emitBinaryExpression($$, new RelationalNode(OpGreaterEq), $1, $3); }
  | RelationalExpr INSTANCEOF ShiftExpr { emitBinaryExpression($$, new RelationalNode(OpInstanceOf), $1, $3); }
  | RelationalExpr IN ShiftExpr         { emitBinaryExpression($$, new RelationalNode(OpIn), $1, $3); }
;

RelationalExprNoIn:
    ShiftExpr
  | RelationalExprNoIn '<' ShiftExpr    { emitBinaryExpression($$, new RelationalNode(OpLess), $1, $3); }
  | RelationalExprNoIn '>' ShiftExpr    { emitBinaryExpression($$, new RelationalNode(OpGreater), $1, $3); }
  | RelationalExprNoIn LE ShiftExpr     { emitBinaryExpression($$, new RelationalNode(OpLessEq), $1, $3); }
  | RelationalExprNoIn GE ShiftExpr     { emitBinaryExpression($$, new RelationalNode(OpGreaterEq), $1, $3); }
  | RelationalExprNoIn INSTANCEOF ShiftExpr
                                        { emitBinaryExpression($$, new RelationalNode(OpInstanceOf), $1, $3); }
;

RelationalExprNoBF:
    ShiftExprNoBF
  | RelationalExprNoBF '<' ShiftExpr    { emitBinaryExpression($$, new RelationalNode(OpLess), $1, $3); }
  | RelationalExprNoBF '>' ShiftExpr    { emitBinaryExpression($$, new RelationalNode(OpGreater), $1, $3); }
  | RelationalExprNoBF LE ShiftExpr     { emitBinaryExpression($$, new RelationalNode(OpLessEq), $1, $3); }
  | RelationalExprNoBF GE ShiftExpr     { emitBinaryExpression($$, new RelationalNode(OpGreaterEq), $1, $3); }
  | RelationalExprNoBF INSTANCEOF ShiftExpr
                                        { emitBinaryExpression($$, new RelationalNode(OpInstanceOf), $1, $3); }
  | RelationalExprNoBF IN ShiftExpr     { emitBinaryExpression($$, new RelationalNode(OpIn), $1, $3); }
;

EqualityExpr:
    RelationalExpr
  | EqualityExpr EQEQ RelationalExpr    { emitBinaryExpression($$, new EqualNode(OpEqEq), $1, $3); }
  | EqualityExpr NE RelationalExpr      { emitBinaryExpression($$, new EqualNode(OpNotEq), $1, $3); }
  | EqualityExpr STREQ RelationalExpr   { emitBinaryExpression($$, new EqualNode(OpStrEq), $1, $3); }
  | EqualityExpr STRNEQ RelationalExpr  { emitBinaryExpression($$, new EqualNode(OpStrNEq), $1, $3); }
;

EqualityExprNoIn:
    RelationalExprNoIn
  | EqualityExprNoIn EQEQ RelationalExprNoIn
                                        { emitBinaryExpression($$, new EqualNode(OpEqEq), $1, $3); }
  | EqualityExprNoIn NE RelationalExprNoIn
                                        { emitBinaryExpression($$, new EqualNode(OpNotEq), $1, $3); }
  | EqualityExprNoIn STREQ RelationalExprNoIn
                                        { emitBinaryExpression($$, new EqualNode(OpStrEq), $1, $3); }
  | EqualityExprNoIn STRNEQ RelationalExprNoIn
                                        { emitBinaryExpression($$, new EqualNode(OpStrNEq), $1, $3); }
;

EqualityExprNoBF:
    RelationalExprNoBF
  | EqualityExprNoBF EQEQ RelationalExpr
                                        { emitBinaryExpression($$, new EqualNode(OpEqEq), $1, $3); }
  | EqualityExprNoBF NE RelationalExpr  { emitBinaryExpression($$, new EqualNode(OpNotEq), $1, $3); }
  | EqualityExprNoBF STREQ RelationalExpr
                                        { emitBinaryExpression($$, new EqualNode(OpStrEq), $1, $3); }
  | EqualityExprNoBF STRNEQ RelationalExpr
                                        { emitBinaryExpression($$, new EqualNode(OpStrNEq), $1, $3); }
;

BitwiseANDExpr:
    EqualityExpr
  | BitwiseANDExpr '&' EqualityExpr     { emitBinaryExpression($$, new BitOperNode(OpBitAnd), $1, $3); }
;

BitwiseANDExprNoIn:
    EqualityExprNoIn
  | BitwiseANDExprNoIn '&' EqualityExprNoIn
                                        { emitBinaryExpression($$, new BitOperNode(OpBitAnd), $1, $3); }
;

BitwiseANDExprNoBF:
    EqualityExprNoBF
  | BitwiseANDExprNoBF '&' EqualityExpr { emitBinaryExpression($$, new BitOperNode(OpBitAnd), $1, $3); }
;

BitwiseXORExpr:
    BitwiseANDExpr
  | BitwiseXORExpr '^' BitwiseANDExpr
                                        { emitBinaryExpression($$, new BitOperNode(OpBitXOr), $1, $3); }
;

BitwiseXORExprNoIn:
    BitwiseANDExprNoIn
  | BitwiseXORExprNoIn '^' BitwiseANDExprNoIn
                                        { emitBinaryExpression($$, new BitOperNode(OpBitXOr), $1, $3); }
;

BitwiseXORExprNoBF:
    BitwiseANDExprNoBF
  | BitwiseXORExprNoBF '^' BitwiseANDExpr
                                        { emitBinaryExpression($$, new BitOperNode(OpBitXOr), $1, $3); }
;

BitwiseORExpr:
    BitwiseXORExpr
  | BitwiseORExpr '|' BitwiseXORExpr
                                        { emitBinaryExpression($$, new BitOperNode(OpBitOr), $1, $3); }
;

BitwiseORExprNoIn:
    BitwiseXORExprNoIn
  | BitwiseORExprNoIn '|' BitwiseXORExprNoIn
                                        { emitBinaryExpression($$, new BitOperNode(OpBitOr), $1, $3); }
;


BitwiseORExprNoBF:
    BitwiseXORExprNoBF
  | BitwiseORExprNoBF '|' BitwiseXORExpr
                                        { emitBinaryExpression($$, new BitOperNode(OpBitOr), $1, $3); }
;

LogicalANDExpr:
    BitwiseORExpr
  | LogicalANDExpr AND BitwiseORExpr
                                        { emitLogicalAND($$, $1, $3); }
;

LogicalANDExprNoIn:
    BitwiseORExprNoIn
  | LogicalANDExprNoIn AND BitwiseORExprNoIn
                                        { emitLogicalAND($$, $1, $3); }
;

LogicalANDExprNoBF:
    BitwiseORExprNoBF
  | LogicalANDExprNoBF AND BitwiseORExpr
                                        { emitLogicalAND($$, $1, $3); }
;

LogicalORExpr:
     LogicalANDExpr
   | LogicalORExpr OR LogicalANDExpr    { emitLogicalOR($$, $1, $3); }
;

LogicalORExprNoIn:
    LogicalANDExprNoIn
  | LogicalORExprNoIn OR LogicalANDExprNoIn
                                        { emitLogicalOR($$, $1, $3); }
;

LogicalORExprNoBF:
     LogicalANDExprNoBF
   | LogicalORExprNoBF OR LogicalANDExpr { emitLogicalOR($$, $1, $3); }
;

ConditionalExpr:
    LogicalORExpr
  | LogicalORExpr '?' AssignmentExpr ':' AssignmentExpr
                                        { emitConditionalExpression($$, $1, $3, $5); }
;

ConditionalExprNoIn:
    LogicalORExprNoIn
  | LogicalORExprNoIn '?' AssignmentExprNoIn ':' AssignmentExprNoIn
                                        { emitConditionalExpression($$, $1, $3, $5); }
;

ConditionalExprNoBF:
    LogicalORExprNoBF
  | LogicalORExprNoBF '?' AssignmentExpr ':' AssignmentExpr
                                        { emitConditionalExpression($$, $1, $3, $5); }
;

AssignmentExpr:
    ConditionalExpr
  | LeftHandSideExpr '=' AssignmentExpr
                                        { if (!emitAssignment($$, $1, $3)) YYABORT; }
  | LeftHandSideExpr ReadModifyAssignmentOperator AssignmentExpr
                                        { if (!emitReadModifyAssignment($$, $1, $2, $3)) YYABORT; }
;

AssignmentExprNoIn:
    ConditionalExprNoIn
  | LeftHandSideExpr '=' AssignmentExprNoIn
                                        { if (!emitAssignment($$, $1, $3)) YYABORT; }
  | LeftHandSideExpr ReadModifyAssignmentOperator AssignmentExprNoIn
                                        { if (!emitReadModifyAssignment($$, $1, $2, $3)) YYABORT; }
;

AssignmentExprNoBF:
    ConditionalExprNoBF
  | LeftHandSideExprNoBF '=' AssignmentExpr
                                        { if (!emitAssignment($$, $1, $3)) YYABORT; }
  | LeftHandSideExprNoBF ReadModifyAssignmentOperator AssignmentExpr
                                        { if (!emitReadModifyAssignment($$, $1, $2, $3)) YYABORT; }
;

ReadModifyAssignmentOperator:
    PLUSEQUAL                           { $$ = OpPlusEq; }
  | MINUSEQUAL                          { $$ = OpMinusEq; }
  | MULTEQUAL                           { $$ = OpMultEq; }
  | DIVEQUAL                            { $$ = OpDivEq; }
  | LSHIFTEQUAL                         { $$ = OpLShift; }
  | RSHIFTEQUAL                         { $$ = OpRShift; }
  | URSHIFTEQUAL                        { $$ = OpURShift; }
  | ANDEQUAL                            { $$ = OpAndEq; }
  | XOREQUAL                            { $$ = OpXOrEq; }
  | OREQUAL                             { $$ = OpOrEq; }
  | MODEQUAL                            { $$ = OpModEq; }
;

Expr:
    AssignmentExpr
  | Expr ',' AssignmentExpr             { emitBinaryExpression($$, new CommaNode, $1, $3); }
;

ExprNoIn:
    AssignmentExprNoIn
  | ExprNoIn ',' AssignmentExprNoIn     { emitBinaryExpression($$, new CommaNode, $1, $3); }
;

ExprNoBF:
    AssignmentExprNoBF
  | ExprNoBF ',' AssignmentExpr         { emitBinaryExpression($$, new CommaNode, $1, $3); }
;

Statement:
    Block
  | VariableStatement
  | ConstStatement
  | EmptyStatement
  | ExprStatement
  | IfStatement
  | DoWhileStatement
  | WhileStatement
  | ForStatement
  /* | ForInStatement */
  | ContinueStatement
  | BreakStatement
  | ReturnStatement
  | WithStatement
  /* | SwitchStatement */
  | LabelledStatement
  | ThrowStatement
  | TryStatement
;

Block:
    '{' '}'                             { $$ = new BlockNode(0); DBG($$, @2, @2); }
  | '{' SourceElements '}'              { $$ = new BlockNode($2); DBG($$, @3, @3); }
;

/*
StatementList:
    Statement                           { $$ = new StatListNode($1); }
  | StatementList Statement             { $$ = new StatListNode($1, $2); }
;
*/

VariableStatement:
    VAR VariableDeclarationList ';'     { $$ = new VarStatementNode($2); DBG($$, @1, @3); }
  | VAR VariableDeclarationList error   { $$ = new VarStatementNode($2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

VariableDeclarationList:
    VariableDeclaration                 { $$ = new VarDeclListNode($1); }
  | VariableDeclarationList ',' VariableDeclaration
                                        { $$ = new VarDeclListNode($1, $3); }
;

VariableDeclarationListNoIn:
    VariableDeclarationNoIn             { $$ = new VarDeclListNode($1); }
  | VariableDeclarationListNoIn ',' VariableDeclarationNoIn
                                        { $$ = new VarDeclListNode($1, $3); }
;

VariableDeclaration:
    IDENT                               { $$ = new VarDeclNode(*$1, 0, VarDeclNode::Variable); }
  | IDENT Initializer                   { $$ = new VarDeclNode(*$1, $2.head, VarDeclNode::Variable); $2.tail->m_next = new VarDeclEndNode($$); }
;

VariableDeclarationNoIn:
    IDENT                               { $$ = new VarDeclNode(*$1, 0, VarDeclNode::Variable); }
  | IDENT InitializerNoIn               { $$ = new VarDeclNode(*$1, $2.head, VarDeclNode::Variable); $2.tail->m_next = new VarDeclEndNode($$); }
;

ConstStatement:
    CONST ConstDeclarationList ';'      { $$ = new VarStatementNode($2); DBG($$, @1, @3); }
  | CONST ConstDeclarationList error    { $$ = new VarStatementNode($2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

ConstDeclarationList:
    ConstDeclaration                    { $$ = new VarDeclListNode($1); }
  | ConstDeclarationList ',' ConstDeclaration
                                        { $$ = new VarDeclListNode($1, $3); }
;

ConstDeclaration:
    IDENT                               { $$ = new VarDeclNode(*$1, 0, VarDeclNode::Constant); }
  | IDENT Initializer                   { $$ = new VarDeclNode(*$1, $2.head, VarDeclNode::Constant); $2.tail->m_next = new VarDeclEndNode($$); }
;

Initializer:
    '=' AssignmentExpr                  { $$ = $2; }
;

InitializerNoIn:
    '=' AssignmentExprNoIn              { $$ = $2; }
;

EmptyStatement:
    ';'                                 { $$ = new EmptyStatementNode; }
;

ExprStatement:
    ExprNoBF ';'                        { emitExpressionStatement($$, $1); DBG($$, @1, @2); }
  | ExprNoBF error                      { emitExpressionStatement($$, $1); DBG($$, @1, @1); AUTO_SEMICOLON; }
;

IfStatement:
    IF '(' Expr ')' Statement %prec IF_WITHOUT_ELSE
                                        { $$ = new IfNode($3.head, $5, 0); $3.tail->m_next = new IfConditionEndNode(static_cast<IfNode*>($$)); DBG($$, @1, @4); }
  | IF '(' Expr ')' Statement ELSE Statement
                                        { $$ = new IfNode($3.head, $5, $7); $3.tail->m_next = new IfConditionEndNode(static_cast<IfNode*>($$)); DBG($$, @1, @4); }
;

DoWhileStatement:
    DO Statement WHILE '(' Expr ')'     { $$ = new DoWhileNode($2, $5.head); $5.tail->m_next = new DoWhileTestExprEndNode(static_cast<DoWhileNode*>($$)); DBG($$, @1, @3);}

WhileStatement:
    WHILE '(' Expr ')' Statement        { $$ = new WhileNode($3.head, $5); $3.tail->m_next = new WhileTestExprEndNode(static_cast<WhileNode*>($$)); DBG($$, @1, @4); }

ForStatement:
  FOR '(' ExprNoInOpt ';' ExprOpt ';' ExprOpt ')' Statement
                                        { emitForLoop($$, $3, 0, $5, $7, $9); DBG($$, @1, @8); }
| FOR '(' VAR VariableDeclarationListNoIn ';' ExprOpt ';' ExprOpt ')' Statement
                                        { emitForLoop($$, Expr(), $4, $6, $8, $10); DBG($$, @1, @9); }

/*
ForInStatement:
    FOR '(' LeftHandSideExpr IN Expr ')' Statement
                                        {
                                            Node *n = $3->nodeInsideAllParens();
                                            if (!n->isLocation())
                                                YYABORT;
                                            $$ = new ForInNode(n, $5, $7);
                                            DBG($$, @1, @6);
                                        }
  | FOR '(' VAR IDENT IN Expr ')' Statement
                                        { $$ = new ForInNode(*$4, 0, $6, $8); DBG($$, @1, @7); }
  | FOR '(' VAR IDENT InitializerNoIn IN Expr ')' Statement
                                        { $$ = new ForInNode(*$4, $5, $7, $9); DBG($$, @1, @8); }
;
*/

ExprOpt:
    /* nothing */                       { emitNoExpr($$); }
  | Expr
;

ExprNoInOpt:
    /* nothing */                       { emitNoExpr($$); }
  | ExprNoIn
;

ContinueStatement:
    CONTINUE ';'                        { $$ = new ContinueNode(); DBG($$, @1, @2); }
  | CONTINUE error                      { $$ = new ContinueNode(); DBG($$, @1, @1); AUTO_SEMICOLON; }
  | CONTINUE IDENT ';'                  { $$ = new ContinueNode(*$2); DBG($$, @1, @3); }
  | CONTINUE IDENT error                { $$ = new ContinueNode(*$2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

BreakStatement:
    BREAK ';'                           { $$ = new BreakNode(); DBG($$, @1, @2); }
  | BREAK error                         { $$ = new BreakNode(); DBG($$, @1, @1); AUTO_SEMICOLON; }
  | BREAK IDENT ';'                     { $$ = new BreakNode(*$2); DBG($$, @1, @3); }
  | BREAK IDENT error                   { $$ = new BreakNode(*$2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

ReturnStatement:
    RETURN ';'                          { $$ = new ReturnNode; DBG($$, @1, @2); }
  | RETURN error                        { $$ = new ReturnNode; DBG($$, @1, @1); AUTO_SEMICOLON; }
  | RETURN Expr ';'                     { emitValueReturn($$, $2); DBG($$, @1, @3); }
  | RETURN Expr error                   { emitValueReturn($$, $2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

WithStatement:
    WITH '(' Expr ')' Statement         { $$ = new WithNode($3.head, $5); $3.tail->m_next = new WithExprEndNode(static_cast<WithNode*>($$)); DBG($$, @1, @4); }
;

/*
SwitchStatement:
    SWITCH '(' Expr ')' CaseBlock       { $$ = new SwitchNode($3, $5); DBG($$, @1, @4); }
;

CaseBlock:
    '{' CaseClausesOpt '}'              { $$ = new CaseBlockNode($2, 0, 0); }
  | '{' CaseClausesOpt DefaultClause CaseClausesOpt '}'
                                        { $$ = new CaseBlockNode($2, $3, $4); }
;

CaseClausesOpt:
*/ /* nothing */ /*                       { $$ = 0; }
  | CaseClauses
;

CaseClauses:
    CaseClause                          { $$ = new ClauseListNode($1); }
  | CaseClauses CaseClause              { $$ = new ClauseListNode($1, $2); }
;

CaseClause:
    CASE Expr ':'                       { $$ = new CaseClauseNode($2); }
  | CASE Expr ':' StatementList         { $$ = new CaseClauseNode($2, $4); }
;

DefaultClause:
    DEFAULT ':'                         { $$ = new CaseClauseNode(0); }
  | DEFAULT ':' StatementList           { $$ = new CaseClauseNode(0, $3); }
;

*/

LabelledStatement:
    IDENT ':' Statement                 { $3->pushLabel(*$1); $$ = new LabelNode(*$1, $3); }
;

ThrowStatement:
    THROW Expr ';'                      { emitThrow($$, $2); DBG($$, @1, @3); }
  | THROW Expr error                    { emitThrow($$, $2); DBG($$, @1, @2); AUTO_SEMICOLON; }
;

TryStatement:
    TRY Block FINALLY Block             { $$ = new TryNode($2, Identifier::null(), 0, $4); DBG($$, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block { $$ = new TryNode($2, *$5, $7, 0); DBG($$, @1, @2); }
  | TRY Block CATCH '(' IDENT ')' Block FINALLY Block
                                        { $$ = new TryNode($2, *$5, $7, $9); DBG($$, @1, @2); }
;

FunctionDeclaration:
    FUNCTION IDENT '(' ')' FunctionBody { $$ = new FuncDeclNode(*$2, $5); }
  | FUNCTION IDENT '(' FormalParameterList ')' FunctionBody
                                        { $$ = new FuncDeclNode(*$2, $4, $6); }
;

FunctionExpr:
    FUNCTION '(' ')' FunctionBody       { emitLeafExpression($$, new FuncExprNode(Identifier::null(), $4)); }
  | FUNCTION '(' FormalParameterList ')' FunctionBody
                                        { emitLeafExpression($$, new FuncExprNode(Identifier::null(), $5, $3)); }
  | FUNCTION IDENT '(' ')' FunctionBody { emitLeafExpression($$, new FuncExprNode(*$2, $5)); }
  | FUNCTION IDENT '(' FormalParameterList ')' FunctionBody
                                        { emitLeafExpression($$, new FuncExprNode(*$2, $6, $4)); }
;

FormalParameterList:
    IDENT                               { $$ = new ParameterNode(*$1); }
  | FormalParameterList ',' IDENT       { $$ = new ParameterNode($1, *$3); }
;

FunctionBody:
'{' '}' /* not in spec */           { $$ = new FunctionBodyNode(0); DBG($$, @1, @2); }
  | '{' SourceElements '}'              { $$ = new FunctionBodyNode($2); DBG($$, @1, @3); }
;

SourceElements:
    SourceElement                       { $$ = new SourceElementsNode($1); }
  | SourceElements SourceElement        { $$ = new SourceElementsNode($1, $2); }
;

SourceElement:
    FunctionDeclaration                 { $$ = $1; }
  | Statement                           { $$ = $1; }
;

Program:
    /* not in spec */                   { Parser::accept(new ProgramNode(0)); }
    | SourceElements                    { Parser::accept(new ProgramNode($1)); }
;
 
%%

static bool emitPrefixExpression(Expr& e, const Expr& loc, Operator op)
{
    Node *n = loc.tail;

    if (!n->isLocation())
        return false;
    
    if (n->isResolveNode()) {
        n->m_opcode = ResolveBaseAndValueEvaluateState;
        emitUnaryExpression(e, new PrefixNode(static_cast<ResolveNode*>(n)->identifier(), op), loc);
    } else if (n->isBracketAccessorNode()) {
        n->m_opcode = BracketAccessorBaseSubscriptAndValueEvaluateState;
        emitUnaryExpression(e, new PrefixBracketNode(op), loc);
    } else {
        n->m_opcode = DotAccessorBaseAndValueEvaluateState;
        emitUnaryExpression(e, new PrefixNode(static_cast<DotAccessorNode*>(n)->identifier(), op), loc);
    }

    return true;
}

static bool emitPostfixExpression(Expr& e, const Expr& loc, Operator op)
{
    Node *n = loc.tail;

    if (!n->isLocation())
        return false;
    
    if (n->isResolveNode()) {
        n->m_opcode = ResolveBaseAndValueEvaluateState;
        emitUnaryExpression(e, new PostfixNode(static_cast<ResolveNode*>(n)->identifier(), op), loc);
    } else if (n->isBracketAccessorNode()) {
        n->m_opcode = BracketAccessorBaseSubscriptAndValueEvaluateState;
        emitUnaryExpression(e, new PostfixBracketNode(op), loc);
    } else {
        n->m_opcode = DotAccessorBaseAndValueEvaluateState;
        emitUnaryExpression(e, new PostfixNode(static_cast<DotAccessorNode*>(n)->identifier(), op), loc);
    }

    return true;
}

static bool emitReadModifyAssignment(Expr& e, const Expr& loc, Operator op, const Expr& expr)
{ 
    Node *n = loc.tail;

    if (!n->isLocation())
        return false;

    if (n->isResolveNode()) {
        n->m_opcode = ResolveBaseAndValueEvaluateState;
        emitBinaryExpression(e, new ReadModifyAssignNode(op, static_cast<ResolveNode*>(n)->ident), loc, expr);
    } else if (n->isBracketAccessorNode()) {
        n->m_opcode = BracketAccessorBaseSubscriptAndValueEvaluateState;
        emitBinaryExpression(e, new ReadModifyAssignBracketNode(op), loc, expr);
    } else {
        ASSERT(n->isDotAccessorNode());
        n->m_opcode = DotAccessorBaseAndValueEvaluateState;
        emitBinaryExpression(e, new ReadModifyAssignNode(op, static_cast<DotAccessorNode*>(n)->ident), loc, expr);
    }

    return true;
}

static bool emitAssignment(Expr& e, const Expr& loc, const Expr& expr)
{ 
    Node *n = loc.tail;

    if (!n->isLocation())
        return false;

    if (n->isResolveNode()) {
        n->m_opcode = ResolveBaseEvaluateState;
        emitBinaryExpression(e, new AssignNode(static_cast<ResolveNode*>(n)->ident), loc, expr);
    } else if (n->isBracketAccessorNode()) {
        n->m_opcode = BracketAccessorBaseAndSubscriptEvaluateState;
        emitBinaryExpression(e, new AssignBracketNode, loc, expr);
    } else {
        ASSERT(n->isDotAccessorNode());
        n->m_opcode = DotAccessorBaseEvaluateState;
        emitBinaryExpression(e, new AssignNode(static_cast<DotAccessorNode*>(n)->ident), loc, expr);
    }

    return true;
}

static void emitFunctionCall(Expr& e, const Expr& func, ArgumentsNode *args)
{
    Node *n = func.tail;
    
    if (!n->isLocation()) {
        emitUnaryExpression(e, new FunctionCallNode(args, false), func);
    } else if (n->isResolveNode()) {
        n->m_opcode = ResolveBaseAndValueEvaluateState;
        emitUnaryExpression(e, new FunctionCallNode(args), func);
    } else if (n->isBracketAccessorNode()) {
        n->m_opcode = BracketAccessorBaseAndValueEvaluateState;
        emitUnaryExpression(e, new FunctionCallNode(args), func);
    } else {
        ASSERT(n->isDotAccessorNode());
        n->m_opcode = DotAccessorBaseAndValueEvaluateState;
        emitUnaryExpression(e, new FunctionCallNode(args), func);
    }
}

static void emitTypeOfExpression(Expr& e, const Expr& loc)
{
    Node *n = loc.tail;

    if (n->isResolveNode()) {
        n->m_opcode = ResolveBaseEvaluateState;
        emitUnaryExpression(e, new TypeOfResolveNode(static_cast<ResolveNode *>(n)->identifier()), loc);
    } else
        emitUnaryExpression(e, new TypeOfValueNode, loc);
}

static void emitDeleteExpression(Expr& e, const Expr& loc)
{
    Node *n = loc.tail;
    
    if (!n->isLocation())
        emitUnaryExpression(e, new DeleteValueNode, loc);
    else if (n->isResolveNode()) {
        n->m_opcode = ResolveBaseEvaluateState;
        emitUnaryExpression(e, new DeleteNode(static_cast<ResolveNode *>(n)->identifier()), loc);
    } else if (n->isBracketAccessorNode()) {
        n->m_opcode = BracketAccessorBaseAndSubscriptEvaluateState;
        emitUnaryExpression(e, new DeleteBracketNode, loc);
    } else {
        ASSERT(n->isDotAccessorNode());
        n->m_opcode = DotAccessorBaseEvaluateState;
        emitUnaryExpression(e, new DeleteNode(static_cast<DotAccessorNode *>(n)->identifier()), loc);
    }
}

static void emitConditionalExpression(Expr& e, const Expr& condition, const Expr& thenExpr, const Expr& elseExpr)
{
    /* conditional expressions of the form "condition ? thenExpr : elseExpr" parse into this structure:
    
                                                                     +---------------------+
                                                                     |                     |
                                                                     |                    \|/
       [ condition ] -> [ <JumpIfFalse> ] -> [ thenExpr ] -> [ <Jump> ] -> [ elseExpr ] -> [ <NoOp> ]
                                      |                                    /|\
                                      |                                     |
                                      +-------------------------------------+
    */
    /* FIXME: instead of a no-op we should insert a placeholder that
       can then be fixed up once the next expression is known */

    Expr conditionWithJump;
    emitUnaryExpression(conditionWithJump, new JumpNode(JumpIfFalseEvaluateState, elseExpr.head), condition);
    NoOpNode* noOp = new NoOpNode;

    Expr conditionWithThenAndJumps;
    emitBinaryExpression(conditionWithThenAndJumps, new JumpNode(JumpEvaluateState, noOp), conditionWithJump, thenExpr);
    
    emitBinaryExpression(e, noOp, conditionWithThenAndJumps, elseExpr);
}

static void emitLogicalAND(Expr& e, const Expr& left, const Expr& right)
{
    /* short circuit AND expressions of the form "left && right" parse into this structure:
    
       [ left ] -> [ <JumpAndSaveIfFalse> ] -> [ right ] -> [ <NoOp> ]
                                         |                  /|\
                                         |                   |
                                         +-------------------+
    */
    /* FIXME: instead of a no-op we should insert a placeholder that
       can then be fixed up once the next expression is known */

    NoOpNode* noOp = new NoOpNode;

    Expr leftWithJump;
    emitUnaryExpression(leftWithJump, new JumpNode(JumpAndSaveIfFalseEvaluateState, noOp), left);
    emitBinaryExpression(e, noOp, leftWithJump, right);
}

static void emitLogicalOR(Expr& e, const Expr& left, const Expr& right)
{
    /* short circuit OR expressions of the form "left || right" parse into this structure:
    
       [ left ] -> [ <JumpAndSaveIfTrue> ] -> [ right ] -> [ <NoOp> ]
                                        |                  /|\
                                        |                   |
                                        +-------------------+
    */
    /* FIXME: instead of a no-op we should insert a placeholder that
       can then be fixed up once the next expression is known */

    NoOpNode* noOp = new NoOpNode;

    Expr leftWithJump;
    emitUnaryExpression(leftWithJump, new JumpNode(JumpAndSaveIfTrueEvaluateState, noOp), left);
    emitBinaryExpression(e, noOp, leftWithJump, right);
}

static void emitNoExpr(Expr& e)
{
    e.head = 0;
    e.tail = 0;
}

static void emitForLoop(StatementNode*& stat, const Expr& i, VarDeclListNode* varDecls, const Expr& t, const Expr& u, StatementNode* body)
{
    Expr init;
    if (varDecls)
        emitNoExpr(init);
    else if (i.head)
        init = i;
    else
        emitLeafExpression(init, new NullNode);

    Expr test;
    if (t.head)
        test = t;
    else
        emitLeafExpression(test, new BooleanNode(true));

    Expr update;
    if (u.head)
        update = u;
    else
        emitLeafExpression(update, new NullNode);

    ForNode* result;

    if (varDecls)
        result = new ForNode(varDecls, test.head, update.head, body);
    else {
        result = new ForNode(init.head, test.head, update.head, body);
        init.tail->m_next = new ForExprEndNode(result, ForNodeInitOrIncrementEndState);
    }

    test.tail->m_next = new ForExprEndNode(result, ForNodeTestEndState);
    update.tail->m_next = new ForExprEndNode(result, ForNodeInitOrIncrementEndState);
    stat = result;
}

static bool makeGetterOrSetterPropertyNode(PropertyNode*& result, Identifier& getOrSet, Identifier& name, ParameterNode *params, FunctionBodyNode *body)
{
    PropertyNode::Type type;
    
    if (getOrSet == "get")
        type = PropertyNode::Getter;
    else if (getOrSet == "set")
        type = PropertyNode::Setter;
    else
        return false;
    
    FuncExprNode* func = new FuncExprNode(Identifier::null(), body, params);
    result = new PropertyNode(new PropertyNameNode(name), func, func, type);

    return true;
}

/* called by yyparse on error */
int yyerror(const char *)
{
    return 1;
}

/* may we automatically insert a semicolon ? */
static bool allowAutomaticSemicolon()
{
    return yychar == '}' || yychar == 0 || Lexer::curr()->prevTerminator();
}
