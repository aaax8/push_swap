#ifndef UNTITLED_B2AUNDERMAXPREDICTOR_H
#define UNTITLED_B2AUNDERMAXPREDICTOR_H

#include "all_include.h"
#include "RingBuffer500.h"
#include "util.h"

template<size_t MAX_N>
class B2AUnderMaxPredictor {
public:
    // 責務: B2A under max predictor の公開結果として、推定 cost と missing 挿入のために top にする idx 集合を保持する。
    // 必要な理由: calc_under_maxb_b2a_turn が従来の cost に加えて、後続 A2B predict 用の top idx も返すため。
    struct B2AUnderResult {
        int cost;
        my_vector<int> need_top_idxs;
        int max_b_idx;
        my_vector<int> missing_cnt;
    };
    // 責務: B2A rotate cost と、後続 A2B predictor が使う top idx/missing 数を保持する。
    // 必要な理由: cost に加えて max_b_idx と missing_cnt を calc_under_maxb_b2a_turn へ渡すため。
    struct RotateCostResult {
        int cost;
        my_vector<int> need_top_idxs;
        my_vector<int> missing_cnt;
    };
  
private:
    // 責務: 文脈ごとの cost と、その cost を実現する元 B の idx を保持する。
    // 必要な理由: min_visit_cnt 系と rotate cost 系の候補選択で、cost と missing 挿入位置を同時に選ぶため。
    struct MissingPlace {
        int cost;
        int idx;
    };

    //max_bいかの物だけのビルド
    static my_vector<int>
    build_kari_build_v2pos(int N, int max_b, const RingBuffer500 &B, const my_vector<int> &desc_next_vals,
                           const my_vector<int> &cur_v2pos) {
        // 責務: 0..max_b を仮構築したときの値ごとの位置を返す。
        // 必要な理由: 0 が実値になったため、下端番兵に 0 ではなく -1 を使うため。
        bool was_last = false;
        int insert_prev_cnt = 0;
        my_vector<int> kari_build_v2pos(N, -1);
        for (int i = 0; i < B.size(); i++) {
            int bv = B[i];
            kari_build_v2pos[bv] = i + insert_prev_cnt;
            int next_v = desc_next_vals[bv];
            if (next_v == -1) {
                my_assert(!was_last);
                was_last = true;
            }
            insert_prev_cnt += bv - (next_v + 1);
        }
        return kari_build_v2pos;
    }

    static my_vector<int> build_cur_v2pos(const RingBuffer500 &b, int max_b) {
        // 責務: 現在の B について、値ごとの位置を返す。
        // 必要な理由: max_b=0 と値 0 を通常ケースとして扱うため。
        my_assert(max_b >= 0);
        my_vector<int> cur_v2pos(max_b + 1, -1);
        for (int i = 0; i < b.size(); i++) {
            const int v = b[i];
            my_assert(0 <= v && v <= max_b);
            my_assert(cur_v2pos[v] == -1);
            cur_v2pos[v] = i;
        }
        return cur_v2pos;
    }

    //降順で次の値を持つ、　値が無い場合は-1
    static my_vector<int> build_desc_next_vals(int N, const my_vector<int> &cur_v2pos, int max_b) {
        // 責務: 0..max_b の存在値について、降順で次に存在する値を返す。
        // 必要な理由: 値 0 を通常値として走査対象に含めるため。
        my_vector<int> desc_next_vals(N, -1);
        int prev_v = -1;
        for (int v = max_b; v >= 0; v--) {
            if (cur_v2pos[v] == -1) {
                continue;
            }
            if (prev_v != -1) {
                desc_next_vals[prev_v] = v;
            }
            prev_v = v;
        }
        return desc_next_vals;
    }

    // 責務: 円環上の [l, r) に相当する範囲の bit 数を返す。
    // 必要な理由: build_is_next_rb がインスタンスを持たずに範囲数を計算するため。
    static int popcnt_circular(const my_bitset<MAX_N> &bset, int size, int l, int r) {
        my_assert(l != r);
        my_assert(0 <= l && l < size);
        my_assert(0 <= r && r <= size);
        assert(l != r);
        assert(0 <= l && l < size);
        assert(0 <= r && r <= size);
        if (l < r) {
            return pop_cnt(bset, l, r);
        } else {
            return pop_cnt(bset, l, size) + pop_cnt(bset, 0, r);
        }
    }

    static my_bitset<MAX_N> build_mask_bitset(int range) {
        my_bitset<MAX_N> bset;
        if (range == 0) {
            return bset;
        }
        my_assert(0 < range && range <= MAX_N);
        bset.set();
        bset >>= (MAX_N - range);
        return bset;
    }

    // 責務: bitset の 0..size-1 範囲を label=[0,1,...] 形式で出力する。
    // 必要な理由: build_is_next_rb と calc_under_maxb_b2a_turn のログで exist_pos と is_next_rb を同じ形式で確認するため。
    static void write_bitset_debug(std::ostream &os, const char *label, const my_bitset<MAX_N> &bset, int size) {
        os << label << "=[";
        for (int i = 0; i < size; i++) {
            if (i > 0) {
                os << ",";
            }
            os << (bset[i] ? 1 : 0);
        }
        os << "]";
    }

    // 責務: my_vector<int> を label=[v0,v1,...] 形式で出力する。
    // 必要な理由: cur_v2pos, desc_next_vals, kari_build_v2pos の中身を同じ形式で確認するため。
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

    // 責務: RingBuffer500 を label=[v0,v1,...] 形式で出力する。
    // 必要な理由: 中間 vector がどの B 入力から作られたかをログ上で確認するため。
    static void write_ring_buffer_debug(std::ostream &os, const char *label, const RingBuffer500 &values) {
        os << label << "=[";
        for (int i = 0; i < values.size(); i++) {
            if (i > 0) {
                os << ",";
            }
            os << values[i];
        }
        os << "]";
    }

    // 責務: build_is_next_rb の 1 loop 分の状態をログへ出力する。
    // 必要な理由: build_is_next_rb の回転方向判定ログを関数化し、計算処理と出力処理を分けるため。
    static void log_next_rb_loop(std::ostream &os, int N, int f_v, int t_v, int f_i, int t_i,
                                 int rb_dis, int rrb_dis, const my_bitset<MAX_N> &exist_pos,
                                 const my_bitset<MAX_N> &is_next_rb) {
        os << "build_is_next_rb"
           << " f_v=" << f_v
           << " t_v=" << t_v
           << " f_i=" << f_i
           << " t_i=" << t_i
           << " rb_dis=" << rb_dis
           << " rrb_dis=" << rrb_dis
           << " ";
        write_bitset_debug(os, "exist_pos", exist_pos, N);
        os << " ";
        write_bitset_debug(os, "is_next_rb", is_next_rb, N);
        os << "\n";
        os.flush();
    }

    // 責務: calc_rotate_cost の 1 loop 分の rotate cost 候補と状態をログへ出力する。
    // 必要な理由: rb/rrb 分岐で重複しているログ出力を共通化し、計算処理と出力処理を分けるため。
    static void log_rotate_loop(std::ostream &os, int BN, const char *dir, int f_v, int t_v, int f_i, int t_i,
                                bool next_rb, int top_rotcnt, int mid_rotcnt, int btm_rotcnt,
                                int chosen_insert_idx,
                                int min_rotate_cost, int rotate_cost_sum, const my_vector<int> &visit_cnt,
                                const my_bitset<MAX_N> &exist_pos) {
        os << "calc_rotate_cost_loop"
           << "\n  dir=" << dir
           << "\n  vals f_v=" << f_v << " t_v=" << t_v
           << "\n  idx f_i=" << f_i << " t_i=" << t_i
           << "\n  is_next_rb=" << (next_rb ? 1 : 0)
           << "\n  " << dir << "_top_outer_rotcnt=" << top_rotcnt
           << "\n  " << dir << "_mid_rotcnt=" << mid_rotcnt
           << "\n  " << dir << "_btm_outer_rotcnt=" << btm_rotcnt
           << "\n  chosen_insert_idx=" << chosen_insert_idx
           << "\n  min_rotate_cost=" << min_rotate_cost
           << "\n  rotate_cost_sum_before=" << rotate_cost_sum
           << "\n  ";
        write_int_vector_debug(os, "visit_cnt", visit_cnt);
        os << "\n  ";
        write_bitset_debug(os, "exist_pos", exist_pos, BN);
        os << "\n";
        os.flush();
    }

    // 責務: 値 0 より上側に残る missing 群の追加 cost をログへ出力する。
    // 必要な理由: 1-based の cost_to_1 表現をやめ、0-based の下端 missing として確認するため。
    static void log_cost_to_min(std::ostream &os, int f_v, int cost_rot_to_min, const my_vector<int> &visit_cnt) {
        os << "calc_rotate_cost_to_min"
           << "\n  f_v=" << f_v
           << "\n  cost_rot_to_min=" << cost_rot_to_min
           << "\n  ";
        write_int_vector_debug(os, "visit_cnt", visit_cnt);
        os << "\n";
        os.flush();
    }

    // 責務: calc_rotate_cost の終了時状態をログへ出力する。
    // 必要な理由: calc_rotate_cost の最終合計と状態ログを関数化し、計算処理と出力処理を分けるため。
    static void log_rotate_end(std::ostream &os, int BN, int f_v, int rotate_cost_sum,
                               const my_vector<int> &visit_cnt, const my_bitset<MAX_N> &exist_pos) {
        os << "calc_rotate_cost_end"
           << "\n  f_v=" << f_v
           << "\n  rotate_cost_sum=" << rotate_cost_sum
           << "\n  ";
        write_int_vector_debug(os, "visit_cnt", visit_cnt);
        os << "\n  ";
        write_bitset_debug(os, "exist_pos", exist_pos, BN);
        os << "\n";
        os.flush();
    }

    // 責務: calc_under_maxb_b2a_turn の開始時入力と初期 exist_pos をログへ出力する。
    // 必要な理由: predictor の入力状態ログを関数化し、計算処理と出力処理を分けるため。
    static void log_predictor_start(std::ostream &os, int N, const RingBuffer500 &b,
                                    const my_bitset<MAX_N> &exist_pos) {
        write_ring_buffer_debug(os, "b", b);
        os << "\n";
        os << "calc_under_maxb_b2a_turn ";
        write_bitset_debug(os, "initial_exist_pos", exist_pos, N);
        os << "\n";
        os.flush();
    }

    // 責務: calc_under_maxb_b2a_turn で作った中間 vector をログへ出力する。
    // 必要な理由: predictor の中間データログを関数化し、計算処理と出力処理を分けるため。
    static void log_predictor_vectors(std::ostream &os, const my_vector<int> &cur_v2pos,
                                      const my_vector<int> &desc_next_vals,
                                      const my_vector<int> &kari_build_v2pos) {
        write_int_vector_debug(os, "cur_v2pos", cur_v2pos);
        os << "\n";
        write_int_vector_debug(os, "desc_next_vals", desc_next_vals);
        os << "\n";
        write_int_vector_debug(os, "kari_build_v2pos", kari_build_v2pos);
        os << "\n";
        os.flush();
    }

    // 責務: サイズ size の円環上の [l, r) に相当する bit 範囲を value に設定する。
    // 必要な理由: build_is_next_rb で通過済み範囲を 1 要素ずつではなく bitset 演算で消すため。
    static void set_circular(my_bitset<MAX_N> &bset, int size, int l, int r, bool value) {
        //空の区間とする
        if (l == r) {
            return;
        }
        my_assert(0 < size && size <= MAX_N);
        my_assert(0 <= l && l < size);
        my_assert(0 <= r && r <= size);
        assert(0 < size && size <= MAX_N);
        assert(l != r);
        assert(0 <= l && l < size);
        assert(0 <= r && r <= size);

        my_bitset<MAX_N> mask;
        if (l < r) {
            mask = build_mask_bitset(r - l) << l;
        } else {
            mask = (build_mask_bitset(size - l) << l) | build_mask_bitset(r);
        }

        if (value) {
            bset |= mask;
        } else {
            bset &= ~mask;
        }
    }

    static int normalize_i(int n, int i) {
        if (i >= n) {
            i -= n;
            my_assert(0 <= i && i < n);
            return i;
        }
        return i;
    }

    //各vの、次の値への回転がrbかどうかを、仮のビルドから作る
    static my_bitset<MAX_N>
    build_is_next_rb(int N, int max_b, const RingBuffer500 &b, const my_vector<int> &desc_next_vals,
                     const my_vector<int> &kari_build_v2pos
                     , std::ostream *log_os
                     ) {
        int bn = b.size();
        my_bitset<MAX_N> is_next_rb;
        // 責務: 仮構築後の 0..max_b の位置集合を作る。
        // 必要な理由: under max 対象の個数が max_b ではなく max_b+1 になるため。
        const int build_size = max_b + 1;
        my_bitset<MAX_N> exist_pos = build_mask_bitset(build_size);
        int f_v = max_b;
        exist_pos[kari_build_v2pos[f_v]] = 0;
        while (true) {
            int t_v = desc_next_vals[f_v];
            if (t_v == -1) {
                break;
            }
            int f_i = kari_build_v2pos[f_v];
            int t_i = kari_build_v2pos[t_v];
            int missing_cnt = calc_missing_cnt(f_v, t_v);
            //exist_posの [f_i+1, t_i)のカウントをする
            //f_iはfalseなのでok
            //f_vの直後にf_v-1~t_v+1があるので、その分減る
            int rb_dis = popcnt_circular(exist_pos, build_size, f_i, t_i) - missing_cnt;
            my_assert(rb_dis >= 0);
            //[t_i, f_i) のカウントをする
            int rrb_dis = popcnt_circular(exist_pos, build_size, t_i, f_i);
            my_assert(exist_pos[f_i] == 0);
            my_assert(exist_pos[t_i]);
            //この二つの関数を作る
            if (rb_dis <= rrb_dis) {
                is_next_rb[f_v] = true;
            } else {
                my_assert(!is_next_rb[f_v]);
            }
            if (log_os != nullptr) {
                log_next_rb_loop(*log_os, build_size, f_v, t_v, f_i, t_i, rb_dis, rrb_dis, exist_pos, is_next_rb);
            }
            exist_pos[t_i] = 0;
            if (missing_cnt) {
                set_circular(exist_pos, build_size, next_i(build_size, f_i),
                             normalize_i(build_size, f_i + 1 + missing_cnt), false);
            }
            f_v = t_v;
        }
        return is_next_rb;
    }


    // 責務: サイズ BN の円環で i の 1 つ前の index を返す。
    // 必要な理由: top_outer_min_place がインスタンスなしで前方向へ走査するため。
    static int prev_i(int BN, int i) {
        if (i == 0) {
            return BN - 1;
        } else {
            return i - 1;
        }
    }

    // 責務: サイズ BN の円環で i の 1 つ後の index を返す。
    // 必要な理由: 複数の static helper がインスタンスなしで円環を走査するため。
    static int next_i(int BN, int i) {
        if (i == BN - 1) {
            return 0;
        } else {
            return i + 1;
        }
    }

    //x
    //iの外側の内一番少ないvisit_cnt
    // 責務: top outer 領域で最初に見つかる最小 visit_cnt と、その idx を返す。
    // 必要な理由: top outer 候補を選んだとき、missing 群を元 B のどの idx 直後へ置くか記録するため。
    static MissingPlace top_outer_min_place(int i, const my_vector<int> &visit_cnt, int f_v, int t_v, const RingBuffer500 &B) {
        my_assert(B[i] == f_v || B[i] == t_v);
        int BN = B.size();
        int j = prev_i(BN, i);
        int min_visit_cnt = numeric_limits<int>::max();
        int min_idx = -1;
        while (true) {
            //vより大きい値はもう消えてる
            if (visit_cnt[j] < min_visit_cnt) {
                min_visit_cnt = visit_cnt[j];
                min_idx = j;
            }
            if(B[j] < f_v || B[j] == t_v || B[j] == f_v){
                break;
            }
            j = prev_i(BN, j);
        }
        return {min_visit_cnt, min_idx};
    }

    //x
    // 責務: bottom outer 領域で最初に見つかる最小 visit_cnt と、その idx を返す。
    // 必要な理由: bottom outer 候補を選んだとき、missing 群を元 B のどの idx 直後へ置くか記録するため。
    static MissingPlace btm_outer_min_place(int i, const my_vector<int> &visit_cnt, int f_v, int t_v, const RingBuffer500 &B) {
        my_assert(B[i] == f_v || B[i] == t_v);
        int BN = B.size();
        int j = i;
        int min_visit_cnt = numeric_limits<int>::max();
        int min_idx = -1;
        while (true) {
            //vより大きい値はもう消えてる
            if (visit_cnt[j] < min_visit_cnt) {
                min_visit_cnt = visit_cnt[j];
                min_idx = j;
            }
            j = next_i(BN, j);
            if(B[j] < f_v || B[j] == t_v || B[j] == f_v){
                break;
            }
        }
        return {min_visit_cnt, min_idx};
    }

    static int calc_missing_cnt(int f_v, int t_v) {
        my_assert(f_v > t_v);
        return (f_v - t_v) - 1;
    }

    //x
    //(f_i, t_i) rb方向で、間にある最小
    // 責務: rb 方向の mid 領域で最初に見つかる最小 visit_cnt と、その idx を返す。
    // 必要な理由: mid 候補を選んだとき、missing 群を元 B のどの idx 直後へ置くか記録するため。
    static MissingPlace mid_min_place(int BN, int f_i, int t_i, const my_vector<int> &visit_cnt) {
        int min_visit_cnt = numeric_limits<int>::max();
        int i = f_i;
        int min_idx = -1;
        while (i != t_i) {
            if (visit_cnt[i] < min_visit_cnt) {
                min_visit_cnt = visit_cnt[i];
                min_idx = i;
            }
            i = next_i(BN, i);
        }
        return {min_visit_cnt, min_idx};
    }

    //x
    //(f_i, t_i)の残ってるものの数
    static int calc_rb_mid_cnt(int BN, int f_i, int t_i, const RingBuffer500 &B, my_bitset<MAX_N> &exist_pos) {
        my_assert(f_i != t_i);
        if (f_i < t_i) {
            return pop_cnt(exist_pos, f_i + 1, t_i);
        } else {
            //next_iは使わなくていい f_i=max-1のときに、next_iを使うとまずい
            return pop_cnt(exist_pos, f_i + 1, BN) + pop_cnt(exist_pos, 0, t_i);
        }
    }

    //x
    static int calc_rrb_mid_cnt(int BN, int f_i, int t_i, const RingBuffer500 &B, my_bitset<MAX_N> &exist_pos) {
        my_assert(f_i != t_i);
        if (t_i < f_i) {
            return pop_cnt(exist_pos, next_i(BN, t_i), f_i);
        } else {
            //next_iを使うとまずい
            return pop_cnt(exist_pos, 0, f_i) + pop_cnt(exist_pos, t_i + 1, BN);
        }
    }

    //x
    //<f-1~t+1> t, ., ., ,...., f
    // 責務: rrb top outer 候補の rotate cost と missing 挿入 idx を返す。
    // 必要な理由: 3 候補から最小 cost を選ぶとき、対応する元 B idx も一緒に選ぶため。
    static MissingPlace calc_rrb_top_outer_place(int BN, int f_v, int t_v, int f_i, int t_i, const RingBuffer500 &B,
                                                 my_bitset<MAX_N> &exist_pos,
                                                 const my_vector<int> &visit_cnt
    ) {
        my_assert(f_v > t_v);
        my_assert(B[f_i] == f_v);
        my_assert(B[t_i] == t_v);
        int missing_cnt = calc_missing_cnt(f_v, t_v);
        if(missing_cnt==0){
            return {inf, -1};
        }
        int mid_cnt = calc_rrb_mid_cnt(BN, f_i, t_i, B, exist_pos);
        MissingPlace min_place = top_outer_min_place(t_i, visit_cnt, f_v, t_v, B);
        //tへ移動する分で+1
        return {mid_cnt + 1 + min_place.cost * missing_cnt + missing_cnt, min_place.idx};
    }

    //x
    // 責務: rrb mid 候補の rotate cost と missing 挿入 idx を返す。
    // 必要な理由: 3 候補から最小 cost を選ぶとき、対応する元 B idx も一緒に選ぶため。
    static MissingPlace calc_rrb_mid_place(int BN, int f_v, int t_v, int f_i, int t_i, const RingBuffer500 &B,
                                           my_bitset<MAX_N> &exist_pos,
                                           const my_vector<int> &visit_cnt
    ) {
        my_assert(B[f_i] == f_v);
        my_assert(B[t_i] == t_v);
        int mid_cnt = calc_rrb_mid_cnt(BN, f_i, t_i, B, exist_pos);
        MissingPlace min_place = mid_min_place(BN, f_i, t_i, visit_cnt);
        int missing_cnt = calc_missing_cnt(f_v, t_v);
        if (missing_cnt == 0) {
            return {mid_cnt + 1, -1};
        }
        return {mid_cnt + 1 + min_place.cost * missing_cnt + missing_cnt, min_place.idx};
    }

    //x
    //t, ., ., ,...., f, <f-1~t+1>
    // 責務: rrb bottom outer 候補の rotate cost と missing 挿入 idx を返す。
    // 必要な理由: 3 候補から最小 cost を選ぶとき、対応する元 B idx も一緒に選ぶため。
    static MissingPlace calc_rrb_btm_outer_place(int BN, int f_v, int t_v, int f_i, int t_i, const RingBuffer500 &B,
                                                 my_bitset<MAX_N> &exist_pos,
                                                 const my_vector<int> &visit_cnt
    ) {
        my_assert(B[f_i] == f_v);
        my_assert(B[t_i] == t_v);
        int missing_cnt = calc_missing_cnt(f_v, t_v);
        if(missing_cnt==0){
            return {inf, -1};
        }
        int mid_cnt = calc_rrb_mid_cnt(BN, f_i, t_i, B, exist_pos);
        MissingPlace min_place = btm_outer_min_place(f_i, visit_cnt, f_v, t_v, B);
        return {mid_cnt + 1 + min_place.cost * missing_cnt, min_place.idx};
    }

    //x
    //<f-1~t+1> f, ., ., ,...., t,
    // 責務: rb top outer 候補の rotate cost と missing 挿入 idx を返す。
    // 必要な理由: 3 候補から最小 cost を選ぶとき、対応する元 B idx も一緒に選ぶため。
    static MissingPlace calc_rb_top_outer_place(int BN, int f_v, int t_v, int f_i, int t_i, const RingBuffer500 &B,
                                                my_bitset<MAX_N> &exist_pos, my_vector<int> &visit_cnt) {
        my_assert(B[f_i] == f_v);
        my_assert(B[t_i] == t_v);
        //f-t間の数
        //f_i+1 ~ t
        int missing_cnt = calc_missing_cnt(f_v, t_v);
        if(missing_cnt==0){
            return {inf, -1};
        }
        int mid_cnt = calc_rb_mid_cnt(BN, f_i, t_i, B, exist_pos);
        MissingPlace min_place = top_outer_min_place(f_i, visit_cnt, f_v, t_v, B);
        return {mid_cnt + min_place.cost * missing_cnt + missing_cnt, min_place.idx};
    }

    //x
    // 責務: rb mid 候補の rotate cost と missing 挿入 idx を返す。
    // 必要な理由: 3 候補から最小 cost を選ぶとき、対応する元 B idx も一緒に選ぶため。
    static MissingPlace calc_rb_mid_place(int BN, int f_v, int t_v, int f_i, int t_i, const RingBuffer500 &B,
                                          my_bitset<MAX_N> &exist_pos, const my_vector<int> &visit_cnt) {
        my_assert(B[f_i] == f_v);
        my_assert(B[t_i] == t_v);
        //f-t間の数
        //f_i+1 ~ t
        int mid_cnt = calc_rb_mid_cnt(BN, f_i, t_i, B, exist_pos);
        MissingPlace min_place = mid_min_place(BN, f_i, t_i, visit_cnt);
        int missing_cnt = calc_missing_cnt(f_v, t_v);
        if (missing_cnt == 0) {
            return {mid_cnt, -1};
        }
        return {mid_cnt + min_place.cost * missing_cnt, min_place.idx};
    }

    static const int inf = 1<<30;

    //x
    //f, ., ., ,...., t,<f-1~t+1>
    // 責務: rb bottom outer 候補の rotate cost と missing 挿入 idx を返す。
    // 必要な理由: 3 候補から最小 cost を選ぶとき、対応する元 B idx も一緒に選ぶため。
    static MissingPlace calc_rb_btm_outer_place(int BN, int f_v, int t_v, int f_i, int t_i, const RingBuffer500 &B,
                                                my_bitset<MAX_N> &exist_pos,
                                                const my_vector<int> &visit_cnt,
                                                const my_vector<int> &desc_next_vals
                                        ) {
        my_assert(B[f_i] == f_v);
        my_assert(B[t_i] == t_v);
        int missing_cnt = calc_missing_cnt(f_v, t_v);
        if(missing_cnt == 0){
            return {inf, -1};
        }
        int mid_cnt = calc_rb_mid_cnt(BN, f_i, t_i, B, exist_pos);
        MissingPlace min_place = btm_outer_min_place(t_i, visit_cnt, f_v, t_v, B);
        int prev_visit_cnt = missing_cnt * min_place.cost;
        if (desc_next_vals[t_v] != -1) {
            //tからf-1へいく+1と
            //f-1~t+1を全部paしたあとの、tへもどる+1
            return {mid_cnt + prev_visit_cnt + 2, min_place.idx};
        } else {
            //f-1~t+1をpa したら、tがtopにくる
            return {mid_cnt + prev_visit_cnt + 1, min_place.idx};
        }
    }

    // 責務: top, mid, bottom の順で tie を保ちながら、最小 cost の MissingPlace を返す。
    // 必要な理由: 既存の min(top, min(mid, btm)) と同じ優先順で cost と idx を同時に選ぶため。
    static MissingPlace choose_missing_place(const MissingPlace &top_place, const MissingPlace &mid_place,
                                             const MissingPlace &btm_place) {
        MissingPlace best_place = top_place;
        if (mid_place.cost < best_place.cost) {
            best_place = mid_place;
        }
        if (btm_place.cost < best_place.cost) {
            best_place = btm_place;
        }
        return best_place;
    }

    // 責務: visit_cnt 全体で最初に見つかる最小 cost と、その idx を返す。
    // 必要な理由: 値 0 より上側に残る最後の missing 群についても、元 B のどの idx 直後へ置くか記録するため。
    static MissingPlace all_min_place(const my_vector<int> &visit_cnt, int BN) {
        int min_visit_cnt = numeric_limits<int>::max();
        int min_idx = -1;
        for (int i = 0; i < BN; i++) {
            if (visit_cnt[i] < min_visit_cnt) {
                min_visit_cnt = visit_cnt[i];
                min_idx = i;
            }
        }
        return {min_visit_cnt, min_idx};
    }

    // 責務: need_top_idx_bits の true 位置を昇順走査順で my_vector<int> に詰める。
    // 必要な理由: max_b_idx を O(1) で追加し、missing_cnt と同じ idx 意味で A2B 側へ渡すため。
    static my_vector<int> build_need_top_idxs(const my_bitset<MAX_N> &need_top_idx_bits, int BN) {
        my_vector<int> need_top_idxs;
        for (int i = 0; i < BN; i++) {
            if (need_top_idx_bits[i]) {
                need_top_idxs.push_back(i);
            }
        }
        return need_top_idxs;
    }
    
    //x
    static void update_visit_cnt(my_vector<int> &visit_cnt, int BN, int f_i, int t_i, bool is_rb) {
        int i;
        int goal_i;
        if (is_rb) {
            i = f_i;
            goal_i = t_i;
        } else {
            i = t_i;
            goal_i = f_i;
        }
        while (i != goal_i) {
            visit_cnt[i]++;
            i = next_i(BN, i);
        }
    }

    // 責務: max_b から降順の次値へ進む各区間の推定 rotate cost と missing 挿入 idx 列を返す。
    // 必要な理由: calc_under_maxb_b2a_turn が B2A 推定 cost と後続 A2B predict 用の挿入場所を作るため。
    //x
    // 責務: max_b から降順の次値へ進む各区間の推定 rotate cost と A2B predictor 用情報を返す。
    // 必要な理由: calc_under_maxb_b2a_turn が B2A 推定 cost、need_top_idxs、missing_cnt を同時に返すため。
    static RotateCostResult calc_rotate_cost(int BN, const RingBuffer500 &B, int max_b,
                                                           const my_vector<int> &desc_next_vals,
                                                           const my_bitset<MAX_N> &is_next_rb,
                                                           const my_vector<int> &cur_v2pos,
                                                           my_bitset<MAX_N> exist_pos, std::ostream *log_os
    ) {
        //[i] := これまでにi-(i+1)間を訪れた回数
        my_vector<int> visit_cnt(BN);
        my_bitset<MAX_N> need_top_idx_bits;
        my_vector<int> missing_cnt(BN);
        int f_v = max_b;
        int rotate_cost_sum = 0;
        need_top_idx_bits[cur_v2pos[max_b]] = true;
        exist_pos[cur_v2pos[max_b]] = 0;
        while (true) {
            int t_v = desc_next_vals[f_v];
            if (t_v == -1) {
                break;
            }
            int cur_missing_cnt = calc_missing_cnt(f_v, t_v);
            int f_i = cur_v2pos[f_v];
            int t_i = cur_v2pos[t_v];
            int min_rotate_cost = numeric_limits<int>::max();
            MissingPlace best_place;
            if (is_next_rb[f_v]) {
                MissingPlace rb_top_outer_place = calc_rb_top_outer_place(BN, f_v, t_v, f_i, t_i, B, exist_pos, visit_cnt);
                MissingPlace rb_mid_place = calc_rb_mid_place(BN, f_v, t_v, f_i, t_i, B, exist_pos, visit_cnt);
                MissingPlace rb_btm_outer_place = calc_rb_btm_outer_place(BN, f_v, t_v, f_i, t_i, B, exist_pos, visit_cnt, desc_next_vals);
                best_place = choose_missing_place(rb_top_outer_place, rb_mid_place, rb_btm_outer_place);
                min_rotate_cost = best_place.cost;
                if (log_os != nullptr) {
                    log_rotate_loop(*log_os, BN, "rb", f_v, t_v, f_i, t_i, is_next_rb[f_v],
                                    rb_top_outer_place.cost, rb_mid_place.cost, rb_btm_outer_place.cost,
                                    best_place.idx,
                                    min_rotate_cost, rotate_cost_sum, visit_cnt, exist_pos);
                }
            } else {
                MissingPlace rrb_top_outer_place = calc_rrb_top_outer_place(BN, f_v, t_v, f_i, t_i, B, exist_pos, visit_cnt);
                MissingPlace rrb_mid_place = calc_rrb_mid_place(BN, f_v, t_v, f_i, t_i, B, exist_pos, visit_cnt);
                MissingPlace rrb_btm_outer_place = calc_rrb_btm_outer_place(BN, f_v, t_v, f_i, t_i, B, exist_pos, visit_cnt);
                best_place = choose_missing_place(rrb_top_outer_place, rrb_mid_place, rrb_btm_outer_place);
                min_rotate_cost = best_place.cost;
                if (log_os != nullptr) {
                    log_rotate_loop(*log_os, BN, "rrb", f_v, t_v, f_i, t_i, is_next_rb[f_v],
                                    rrb_top_outer_place.cost, rrb_mid_place.cost, rrb_btm_outer_place.cost,
                                    best_place.idx,
                                    min_rotate_cost, rotate_cost_sum, visit_cnt, exist_pos);
                }
            }
            if (best_place.idx != -1) {
                int need_top_idx = next_i(BN, best_place.idx);
                need_top_idx_bits[need_top_idx] = true;
                missing_cnt[need_top_idx] += cur_missing_cnt;
            }
            rotate_cost_sum += min_rotate_cost;
            exist_pos[t_i] = 0;
            update_visit_cnt(visit_cnt, BN, f_i, t_i, is_next_rb[f_v]);
            f_v = t_v;
        }
        if(f_v > 0){
            int last_missing_cnt = calc_missing_cnt(f_v, -1);
            MissingPlace min_place = all_min_place(visit_cnt, BN);
            int cost_rot_to_min = last_missing_cnt * min_place.cost;
            if (min_place.idx != -1) {
                int need_top_idx = next_i(BN, min_place.idx);
                need_top_idx_bits[need_top_idx] = true;
                missing_cnt[need_top_idx] += last_missing_cnt;
            }
            if (log_os != nullptr) {
                log_cost_to_min(*log_os, f_v, cost_rot_to_min, visit_cnt);
            }
            rotate_cost_sum += cost_rot_to_min;
        }
        if (log_os != nullptr) {
            log_rotate_end(*log_os, BN, f_v, rotate_cost_sum, visit_cnt, exist_pos);
        }
        return {rotate_cost_sum, build_need_top_idxs(need_top_idx_bits, BN), std::move(missing_cnt)};
    }

    // 責務: B の最大値を top にした状態から max_b..0 を A へ戻す推定 cost と missing 挿入 idx 集合を返す。
    // 必要な理由: A2B 初期化 beam に入れる前に、B->A 推定 cost と後続 A2B predict 用の挿入場所を独立して検証するため。
    //x
public:
    static B2AUnderResult calc_under_maxb_b2a_turn(int N, int max_b, const RingBuffer500 &b, std::ostream *log_os = nullptr) {
        my_assert(!b.empty());
        assert(!b.empty());
        my_bitset<MAX_N> exist_pos = build_mask_bitset(b.size());
        my_assert(0 <= max_b && max_b < N);
        if (log_os != nullptr) {
            log_predictor_start(*log_os, N, b, exist_pos);
        }
        const int b_size = b.size();
        //[v] := Bのvのpos
        my_vector<int> cur_v2pos = build_cur_v2pos(b, max_b);
        my_vector<int> desc_next_vals = build_desc_next_vals(N, cur_v2pos, max_b);

        //[v] := 仮でBの直後に構築した場合のpos
        my_vector<int> kari_build_v2pos = build_kari_build_v2pos(N, max_b, b, desc_next_vals, cur_v2pos);

        if (log_os != nullptr) {
            log_predictor_vectors(*log_os, cur_v2pos, desc_next_vals, kari_build_v2pos);
        }
        my_bitset<MAX_N> is_next_rb = build_is_next_rb(N, max_b, b, desc_next_vals, kari_build_v2pos, log_os);
        auto rotate_result = calc_rotate_cost(b_size, b, max_b, desc_next_vals, is_next_rb, cur_v2pos, exist_pos, log_os);
        return {rotate_result.cost + max_b + 1, std::move(rotate_result.need_top_idxs),
                cur_v2pos[max_b], std::move(rotate_result.missing_cnt)};
    }

    static B2AUnderResult calc_under_maxb_b2a_turn(int N, const RingBuffer500 &b, std::ostream *log_os = nullptr) {
        my_assert(!b.empty());
        assert(!b.empty());
        int max_b = b[0];
        for (int i = 1; i < b.size(); i++) {
            max_b = max(max_b, b[i]);
        }
        return calc_under_maxb_b2a_turn(N, max_b, b, log_os);
    }
};

#endif //UNTITLED_B2AUNDERMAXPREDICTOR_H
