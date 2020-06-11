// A rewritten form of IRPrinter.cc

#include "IRCCPrinter.h"

namespace Boost {

namespace Internal {


std::string IRCCPrinter::print(const Expr &expr) {
    oss.clear();
    expr.visit_expr(this);
    return oss.str();
}


std::string IRCCPrinter::print(const Stmt &stmt) {
    oss.clear();
    stmt.visit_stmt(this);
    return oss.str();
}


std::string IRCCPrinter::print(const Group &group) {
    oss.clear();
    group.visit_group(this);
    return oss.str();
}


void IRCCPrinter::visit(Ref<const IntImm> op) {
    oss << op->value();
}


void IRCCPrinter::visit(Ref<const FloatImm> op) {
    oss << op->value();
}


void IRCCPrinter::visit(Ref<const Binary> op) {
    oss << "(";
    (op->a).visit_expr(this);
    if (op->op_type == BinaryOpType::Add) {
        oss << " + ";
    } else if (op->op_type == BinaryOpType::Sub) {
        oss << " - ";
    } else if (op->op_type == BinaryOpType::Mul) {
        oss << " * ";
    } else if (op->op_type == BinaryOpType::Div) {
        oss << " / ";
    } else if (op->op_type == BinaryOpType::Mod) {
        oss << " % ";
    } else if (op->op_type == BinaryOpType::And) {
        oss << " && ";
    } else if (op->op_type == BinaryOpType::Or) {
        oss << " || ";
    }
    (op->b).visit_expr(this);
    oss << ")";
}


void IRCCPrinter::visit(Ref<const Compare> op) {
    (op->a).visit_expr(this);
    if (op->op_type == CompareOpType::LT) {
        oss << " < ";
    } else if (op->op_type == CompareOpType::LE) {
        oss << " <= ";
    } else if (op->op_type == CompareOpType::EQ) {
        oss << " == ";
    } else if (op->op_type == CompareOpType::GE) {
        oss << " >= ";
    } else if (op->op_type == CompareOpType::GT) {
        oss << " > ";
    } else if (op->op_type == CompareOpType::NE) {
        oss << " != ";
    }
    (op->b).visit_expr(this);
}


void IRCCPrinter::visit(Ref<const Var> op) {
    if ((print_decl || print_arg)
        && op->shape.size() == 1 && op->shape[0] == 1) {
        print_type(op->type().code);
        if (print_arg)
            oss << "&";
        oss << op->name;
        if (print_decl)
            oss << " = 0;\n";
    } else if (print_decl || print_arg) {
        print_type(op->type().code);
        if (print_arg)
            oss << "(&" << op->name << ")";
        else
            oss << op->name;
        for (size_t v : op->shape)
            oss << "[" << v << "]";
        if (print_decl)
            oss << " = {};\n";
    } else if (op->shape.size() > 1 || op->shape[0] > 1) {
        oss << op->name;
        for (auto v : op->args) {
            oss << "[";
            v.visit_expr(this);
            oss << "]";
        }
    } else {
        oss << op->name;
    }
}


void IRCCPrinter::visit(Ref<const Dom> op) {
    oss << curvar << " = ";
    (op->begin).visit_expr(this);
    oss << "; " << curvar << " < ";
    (op->extent).visit_expr(this);
    oss << "; ++" << curvar;    // prefix operator++ is used for consistency
}


void IRCCPrinter::visit(Ref<const Index> op) {
    curvar = op->name;
    
    if (print_range) {
        print_type(op->type().code);
        (op->dom).visit_expr(this);
    } else {
        oss << curvar;
    }
}


void IRCCPrinter::visit(Ref<const LoopNest> op) {
    print_range = true;
    for (auto index : op->index_list) {
        print_indent();
        oss << "for (";
        index.visit_expr(this);
        oss << ") {\n";
        enter();
    }
    print_range = false;
    for (auto body : op->body_list) {
        body.visit_stmt(this);
    }
    for (auto index : op->index_list) {
        exit();
        print_indent();
        oss << "}\n";
    }
}


void IRCCPrinter::visit(Ref<const IfThenElse> op) {
    print_indent();
    oss << "if (";
    (op->cond).visit_expr(this);
    oss << ") {\n";
    enter();
    (op->true_case).visit_stmt(this);
    exit();
    print_indent();
    oss << "}\n";
}


void IRCCPrinter::visit(Ref<const Move> op) {
    print_indent();
    (op->dst).visit_expr(this);
    oss << " = ";
    (op->src).visit_expr(this);
    oss << ";\n";
}


void IRCCPrinter::visit(Ref<const Kernel> op) {
    std::set<std::string> input_set{};
    print_indent();
    oss << "#include \"../run2.h\"\n\n";
    oss << "void " << op->name << "(";
    print_arg = true;
    for (size_t i = 0; i < op->inputs.size(); ++i) {
        input_set.insert(op->inputs[i].as<Var>()->name);
        op->inputs[i].visit_expr(this);
        if (i < op->inputs.size() - 1) {
            oss << ", ";
        }
    }
    for (size_t i = 0; i < op->outputs.size(); ++i) {
        if (input_set.find(op->outputs[i].as<Var>()->name) != input_set.end())
            continue;
        if (i || op->inputs.size())
            oss << ", ";
        op->outputs[i].visit_expr(this);
    }
    print_arg = false;
    oss << ") {\n";
    enter();
    print_decl = true;
    for (auto loop : temps)
        for (auto temp : loop) {
            print_indent();
            temp.visit_expr(this);
        }
    print_decl = false;
    for (auto stmt : op->stmt_list) {
        stmt.visit_stmt(this);
    }
    exit();
    oss << "}\n";
}


}  // namespace Internal

}  // namespace Boost
