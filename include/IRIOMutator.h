#ifndef BOOST_IRIOMUTATOR_H
#define BOOST_IRIOMUTATOR_H

#include "IR.h"
#include "IRMutator.h"

#include <set>
#include <vector>


namespace Boost {

namespace Internal {

class IRIOMutator : public IRMutator {
 public:
    IRIOMutator() : refvars{}, grad_vec{} {}

    Expr visit(Ref<const Var>) override;
    Stmt visit(Ref<const Move>) override;
    Group visit(Ref<const Kernel>) override;
 private:
    std::set<std::string> refvars;
    std::vector<Expr> grad_vec;
};

}  // namespace Internal

}  // namespace Boost


#endif  // BOOST_IRIOMUTATOR_H