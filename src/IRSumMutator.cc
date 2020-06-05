#include "IRSumMutator.h"

namespace Boost {

namespace Internal {

Stmt IRSumMutator::visit(Ref<const LoopNest> op) {
    temps.push_back({});
    Expr lhs = op->body_list.back().as<Move>()->dst;
    Expr acc = new_temp(lhs.as<Var>());
    std::vector<Expr> new_index_list = lhs.as<Var>()->args;
    std::vector<Stmt> inner_body_list;
    Stmt final_move = Move::make(lhs, acc, MoveType::MemToMem);

    get_body(inner_body_list, lhs);
    
    return LoopNest::make({}, {LoopNest::make(new_index_list, inner_body_list),
                               LoopNest::make(new_index_list, {final_move})});
}


void IRSumMutator::get_body(std::vector<Stmt> &body_list, Expr lhs) {
    std::vector<Expr> term_list = terms.front();
    std::vector<BinaryOpType> type_list = signs.front();
    std::vector<std::vector<Expr> > temp_index_list = temp_indices.front();
    Type type = lhs->type();
    terms.pop_front();
    signs.pop_front();
    temp_indices.pop_front();
    int size = term_list.size();
    Expr acc = temps.back().back();
    body_list.push_back(Move::make(acc, IntImm::make(Type::int_scalar(64), 0), MoveType::LocalToMem));
    for (int i = 0; i < size; ++i) {
        Expr temp = new_temp(type);
        body_list.push_back(Move::make(temp, IntImm::make(Type::int_scalar(64), 0), MoveType::LocalToLocal));
        Stmt move_temp = Move::make(temp, Binary::make(type, BinaryOpType::Add, temp, term_list.at(i)),
                                    MoveType::LocalToLocal);

        body_list.push_back(LoopNest::make(temp_index_list.at(i), {move_temp}));
        body_list.push_back(Move::make(acc, Binary::make(type, type_list.at(i), acc, temp), MoveType::LocalToMem));
    }
}

}  // namespace Internal

}  // namespace Boost
