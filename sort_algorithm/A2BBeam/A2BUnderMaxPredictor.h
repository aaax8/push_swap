#ifndef UNTITLED_A2BUNDERMAXPREDICTOR_H
#define UNTITLED_A2BUNDERMAXPREDICTOR_H

#include "all_include.h"
#include "RingBuffer500.h"
#include "util.h"

template<size_t MAX_N>
class A2BUnderMaxPredictor {
private:
    // 責務: B 円環上で from から to へ rb/rrb のどちらで動くかを保持する。
    // 必要な理由: rotate cost と missing 追加 cost を同じ区間列から計算するため。
    //_cはcloseの略
    struct Segment {
        int from_c;
        int to_c;
        bool is_rb;
    };
    
    // 責務: 端点を含む linear 区間 [l, r] を保持する。
    // 必要な理由: missing_cnt の累積和と既訪問区間の差分を O(1) 個の区間で計算するため。
    //_cはcloseの略
    struct LinearRange {
        int l_c;
        int r_c;
    };

    // 責務: 1 つの A2B 経路候補について rotate cost、missing cost、合計 cost を保持する。
    // 必要な理由: ログでどの部分が手数に効いたかを確認しつつ、既存の total 比較を保つため。
    struct PathCostResult {
        int rotate_cost;
        int missing_cost;
        int total_cost;
    };

    // 責務: from から to へ rb で進む rotate 回数を返す。
    // 必要な理由: 候補経路の rotate cost を方向ごとに足すため。
    static int rb_dis(int B_size, int from, int to) {
        if (to >= from) {
            return to - from;
        }
        return B_size - from + to;
    }
    
    // 責務: from から to へ rrb で進む rotate 回数を返す。
    // 必要な理由: 候補経路の rotate cost を方向ごとに足すため。
    static int rrb_dis(int B_size, int from, int to) {
        if (from >= to) {
            return from - to;
        }
        return from + B_size - to;
    }

    // 責務: segment の from から to へ進む rotate 回数を返す。
    // 必要な理由: 経路評価で rb/rrb 分岐を一箇所に閉じ込めるため。
    static int seg_dis(int B_size, const Segment &seg) {
        if (seg.is_rb) {
            return rb_dis(B_size, seg.from_c, seg.to_c);
        }
        return rrb_dis(B_size, seg.from_c, seg.to_c);
    }

    // 責務: rotate 方向をログ用文字列へ変換する。
    // 必要な理由: segment/candidate ログで rb/rrb を同じ表記で出すため。
    static const char *dir_name(bool is_rb) {
        if (is_rb) {
            return "rb";
        }
        return "rrb";
    }

    // 責務: my_vector<int> を label=[v0,v1,...] 形式で出力する。
    // 必要な理由: need_top_idxs と missing_cnt を同じ形式で確認するため。
    static void write_int_vector_debug(std::ostream &os, const char *label, const my_vector<int> &values) {
        os << label << "=[";
        for (int i = 0; i < values.size(); i++) {
            if (i > 0) {
                os << ",";
            }
            os << values[i];
        }
        os << "]";
    }

    // 責務: Segment を {from,to,dir} 形式で出力する。
    // 必要な理由: 候補がどの区間をどの方向に進んだかを確認するため。
    static void write_segment_debug(std::ostream &os, const Segment &seg) {
        os << "{from=" << seg.from_c << ",to=" << seg.to_c << ",dir=" << dir_name(seg.is_rb) << "}";
    }

    // 責務: pre_goal_segments を配列形式で出力する。
    // 必要な理由: no_turn/turn 候補の前半経路をログで確認するため。
    static void write_segments_debug(std::ostream &os, const my_vector<Segment> &segments) {
        os << "[";
        for (int i = 0; i < segments.size(); i++) {
            if (i > 0) {
                os << ",";
            }
            write_segment_debug(os, segments[i]);
        }
        os << "]";
    }

    // 責務: LinearRange の集合を [{l,r},...] 形式で出力する。
    // 必要な理由: missing cost の二重計上除外がどの区間を既訪問扱いにしたか確認するため。
    static void write_seen_debug(std::ostream &os, const my_vector<LinearRange> &seen) {
        os << "[";
        for (int i = 0; i < seen.size(); i++) {
            if (i > 0) {
                os << ",";
            }
            os << "{l=" << seen[i].l_c << ",r=" << seen[i].r_c << "}";
        }
        os << "]";
    }

    // 責務: start=0 から goal_idx へ向かう rb/rrb の小さい rotate cost を返す。
    // 必要な理由: A が空、または goal 以外の need が無い場合でも max_b を top にする必要があるため。
    static int calc_goal_rot(int B_size, int goal_idx) {
        return min(rb_dis(B_size, 0, goal_idx), rrb_dis(B_size, 0, goal_idx));
    }

     // 責務: missing_cnt の prefix sum を作り、[l, r] の和を O(1) で取れる形にする。
    // 必要な理由: 各候補経路の missing 追加 cost を区間和で評価するため。
    static my_vector<int> build_prefix(int B_size, const my_vector<int> &missing_cnt) {
        my_assert(missing_cnt.size() == B_size);
        my_vector<int> prefix(B_size + 1);
        for (int i = 0; i < B_size; i++) {
            prefix[i + 1] = prefix[i] + missing_cnt[i];
        }
        return prefix;
    }
    
    // 責務: need_top_idxs から goal_idx を除いた idx の bit 配列を作る。
    // 必要な理由: 方向変更候補の endpoint を前後表で O(1) 取得するため。
    static my_vector<int> build_need_bits(int B_size, const my_vector<int> &need_top_idxs, int goal_idx) {
        my_vector<int> need_bits(B_size);
        for (int i = 0; i < need_top_idxs.size(); i++) {
            int idx = need_top_idxs[i];
            my_assert(0 <= idx && idx < B_size);
            if (idx != goal_idx) {
                need_bits[idx] = 1;
            }
        }
        return need_bits;
    }

    // 責務: 各 i について、i 以上方向で最初に見つかる need idx を円環として返す表を作る。
    // 必要な理由: no-turn と turn 後 endpoint 計算で -1 fallback を使わないため。
    static my_vector<int> build_circular_next_need(int B_size, const my_vector<int> &need_bits) {
        my_vector<int> next_need(B_size, -1);
        int next_idx = -1;
        for (int i = B_size - 1; i >= 0; i--) {
            if (need_bits[i]) {
                next_idx = i;
            }
            next_need[i] = next_idx;
        }
        if (next_idx == -1) {
            return next_need;
        }
        int first_idx = next_idx;
        my_assert(first_idx != -1);
        for (int i = B_size - 1; i >= 0; i--) {
            if (next_need[i] == -1) {
                next_need[i] = first_idx;
            }else{
                break;
            }
        }        
        #ifdef DEBUG 
        for (int i = 0; i < B_size; i++) {
            my_assert(next_need[i] != -1);
        }   
        #endif
        return next_need;
    }
    
    // 責務: 各 i について、i 以下方向で最初に見つかる need idx を円環として返す表を作る。
    // 必要な理由: no-turn と turn 後 endpoint 計算で -1 fallback を使わないため。
    static my_vector<int> build_circular_prev_need(int B_size, const my_vector<int> &need_bits) {
        my_vector<int> prev_need(B_size, -1);
        int prev_idx = -1;
        for (int i = 0; i < B_size; i++) {
            if (need_bits[i]) {
                prev_idx = i;
            }
            prev_need[i] = prev_idx;
        }
        if (prev_idx == -1) {
            return prev_need;
        }
        int last_idx = prev_idx;
        for (int i = 0; i < B_size; i++) {
            if (prev_need[i] == -1) {
                prev_need[i] = last_idx;
            }else{
                break;
            }
        }
        #ifdef DEBUG 
        for (int i = 0; i < B_size; i++) {
            my_assert(prev_need[i] != -1);
        }   
        #endif
        return prev_need;
    }
    
    // 責務: missing_cnt 全体の合計を返す。
    // 必要な理由: missing_sum==0 の場合に A の全要素が max_b より大きいことを DEBUG 確認するため。
    static int calc_missing_sum(int B_size, const my_vector<int> &missing_cnt) {
        int sum = 0;
        for (int i = 0; i < B_size; i++) {
            sum += missing_cnt[i];
        }
        return sum;
    }

    // 責務: need_bits に goal 以外の need が 1 つ以上あるかを返す。
    // 必要な理由: missing_sum>0 なのに見るべき挿入 idx が無い異常系を検出するため。
    static bool has_non_goal_need(const my_vector<int> &need_bits) {
        for (int i = 0; i < need_bits.size(); i++) {
            if (need_bits[i]) {
                return true;
            }
        }
        return false;
    }
    
    // 責務: A の全要素が max_b より大きいことを DEBUG build で確認する。
    // 必要な理由: missing_sum==0 のとき、A に max_b 以下が残っていない前提を実値で検証するため。
    static void debug_assert_all_a_over_max_b(const RingBuffer500 &a, int max_b) {
#ifdef DEBUG
        for (int i = 0; i < a.size(); i++) {
            my_assert(a[i] > max_b);
        }
#else
        (void) a;
        (void) max_b;
#endif
    }

    // 責務: calc_a2b_top_max の入力と missing_sum をログ出力する。
    // 必要な理由: B2A が作った need_top_idxs/missing_cnt を A2B 側がどう受け取ったか確認するため。
    static void log_a2b_start(std::ostream &os, int B_size, const RingBuffer500 &a, int max_b,
                              const my_vector<int> &need_top_idxs, const my_vector<int> &missing_cnt,
                              int goal_idx, int missing_sum) {
        os << "calc_a2b_top_max_start"
           << "\n  B_size=" << B_size
           << "\n  a_size=" << a.size()
           << "\n  max_b=" << max_b
           << "\n  goal_idx=" << goal_idx
           << "\n  missing_sum=" << missing_sum
           << "\n  ";
        write_int_vector_debug(os, "need_top_idxs", need_top_idxs);
        os << "\n  ";
        write_int_vector_debug(os, "missing_cnt", missing_cnt);
        os << "\n";
        os.flush();
    }

    // 責務: early return する理由と cost 内訳をログ出力する。
    // 必要な理由: missing_sum==0 や goal_only のとき候補列挙をしない理由を確認するため。
    static void log_a2b_special(std::ostream &os, const char *kind, int pb_cost, int goal_rot, int total_cost) {
        os << "calc_a2b_top_max_special"
           << "\n  kind=" << kind
           << "\n  pb_cost=" << pb_cost
           << "\n  goal_rot=" << goal_rot
           << "\n  total_cost=" << total_cost
           << "\n";
        os.flush();
    }

    // 責務: 1 つの goal 方向候補の経路と cost 内訳をログ出力する。
    // 必要な理由: どの turn_idx と goal_dir が best になったか確認するため。
    static void log_a2b_candidate(std::ostream &os, bool start_rb, const char *kind, int turn_idx,
                                  const my_vector<Segment> &pre_goal_segments, const Segment &goal_segment,
                                  const PathCostResult &result, int pb_cost) {
        os << "calc_a2b_top_max_candidate"
           << "\n  start_dir=" << dir_name(start_rb)
           << "\n  kind=" << kind
           << "\n  turn_idx=" << turn_idx
           << "\n  pre_goal_segments=";
        write_segments_debug(os, pre_goal_segments);
        os << "\n  goal_segment=";
        write_segment_debug(os, goal_segment);
        os << "\n  rotate_cost=" << result.rotate_cost
           << "\n  missing_cost=" << result.missing_cost
           << "\n  pb_cost=" << pb_cost
           << "\n  candidate_total=" << pb_cost + result.total_cost
           << "\n";
        os.flush();
    }

    // 責務: calc_a2b_top_max の最終 best cost をログ出力する。
    // 必要な理由: 候補列挙ログと返り値を照合できるようにするため。
    static void log_a2b_best(std::ostream &os, int pb_cost, int rotate_cost, int total_cost) {
        os << "calc_a2b_top_max_best"
           << "\n  pb_cost=" << pb_cost
           << "\n  rotate_cost=" << rotate_cost
           << "\n  total_cost=" << total_cost
           << "\n";
        os.flush();
    }
    
    // 責務: 端点込み [l, r] の missing_cnt 合計を返す。
    // 必要な理由: 円環区間を linear に分けた後の cost 計算を O(1) にするため。
    static int range_sum(const my_vector<int> &prefix, int l, int r) {
        my_assert(0 <= l && l <= r);
        my_assert(r + 1 < prefix.size());
        return prefix[r + 1] - prefix[l];
    }
    
    // 責務: seen の linear range が昇順かつ互いに交差しないことを DEBUG assert する。
    // 必要な理由: unseen_sum が seen の非交差性を前提に差分計算するため。
    static void assert_seen_ranges(const my_vector<LinearRange> &seen) {
#ifdef DEBUG
        for (int i = 1; i < seen.size(); i++) {
            my_assert(seen[i - 1].r_c < seen[i].l_c);
        }
#endif
    }

    // 責務: seen に linear 区間を追加し、重なる区間と隣接区間を merge する。
    // 必要な理由: 後ろから見たとき、すでに最後訪問が確定した idx を二重計上しないため。
    //todo
    static void add_seen(my_vector<LinearRange> &seen, LinearRange add_range) {
        assert_seen_ranges(seen);
        my_vector<LinearRange> merged;
        bool inserted = false;
        for (int i = 0; i < seen.size(); i++) {
            LinearRange cur = seen[i];
            if (cur.r_c + 1 < add_range.l_c) {
                merged.push_back(cur);
            } else if (add_range.r_c + 1 < cur.l_c) {
                if (!inserted) {
                    merged.push_back(add_range);
                    inserted = true;
                }
                merged.push_back(cur);
            } else {
                add_range.l_c = min(add_range.l_c, cur.l_c);
                add_range.r_c = max(add_range.r_c, cur.r_c);
            }
        }
        if (!inserted) {
            merged.push_back(add_range);
        }
        seen = std::move(merged);
        assert_seen_ranges(seen);
    }
    
    // 責務: range 内で seen に含まれない idx の missing_cnt 合計を返す。
    // 必要な理由: 各 idx の最後訪問方向だけが missing 追加 cost に影響するため。
    static int unseen_sum(const my_vector<int> &prefix, const my_vector<LinearRange> &seen, LinearRange range) {
        assert_seen_ranges(seen);
        int sum = range_sum(prefix, range.l_c, range.r_c);
        for (int i = 0; i < seen.size(); i++) {
            int l = max(range.l_c, seen[i].l_c);
            int r = min(range.r_c, seen[i].r_c);
            if (l <= r) {
                sum -= range_sum(prefix, l, r);
            }
        }
        return sum;
    }

    // 責務: 1 個の linear range について missing 追加 cost と seen 更新を行う。
    // 必要な理由: hot path で一時 vector を作らず segment cost を計算するため。
    static int add_linear_cost(const my_vector<int> &prefix, my_vector<LinearRange> &seen,
                               LinearRange range, bool add_missing_cost, bool seg_rb,
                               std::ostream *log_os) {
        int cost = 0;
        if (add_missing_cost) {
            cost += unseen_sum(prefix, seen, range);
        }
        add_seen(seen, range);
        if (log_os != nullptr) {
            *log_os << "calc_a2b_top_max_segment_range"
                    << "\n  dir=" << dir_name(seg_rb)
                    << "\n  linear_range=[" << range.l_c << "," << range.r_c << "]"
                    << "\n  add_missing=" << cost
                    << "\n  seen_after=";
            write_seen_debug(*log_os, seen);
            *log_os << "\n";
            log_os->flush();
        }
        return cost;
    }

    // 責務: segment のうち、まだ最後訪問が確定していない idx の missing cost を加算して seen を更新する。
    // 必要な理由: rb で最後訪問された idx だけ +missing、rrb は +0 とする規則を実装するため。
    static int add_segment_cost(int B_size, const my_vector<int> &prefix, my_vector<LinearRange> &seen,
                                const Segment &seg, std::ostream *log_os) {
        int cost = 0;
        if (log_os != nullptr) {
            *log_os << "calc_a2b_top_max_segment"
                    << "\n  segment=";
            write_segment_debug(*log_os, seg);
            *log_os << "\n";
            log_os->flush();
        }
        if (seg.is_rb) {
            if (seg.from_c <= seg.to_c) {
                cost += add_linear_cost(prefix, seen, {seg.from_c, seg.to_c}, true, seg.is_rb, log_os);
            } else {
                cost += add_linear_cost(prefix, seen, {seg.from_c, B_size - 1}, true, seg.is_rb, log_os);
                cost += add_linear_cost(prefix, seen, {0, seg.to_c}, true, seg.is_rb, log_os);
            }
        } else {
            if (seg.to_c <= seg.from_c) {
                cost += add_linear_cost(prefix, seen, {seg.to_c, seg.from_c}, false, seg.is_rb, log_os);
            } else {
                cost += add_linear_cost(prefix, seen, {0, seg.from_c}, false, seg.is_rb, log_os);
                cost += add_linear_cost(prefix, seen, {seg.to_c, B_size - 1}, false, seg.is_rb, log_os);
            }
        }
        return cost;
    }

    // 責務: 前半 segment と最後の goal segment から rotate cost と missing 追加 cost を返す。
    // 必要な理由: 候補ごとに、最後の goal_idx だけ missing cost 0 とする規則を含めて比較するため。
    static PathCostResult calc_path_cost(int B_size, const my_vector<int> &prefix,
                                         const my_vector<Segment> &pre_goal_segments,
                                         const Segment &last_seg, int goal_idx,
                                         std::ostream *log_os) {
        int rotate_cost = 0;
        for (int i = 0; i < pre_goal_segments.size(); i++) {
            rotate_cost += seg_dis(B_size, pre_goal_segments[i]);
        }
        rotate_cost += seg_dis(B_size, last_seg);
        my_vector<LinearRange> seen;
        add_seen(seen, {goal_idx, goal_idx});
        int missing_cost = 0;
        missing_cost += add_segment_cost(B_size, prefix, seen, last_seg, log_os);
        int pre_goal_size = pre_goal_segments.size();
        for (int i = pre_goal_size - 1; i >= 0; i--) {
            missing_cost += add_segment_cost(B_size, prefix, seen, pre_goal_segments[i], log_os);
        }
        return {rotate_cost, missing_cost, rotate_cost + missing_cost};
    }
   
    // 責務: pre_goal_segments の終了位置から goal_idx へ rb/rrb 両方で向かう cost の小さい方を返す。
    // 必要な理由: 後半 goal への greedy を missing 追加 cost 込みで評価するため。
    static int calc_with_goal(int B_size, const my_vector<int> &prefix, const my_vector<Segment> &pre_goal_segments,
                              int from_idx, int goal_idx, int pb_cost, bool start_rb, const char *kind,
                              int turn_idx, std::ostream *log_os) {
        Segment rb_seg{from_idx, goal_idx, true};
        Segment rrb_seg{from_idx, goal_idx, false};
        PathCostResult rb_result = calc_path_cost(B_size, prefix, pre_goal_segments, rb_seg, goal_idx, log_os);
        PathCostResult rrb_result = calc_path_cost(B_size, prefix, pre_goal_segments, rrb_seg, goal_idx, log_os);
        if (log_os != nullptr) {
            log_a2b_candidate(*log_os, start_rb, kind, turn_idx, pre_goal_segments, rb_seg, rb_result, pb_cost);
            log_a2b_candidate(*log_os, start_rb, kind, turn_idx, pre_goal_segments, rrb_seg, rrb_result, pb_cost);
        }
        return min(rb_result.total_cost, rrb_result.total_cost);
    }
    
    // 責務: start_dir で goal 以外の need idx をすべて見る最短前半 endpoint を返す。
    // 必要な理由: 前半で方向を変えない候補を O(1) 評価へ渡すため。
    static int no_turn_end(int B_size, const my_vector<int> &circular_next_need,
                           const my_vector<int> &circular_prev_need,
                           bool start_rb) {
        int res;
        if (start_rb) {
            res = circular_prev_need[B_size - 1];
        } else {
            if (B_size == 1) {
                res = circular_next_need[0];
            } else {
                res = circular_next_need[1];
            }
        }
        my_assert(res != -1);
        return res;
    }
    
    // 責務: turn_idx で方向変更した後、逆方向で見るべき need が残っているかを返す。
    // 必要な理由: 残り need が無い場合は turn_end を呼ばないようにするため。
    static bool has_after_turn_need(int B_size, const my_vector<int> &circular_next_need,
                                    const my_vector<int> &circular_prev_need, bool start_rb, int turn_idx) {
        if (start_rb) {
            if (turn_idx == B_size - 1) {
                return false;
            }
            int next_idx = circular_next_need[turn_idx + 1];
            my_assert(next_idx != -1);
            return next_idx > turn_idx;
        }
        if (turn_idx == 0) {
            return false;
        }
        int prev_idx = circular_prev_need[turn_idx - 1];
        my_assert(prev_idx != -1);
        return prev_idx < turn_idx;
    }

    // 責務: 0 から turn_idx まで start_dir で見た後、逆方向で残り need idx を見る endpoint を返す。
    // 必要な理由: 方向変更あり候補を、訪問した need_top_idx ごとに列挙するため。
    static int turn_end(int B_size, const my_vector<int> &circular_next_need,
                        const my_vector<int> &circular_prev_need,
                        bool start_rb, int turn_idx) {
        if (start_rb) {
            my_assert(turn_idx < B_size - 1);
            int next_idx = circular_next_need[turn_idx + 1];
            my_assert(next_idx != -1);
            my_assert(next_idx > turn_idx);
            return next_idx;
        }
        my_assert(turn_idx > 0);
        int prev_idx = circular_prev_need[turn_idx - 1];
        my_assert(prev_idx != -1);
        my_assert(prev_idx < turn_idx);
        (void) B_size;
        return prev_idx;
    }

    // 責務: start_rb 固定で、方向変更なしと 1 回変更の候補をすべて評価した最小 rotate cost を返す。
    // 必要な理由: rb 開始/rrb 開始を対称に試す公開関数の中身を分けるため。
    static int calc_start_dir(int B_size, const my_vector<int> &prefix, const my_vector<int> &need_top_idxs,
                              const my_vector<int> &circular_next_need, const my_vector<int> &circular_prev_need,
                              int goal_idx, bool start_rb, int pb_cost, std::ostream *log_os) {
        int best_cost = numeric_limits<int>::max();
        int end_idx = no_turn_end(B_size, circular_next_need, circular_prev_need, start_rb);
        my_vector<Segment> pre_goal_segments;
        pre_goal_segments.push_back({0, end_idx, start_rb});
        best_cost = min(best_cost, calc_with_goal(B_size, prefix, pre_goal_segments, end_idx, goal_idx,
                                                  pb_cost, start_rb, "no_turn", -1, log_os));
        for (int i = 0; i < need_top_idxs.size(); i++) {
            int turn_idx = need_top_idxs[i];
            if (!has_after_turn_need(B_size, circular_next_need, circular_prev_need, start_rb, turn_idx)) {
                continue;
            }
            my_vector<Segment> turn_pre_goal_segments;
            turn_pre_goal_segments.push_back({0, turn_idx, start_rb});
            int turn_end_idx = turn_end(B_size, circular_next_need, circular_prev_need, start_rb, turn_idx);
            turn_pre_goal_segments.push_back({turn_idx, turn_end_idx, !start_rb});
            best_cost = min(best_cost, calc_with_goal(B_size, prefix, turn_pre_goal_segments, turn_end_idx, goal_idx,
                                                      pb_cost, start_rb, "turn", turn_idx, log_os));
        }
        return best_cost;
    }
    
public:
    // 責務: A 全要素を pb する cost と、B 上の need_top_idxs 全訪問から goal へ行く最小 rotate cost を返す。
    // 必要な理由: B2AUnderMaxPredictor の結果から、現状態の A2B 下限寄り評価値を作るため。
    static int calc_a2b_top_max(int B_size, const RingBuffer500 &a, int max_b, const my_vector<int> &need_top_idxs,
                             const my_vector<int> &missing_cnt, int goal_idx, std::ostream *log_os = nullptr) {
        my_assert(B_size > 0);
        my_assert(0 <= goal_idx && goal_idx < B_size);
        my_assert(missing_cnt.size() == B_size);
        int missing_sum = calc_missing_sum(B_size, missing_cnt);
        if (log_os != nullptr) {
            log_a2b_start(*log_os, B_size, a, max_b, need_top_idxs, missing_cnt, goal_idx, missing_sum);
        }
        if (missing_sum == 0) {
            my_assert(need_top_idxs.size() == 1);
            my_assert(need_top_idxs[0] == goal_idx);
            debug_assert_all_a_over_max_b(a, max_b);
            int goal_rot = calc_goal_rot(B_size, goal_idx);
            int total_cost = a.size() + goal_rot;
            if (log_os != nullptr) {
                log_a2b_special(*log_os, "missing_sum_zero", a.size(), goal_rot, total_cost);
            }
            return total_cost;
        }
        my_vector<int> prefix = build_prefix(B_size, missing_cnt);
        my_vector<int> need_bits = build_need_bits(B_size, need_top_idxs, goal_idx);
        if (!has_non_goal_need(need_bits)) {
            int goal_rot = calc_goal_rot(B_size, goal_idx);
            int total_cost = a.size() + goal_rot;
            if (log_os != nullptr) {
                log_a2b_special(*log_os, "goal_only", a.size(), goal_rot, total_cost);
            }
            return total_cost;
        }
        my_vector<int> circular_next_need = build_circular_next_need(B_size, need_bits);
        my_vector<int> circular_prev_need = build_circular_prev_need(B_size, need_bits);
        int pb_cost = a.size();
        int rotate_cost = min(calc_start_dir(B_size, prefix, need_top_idxs, circular_next_need, circular_prev_need,
                                             goal_idx, true, pb_cost, log_os),
                              calc_start_dir(B_size, prefix, need_top_idxs, circular_next_need, circular_prev_need,
                                             goal_idx, false, pb_cost, log_os));
        int total_cost = pb_cost + rotate_cost;
        if (log_os != nullptr) {
            log_a2b_best(*log_os, pb_cost, rotate_cost, total_cost);
        }
        return total_cost;
    }
};

#endif //UNTITLED_A2BUNDERMAXPREDICTOR_H
