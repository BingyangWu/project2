#include "IRIOMutator.h"

namespace Boost {

namespace Internal {

Expr IRIOMutator::visit(Ref<const Var> op) {
    refvars.insert(op->name);
    return op;
}


Stmt IRIOMutator::visit(Ref<const Move> op) {
    Ref<const Var> lhs = (op->dst).as<Var>();
    CHECK(lhs.defined(), "internal error\n");
    CHECK(grad_vec.empty() || grad_vec.back().as<Var>(), "internal error\n");
    if (grad_vec.empty() || grad_vec.back().as<Var>()->name != lhs->name)
        grad_vec.push_back(lhs);
    CHECK((op->src).as<Binary>(), "internal error\n");
    mutate((op->src).as<Binary>()->b);
    return op;
}


Group IRIOMutator::visit(Ref<const Kernel> op) {
    std::vector<Expr> new_inputs;
    std::vector<Stmt> new_stmt_list;
    for (auto stmt : op->stmt_list) {
        mutate(stmt);
    }
    for (auto expr : op->inputs) {
        Ref<const Var> arg = expr.as<Var>();
        CHECK(arg.defined(), "one input arg is not of Var type");
        
        if (refvars.find(arg->name) != refvars.end()) {
            new_inputs.push_back(expr);
        }
    }
    for (auto expr : op->inputs) {
        Ref<const Var> arg = expr.as<Var>();
        CHECK(arg.defined(), "internal error\n");
        
        if (refvars.find("d" + arg->name) != refvars.end()) {
            new_inputs.push_back(Var::make(arg->type(), "d" + arg->name, arg->args, arg->shape));
        }
    }
    for (auto expr : op->outputs) {
        Ref<const Var> arg = expr.as<Var>();
        CHECK(arg.defined(), "internal error\n");
        
        if (refvars.find("d" + arg->name) != refvars.end()) {
            new_inputs.push_back(Var::make(arg->type(), "d" + arg->name, arg->args, arg->shape));
        }
    }
    return Kernel::make(op->name, new_inputs, grad_vec, op->stmt_list, op->kernel_type);
}


}  // namespace Internal

}  // namespace Boost
