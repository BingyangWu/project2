#ifndef BOOST_IRSUMMUTATOR_H
#define BOOST_IRSUMMUTATOR_H

#include "IR.h"
#include "IRMutator.h"
#include "IRTermVisitor.h"

#include <vector>
#include <set>

namespace Boost {

namespace Internal {

class IRSumMutator : public IRMutator {
 public:
    std::deque<std::vector<Expr> > temps;

    IRSumMutator(const IRTermVisitor &visitor) :
        IRMutator(), terms(visitor.terms), signs(visitor.signs), temp_indices(visitor.temp_indices),
        names(visitor.names), temp_index(0) {}

    Stmt visit(Ref<const LoopNest>) override;

    void get_body(std::vector<Stmt> &body_list, Expr lhs);

    Expr new_temp(Type type) {
        std::string name{};
        while (names.find(name = "t" + std::to_string(temp_index++)) != names.end())
            ;
        Expr temp = Var::make(type, name, {}, {1});
        names.insert(name);
        temps.back().push_back(temp);
        return temp;
    }

    Expr new_temp(Ref<const Var> old_var) {
        std::string name{};
        while (names.find(name = "t" + std::to_string(temp_index++)) != names.end())
            ;
        Expr temp = Var::make(old_var->type(), name, old_var->args, old_var->shape);
        names.insert(name);
        temps.back().push_back(temp);
        return temp;
    }
    
 private:
    std::deque<std::vector<Expr> > terms;
    std::deque<std::vector<BinaryOpType> > signs;
    std::deque<std::vector<std::vector<Expr> > > temp_indices;
    std::set<std::string> names;
    // identical to those in IRTermVisitor 

    size_t temp_index;
};

}  // namespace Internal

}  // namespace Boost


#endif  // BOOST_IRSUMMUTATOR_H