//x
// filepath: sort_algorithm/YakinamashiQuery/QueriesState.h
#pragma once

#include "Command/Command.h"
#include "YQAliases.h"
#include "OrdTypes.h"
#include <utility>

//x
class QueriesState {
public:
    using FinalRotInfo = std::pair<command, int>;

    // queriesは必ずQueriesStateのメンバとして扱うこと（QRIList単体利用禁止）
    QRIList queries;
    int sum_turn;
    FinalRotInfo final_rot_info;
    OrdPos first_top_ord_a;
    OrdPos first_top_ord_b;
    int current_N;

    QueriesState(QRIList init_queries,
                     int init_sum_turn,
                     FinalRotInfo init_final_rot_info,
                     OrdPos init_first_top_ord_a,
                     OrdPos init_first_top_ord_b,
                     int init_current_N)
            : queries(std::move(init_queries)),
              sum_turn(init_sum_turn),
              final_rot_info(init_final_rot_info),
              first_top_ord_a(init_first_top_ord_a),
              first_top_ord_b(init_first_top_ord_b),
              current_N(init_current_N) {
        my_assert(sum_turn >= 0);
        my_assert(current_N >= 0);
        validate_final_rot_info(final_rot_info);
    }

    bool operator==(const QueriesState &rhs) const {
        return queries == rhs.queries &&
               sum_turn == rhs.sum_turn &&
               final_rot_info == rhs.final_rot_info &&
               first_top_ord_a == rhs.first_top_ord_a &&
               first_top_ord_b == rhs.first_top_ord_b &&
               current_N == rhs.current_N;
    }

    bool operator!=(const QueriesState &rhs) const {
        return !(*this == rhs);
    }

private:
    //x
    static void validate_final_rot_info(const FinalRotInfo &rot_info) {
        my_assert(rot_info.second >= 0);
        my_assert(rot_info.first == NO_COMMAND || rot_info.first == RA || rot_info.first == RRA);
        if (rot_info.second == 0) {
            my_assert(rot_info.first == NO_COMMAND || rot_info.first == RA);
        } else {
            my_assert(rot_info.first == RA || rot_info.first == RRA);
        }
    }
};
