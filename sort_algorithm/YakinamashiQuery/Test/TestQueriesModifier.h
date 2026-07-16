// TestQueriesModifier.h
#pragma once

#include "all_include.h"
#include "State.h"
#include "ChunkAndBeamSolver.h"
#include "QueriesModifier.h"
#include "YstQueriesBuildPipeline.h"
#include "YstFactory.h"
#include "YakinamashiStateValidator.h"
#include "QueriesState.h"
#include "AOrder.h"
#include "BOrder.h"
#include "QueryProvider.h"
#include "FinalRotCalculator.h"
#include "Validation/QueriesModifierArgValidator.h"
#include "QueryStateApplyUtil.h"
#include "QueriesCalculator.h"
#include "RoughQuery.h"

template<size_t MAX_N>
class TestQueriesModifier {
    using YakinamashiState = ::YakinamashiState<MAX_N>;
public:
	TestQueriesModifier(QueriesModifier &modifier,
						QueriesState &yaki_state,
						const State &initial_state,
						const YakinamashiState &yst,
						int test_n = 1000,
						int seed = 20260305)
			: modifier(modifier),
			  yaki_state(yaki_state),
			  initial_state(initial_state),
			  aorder(const_cast<AOrder&>(yst.query_order.aorder)),
			  border(const_cast<BOrder&>(yst.query_order.border)),
			  test_n(test_n),
			  rng(seed) {
	}

	void run_all() {
		test_upd_ri();
		test_upd_swap();
		test_upd_move_to();
	}

	void test_upd_ri() {
		my_assert(!yaki_state.queries.empty());
		const QueriesState snapshot = yaki_state;
		for (int iter = 0; iter < test_n; ++iter) {
			int q_idx = rand_int(0, static_cast<int>(yaki_state.queries.size()));
			modifier.upd_ri(q_idx);
			QueriesStateValidator<MAX_N>::validate_state(initial_state, yaki_state);
			my_assert(yaki_state == snapshot);
		}
	}

	void test_upd_swap() {
		for (int iter = 0; iter < test_n; ++iter) {
			my_vector<pair<int, int> > candidates = collect_valid_swap_candidates();
			my_assert(!candidates.empty());
			const auto [q_idx1, q_idx2] = candidates[rand_int(0, static_cast<int>(candidates.size()))];

			my_vector<RoughQuery> before = to_rough_queries(yaki_state.queries);
            QueriesState bef_yst = yaki_state;
			out("--- iter", iter, "swap", q_idx1, q_idx2, "---");
			dump_queries_with_state("BEFORE_SWAP");
			modifier.upd_swap(q_idx1, q_idx2);
			dump_queries_with_state("AFTER_SWAP");
			QueriesStateValidator<MAX_N >::validate_state(initial_state, yaki_state);
			my_vector<RoughQuery> after = to_rough_queries(yaki_state.queries);

			my_assert(before.size() == after.size());
			for (int i = 0; i < static_cast<int>(before.size()); ++i) {
				if (i == q_idx1) {
					my_assert(after[i] == before[q_idx2]);
				} else if (i == q_idx2) {
					my_assert(after[i] == before[q_idx1]);
				} else {
					my_assert(after[i] == before[i]);
				}
			}
		}
	}

	void test_upd_move_to() {
		for (int iter = 0; iter < test_n; ++iter) {
			my_vector<pair<int, int> > candidates = collect_valid_move_to_candidates();
			my_assert(!candidates.empty());
			const auto [from_qi, to_qi] = candidates[rand_int(0, static_cast<int>(candidates.size()))];

			my_vector<RoughQuery> before = to_rough_queries(yaki_state.queries);
			my_vector<RoughQuery> expected = calc_move_to_expected(before, from_qi, to_qi);

			modifier.upd_move_to(from_qi, to_qi);
			QueriesStateValidator<MAX_N>::validate_state(initial_state, yaki_state);

			my_vector<RoughQuery> after = to_rough_queries(yaki_state.queries);
			my_assert(after == expected);
		}
	}

private:
	QueriesModifier &modifier;
	QueriesState &yaki_state;
	const State &initial_state;
	AOrder &aorder;
	BOrder &border;
	int test_n;
	std::mt19937 rng;

	static inline string valstate_to_str(ValState vs) {
		switch (vs) {
			case ValState::INIT: return "INIT";
			case ValState::TARGET: return "TARGET";
			case ValState::NONE: return "NONE";
			default: return "???";
		}
	}

	void dump_queries_with_state(const char* label) {
		out("===", label, "===");
		int N = initial_state.initial_N;
		out("AllOrd_Map_A:");
		for (int v = 0; v < N; ++v) {
			AllOrd ao_init = aorder.get_all_order(v, ValState::INIT);
			out("v" + to_string(v) + ": INIT=" + to_string(ao_init.get()));
			if (aorder.has_target_order(v)) {
				AllOrd ao_target = aorder.get_all_order(v, ValState::TARGET);
				out("v" + to_string(v) + ": TARGET=" + to_string(ao_target.get()));
			}
		}

		State st(initial_state);
		QueriesCalculator qs_calc(N, yaki_state.queries);
		my_vector<ValState> a_val(N, ValState::NONE);
		my_vector<ValState> b_val(N, ValState::NONE);
		for (int i = 0; i < (int)st.A.size(); ++i) {
			int v = st.A[i];
			a_val[v] = ValState::INIT;
		}
		for (int i = 0; i < (int)st.B.size(); ++i) {
			b_val[st.B[i]] = ValState::TARGET;
		}
		for (int qi = 0; qi < (int)yaki_state.queries.size(); ++qi) {
			const auto &qri = yaki_state.queries[qi];
			out("q", qi);
			out(qri);
			if (qri.is_a2b_type()) {
				out("orig_a:", qs_calc.get_orig_a_cmd_cnt_A2B(qi),
					"orig_b:", qs_calc.get_orig_b_cmd_cnt_A2B(qi));
			} else if (qri.is_b2a_type()) {
				out("orig_a:", qs_calc.get_orig_a_cmd_cnt_B2A(qi),
					"orig_b:", qs_calc.get_orig_b_cmd_cnt_B2A(qi));
			}
			QueryStateApplyUtil::apply_query_to_state(st, qs_calc, qi, qri);
			// ValState更新
			int v = qri.get_tar_v();
			if (qri.is_a2b_type()) {
				a_val[v] = ValState::NONE;
				b_val[v] = ValState::TARGET;
			} else if (qri.is_b2a_type()) {
				b_val[v] = ValState::NONE;
				a_val[v] = ValState::TARGET;
			} else {
				a_val[v] = ValState::TARGET;
			}
			// A(v allord valstate)
			out("A:");
			for (int i = 0; i < (int)st.A.size(); ++i) {
				int sv = st.A[i];
				string vs_str = valstate_to_str(a_val[sv]);
				out(to_string(sv) + string("(all_ord:") + to_string(aorder.get_all_order(sv, a_val[sv]).get()) +
					" " + vs_str + ")");
			}
			out("B:");
			for (int i = 0; i < (int)st.B.size(); ++i) {
				int sv = st.B[i];
				string vs_str = valstate_to_str(b_val[sv]);
				out(to_string(sv) + "(" + vs_str + ")");
			}
		}
		out("=== final_rot_info ===");
		out("cmd:", yaki_state.final_rot_info.first, "turn:", yaki_state.final_rot_info.second);
		out("final A:");
		my_vector<ValState> final_a_val(N, ValState::NONE);
		for (int i = 0; i < (int)st.A.size(); ++i) {
			final_a_val[st.A[i]] = a_val[st.A[i]];
		}
		for (int i = 0; i < (int)st.A.size(); ++i) {
			int sv = st.A[i];
			string vs_str = valstate_to_str(final_a_val[sv]);
			out(to_string(sv) + string("(all_ord:") + to_string(aorder.get_all_order(sv, final_a_val[sv]).get()) +
				" " + vs_str + ")");
		}
	}

	int rand_int(int low, int high_exclusive) {
		my_assert(low < high_exclusive);
		std::uniform_int_distribution<int> dist(low, high_exclusive - 1);
		return dist(rng);
	}

	my_vector<pair<int, int> > collect_valid_swap_candidates() const {
		my_vector<pair<int, int> > candidates;
		for (int i = 0; i + 1 < static_cast<int>(yaki_state.queries.size()); ++i) {
			const int j = i + 1;
			if (QueriesModifierArgValidator::is_swap_arg_valid(yaki_state.queries, i, j)) {
				candidates.emplace_back(i, j);
			}
		}
		return candidates;
	}

	my_vector<pair<int, int> > collect_valid_move_to_candidates() const {
		my_vector<pair<int, int> > candidates;
		const int qn = static_cast<int>(yaki_state.queries.size());
		for (int from_qi = 0; from_qi < qn; ++from_qi) {
			for (int to_qi = 0; to_qi <= qn; ++to_qi) {
				if (from_qi == to_qi || from_qi + 1 == to_qi) {
					continue;
				}
				bool ok = false;
				if (from_qi < to_qi) {
					ok = QueriesModifierArgValidator::is_move_down_arg_valid(yaki_state.queries, from_qi, to_qi);
				} else {
					if (to_qi < qn) {
						ok = QueriesModifierArgValidator::is_move_up_arg_valid(yaki_state.queries, from_qi, to_qi);
					}
				}
				if (ok) {
					candidates.emplace_back(from_qi, to_qi);
				}
			}
		}
		return candidates;
	}

	static my_vector<RoughQuery> calc_move_to_expected(const my_vector<RoughQuery> &before,
													   int from_qi,
													   int to_qi) {
		my_vector<RoughQuery> expected = before;
		RoughQuery moved = expected[from_qi];
		expected.erase(expected.begin() + from_qi);
		const int insert_pos = (from_qi < to_qi) ? (to_qi - 1) : to_qi;
		expected.insert(expected.begin() + insert_pos, moved);
		return expected;
	}
};

template<size_t MAX_N, int HABA = 3, int K = 2>
void run_test_queries_modifier_case(const State &init_state) {
    clear_file();
	YstFactory<MAX_N, HABA, K> factory;
	YakinamashiState<MAX_N> yst = factory.build(init_state);


	QueryProvider provider(yst, init_state);
	// _BEGIN: test 経路でも query mutation 後 validator を共有する。
	QueriesModifier modifier(init_state, yst.query_order.aorder, yst.queries_state, provider);
	// _END
	TestQueriesModifier<MAX_N> tester(modifier, yst.queries_state, init_state, yst);
//	tester.run_all();
}

template<size_t MAX_N, int HABA = 3, int K = 2>
void test_queries_modifier() {
//    clear_file();
	int TEST_N = my_rand(10, 101);
    vector<int> perm(TEST_N);
    std::iota(perm.begin(), perm.end(), 0);
    std::mt19937 engine((uint64_t) std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count());
    std::shuffle(perm.begin(), perm.end(), engine);
//    perm = {0, 9, 8, 1, 7, 6, 3, 5,2 ,4 };
    State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
    run_test_queries_modifier_case<MAX_N, HABA, K>(init_state);
}
