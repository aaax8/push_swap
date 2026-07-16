// PrevA2ARotSum.h
// A2A回転量集計用構造体と集計関数

#pragma once
// _BEGIN: e_a2a_type must be visible here because this header declares A2A sync helpers.
#include "../../InitBeam/BeamEnums.h"
#include "util.h"
// _END
struct PrevA2ARotSum {
    static constexpr int error_val = -99999;
    int ra_sum;
    int rra_sum;
    void validate_sum(int sum)const{
        my_assert(sum >= 0 && sum != error_val);
    }
    void validate()const{
        validate_sum(ra_sum);
        validate_sum(rra_sum);
    }
    PrevA2ARotSum() : ra_sum(error_val), rra_sum(error_val) {}
    PrevA2ARotSum(int ra_sum, int rra_sum) : ra_sum(ra_sum), rra_sum(rra_sum) {}
};

namespace PrevA2ARotSumCalc {    
//x
    // queriesとprev_idxを渡して、前に連なるa2aのra/rra合計を返す
    template <class QList>
    inline PrevA2ARotSum calc_prev_range_P(const QList& queries, int prev_idx) {
        int ra_sum = 0;
        int rra_sum = 0;
        for (int i = prev_idx; i >= 0; i--) {
            const auto& qri = queries[i];
            const auto& q = qri.get_query();
            if (!q.is_a2a_type()) {
                break;
            }
            auto [ra_add, rra_add] = qri.get_can_sync_ra_rra_A2A();
            ra_sum += ra_add;
            rra_sum += rra_add;
        }
        return PrevA2ARotSum{ra_sum, rra_sum};
    }

    inline PrevA2ARotSum calc_a2a_sync_rot_sum(command rot_cmd,
                                               int rot_cnt,
                                               int flip_dis_dir,
                                               e_a2a_type a2a_type) {
        PrevA2ARotSum sum(0, 0);
        if (rot_cmd == RA) {
            sum.ra_sum += rot_cnt;
        } else if (rot_cmd == RRA) {
            sum.rra_sum += rot_cnt;
        } else {
            my_assert(rot_cmd == NO_COMMAND);
            my_assert(rot_cnt == 0);
        }
        if (flip_dis_dir == 0) {
            return sum;
        }
        if (a2a_type == e_a2a_type::swap_rot) {
            if (flip_dis_dir > 0) {
                sum.ra_sum += abs(flip_dis_dir) - 1;
            } else {
                sum.rra_sum += abs(flip_dis_dir) - 1;
            }
            return sum;
        }
        my_assert(a2a_type == e_a2a_type::push_rot);
        return sum;
    }

    //x
    // ra/rraとprev_a2a_rot_sumから同期rb/rrbを計算
    inline std::pair<int, int> get_prev_a2a_syncd_rot_b(int rb_cnt, int rrb_cnt, const PrevA2ARotSum &prev_a2a_rot_sum) {
        prev_a2a_rot_sum.validate();
        int sync_rb_cnt = u0(rb_cnt - prev_a2a_rot_sum.ra_sum);
        int sync_rrb_cnt = u0(rrb_cnt - prev_a2a_rot_sum.rra_sum);
        return {sync_rb_cnt, sync_rrb_cnt};
    }
}
