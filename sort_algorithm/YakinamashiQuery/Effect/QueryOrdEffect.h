#pragma once
#include "Yakinamashi/Order/OrdTypes.h"
#include "util.h"

// : QueryProvider内のネスト構造体を独立ファイルに移動。Effect責務の分離のため。
// A/B 片側の AllOrd 追加・削除を表す。flagで有無を管理しAllOrdの>=0不変条件を維持。
//BOrderのinsert_update_ordと同じ仕様で、すでに同じall_ordがある場合は、それよりも前に挿入したあつかいになる(addだけ)
struct SingleSideOrdEffect {
private:
    AllOrd add_ord_ = AllOrd(0);
    AllOrd del_ord_ = AllOrd(0);
    bool has_add_flag_ = false;
    bool has_del_flag_ = false;
public:
    bool has_add() const { return has_add_flag_; }
    bool has_del() const { return has_del_flag_; }
    AllOrd get_add_ord() const { my_assert(has_add_flag_); return add_ord_; }
    AllOrd get_del_ord() const { my_assert(has_del_flag_); return del_ord_; }
    void set_add(AllOrd ord) { add_ord_ = ord; has_add_flag_ = true; }
    void set_del(AllOrd ord) { del_ord_ = ord; has_del_flag_ = true; }
};

// : QueryProvider内のネスト構造体を独立ファイルに移動。Effect責務の分離のため。
// A/B 片側ずつの ord 変化と stack_a_cnt の変化を保持する
struct QueryOrdEffect {
    SingleSideOrdEffect a_effect;
    SingleSideOrdEffect b_effect;
    int diff_stack_a_cnt;
};
