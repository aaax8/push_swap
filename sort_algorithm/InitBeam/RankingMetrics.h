//
// Created by Auto
//

#ifndef UNTITLED_RANKINGMETRICS_H
#define UNTITLED_RANKINGMETRICS_H

#include "all_include.h"
#include <cmath>
#include <unordered_map>
#include <algorithm>
#include "util.h"
using namespace std;

inline my_vector<int> make_rank(const my_vector<int>& order) {
    int N = order.size();
    my_vector<int> rank(N);
    my_vector<int> was(N);
    for (int i = 0; i < N; i++) {
        my_assert(order[i] >= 0 && order[i] < N);
        my_assert(!was[order[i]]);
        was[order[i]]=true;
        rank[order[i]] = i;
    }
    return rank;
}
inline void validate_perm(const my_vector<int>& perm){
    my_vector<int> a = perm;
    std::sort(a.begin(), a.end());
    for (int i = 0; i < perm.size(); i++){
        my_assert(a[i]==i);
    }
}
inline double spearman_rho(const my_vector<int>& rough,
                    const my_vector<int>& playout) {
    int N = rough.size();
    auto rank_r = make_rank(rough);
    auto rank_p = make_rank(playout);
    validate_perm(rank_r);
    validate_perm(rank_p);
    long long sum_d2 = 0;
    for (int i = 0; i < N; i++) {
        long long d = rank_r[i] - rank_p[i];
        sum_d2 += d * d;
    }

    if (N <= 1) return 1.0;
    double res= 1.0 - 6.0 * (double)sum_d2 / (double)(N * ((double)N * (double)N - 1));
    my_assert(-1.1 <= res && res <= 1.1);
    return res;
}

inline double kendall_tau(const my_vector<int>& rough,
                   const my_vector<int>& playout) {
    int N = rough.size();
    auto rank_r = make_rank(rough);
    auto rank_p = make_rank(playout);

    long long concordant = 0;
    long long discordant = 0;

    for (int i = 0; i < N; i++) {
        for (int j = i + 1; j < N; j++) {
            long long dr = rank_r[i] - rank_r[j];
            long long dp = rank_p[i] - rank_p[j];
            if (dr * dp > 0) concordant++;
            else if (dr * dp < 0) discordant++;
        }
    }

    long long total = concordant + discordant;
    if (total == 0) return 0.0;
    return (double)(concordant - discordant) / total;
}

inline double top_k_overlap(const my_vector<int>& rough,
                     const my_vector<int>& playout,
                     int k) {
    int N = rough.size();
    k = min(k, N);

    unordered_map<int, bool> in_playout_topk;
    for (int i = 0; i < k; i++) {
        in_playout_topk[playout[i]] = true;
    }

    int hit = 0;
    for (int i = 0; i < k; i++) {
        if (in_playout_topk.count(rough[i])) hit++;
    }

    return (double)hit / k;
}

inline double ndcg_at_k(const my_vector<int>& rough,
                 const my_vector<int>& playout,
                 int k) {
    int N = rough.size();
    k = min(k, N);

    auto rank_p = make_rank(playout);

    auto dcg = [&](const my_vector<int>& order) {
        double s = 0.0;
        for (int i = 0; i < k; i++) {
            int idx = order[i];
            double rel = (double)(N - rank_p[idx]);
            s += rel / log2(i + 2.0);
        }
        return s;
    };

    double dcg_val = dcg(rough);
    double idcg_val = dcg(playout);

    if (idcg_val == 0.0) return 0.0;
    return dcg_val / idcg_val;
}

inline double playout_top_a_in_rough_top_b(const my_vector<int>& rough,
                                           const my_vector<int>& playout,
                                           int a, int b) {
    int N = rough.size();
    a = min(a, N);
    b = min(b, N);
    if (a == 0) return 0.0;

    unordered_map<int, bool> in_rough_topb;
    for (int i = 0; i < b; i++) {
        in_rough_topb[rough[i]] = true;
    }

    int hit = 0;
    for (int i = 0; i < a; i++) {
        if (in_rough_topb.count(playout[i])) hit++;
    }

    return (double)hit / a;
}

#endif //UNTITLED_RANKINGMETRICS_H

