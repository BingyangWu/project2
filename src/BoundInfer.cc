#include "BoundInfer.h"
#include "debug.h"
#include "IRMutator.h"
#include "IRVisitor.h"
#include "IRPrinter.h"
#include <iostream>
#include "IR.h"
#include "type.h"
#include <map>
#include <vector>
#include <cmath>

namespace Boost {
namespace Internal {

using std::map;
using std::vector;
using std::pair;
using std::make_pair;
using std::floor;
using std::ceil;

class Expr_compare;
typedef map<const Expr, pair<int, int>, Expr_compare> BoundMap;

Type index_type = Type::int_scalar(32);
Type compare_type = Type::int_scalar(32);

// 试图给Expr, Stmt一个ID值
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
        if (a.node_type() == IRNodeType::IntImm){
            auto o1 = a.as<IntImm>();
            auto o2 = b.as<IntImm>();
            return o1->value() < o2->value();
        }
        if (a.node_type() == IRNodeType::FloatImm){
            auto o1 = a.as<FloatImm>();
            auto o2 = b.as<FloatImm>();
            return o1->value() < o2->value();
        }
        std::cout << a.type() << "\n";
        // CHECK(false, "index should be Index or BinaryOp");
        return a.real_ptr() < b.real_ptr();
    }
};

class Stmt_compare{
public:
    bool operator()(const Stmt& a, const Stmt& b) const {
        return a.real_ptr() < b.real_ptr();
    }
};

void updated(BoundMap& origin_bound, const Expr t, int updated_min, int updated_max);

// 收集边界信息
class BoundCollection : public IRVisitor {
    using IRVisitor::visit;

    void visit(Ref<const Var> op) override {
        int size = op->args.size();
        for(int i = 0; i < size; ++i)
            updated(bound_info, op->args[i], 0, op->shape[i]);
    }

public:
    BoundMap bound_info;
};

void updated(BoundMap& origin_bound, const Expr t, int updated_min, int updated_max){
    auto iter = origin_bound.find(t);
    if(iter == origin_bound.end()){
        origin_bound.insert(make_pair(t, pair<int, int>(updated_min, updated_max)));
    } else {
        pair<int, int> origin_pair = origin_bound[t];
        origin_bound[t] = pair<int, int>(std::max(origin_pair.first, updated_min), 
                                         std::min(origin_pair.second, updated_max));
    }
}

// 边界推测
BoundMap _bound_infer(const BoundMap& bound_info){
    BoundMap ans(bound_info);
    bool changed = false;
    do{
        BoundMap temp(ans);
        ans.clear();
        for(auto item : temp){
            changed = false;
            if(item.first.node_type() == IRNodeType::Index){
                updated(ans, item.first, item.second.first, item.second.second);
                continue;
            }
            changed = true;
            int min = item.second.first;
            int max = item.second.second;
            auto wrapper = item.first.as<Binary>();
            int value = 0;
            CHECK(wrapper != nullptr, "index should be Index or BinaryOp");
            // project2中有时出现例外，我们不讨论以下情况：
            if(wrapper->a.node_type() == IRNodeType::IntImm && wrapper->b.node_type() == IRNodeType::IntImm){
                continue;
            }
            switch (wrapper->op_type){
            case BinaryOpType::Add :
                if(wrapper->a.node_type() != IRNodeType::IntImm && wrapper->b.node_type() != IRNodeType::IntImm){
                    updated(ans, wrapper->a, min, max);
                    updated(ans, wrapper->b, min, max);
                } else {
                    if (wrapper->a.node_type() == IRNodeType::IntImm){
                        value = wrapper->a.as<IntImm>()->value();
                        updated(ans, wrapper->b, min - value, max - value);
                    } else {
                        value = wrapper->b.as<IntImm>()->value();
                        updated(ans, wrapper->a, min - value, max - value);
                    }
                }
                break;
            case BinaryOpType::Sub :
                // project2中有时会进入此分支，我们不讨论以下情况：
                if(wrapper->a.node_type() != IRNodeType::IntImm && wrapper->b.node_type() != IRNodeType::IntImm){
                    break;
                }
                if(wrapper->a.node_type() == IRNodeType::IntImm){
                    value = wrapper->a.as<IntImm>()->value();
                    updated(ans, wrapper->b, value - min, value - max);
                } else {
                    value = wrapper->b.as<IntImm>()->value();
                    updated(ans, wrapper->a, min + value, max + value);
                }
                break;
            case BinaryOpType::Mul :
                // project2中有时会进入此分支，我们不讨论以下情况：
                if(wrapper->a.node_type() != IRNodeType::IntImm && wrapper->b.node_type() != IRNodeType::IntImm){
                    break;
                }
                if(wrapper->a.node_type() == IRNodeType::IntImm){
                    value = wrapper->a.as<IntImm>()->value();
                    updated(ans, wrapper->b, floor(min * 1.0 / value), ceil(max * 1.0 / value));
                } else {
                    value = wrapper->b.as<IntImm>()->value();
                    updated(ans, wrapper->a, floor(min * 1.0 / value), ceil(max * 1.0 / value));
                }
                break;
            case BinaryOpType::Div :
                // project2中有时会进入此分支，我们不讨论以下情况：
                if(wrapper->a.node_type() != IRNodeType::IntImm && wrapper->b.node_type() != IRNodeType::IntImm){
                    break;
                }
                if(wrapper->a.node_type() == IRNodeType::IntImm){
                    value = wrapper->a.as<IntImm>()->value();
                    updated(ans, wrapper->b, floor(value * 1.0 / min), ceil(value * 1.0 / max));
                } else {
                    value = wrapper->b.as<IntImm>()->value();
                    updated(ans, wrapper->a, min * value, max * value);
                }
                break;
            case BinaryOpType::Mod :
                // project2中有时会进入此分支，我们不讨论以下情况：
                if(wrapper->a.node_type() != IRNodeType::IntImm && wrapper->b.node_type() != IRNodeType::IntImm){
                    break;
                }
                if(wrapper->a.node_type() == IRNodeType::IntImm){
                    updated(ans, wrapper->b, min < 0 ? min - 1 : min, max > 0 ? max + 1 : max);
                }
                // 另一种情况我们不考虑，x % 3的范围是[0, 2)并不能说明x的取值
                break;                                
            default:
                CHECK(false, "unkown BinaryOp type");
            }
        }
    }while(changed);
    return ans;
}

// 更新Stmt
class BoundModify : public IRMutator {
    using IRMutator::visit;
    BoundMap var_bound;
    
    Stmt visit(Ref<const LoopNest> op) override {
        vector<Stmt> updated_body_list;
        for(Stmt i : op->body_list){
            updated_body_list.push_back(mutate(i));
        }
        vector<Expr> updated_index;
        for(Expr t : op->index_list){
            // 存疑
            auto index = t.as<Index>();
            CHECK(index != nullptr, "loop index type wrong.");
            std::cout << index->name << " " << var_bound.size() << "\n";
            auto iter = var_bound.find(t);
            CHECK(iter != var_bound.end(), "internel error.");
            int min = iter->second.first;
            int max = iter->second.second;
            updated_index.push_back(Index::make(index_type, index->name, Dom::make(index_type, min, max), index->index_type));
        }
        return LoopNest::make(updated_index, updated_body_list);
    }

    Stmt visit(Ref<const Move> op) override {
        BoundCollection bc;
        Stmt op_stmt(op);
        op_stmt.visit_stmt(&bc);
        auto updated_body = op_stmt;
        for(auto item : bc.bound_info){
            int min = item.second.first;
            int max = item.second.second;
            Expr cond = Binary::make(compare_type, BinaryOpType::And, 
                                     Compare::make(compare_type, CompareOpType::LE, min, item.first), 
                                     Compare::make(compare_type, CompareOpType::LT, item.first, max));
            updated_body = IfThenElse::make(cond, updated_body, Stmt());
        }
        return updated_body;
    }

public:
    BoundModify(const BoundMap& _var_bound):var_bound(_var_bound){}
};

class BoundCollectionWrapper : public IRVisitor{
    using IRVisitor::visit;
    void visit(Ref<const Move> op) override {
        Stmt op_stmt(op);
        BoundCollection bc;
        op_stmt.visit_stmt(&bc);
        boundInfo_stmt.push_back(bc.bound_info);
    }

public:
    vector<BoundMap> boundInfo_stmt;
    BoundCollectionWrapper(){
        boundInfo_stmt.clear();
    }
};

class BoundModifyWrapper : public IRMutator {
    using IRMutator::visit;
    vector<BoundMap> bound_infer;
    
    Group visit(Ref<const Kernel> op) override {
        size_t size = op->stmt_list.size();
        CHECK(size == bound_infer.size(), "internel error");
        vector<Stmt> updated_list;
        for(size_t i = 0; i < size; ++i){
            BoundModify bm(bound_infer[i]);
            updated_list.push_back(bm.mutate(op->stmt_list[i]));
        }
        return Kernel::make(op->name, op->inputs, op->outputs, updated_list, op->kernel_type);
    }

public:
    BoundModifyWrapper(const vector<BoundMap>& _bound_infer):
        bound_infer(_bound_infer){}
};

Group bound_infer(const Group & origin_kernel, const Group & new_kernel){

    BoundCollectionWrapper collect;
    origin_kernel.visit_group(&collect);

    vector<BoundMap> expr_bound;
    for(auto bound_item : collect.boundInfo_stmt){
        expr_bound.push_back(_bound_infer(bound_item));
    }

    BoundModifyWrapper bm(expr_bound);
    return bm.mutate(new_kernel);
}

Stmt loop_bound_infer(const Stmt & origin_loop){
    BoundCollection collect;
    origin_loop.visit_stmt(&collect);

    BoundModify bm(_bound_infer(collect.bound_info));
    return bm.mutate(origin_loop); 
}

}
}