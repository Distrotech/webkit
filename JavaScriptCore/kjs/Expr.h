#ifndef EXPR_H
#define EXPR_H 1

namespace KJS {

 class ExprNode;

 struct Expr {
     ExprNode* head;
     ExprNode* tail;
 };


};

#endif // EXPR_H

