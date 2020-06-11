#include "IR.h"
#include <string>
#include <vector>
#include <map>
#include "IRVisitor.h"

using std::string;
using std::vector;
using std::map;

namespace Boost{
namespace Internal{

class Expr_compare{
public:
    bool operator()(const Expr& a, const Expr& b) const {
        return a.real_ptr() < b.real_ptr();
    }
};

class getGradVec : public IRVisitor {
    using IRVisitor::visit;
    map<Expr, Expr, Expr_compare> grad_inter;

    void visit(Ref<const Move> op) override {
        auto dst = op->dst.as<Var>();
        CHECK(dst, "inter");
        grad_inter[op->src] = Var::make(dst->type(), 
                                        "d" + dst->name,
                                        dst->args,
                                        dst->shape);
        (op->src).visit_expr(this);
        return;
    }

    void visit(Ref<const Binary> op) override {
        switch (op->op_type)
        {
        case BinaryOpType::Add :
            grad_inter[op->a] = grad_inter[Expr(op)];
            grad_inter[op->b] = grad_inter[Expr(op)];
            break;

        case BinaryOpType::Sub :
            CHECK(false, "didn't impl sub operation");
            break;

        case BinaryOpType::Mul :
            grad_inter[op->a] = Binary::make(op->type(),
                                             BinaryOpType::Mul,
                                             grad_inter[Expr(op)],
                                             op->b);
            grad_inter[op->b] =  Binary::make(op->type(),
                                             BinaryOpType::Mul,
                                             grad_inter[Expr(op)],
                                             op->a);
            break;

        case BinaryOpType::Div :
            CHECK(op->b.as<IntImm>() || op->b.as<FloatImm>(), "can't support this");
            grad_inter[op->a] = Binary::make(op->type(),
                                             BinaryOpType::Div,
                                             grad_inter[Expr(op)],
                                             op->b);
            break;

        default:
            CHECK(false, "didn't impl other operation");
        }
        op->a.visit_expr(this);
        op->b.visit_expr(this);
        return;
    }

    void visit(Ref<const Var> op) override {
        if(op->name == gradStr){
            gradArg.push_back(Var::make(op->type(),
                                        "d" + op->name,
                                        op->args,
                                        op->shape));
            gradVec.push_back(grad_inter[Expr(op)]);
        }
        return;
    }
    
public:
    vector<Expr> gradArg;
    vector<Expr> gradVec;
    string gradStr;
    getGradVec(const string _gradStr):gradStr(_gradStr){}
};

Group toGard(const vector<string> & gradient_vec, const Group & origin_kernel){
    vector<Stmt> updated_stmt_list;
    auto kernel = origin_kernel.as<Kernel>();
    CHECK(kernel, "internal error");
    for(auto i : kernel->stmt_list){
        auto for_stmt = i.as<LoopNest>();
        CHECK(for_stmt, "internal error");
        CHECK(for_stmt->body_list.size() == 1, "internal error");
        for(string grad : gradient_vec){
            getGradVec gv(grad);
            for_stmt->body_list[0].visit_stmt(&gv);
            size_t size = gv.gradVec.size();
            for(size_t j = 0; j < size; ++j){
                updated_stmt_list.push_back(LoopNest::make(for_stmt->index_list,
                                                           {Move::make(gv.gradArg[j], 
                                                                       Binary::make(gv.gradArg[j].type(),
                                                                                    BinaryOpType::Add,
                                                                                    gv.gradArg[j],
                                                                                    gv.gradVec[j]),
                                                                       MoveType::LocalToLocal)}));
            }
        }
    }

    return Kernel::make(kernel->name, kernel->inputs, kernel->outputs,
                        updated_stmt_list, kernel->kernel_type);
}
}
}