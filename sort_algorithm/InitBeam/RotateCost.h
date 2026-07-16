#ifndef UNTITLED_ROTATECOST_H
#define UNTITLED_ROTATECOST_H

#include "all_include.h"

// 同方向回転は max(dis_a, dis_b) でまとめて回せる、逆方向は和
inline int calc_joint_rot_cost(int dis_a, int dis_b, bool same_dir) {
    return same_dir ? max(dis_a, dis_b) : (dis_a + dis_b);
}

inline int calc_rot_cost_sync(int dis_a, int dis_b) {
    return max(dis_a, dis_b);
}

inline int calc_rot_cost_not_sync(int dis_a, int dis_b) {
    return dis_a + dis_b;
}

// 挿入にかかる最小ターン数（ra/rb、rra/rrb の4組から最小を取って +1(push)）
inline int calc_min_insert_turn(int ra_dis, int rb_dis, int rra_dis, int rrb_dis) {
    int cost_ra_rb   = calc_joint_rot_cost(ra_dis,  rb_dis,  true);
    int cost_rra_rrb = calc_joint_rot_cost(rra_dis, rrb_dis, true);
    int cost_ra_rrb  = calc_joint_rot_cost(ra_dis,  rrb_dis, false);
    int cost_rra_rb  = calc_joint_rot_cost(rra_dis, rb_dis,  false);
    return min({cost_ra_rb, cost_rra_rrb, cost_ra_rrb, cost_rra_rb}) + 1;
}

inline int rotate_dis(int f, int t, int size) {
    my_assert(0 <= f && f < size);
    my_assert(0 <= t && t < size);
    if (f <= t) {
        return t - f;
    } else {
        return size - f + t;
    }
}

inline int rev_rotate_dis(int f, int t, int size) {
    my_assert(0 <= f && f < size);
    my_assert(0 <= t && t < size);
    if (t <= f) {
        return f - t;
    } else {
        return size - t + f;
    }
}
#endif //UNTITLED_ROTATECOST_H

