//x
// filepath: sort_algorithm/YakinamashiQuery/NH/NH_Base.h
#pragma once

#include <cstddef>

//x NHはNeighborの略
//x PastNH_Baseと同様の責務を持つ基底クラス
template<class T, size_t bitset_size>
class NH_Base {
public:
    virtual ~NH_Base() = default;

    //x new_score - past_score を返す
    virtual T calc_score_diff() = 0;

    //x calc_score_diff で状態を変更した場合に元へ戻す
    virtual void revert_calc() = 0;

    //x 近傍遷移を確定適用
    virtual void transition() = 0;
};
