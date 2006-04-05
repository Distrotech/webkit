/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003 Apple Computer, Inc.
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
#include "nodes.h"

#include "collector.h"
#include "context.h"
#include "debugger.h"
#include "function_object.h"
#include "internal.h"
#include "interpreter.h"
#include "lexer.h"
#include "object.h"
#include "operations.h"
#include "reference_list.h"
#include "types.h"
#include "ustring.h"
#include "value.h"
#include <kxmlcore/HashCountedSet.h>
#include <kxmlcore/HashSet.h>

#include <assert.h>
#include <math.h>

#ifdef KJS_DEBUG_MEM
#include <stdio.h>
#include <typeinfo>
#endif

using namespace KJS;

#define KJS_EVALUATE_METHOD_CHECKEXCEPTION \
if (exec->hadException()) { \
    setExceptionDetailsIfNeeded(exec); \
    JSValue *ex = exec->exception(); \
    exec->clearException(); \
    return Completion(Throw, ex); \
} \
if (Collector::isOutOfMemory()) \
    return Completion(Throw, Error::create(exec, GeneralError, "Out of memory"));

// ------------------------------ Node -----------------------------------------

#ifndef NDEBUG
struct NodeCounter { 
    static int count; 
    ~NodeCounter() { if (count != 0) fprintf(stderr, "LEAK: %d KJS::Node\n", count); }
};
int NodeCounter::count = 0;
static NodeCounter nodeImplCounter;
#endif NDEBUG

static HashSet<Node*>* newNodes;
static HashCountedSet<Node*>* nodeExtraRefCounts;

Node::Node(InterpreterState interpreterState) : m_interpreterState(interpreterState)
{
#ifndef NDEBUG
    ++NodeCounter::count;
#endif
  m_line = Lexer::curr()->lineNo();
  if (!newNodes)
      newNodes = new HashSet<Node*>;
  newNodes->add(this);
}

Node::~Node()
{
#ifndef NDEBUG
    --NodeCounter::count;
#endif
}

void Node::ref()
{
    // bumping from 0 to 1 is just removing from the new nodes set
    if (newNodes) {
        HashSet<Node*>::iterator it = newNodes->find(this);
        if (it != newNodes->end()) {
            newNodes->remove(it);
            ASSERT(!nodeExtraRefCounts || !nodeExtraRefCounts->contains(this));
            return;
        }
    }   

    ASSERT(!newNodes || !newNodes->contains(this));
    
    if (!nodeExtraRefCounts)
        nodeExtraRefCounts = new HashCountedSet<Node*>;
    nodeExtraRefCounts->add(this);
}

void Node::deref()
{
    ASSERT(!newNodes || !newNodes->contains(this));
    
    if (!nodeExtraRefCounts) {
        delete this;
        return;
    }

    HashCountedSet<Node*>::iterator it = nodeExtraRefCounts->find(this);
    if (it == nodeExtraRefCounts->end())
        delete this;
    else
        nodeExtraRefCounts->remove(it);
}

unsigned Node::refcount()
{
    if (newNodes && newNodes->contains(this)) {
        ASSERT(!nodeExtraRefCounts || !nodeExtraRefCounts->contains(this));
        return 0;
    }
 
    ASSERT(!newNodes || !newNodes->contains(this));

    if (!nodeExtraRefCounts)
        return 1;

    return 1 + nodeExtraRefCounts->count(this);
}

void Node::clearNewNodes()
{
    if (!newNodes)
        return;

#ifndef NDEBUG
    HashSet<Node*>::iterator end = newNodes->end();
    for (HashSet<Node*>::iterator it = newNodes->begin(); it != end; ++it)
        ASSERT(!nodeExtraRefCounts || !nodeExtraRefCounts->contains(*it));
#endif
    deleteAllValues(*newNodes);
    delete newNodes;
    newNodes = 0;
}

static void substitute(UString &string, const UString &substring)
{
    int position = string.find("%s");
    assert(position != -1);
    string = string.substr(0, position) + substring + string.substr(position + 2);
}

static inline int currentSourceId(ExecState* exec)
{
    return exec->context().imp()->currentBody()->sourceId();
}

static inline const UString& currentSourceURL(ExecState* exec)
{
    return exec->context().imp()->currentBody()->sourceURL();
}

Completion Node::createErrorCompletion(ExecState* exec, ErrorType e, const char *msg)
{
    return Completion(Throw, Error::create(exec, e, msg, lineNo(), currentSourceId(exec), &currentSourceURL(exec)));
}

Completion Node::createErrorCompletion(ExecState *exec, ErrorType e, const char *msg, const Identifier &ident)
{
    UString message = msg;
    substitute(message, ident.ustring());
    return Completion(Throw, Error::create(exec, e, message, lineNo(), currentSourceId(exec), &currentSourceURL(exec)));
}

JSValue *Node::throwError(ExecState* exec, ErrorType e, const char *msg)
{
    return KJS::throwError(exec, e, msg, lineNo(), currentSourceId(exec), &currentSourceURL(exec));
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, JSValue *v, Node *expr)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, expr->toString());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), &currentSourceURL(exec));
}


JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, const Identifier &label)
{
    UString message = msg;
    substitute(message, label.ustring());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), &currentSourceURL(exec));
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, JSValue *v, Node *e1, Node *e2)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, e1->toString());
    substitute(message, e2->toString());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), &currentSourceURL(exec));
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, JSValue *v, Node *expr, const Identifier &label)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, expr->toString());
    substitute(message, label.ustring());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), &currentSourceURL(exec));
}

JSValue *Node::throwError(ExecState *exec, ErrorType e, const char *msg, JSValue *v, const Identifier &label)
{
    UString message = msg;
    substitute(message, v->toString(exec));
    substitute(message, label.ustring());
    return KJS::throwError(exec, e, message, lineNo(), currentSourceId(exec), &currentSourceURL(exec));
}

JSValue *Node::throwUndefinedVariableError(ExecState *exec, const Identifier &ident)
{
    return throwError(exec, ReferenceError, "Can't find variable: %s", ident);
}

void Node::setExceptionDetailsIfNeeded(ExecState *exec)
{
    JSValue *exceptionValue = exec->exception();
    if (exceptionValue->isObject()) {
        JSObject *exception = static_cast<JSObject *>(exceptionValue);
        if (!exception->hasProperty(exec, "line") && !exception->hasProperty(exec, "sourceURL")) {
            exception->put(exec, "line", jsNumber(m_line));
            exception->put(exec, "sourceURL", jsString(currentSourceURL(exec)));
        }
    }
}

Node *Node::nodeInsideAllParens()
{
    return this;
}

Node *GroupNode::nodeInsideAllParens()
{
    Node *n = this;
    do
        n = static_cast<GroupNode *>(n)->group.get();
    while (n->isGroupNode());
    return n;
}

// ------------------------------ StatementNode --------------------------------

void StatementNode::setLoc(int firstLine, int lastLine)
{
    m_line = firstLine;
    m_lastLine = lastLine;
}

// return true if the debugger wants us to stop at this point
bool StatementNode::hitStatement(ExecState* exec)
{
  Debugger *dbg = exec->dynamicInterpreter()->imp()->debugger();
  if (dbg)
    return dbg->atStatement(exec, currentSourceId(exec), firstLine(), lastLine());
  else
    return true; // continue
}

void StatementNode::processFuncDecl(ExecState*)
{
}

// ------------------------------ ElementNode ----------------------------------

void ElementNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ PropertyListNode -----------------------------

void PropertyListNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ ArgumentListNode -----------------------------

void ArgumentListNode::breakCycle() 
{ 
    next = 0;
}

// ECMA 11.13

static ALWAYS_INLINE JSValue *valueForReadModifyAssignment(ExecState * exec, JSValue *v1, JSValue *v2, Operator oper)
{
  JSValue *v;
  int i1;
  int i2;
  unsigned int ui;
  switch (oper) {
  case OpMultEq:
    v = mult(exec, v1, v2, '*');
    break;
  case OpDivEq:
    v = mult(exec, v1, v2, '/');
    break;
  case OpPlusEq:
    v = add(exec, v1, v2, '+');
    break;
  case OpMinusEq:
    v = add(exec, v1, v2, '-');
    break;
  case OpLShift:
    i1 = v1->toInt32(exec);
    i2 = v2->toInt32(exec);
    v = jsNumber(i1 << i2);
    break;
  case OpRShift:
    i1 = v1->toInt32(exec);
    i2 = v2->toInt32(exec);
    v = jsNumber(i1 >> i2);
    break;
  case OpURShift:
    ui = v1->toUInt32(exec);
    i2 = v2->toInt32(exec);
    v = jsNumber(ui >> i2);
    break;
  case OpAndEq:
    i1 = v1->toInt32(exec);
    i2 = v2->toInt32(exec);
    v = jsNumber(i1 & i2);
    break;
  case OpXOrEq:
    i1 = v1->toInt32(exec);
    i2 = v2->toInt32(exec);
    v = jsNumber(i1 ^ i2);
    break;
  case OpOrEq:
    i1 = v1->toInt32(exec);
    i2 = v2->toInt32(exec);
    v = jsNumber(i1 | i2);
    break;
  case OpModEq: {
    double d1 = v1->toNumber(exec);
    double d2 = v2->toNumber(exec);
    v = jsNumber(fmod(d1, d2));
  }
    break;
  default:
    assert(0);
    v = jsUndefined();
  }
  
  return v;
}

// ------------------------------ StatListNode ---------------------------------

StatListNode::StatListNode(StatementNode *s)
  : StatementNode(StatListNodeExecuteState), statement(s), next(this)
{
    Parser::noteNodeCycle(this);
    setLoc(s->firstLine(), s->lastLine());
}
 
StatListNode::StatListNode(StatListNode *l, StatementNode *s)
  : StatementNode(StatListNodeExecuteState), statement(s), next(l->next)
{
    l->next = this;
    setLoc(l->firstLine(), s->lastLine());
}

void StatListNode::processVarDecls(ExecState *exec)
{
  for (StatListNode *n = this; n; n = n->next.get())
    n->statement->processVarDecls(exec);
}

void StatListNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ VarDeclNode ----------------------------------

void VarDeclNode::processVarDecls(ExecState *exec)
{
  JSObject *variable = exec->context().imp()->variableObject();

  // If a variable by this name already exists, don't clobber it -
  // it might be a function parameter
  if (!variable->hasProperty(exec, ident)) {
    int flags = Internal;
    if (exec->context().imp()->codeType() != EvalCode)
      flags |= DontDelete;
    if (varType == VarDeclNode::Constant)
      flags |= ReadOnly;
    variable->put(exec, ident, jsUndefined(), flags);
  }
}

// ------------------------------ VarDeclListNode ------------------------------

void VarDeclListNode::processVarDecls(ExecState *exec)
{
  for (VarDeclListNode *n = this; n; n = n->next.get())
    n->var->processVarDecls(exec);
}

void VarDeclListNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ VarStatementNode -----------------------------

void VarStatementNode::processVarDecls(ExecState *exec)
{
  next->processVarDecls(exec);
}

// ------------------------------ BlockNode ------------------------------------

BlockNode::BlockNode(SourceElementsNode *s) : StatementNode(BlockNodeExecuteState)
{
  if (s) {
    source = s->next;
    Parser::removeNodeCycle(source.get());
    s->next = 0;
    setLoc(s->firstLine(), s->lastLine());
  } else {
    source = 0;
  }
}

void BlockNode::processVarDecls(ExecState *exec)
{
  if (source)
    source->processVarDecls(exec);
}

// ------------------------------ IfNode ---------------------------------------

void IfNode::processVarDecls(ExecState *exec)
{
  statement1->processVarDecls(exec);

  if (statement2)
    statement2->processVarDecls(exec);
}

// ------------------------------ DoWhileNode ----------------------------------

void DoWhileNode::processVarDecls(ExecState *exec)
{
  statement->processVarDecls(exec);
}

// ------------------------------ WhileNode ------------------------------------

void WhileNode::processVarDecls(ExecState *exec)
{
  statement->processVarDecls(exec);
}

// ------------------------------ ForNode --------------------------------------

void ForNode::processVarDecls(ExecState *exec)
{
  if (expr1)
    expr1->processVarDecls(exec);

  statement->processVarDecls(exec);
}

// ------------------------------ ForInNode ------------------------------------

ForInNode::ForInNode(Node *l, Node *e, StatementNode *s)
  : StatementNode(ForInNodeExecuteState), init(0L), lexpr(l), expr(e), varDecl(0L), statement(s)
{
}

ForInNode::ForInNode(const Identifier &i, AssignExprNode *in, Node *e, StatementNode *s)
  : StatementNode(ForInNodeExecuteState), ident(i), init(in), expr(e), statement(s)
{
  varDecl = new VarDeclNode(ident, init.get(), VarDeclNode::Variable);
  lexpr = new ResolveNode(ident);
}

void ForInNode::processVarDecls(ExecState *exec)
{
  if (varDecl)
    varDecl->processVarDecls(exec);
  statement->processVarDecls(exec);
}

// ------------------------------ WithNode -------------------------------

void WithNode::processVarDecls(ExecState *exec)
{
    statement->processVarDecls(exec);
}

// ------------------------------ CaseClauseNode -------------------------------

// ECMA 12.11
Completion CaseClauseNode::evalStatements(ExecState *exec)
{
  if (next)
    return next->execute(exec);
  else
    return Completion(Normal, jsUndefined());
}

void CaseClauseNode::processVarDecls(ExecState *exec)
{
  if (next)
    next->processVarDecls(exec);
}

// ------------------------------ ClauseListNode -------------------------------

// ECMA 12.11
void ClauseListNode::processVarDecls(ExecState *exec)
{
  for (ClauseListNode *n = this; n; n = n->next.get())
    if (n->clause)
      n->clause->processVarDecls(exec);
}

void ClauseListNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ CaseBlockNode --------------------------------

CaseBlockNode::CaseBlockNode(ClauseListNode *l1, CaseClauseNode *d, ClauseListNode *l2)
    : Node(InternalErrorState)
{
  if (l1) {
    list1 = l1->next;
    Parser::removeNodeCycle(list1.get());
    l1->next = 0;
  } else
    list1 = 0;

  def = d;

  if (l2) {
    list2 = l2->next;
    Parser::removeNodeCycle(list2.get());
    l2->next = 0;
  } else
    list2 = 0;
}
 
// ECMA 12.11
Completion CaseBlockNode::evalBlock(ExecState *exec, JSValue *input)
{
  JSValue *v;
  Completion res;
  ClauseListNode *a = list1.get();
  ClauseListNode *b = list2.get();
  CaseClauseNode *clause;

    while (a) {
      clause = a->getClause();
      a = a->getNext();
      v = clause->evaluate(exec);
      KJS_EVALUATE_METHOD_CHECKEXCEPTION
      if (strictEqual(exec, input, v)) {
	res = clause->evalStatements(exec);
	if (res.complType() != Normal)
	  return res;
	while (a) {
	  res = a->getClause()->evalStatements(exec);
	  if (res.complType() != Normal)
	    return res;
	  a = a->getNext();
	}
	break;
      }
    }

  while (b) {
    clause = b->getClause();
    b = b->getNext();
    v = clause->evaluate(exec);
    KJS_EVALUATE_METHOD_CHECKEXCEPTION
    if (strictEqual(exec, input, v)) {
      res = clause->evalStatements(exec);
      if (res.complType() != Normal)
	return res;
      goto step18;
    }
  }

  // default clause
  if (def) {
    res = def->evalStatements(exec);
    if (res.complType() != Normal)
      return res;
  }
  b = list2.get();
 step18:
  while (b) {
    clause = b->getClause();
    res = clause->evalStatements(exec);
    if (res.complType() != Normal)
      return res;
    b = b->getNext();
  }

  // bail out on error
  KJS_EVALUATE_METHOD_CHECKEXCEPTION

  return Completion(Normal);
}

void CaseBlockNode::processVarDecls(ExecState *exec)
{
  if (list1)
    list1->processVarDecls(exec);
  if (def)
    def->processVarDecls(exec);
  if (list2)
    list2->processVarDecls(exec);
}

// ------------------------------ SwitchNode -----------------------------------

void SwitchNode::processVarDecls(ExecState *exec)
{
  block->processVarDecls(exec);
}

// ------------------------------ LabelNode ------------------------------------

void LabelNode::processVarDecls(ExecState *exec)
{
  statement->processVarDecls(exec);
}

// ------------------------------ TryNode --------------------------------------

void TryNode::processVarDecls(ExecState *exec)
{
  tryBlock->processVarDecls(exec);
  if (catchBlock)
    catchBlock->processVarDecls(exec);
  if (finallyBlock)
    finallyBlock->processVarDecls(exec);
}

// ------------------------------ ParameterNode --------------------------------

void ParameterNode::breakCycle() 
{ 
    next = 0;
}

// ------------------------------ FunctionBodyNode -----------------------------

FunctionBodyNode::FunctionBodyNode(SourceElementsNode *s)
    : BlockNode(s)
    , m_sourceURL(Lexer::curr()->sourceURL())
    , m_sourceId(Parser::sid)
{

  setLoc(-1, -1);
}

void FunctionBodyNode::processFuncDecl(ExecState *exec)
{
    if (source)
        source->processFuncDecl(exec);
}

// ------------------------------ FuncDeclNode ---------------------------------

// ECMA 13
void FuncDeclNode::processFuncDecl(ExecState *exec)
{
  ContextImp *context = exec->context().imp();

  // TODO: let this be an object with [[Class]] property "Function"
  FunctionImp *func = new DeclaredFunctionImp(exec, ident, body.get(), context->scopeChain());

  JSObject *proto = exec->lexicalInterpreter()->builtinObject()->construct(exec, List::empty());
  proto->put(exec, constructorPropertyName, func, ReadOnly|DontDelete|DontEnum);
  func->put(exec, prototypePropertyName, proto, Internal|DontDelete);

  int plen = 0;
  for(ParameterNode *p = param.get(); p != 0L; p = p->nextParam(), plen++)
    func->addParameter(p->ident());

  func->put(exec, lengthPropertyName, jsNumber(plen), ReadOnly|DontDelete|DontEnum);

  // ECMA 10.2.2
  context->variableObject()->put(exec, ident, func, Internal | (context->codeType() == EvalCode ? 0 : DontDelete));

  if (body) {
    // hack the scope so that the function gets put as a property of func, and it's scope
    // contains the func as well as our current scope
    JSObject *oldVar = context->variableObject();
    context->setVariableObject(func);
    context->pushScope(func);
    body->processFuncDecl(exec);
    context->popScope();
    context->setVariableObject(oldVar);
  }
}

// ------------------------------ SourceElementsNode ---------------------------

int SourceElementsNode::count = 0;

SourceElementsNode::SourceElementsNode(StatementNode *s1)
    : StatementNode(SourceElementsNodeExecuteState), node(s1), next(this)
{
    Parser::noteNodeCycle(this);
    setLoc(s1->firstLine(), s1->lastLine());
}

SourceElementsNode::SourceElementsNode(SourceElementsNode *s1, StatementNode *s2)
    : StatementNode(SourceElementsNodeExecuteState), node(s2), next(s1->next)
{
    s1->next = this;
    setLoc(s1->firstLine(), s2->lastLine());
}

// ECMA 14
void SourceElementsNode::processFuncDecl(ExecState *exec)
{
  for (SourceElementsNode *n = this; n; n = n->next.get())
    n->node->processFuncDecl(exec);
}

void SourceElementsNode::processVarDecls(ExecState *exec)
{
  for (SourceElementsNode *n = this; n; n = n->next.get())
    n->node->processVarDecls(exec);
}

void SourceElementsNode::breakCycle() 
{ 
    next = 0;
}

ProgramNode::ProgramNode(SourceElementsNode *s) : FunctionBodyNode(s)
{
}
