//QueriesModifierArgValidator.h

#pragma once

#include "QueryCommonRI.h"

class QueriesModifierArgValidator {
public:
    static bool is_order_requirement_valid(const QueryCommon &left, const QueryCommon &right) {
        const bool same_tar_v = left.get_tar_v() == right.get_tar_v();
        if (!same_tar_v) {
            return true;
        }
        if (left.is_a2a_type()) {
            return false;
        }
        if (left.is_b2a_type()) {
            return false;
        }
        if (left.is_a2b_type()) {
            return right.is_b2a_type();
        }
        return true;
    }

    // why: a の後に b を置いてよいかを単一箇所で判定し、操作別検証の重複を防ぐ。
    static void validate_order_requirement(const QueryCommon &left, const QueryCommon &right) {
#ifdef DEBUG
        // why: テストでは bool 判定を再利用し、運用では即時失敗で異常遷移を止める。
        my_assert(is_order_requirement_valid(left, right));
#endif
    }

    static bool is_swap_arg_valid(const my_vector<QueryCommonRI> &queries, int q_idx1, int q_idx2) {
        if (!(0 <= q_idx1 && q_idx1 < static_cast<int>(queries.size()))) {
            return false;
        }
        if (!(0 <= q_idx2 && q_idx2 < static_cast<int>(queries.size()))) {
            return false;
        }
        if (!(q_idx1 + 1 == q_idx2)) {
            return false;
        }
        return is_order_requirement_valid(queries[q_idx2].get_query(), queries[q_idx1].get_query());
    }

    // why: swap 後に隣接順が逆転するため、(qj, qi) の順序要件を直接確認する。
    static void validate_swap_arg(const my_vector<QueryCommonRI> &queries, int q_idx1, int q_idx2) {
#ifdef DEBUG
        my_assert(is_swap_arg_valid(queries, q_idx1, q_idx2));
#endif
    }

    static bool is_move_down_arg_valid(const my_vector<QueryCommonRI> &queries, int from_qi, int to_qi) {
        if (!(0 <= from_qi && from_qi < static_cast<int>(queries.size()))) {
            return false;
        }
        if (!(0 <= to_qi && to_qi <= static_cast<int>(queries.size()))) {
            return false;
        }
        if (!(from_qi + 1 < to_qi)) {
            return false;
        }
        const QueryCommon &from_q = queries[from_qi].get_query();
        for (int q_idx = from_qi + 1; q_idx < to_qi; q_idx++) {
            if (!is_order_requirement_valid(queries[q_idx].get_query(), from_q)) {
                return false;
            }
        }
        return true;
    }

    // why: move_down は from を右辺として [from+1, to) が左辺に来る順序要件を満たす必要がある。
    static void validate_move_down_arg(const my_vector<QueryCommonRI> &queries, int from_qi, int to_qi) {
#ifdef DEBUG
        my_assert(is_move_down_arg_valid(queries, from_qi, to_qi));
#endif
    }

    static bool is_move_up_arg_valid(const my_vector<QueryCommonRI> &queries, int from_qi, int to_qi) {
        if (!(0 <= from_qi && from_qi < static_cast<int>(queries.size()))) {
            return false;
        }
        if (!(0 <= to_qi && to_qi < static_cast<int>(queries.size()))) {
            return false;
        }
        if (!(from_qi > to_qi)) {
            return false;
        }
        const QueryCommon &from_q = queries[from_qi].get_query();
        for (int q_idx = to_qi; q_idx < from_qi; q_idx++) {
            if (!is_order_requirement_valid(from_q, queries[q_idx].get_query())) {
                return false;
            }
        }
        return true;
    }

    // why: move_up は from が前に出るため、from が [to, from) の各要素の左辺に来られるか確認する。
    static void validate_move_up_arg(const my_vector<QueryCommonRI> &queries, int from_qi, int to_qi) {
#ifdef DEBUG
        my_assert(is_move_up_arg_valid(queries, from_qi, to_qi));
#endif
    }
};
