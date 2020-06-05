// A rewritten form of IRPrinter.h

#ifndef BOOST_IRCCPRINTER_H
#define BOOST_IRCCPRINTER_H

#include <string>
#include <sstream>
#include <deque>
#include <vector>
#include <set>

#include "IRVisitor.h"


namespace Boost {

namespace Internal {

class IRCCPrinter : public IRVisitor {
 public:
    IRCCPrinter(const std::deque<std::vector<Expr> > &_temps = {}) :
        IRVisitor(), curvar{}, temps(_temps), indent(0), print_range(false), print_decl(false), print_arg(false) {}

    std::string print(const Expr&);
    std::string print(const Stmt&);
    std::string print(const Group&);

    void print_indent() {
        for (int i = 0; i < indent; ++i)
            oss << " ";
    }

    void enter() {
        indent += 4;
    }

    void exit() {
        indent -= 4;
    }

    void print_type(const TypeCode &code) {
        switch (code) {
        case TypeCode::Int:
            oss << "int ";
            break;
        case TypeCode::Float:
            oss << "float ";
            break;
        default:
            oss << "unknown_type ";
        }
    }

    void visit(Ref<const IntImm>) override;
    void visit(Ref<const FloatImm>) override;
    void visit(Ref<const Binary>) override;
    void visit(Ref<const Compare>) override;
    void visit(Ref<const Var>) override;
    void visit(Ref<const Index>) override;
    void visit(Ref<const Dom>) override;
    void visit(Ref<const LoopNest>) override;
    void visit(Ref<const IfThenElse>) override;
    void visit(Ref<const Move>) override;
    void visit(Ref<const Kernel>) override;
 private:
    std::string curvar;
    std::ostringstream oss;
    std::deque<std::vector<Expr> > temps;
    int indent;
    bool print_range;
    bool print_decl;
    bool print_arg;
};

}  // namespace Internal

}  // namespace Boost


#endif  // BOOST_IRCCPRINTER_H