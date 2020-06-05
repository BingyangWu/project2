// A rewritten form of IRVisitor.cc

#include "IRTermVisitor.h"
#include "IRCCPrinter.h"
#include <iostream>

namespace Boost {

namespace Internal {


void IRTermVisitor::visit(Ref<const IntImm> op) {
    if (look_for_terms) {
        temp_indices.back().push_back({});
        terms.back().push_back(op);
        signs.back().push_back(parentOpType);
    }
    return;
}


void IRTermVisitor::visit(Ref<const FloatImm> op) {
    if (look_for_terms) {
        temp_indices.back().push_back({});
        terms.back().push_back(op);
        signs.back().push_back(parentOpType);
    }
    return;
}


void IRTermVisitor::visit(Ref<const Binary> op) {
    if (look_for_terms
        && (paren_exprs.find(op) != paren_exprs.end() || op->op_type == BinaryOpType::Mul
            || op->op_type == BinaryOpType::Div || op->op_type == BinaryOpType::Mod)) {
        temp_indices.back().push_back({});
        terms.back().push_back(op);
        signs.back().push_back(parentOpType);
        look_for_terms = false;
        (op->a).visit_expr(this);
        (op->b).visit_expr(this);
        look_for_terms = true;
    } else {
        parentOpType = BinaryOpType::Add;
        (op->a).visit_expr(this);
        parentOpType = op->op_type;
        (op->b).visit_expr(this);
    }
    
    return;
}


void IRTermVisitor::visit(Ref<const Var> op) {
    names.insert(op->name);
    if (record_lhs) {
        lhs = op;
    } else if (look_for_terms) {
        temp_indices.back().push_back({});
        terms.back().push_back(op);
        signs.back().push_back(parentOpType);
    }
    bool temp_lft = look_for_terms;
    look_for_terms = false;
    for (auto arg : op->args) {
        arg.visit_expr(this);
    }
    look_for_terms = temp_lft;
    return;
}


void IRTermVisitor::visit(Ref<const Index> op) {
    names.insert(op->name);
    if (look_for_indices && !isin(op)) {
        
        temp_indices.back().back().push_back(op);  // it only appears on RHS
    }
    (op->dom).visit_expr(this);
    return;
}


void IRTermVisitor::visit(Ref<const Move> op) {
    record_lhs = true;
    (op->dst).visit_expr(this);
    record_lhs = false;
    look_for_terms = true;
    look_for_indices = true;
    terms.push_back({});
    signs.push_back({});
    temp_indices.push_back({});
    (op->src).visit_expr(this);
    look_for_terms = false;
    look_for_indices = false;
    return;
}


}  // namespace Internal

}  // namespace Boost
