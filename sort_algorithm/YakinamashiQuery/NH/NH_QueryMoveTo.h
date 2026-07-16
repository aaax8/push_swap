#pragma once
#include "NH_Base.h"
#include "QueriesState.h"
#include "YakinamashiState.h"
#include "State.h"
#include "QueriesModifier.h"
#include "QueryProvider.h"
#include "QueriesModifierArgValidator.h"
#include <limits>
#include <string>
#include "util.h"

template<size_t bitset_size>
class NH_MoveTo : public NH_Base<int, bitset_size> {
public:
	static constexpr int INVALID_INDEX = -1;
	static constexpr int SCORE_INF = std::numeric_limits<int>::max();

	NH_MoveTo(YakinamashiState<bitset_size> &yst, const State &init_st, int target_to_idx)
		: yst(yst), init_st(init_st), to_idx(target_to_idx)
		, best_from_idx(INVALID_INDEX), best_score_diff(SCORE_INF)
		, qs_snapshot(yst.queries_state)
	{
		my_assert(yst.queries_state.queries.size() > 0);
		my_assert(0 <= to_idx && to_idx <= (int)yst.queries_state.queries.size());
	}

	int calc_score_diff() override {
		qs_snapshot = yst.queries_state;
		int prev_sum_turn = yst.queries_state.sum_turn;
		best_from_idx = INVALID_INDEX;
		best_score_diff = SCORE_INF;

		out("[NH_MoveTo] to_idx=", to_idx, "prev_sum_turn=", prev_sum_turn);

		QueryProvider provider(yst, init_st);
		// _BEGIN: move 系 mutation 後も共通 validator を使う。
		QueriesModifier modifier(init_st, yst.query_order.aorder, yst.queries_state, provider);
		// _END
		int q_size = static_cast<int>(yst.queries_state.queries.size());
		std::string diffs_log = "[NH_MoveTo] diffs:";
		for (int fi = 0; fi < q_size; fi++) {
			// upd_move_to の assert 条件を満たすためスキップ
			if (fi == to_idx || fi + 1 == to_idx) {
				continue;
			}
			// 順序制約・stack_a_cnt 数値制約を満たさない移動はスキップ
			const auto &queries = yst.queries_state.queries;
			if (fi < to_idx && !QueriesModifierArgValidator::is_move_down_arg_valid(queries, fi, to_idx)) {
				continue;
			}
			if (fi > to_idx && !QueriesModifierArgValidator::is_move_up_arg_valid(queries, fi, to_idx)) {
				continue;
			}
			if (!is_stack_cnt_safe(queries, fi, to_idx, yst.queries_state.current_N)) {
				continue;
			}
			modifier.upd_move_to(fi, to_idx);
			int score_diff = yst.queries_state.sum_turn - prev_sum_turn;
			diffs_log += " {" + std::to_string(fi) + ", " + std::to_string(score_diff) + "}";
			if (score_diff < best_score_diff) {
				best_from_idx = fi;
				best_score_diff = score_diff;
			}
			yst.queries_state = qs_snapshot;
		}
		out(diffs_log);
		out("[NH_MoveTo] best_from=", best_from_idx, "best_diff=", best_score_diff);
		return best_score_diff;
	}

	// calc_score_diff 内でスナップショット復元が完結しているため何もしない
	void revert_calc() override {}

	void transition() override {
		if (best_from_idx == INVALID_INDEX) {
			return;
		}
		QueryProvider provider(yst, init_st);
		// _BEGIN: move 系 mutation 後も共通 validator を使う。
		QueriesModifier modifier(init_st, yst.query_order.aorder, yst.queries_state, provider);
		// _END
		modifier.upd_move_to(best_from_idx, to_idx);
	}

private:
	// move_down/up による中間クエリの stack_a_cnt 変化が [0, N] に収まるか確認
	static bool is_stack_cnt_safe(const QRIList &queries, int fi, int to_idx, int current_N) {
		const CommonPushType push_type = queries[fi].get_query().get_push_type();
		// A2A は stack_a_cnt を変化させないため常にOK
		if (push_type != CommonPushType::A2B && push_type != CommonPushType::B2A) {
			return true;
		}
		// move_down: [fi+1, to_idx) の中間クエリから queries[fi] の効果が消える
		// move_up:   [to_idx, fi) の中間クエリに queries[fi] の効果が加わる
		int lo = (fi < to_idx) ? fi + 1 : to_idx;
		int hi = (fi < to_idx) ? to_idx  : fi;
		// delta: 中間クエリの stack_a_cnt に加算される値
		bool adds_one;
		if (fi < to_idx) {
			// move_down: A2B が前から消えると +1, B2A が消えると -1
			adds_one = (push_type == CommonPushType::A2B);
		} else {
			// move_up: B2A が前に追加されると +1, A2B が追加されると -1
			adds_one = (push_type == CommonPushType::B2A);
		}
		for (int qi = lo; qi < hi; qi++) {
			int cnt = queries[qi].get_query().get_stack_a_cnt();
			if (adds_one && cnt + 1 > current_N) return false;
			if (!adds_one && cnt - 1 < 0) return false;
		}
		return true;
	}

	YakinamashiState<bitset_size> &yst;
	const State &init_st;
	int to_idx;
	int best_from_idx;
	int best_score_diff;
	QueriesState qs_snapshot;
};
