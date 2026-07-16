// QueryBuildValidationUtil.h
// Query build 時の検証処理をまとめるユーティリティ
#pragma once

#include "StackOrdUtil.h"
#include "YakinamashiStateValidator.h"

template<size_t MAX_N>
class QueryBuildValidationUtil {
public:
    // 責務: beam query適用後のstack orderとquery状態の整合性を検証する。
    // 必要な理由: query build中の不整合検出は残しつつ、今回ログだけを止めた対象関数だと分かるようにするため。
    template<typename QueryRIList>
    static bool validate_after_beam_query(
            const my_vector<BeamQuery> &beam_queries,
            int N,
                                          int bq_i,
                      const QueryRIList &yst_queries,
                                          State &st,
                                          AOrder &aorder,
                                          BOrder &border,
                                          my_vector<ValState> &now_a_val_state,
                                          my_vector<ValState> &now_b_val_state) {
        // //Optuna out("=== bq_i", bq_i, "===");
        // //Optuna out(beam_queries[bq_i]);
        // //Optuna StackOrdUtil::print_stack_ord<MAX_N>(st, aorder, border, now_a_val_state, now_b_val_state);
        // //Optuna st.print();
        // //Optuna out("a_val_st"); out(now_a_val_state);
        // //Optuna out("b_val_st"); out(now_b_val_state);
        if(!StackOrdUtil::validate_ring_sorted_stack_ords<MAX_N>(st, aorder, border, now_a_val_state, now_b_val_state)){
            return false;
        }
        QueriesStateValidator<MAX_N>::validate_finished_top(
            N,
            yst_queries,
            st,
            aorder,
            border,
            now_a_val_state,
            now_b_val_state
        );
        return true;
    }
};
