//QueriesModifier.h

#pragma once

#include "QueryProvider.h"
#include "Effect/QueryOrdEffectSet.h"
#include "QueryCommonRIFactory.h"
#include "State.h"
#include "QueriesState.h"
#include "QueryCommon.h"
#include "FinalRotCalculator.h"
#include "Validation/QueriesModifierArgValidator.h"
#include "QueriesCalculator.h"

//クエリ列を加工するクラス
//メンバ変数が参照でずれやすいので、このクラスのインスタンスは適宜作りなおす事
class QueriesModifier {
public:
    QueriesModifier(const State &init_st,
                    const AOrder &aorder,
                    QueriesState &yaki_state,
                    QueryProvider &provider)
            : cri_factory(),
              aorder(aorder),
              yaki_state(yaki_state),
              provider(provider) {}


    //x 
    //queries[q_idx]をqで置き換える
    void upd_ri(int q_idx, const QueryCommon &q) {
        QCRIFactory::validate_exist_q_idx(yaki_state, q_idx);
        yaki_state.sum_turn -= yaki_state.queries[q_idx].get_ins_turn();
        if (q_idx == 0) {
            yaki_state.queries[q_idx] = cri_factory.build_first_ri(yaki_state, q);
        } else {
            yaki_state.queries[q_idx] = cri_factory.build_ri(yaki_state, q, q_idx);
        }
        yaki_state.sum_turn += yaki_state.queries[q_idx].get_ins_turn();
    }

    //x
    //A2Aはtop_ord_bが前クエリに依存するため、upd前に正しい値を設定する
    void upd_ri(int q_idx) {
        QCRIFactory::validate_exist_q_idx(yaki_state, q_idx);
        QueryCommon q = yaki_state.queries[q_idx].get_query();
        if (q.is_a2a_type()) {
            int current_N = yaki_state.current_N;
            OrdPos prev_finished_b = (q_idx == 0)
                ? yaki_state.first_top_ord_b
                : yaki_state.queries[q_idx - 1].get_query().get_finished_top_ord_b(current_N);
            q.set_top_ord_pos_b_A2A(prev_finished_b);
        }
        return upd_ri(q_idx, q);
    }

    //x 
    //[q_idx]を変更したときに呼ぶ
    //[q_idx]がa2aの時に、次のabタイプのprev_a2aが影響を受けるため更新する
    //間違ってる気がするし仕様が明確でないので消した
    //range a2aをupdateしていないのが気になる
//    void upd_next_ab_by_a2a_updated(int updated_a2a_idx) {
//        ctx.qs_validator.validate_qi(updated_a2a_idx);
//        if (!yaki_state.queries[updated_a2a_idx].get_query().is_a2a_type()) {
//            return;
//        }
//        updated_a2a_idx++;
//        while (updated_a2a_idx < yaki_state.queries.size() &&
//               yaki_state.queries[updated_a2a_idx].get_query().is_a2a_type()) {
//            updated_a2a_idx++;
//        }
//        if (updated_a2a_idx < yaki_state.queries.size()) {
//            yaki_state.queries[updated_a2a_idx].get_query().validate_is_ab_type();
//            upd_ri(updated_a2a_idx);
//        }
//    }

    //x
    //以下の可能性がある時に呼ぶ
    //・q_idxのクエリがa2aで、rot_cntか、top_ord_bが変更された(ord_aは変化していない)
    //a2aでない場合は何もしない
    //a2aの時、以下を行う
    //・q_idxの次以降に連続しているa2aのtop_ord_bを変更する
    //・q_idx以降の最初のabクエリを、新しいprev_a2aのrot_sumで更新する
    void propagate_may_a2a_local_changed(int may_updated_idx) {
        QCRIFactory::validate_exist_q_idx(yaki_state, may_updated_idx);
        if (!yaki_state.queries[may_updated_idx].get_query().is_a2a_type()) {
            return;
        }
        int q_idx = may_updated_idx + 1;
        //次のab(含む)まで更新する
        while (q_idx < yaki_state.queries.size()) {
            QueryCommonRI &cur_qri = yaki_state.queries[q_idx];
            if (cur_qri.is_a2a_type()) {
                int current_N = yaki_state.current_N;
                OrdPos start_ord_b = yaki_state.queries[q_idx - 1].get_query().get_finished_top_ord_b(
                        current_N);
                cur_qri.get_query().set_top_ord_pos_b_A2A(start_ord_b);
            } else {
                my_assert(cur_qri.is_ab_type());
                upd_ri(q_idx);
                break;
            }
            q_idx++;
        }
    }

//x
    void update_final_rot() {
        yaki_state.sum_turn -= yaki_state.final_rot_info.second;
        yaki_state.final_rot_info = FinalRotCalculator::build_final_rot_info(yaki_state.current_N,
                                                                             yaki_state);
        yaki_state.sum_turn += yaki_state.final_rot_info.second;                                                                             
    }

    //x
    //現れる最大のprev_a2aの長さをkとして、O(k)
    void upd_swap(int q_idx1, int q_idx2) {
        QueriesModifierArgValidator::validate_swap_arg(yaki_state.queries, q_idx1, q_idx2);
        my_assert(q_idx1 + 1 == q_idx2);
        //q1, q2の順に並ぶ
        QueryCommonRI new_qri1 = provider.get_prev_reverted_qri(q_idx2, q_idx1);//todo
        int current_N = yaki_state.current_N;
        const QueryCommon &new_q1 = new_qri1.get_query();
        const QueryCommon &new_q2 = provider.get_applied_common(new_q1.get_finished_top_ord_b(current_N), q_idx1, provider.get_apply_effect(q_idx2));
        upd_ri(q_idx1, new_q1);
        upd_ri(q_idx2, new_q2);
        int q_idx3 = q_idx2 + 1;
        if (q_idx3 < yaki_state.queries.size()) {
            upd_ri(q_idx3);
            propagate_may_a2a_local_changed(q_idx3);
        } else {
            my_assert(q_idx3 == yaki_state.queries.size());
            update_final_rot();
        }
    }

    //x
    //O(to_qi - from_qi)
    void upd_move_down(int from_qi, int to_qi) {
        QueriesModifierArgValidator::validate_move_down_arg(yaki_state.queries, from_qi, to_qi);
        my_assert(0 <= from_qi && from_qi < yaki_state.queries.size());
        my_assert(0 <= to_qi && to_qi <= yaki_state.queries.size());
        my_assert(from_qi + 1 < to_qi);
        QueryCommon prev_q = yaki_state.queries[from_qi].get_query();
        int cur_qi = from_qi;
        int current_N = yaki_state.current_N;
        for (; cur_qi + 1 < to_qi; cur_qi++) {
            const QueryCommon &next_q = yaki_state.queries[cur_qi + 1].get_query();
            QueryCommon swap_up_q = provider.get_reverted_common(calc_start_ord_b(yaki_state, cur_qi), next_q, prev_q);
            upd_ri(cur_qi, swap_up_q);
            prev_q = provider.get_applied_common(swap_up_q.get_finished_top_ord_b(current_N), prev_q, next_q);
        }
        my_assert(cur_qi + 1 == to_qi);
        upd_ri(cur_qi, prev_q);
        if (to_qi < yaki_state.queries.size()) {
            upd_ri(to_qi);
            propagate_may_a2a_local_changed(to_qi);
        } else {
            my_assert(to_qi == yaki_state.queries.size());
            update_final_rot();
        }
    }

    //x
    void resize_queries_buff(my_vector<QueryCommon> &upd_queries_buff) {
        if (upd_queries_buff.size() < yaki_state.queries.size()) {
            QueryCommon dummy(CommonPushType::A2B, 0, 0, 0, 0);
            upd_queries_buff.assign(yaki_state.queries.size() * 2, dummy);
        }
    }

    //x
    void upd_move_up(int from_qi, int to_qi) {
        QueriesModifierArgValidator::validate_move_up_arg(yaki_state.queries, from_qi, to_qi);
        my_assert(0 <= from_qi && from_qi < yaki_state.queries.size());
        my_assert(0 <= to_qi && to_qi < yaki_state.queries.size());
        my_assert(from_qi > to_qi);
        static my_vector<QueryCommon> upd_queries_buff;
        resize_queries_buff(upd_queries_buff);
        upd_queries_buff[from_qi] = yaki_state.queries[from_qi].get_query();
        int current_N = yaki_state.current_N;
        for (int prev_qi = from_qi - 1; prev_qi >= to_qi; prev_qi--) {
            int nex_qi = prev_qi +1;
            QueryCommon& prev_q = yaki_state.queries[prev_qi].get_query();
            QueryCommon& nex_q = upd_queries_buff[nex_qi];
            upd_queries_buff[prev_qi] = provider.get_reverted_common(calc_start_ord_b(yaki_state, prev_qi), nex_q, prev_q);
            upd_queries_buff[nex_qi] = provider.get_applied_common(upd_queries_buff[prev_qi].get_finished_top_ord_b(current_N), prev_q, nex_q);
        }
        for (int i = to_qi; i <= from_qi; i++){
            QueryCommon q = upd_queries_buff[i];
            // why: バッファ計算時のstart_ord_bは暫定値のため、A2Aのtop_ord_pos_bが不正になる。
            // yaki_state[i-1]はこの時点で正しく更新済みなので、そこから再設定する。
            if (q.is_a2a_type()) {
                OrdPos prev_finished_b = (i == 0)
                    ? yaki_state.first_top_ord_b
                    : yaki_state.queries[i - 1].get_query().get_finished_top_ord_b(current_N);
                q.set_top_ord_pos_b_A2A(prev_finished_b);
            }
            upd_ri(i, q);
        }
        if(from_qi + 1 < yaki_state.queries.size()){
            upd_ri(from_qi + 1);
            propagate_may_a2a_local_changed(from_qi + 1);
        }else{
            my_assert(yaki_state.queries.size() > 0);
            my_assert(from_qi == yaki_state.queries.size() - 1);
            update_final_rot();
        }
    }


    //x
    //だいたいO(abs(to_qi - from_qi))
    void upd_move_to(int from_qi, int to_qi) {
        my_assert(0 <= from_qi && from_qi < yaki_state.queries.size());
        my_assert(0 <= to_qi && to_qi <= yaki_state.queries.size());
        my_assert(from_qi != to_qi);
        my_assert(from_qi + 1 != to_qi);
        if (from_qi < to_qi) {
            upd_move_down(from_qi, to_qi);
        } else {
            upd_move_up(from_qi, to_qi);
        }
    }

    //x
    // idxsはソート済みである前提。idxs[i]の直前にquery_commons[i]を挿入する。
    // 複数を一括挿入することでO(N)に抑える。
    template<size_t MAX_N>
    void insert_queries(const my_vector<int>& idxs, const my_vector<QueryCommon>& query_commons) {
        my_assert(idxs.size() == query_commons.size());
        validate_sorted(idxs);

        static my_vector<QueryCommon> read_buff;
        const int orig_size = (int)yaki_state.queries.size();
        if ((int)read_buff.size() < orig_size) {
            QueryCommon dummy(CommonPushType::A2B, 0, 0, 0, 0);
            read_buff.assign(orig_size * 2, dummy);
        }
        for (int i = 0; i < orig_size; i++) {
            read_buff[i] = yaki_state.queries[i].get_query();
        }
        yaki_state.queries.clear();

        const int current_N = yaki_state.current_N;
        QueryOrdEffectSet<MAX_N> effect;
        int insert_ptr = 0;

        // クエリins_turn分をリセット。追加のたびに足し、最後にupdate_final_rotで整合させる
        yaki_state.sum_turn = yaki_state.final_rot_info.second;

        for (int read_idx = 0; read_idx <= orig_size; read_idx++) {
            while (insert_ptr < (int)idxs.size() && idxs[insert_ptr] == read_idx) {
                const int write_idx = (int)yaki_state.queries.size();
                OrdPos new_top_b = yaki_state.queries.empty()
                    ? yaki_state.first_top_ord_b
                    : yaki_state.queries.back().get_query().get_finished_top_ord_b(current_N);
                QueryCommon applied_q = provider.get_applied_common_set<MAX_N>(
                    new_top_b, query_commons[insert_ptr], effect);
                QueryCommonRI new_ri = cri_factory.build_ri(yaki_state, applied_q, write_idx);
                yaki_state.sum_turn += new_ri.get_ins_turn();
                yaki_state.queries.push_back(new_ri);
                add_effect_to_set(effect, provider.get_apply_effect(query_commons[insert_ptr]));
                insert_ptr++;
            }
            if (read_idx == orig_size) break;

            const int write_idx = (int)yaki_state.queries.size();
            OrdPos new_top_b = yaki_state.queries.empty()
                ? yaki_state.first_top_ord_b
                : yaki_state.queries.back().get_query().get_finished_top_ord_b(current_N);
            QueryCommon new_q = provider.get_applied_common_set<MAX_N>(
                new_top_b, read_buff[read_idx], effect);
            QueryCommonRI new_ri = cri_factory.build_ri(yaki_state, new_q, write_idx);
            yaki_state.queries.push_back(new_ri);
            yaki_state.sum_turn += new_ri.get_ins_turn();
        }
        update_final_rot();
    }

    // tar_idxsはソート済みである前提。1パスでO(1)判定する。
    template<size_t MAX_N>
    void erase_queries(const my_vector<int>& tar_idxs) {
        validate_sorted(tar_idxs);
        QueryOrdEffectSet<MAX_N> effect;
        int write_idx = 0;
        int erase_ptr = 0;
        int current_N = yaki_state.current_N;
        for (int read_idx = 0; read_idx < (int)yaki_state.queries.size(); read_idx++) {
            if (erase_ptr < (int)tar_idxs.size() && read_idx == tar_idxs[erase_ptr]) {
                yaki_state.sum_turn -= yaki_state.queries[read_idx].get_ins_turn();
                add_effect_to_set(effect, provider.get_revert_effect(yaki_state.queries[read_idx].get_query()));
                erase_ptr++;
            } else {
                yaki_state.sum_turn -= yaki_state.queries[read_idx].get_ins_turn();
                OrdPos new_top_ord_pos_b = (write_idx == 0)
                    ? yaki_state.first_top_ord_b
                    : yaki_state.queries[write_idx - 1].get_query().get_finished_top_ord_b(current_N);
                QueryCommon new_q = provider.get_applied_common_set<MAX_N>(new_top_ord_pos_b, read_idx, effect);
                QueryCommonRI new_ri = (write_idx == 0)
                    ? cri_factory.build_first_ri(yaki_state, new_q)
                    : cri_factory.build_ri(yaki_state, new_q, write_idx);
                yaki_state.queries[write_idx] = new_ri;
                yaki_state.sum_turn += new_ri.get_ins_turn();
                write_idx++;
            }
        }
        // QueryCommonRI は default 構築不可のため、resize ではなく erase で末尾削除してビルドを通した。
        yaki_state.queries.erase(yaki_state.queries.begin() + write_idx, yaki_state.queries.end());
        update_final_rot();
    }

    QCRIFactory cri_factory;
    const AOrder &aorder;
    QueriesState &yaki_state;
    QueryProvider &provider;

private:
    template<size_t MAX_N>
    static void add_effect_to_set(QueryOrdEffectSet<MAX_N>& effect, const QueryOrdEffect& ord_effect) {
        if (ord_effect.a_effect.has_add()) effect.add_apply_a(ord_effect.a_effect.get_add_ord());
        if (ord_effect.a_effect.has_del()) effect.add_revert_a(ord_effect.a_effect.get_del_ord());
        if (ord_effect.b_effect.has_add()) effect.add_apply_b(ord_effect.b_effect.get_add_ord());
        if (ord_effect.b_effect.has_del()) effect.add_revert_b(ord_effect.b_effect.get_del_ord());
        effect.diff_stack_a_cnt += ord_effect.diff_stack_a_cnt;
    }
};
