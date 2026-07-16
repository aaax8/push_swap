#pragma once
#include "QueryCommonRI.h"
#include "QueriesState.h"
#include "YQAliases.h"
#include "util.h"
#include <utility>

namespace FinalRotCalculator {
    inline int light_mod(int a, int mod_v){
        int res;
        if(a < 0){
            res = a + mod_v;
        }else if(a >= mod_v){
            res = a - mod_v;
        }else{
            res = a;
        }
        my_assert(mod(a, mod_v) == res);
        return res;
    }

    //全てのクエリが終わった状態での、Aのtopのordを渡す
    inline std::pair<command, int> build_final_rot(int state_N, OrdPos final_ord_pos_a) {
        validate_OrdPos(final_ord_pos_a, state_N);
        const int ra_dis = light_mod(-final_ord_pos_a.get(), state_N);
        const int rra_dis = final_ord_pos_a.get();
        if (ra_dis == 0) {
            return {RA, 0};
        } else if (ra_dis < rra_dis) {
            return {RA, ra_dis};
        } else {
            return {RRA, rra_dis};
        }
    }

    // : empty query でも final_rot の責務は残るので、先頭基準の first_top_ord_a から再計算できる形へ直す。
    inline std::pair<command, int> build_final_rot_info(int state_N, const QueriesState &queries_state) {
        const OrdPos top_ord_a = queries_state.queries.empty()
            ? queries_state.first_top_ord_a
            : queries_state.queries.back().get_finished_top_ord_a();

        validate_OrdPos(top_ord_a, state_N);
        return build_final_rot(state_N, top_ord_a);
    }
}
