// A rewritten form of IRPrinter.h

#ifndef BOOST_IRTERMVISITOR_H
#define BOOST_IRTERMVISITOR_H

#include <string>
#include <sstream>
#include <deque>
#include <set>
#include <iostream>

#include "IRVisitor.h"
#include "IRCCPrinter.h"
#include "GenerateAST.h"

namespace Boost {

namespace Internal {

class IRTermVisitor : public IRVisitor {
 public:
    std::deque<std::vector<Expr> > terms;
    std::deque<std::vector<BinaryOpType> > signs;
    std::deque<std::vector<std::vector<Expr> > > temp_indices;
    std::set<std::string> names;
    
    IRTermVisitor(const std::set<Expr, Expr_compare> &set) :
        IRVisitor(), terms{}, signs{}, temp_indices{}, names{}, parentOpType{}, paren_exprs(set),
        look_for_terms(false), look_for_indices(false), record_lhs(false) {}

    bool isin(Expr newidx) {
        for (Expr index : lhs->args)
            if (index.get() == newidx.get())
                return true;

        // we do not want duplicate elements
        for (auto outer : temp_indices.back())
            for (Expr index : outer)
                if (index.get() == newidx.get())
                    return true;
        
        return false;
    }

    void visit(Ref<const IntImm>) override;
    void visit(Ref<const FloatImm>) override;
    void visit(Ref<const Binary>) override;
    void visit(Ref<const Var>) override;
    void visit(Ref<const Index>) override;
    void visit(Ref<const Move>) override;
 private:
    Ref<const Var> lhs;
    BinaryOpType parentOpType;
    std::set<Expr, Expr_compare> paren_exprs;
    bool look_for_terms;
    bool look_for_indices;
    bool record_lhs;
};

}  // namespace Internal

}  // namespace Boost


#endif  // BOOST_IRTERMVISITOR_H