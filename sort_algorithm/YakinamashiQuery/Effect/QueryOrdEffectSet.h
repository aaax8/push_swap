#pragma once
#include "QueryCommon.h"
#include "QueryCommonRI.h"
#include "YakinamashiQueryConstants.h"
#include "Yakinamashi/Order/OrdTypes.h"
#include "util.h"

// : Query/QueryOrdEffectSet.h から Effect/ フォルダへ移動。Effect責務の集約のため。
// クエリ削除の累積効果をbitsetで保持する
// apply_X_ords: 削除クエリが消したAllOrdを復元（存在すべきが消えていた）
// revert_X_ords: 削除クエリが追加したAllOrdを取り消し（不在であるべきが追加されていた）
template<size_t MAX_N>
struct QueryOrdEffectSet {
    using ExistOrdsA = my_bitset<YQConstants::GET_MAX_ALL_ORD_A<MAX_N>()>;
    using ExistOrdsB = my_bitset<YQConstants::GET_MAX_ALL_ORD_B<MAX_N>()>;

    ExistOrdsA apply_a_ords;
    ExistOrdsA revert_a_ords;
    ExistOrdsB apply_b_ords;
    ExistOrdsB revert_b_ords;
    int diff_stack_a_cnt = 0;
    int diff_stack_b_cnt = 0;

    // AllOrd未満のapply/revert数の差 → ord補正量
    int get_ord_delta_a(AllOrd ord) const {
        return pop_cnt(apply_a_ords, 0, ord.get()) - pop_cnt(revert_a_ords, 0, ord.get());
    }

    int get_ord_delta_b(AllOrd ord) const {
        return pop_cnt(apply_b_ords, 0, ord.get()) - pop_cnt(revert_b_ords, 0, ord.get());
    }

    void add_apply_a(AllOrd ord) { add_apply_impl(apply_a_ords, revert_a_ords, ord); }
    void add_revert_a(AllOrd ord) { add_revert_impl(apply_a_ords, revert_a_ords, ord); }
    void add_apply_b(AllOrd ord) { add_apply_impl(apply_b_ords, revert_b_ords, ord); }
    void add_revert_b(AllOrd ord) { add_revert_impl(apply_b_ords, revert_b_ords, ord); }

private:
    template<size_t SIZE>
    static void add_apply_impl(my_bitset<SIZE>& apply_ords, my_bitset<SIZE>& revert_ords, AllOrd ord) {
        my_assert(!apply_ords[ord.get()]); // 2重登録は不正
        if (revert_ords[ord.get()]) {
            // revert → apply のキャンセル
            revert_ords.reset(ord.get());
        } else {
            apply_ords.set(ord.get());
        }
    }

    template<size_t SIZE>
    static void add_revert_impl(my_bitset<SIZE>& apply_ords, my_bitset<SIZE>& revert_ords, AllOrd ord) {
        my_assert(!revert_ords[ord.get()]); // 2重登録は不正
        if (apply_ords[ord.get()]) {
            // apply → revert のキャンセル
            apply_ords.reset(ord.get());
        } else {
            revert_ords.set(ord.get());
        }
    }
};

// TODO_NEXTSTEP: erase_queriesを実装するクラスのメンバとして実装
// クエリリスト・AOrder・BOrder・is_lis・N へのアクセスが必要
// get_applied_common: QueryCommon の ord/stack_a_cnt を effect に基づいて補正して返す
// get_applied_qri: 補正後 QueryCommon から QCRIFactory 経由で RI を再構築して返す
template<size_t MAX_N>
QueryCommon get_applied_common(int q_idx, const QueryOrdEffectSet<MAX_N>& effect);

template<size_t MAX_N>
QueryCommonRI get_applied_qri(int q_idx, const QueryOrdEffectSet<MAX_N>& effect);
