// AllOrdCalculator.h
// AllOrd計算系の責務を分離
#pragma once
#include "QueryCommon.h"
#include "YakinamashiState.h"
#include "Yakinamashi/Order/OrdTypes.h"

namespace AllOrdCalc {
    template<size_t MAX_N>
    inline AllOrd get_init_ord_a(int v, const YakinamashiState<MAX_N> &yst_state) {
        return yst_state.query_order.aorder.get_init_order(v);
    }

    template<size_t MAX_N>
    inline AllOrd get_target_ord_a(int v, const YakinamashiState<MAX_N> &yst_state) {
        return yst_state.query_order.aorder.get_target_order(v);
    }

    template<size_t MAX_N>
    inline AllOrd get_target_ord_b(int v, const YakinamashiState<MAX_N> &yst_state) {
        return yst_state.query_order.border.get_target_order(v);
    }

    template<size_t MAX_N>
    inline AllOrd get_tar_all_ord_a_A2B(const QueryCommon &query, const YakinamashiState<MAX_N> &yst_state) {
        query.validate_is_a2b_type();
        int tar_v = query.get_tar_v();
        return get_init_ord_a(tar_v, yst_state);
    }

    template<size_t MAX_N>
    inline AllOrd get_tar_all_ord_b_A2B(const QueryCommon &query, const YakinamashiState<MAX_N> &yst_state) {
        query.validate_is_a2b_type();
        int tar_v = query.get_tar_v();
        return get_target_ord_b(tar_v, yst_state);
    }

    template<size_t MAX_N>
    inline AllOrd get_tar_all_ord_a_B2A(const QueryCommon &query, const YakinamashiState<MAX_N> &yst_state) {
        query.validate_is_b2a_type();
        int tar_v = query.get_tar_v();
        return get_target_ord_a(tar_v, yst_state);
    }

    template<size_t MAX_N>
    inline AllOrd get_tar_all_ord_b_B2A(const QueryCommon &query, const YakinamashiState<MAX_N> &yst_state) {
        query.validate_is_b2a_type();
        int tar_v = query.get_tar_v();
        return get_target_ord_b(tar_v, yst_state);
    }

    template<size_t MAX_N>
    inline AllOrd get_tar_all_ord_a1_A2A(const QueryCommon &query, const YakinamashiState<MAX_N> &yst_state) {
        query.validate_is_a2a_type();
        int tar_v = query.get_tar_v();
        return get_init_ord_a(tar_v, yst_state);
    }

    template<size_t MAX_N>
    inline AllOrd get_tar_all_ord_a2_A2A(const QueryCommon &query, const YakinamashiState<MAX_N> &yst_state) {
        query.validate_is_a2a_type();
        int tar_v = query.get_tar_v();
        return get_target_ord_a(tar_v, yst_state);
    }
}
