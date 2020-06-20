#include "IR.h"
#include <string>
#include <vector>
#include <map>
#include "IRVisitor.h"
#include "IRMutator.h"

using std::string;
using std::vector;
using std::map;
using std::pair;

namespace Boost{
namespace Internal{

class Expr_compare{
public:
    bool operator()(const Expr& a, const Expr& b) const {
        if(a.node_type() != b.node_type())
            return a.node_type() < b.node_type();
        if (a.node_type() == IRNodeType::Binary){
            auto o1 = a.as<Binary>();
            auto o2 = b.as<Binary>();
            if(o1->op_type != o2->op_type)
                return o1->op_type < o2->op_type;
            return operator()(o1->a, o2->a) && operator()(o1->b, o2->b);
        }
        if (a.node_type() == IRNodeType::Index){
            auto o1 = a.as<Index>();
            auto o2 = b.as<Index>();
            return o1->name < o2->name;
        }
        CHECK(false, "index should be Index or BinaryOp");
        return false;
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

class Replace : public IRMutator{
    using IRMutator::visit;
    Expr src;
    Expr dst;
    Expr_compare com;

    bool eq(const Expr a, const Expr b){
        return !com(a, b) && !com(b, a);
    }

    Expr visit(Ref<const Binary> op) override {
        Expr a = eq(op->a, src) ?
                 dst :
                 mutate(op->a);
        Expr b = eq(op->b, src) ?
                 dst :
                 mutate(op->b);
        return Binary::make(op->type(), op->op_type, a, b);
    }

    Expr visit(Ref<const Var> op) override {
        std::vector<Expr> new_args;
        for (auto arg : op->args) {
            Expr _arg = eq(arg, src) ?
                       dst :
                       mutate(arg);
            new_args.push_back(_arg);
        }
        return Var::make(op->type(), op->name, new_args, op->shape);
    }

public:
    Replace(Expr _src, Expr _dst):src(_src), dst(_dst){}
};

Expr replace(Expr src, Expr a, Expr b){
    Replace re(a, b);
    return re.mutate(src);
}

Stmt replace(Stmt src, Expr a, Expr b){
    Replace re(a, b);
    return re.mutate(src);
}

// 我们不考虑多重%符号重叠
class FindNeedExpr : public IRVisitor {
    using IRVisitor::visit;

    void visit(Ref<const Binary> op) override {
        if(op->op_type == BinaryOpType::Mod){
            ans.push_back(op);
            return ;
        }
        op->a->visit_node(this);
        op->b->visit_node(this);
        return ;
    }

public:
    vector<Expr> ans;
};

vector<Expr> getNeedExpr(Stmt a){
    FindNeedExpr fn;
    a.visit_stmt(&fn);
    return vector<Expr>(fn.ans);
}

pair<Expr, Expr> getReplace(Expr cal){
    // 此处只考虑操作的第一个运算符不是数字的情况，否则不做操作（会得到能运算但是不符合题意的答案）
    Expr first = cal;
    Expr item = Index::make(Type::int_scalar(32), "*", Dom::make(Type::int_scalar(32), 0, 1), IndexType::Spatial);
    Expr second = item;
    while(first.as<Index>() == nullptr){
        auto wrapper = first.as<Binary>();
        CHECK(wrapper != nullptr, "index should be Index or BinaryOp");
        if(wrapper->a.node_type() == IRNodeType::IntImm){
            return pair<Expr, Expr>(Expr(), Expr());
        }
        first = wrapper->a;
        switch (wrapper->op_type)
        {
        case BinaryOpType::Add:
            second = Binary::make(first->type(), BinaryOpType::Sub, second, wrapper->b);
            break;
        case BinaryOpType::Sub:
            second = Binary::make(first->type(), BinaryOpType::Add, second, wrapper->b);
            break;
        case BinaryOpType::Mul:
            second = Binary::make(first->type(), BinaryOpType::Div, second, wrapper->b);
            break;
        case BinaryOpType::Div:
            second = Binary::make(first->type(), 
                                  BinaryOpType::Add,
                                  Binary::make(first->type(),
                                               BinaryOpType::Mul,
                                               second,
                                               wrapper->b),
                                  Binary::make(first->type(),
                                               BinaryOpType::Mod,
                                               second,
                                               wrapper->b));
            break;
        default:
            // leave % later
            return pair<Expr, Expr>(Expr(), Expr());
        }
    }
    return pair<Expr, Expr>(first, replace(second, item, first));
}

Group toGrad(const vector<string> & gradient_vec, const Group & origin_kernel){
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
                // 检测gv.gradArg[j]是否存在运算符,产出vector
                std::vector<Expr> needHandle;
                for(Expr index : gv.gradArg[j].as<Var>()->args){
                    if(index.as<Index>() == nullptr){
                        needHandle.push_back(index);
                    }
                }
                Expr dst = gv.gradArg[j];
                Expr src = gv.gradVec[j];
                // 对里面每一项做循环，替换式子
                for(Expr index : needHandle){
                    pair<Expr, Expr> replaceItem = getReplace(index);
                    if(replaceItem.first.defined()){
                        dst = replace(dst, index, replaceItem.first);
                        src = replace(src, replaceItem.first, replaceItem.second);
                    }
                }
                src = Binary::make(dst.type(), BinaryOpType::Add, dst, src);
                Stmt move = Move::make(dst, src, MoveType::LocalToLocal);
                // 替换%，因为和边界无关，所以每一个%都用一个自变量替代，
                vector<Expr> need_replace = getNeedExpr(move);
                vector<Expr> index_list(for_stmt->index_list);
                size_t temp_size = need_replace.size();
                for(size_t k = 0 ; k < temp_size; ++k){
                    Expr com_index = Index::make(Type::int_scalar(32), 
                                                 "com_" + std::to_string(k), 
                                                 Dom::make(Type::int_scalar(32), 0, 1), 
                                                 IndexType::Spatial);
                    index_list.push_back(com_index);
                    move = replace(move, need_replace[k], com_index);
                }
                updated_stmt_list.push_back(LoopNest::make(index_list, {move}));
            }
        }
    }

    return Kernel::make(kernel->name, kernel->inputs, kernel->outputs,
                        updated_stmt_list, kernel->kernel_type);
}
}
}