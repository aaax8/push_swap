// QueryListOrdUtil.h
// QueryRI リストから完成後の top ord を取得するユーティリティ
#pragma once

#include "Yakinamashi/Order/OrdTypes.h"
#include "State.h"
#include "QueriesState.h"

class QueryListOrdUtil {
public:
    static OrdPos get_init_top_ord_a(const State &st, const my_vector<int> &a_ords) {
        my_assert(st.B.empty());
        my_assert(st.A.size() == a_ords.size());
        return OrdPos(get_top_ord(a_ords));
    }

    static OrdPos get_init_top_ord_b(const State &st) {
        my_assert(st.B.empty());
        return get_empty_top_ord();
    }

    template<typename QueryRIList>
    static OrdPos get_finished_top_ord_a(int N,
                                         const QueryRIList &yst_queries,
                                         const State &st,
                                         const my_vector<int> &a_ords) {
        if (yst_queries.empty()) {
            return get_init_top_ord_a(st, a_ords);
        }
        return yst_queries.back().get_finished_top_ord_a();
    }

    template<typename QueryRIList>
    static OrdPos get_finished_top_ord_b(int N,
                                         const QueryRIList &yst_queries,
                                         const State &st) {
        if (yst_queries.empty()) {
            return get_init_top_ord_b(st);
        }
        return yst_queries.back().get_query().get_finished_top_ord_b(N);
    }

    static OrdPos get_all_finished_top_ord_a(int N, const QueriesState &state) {
        if (state.queries.empty()) {
            return get_empty_top_ord();
        }
        return state.queries.back().get_finished_top_ord_a();
    }

    static OrdPos get_all_finished_top_ord_b(int N, const QueriesState &state) {
        if (state.queries.empty()) {
            return get_empty_top_ord();
        }
        return state.queries.back().get_query().get_finished_top_ord_b(N);
    }

private:
    static OrdPos get_empty_top_ord() {
        return OrdPos(0);
    }

    static int get_top_ord(const my_vector<int> &ords) {
        if (ords.empty()) {
            return 0;
        }
        return ords[0];
    }
};
