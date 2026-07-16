//
//

#ifndef UNTITLED_STATEHASH_H
#define UNTITLED_STATEHASH_H

#include "all_include.h"
#include "State.h"
#include "util.h"
#include "RingBuffer500.h"


/*@formatter:off*/
//deque_hash
//hashを共有したくない場合、中に書く
//const uint64_t base = 61520083;
//        (uint64_t) my_rand() * my_rand();
//my_rand((uint64_t) 868491463, ((uint64_t) 1 << 61) - 1 - 1);
//安全で爆速なrollinghash
extern const uint64_t base;

using bint =long long;

//拡張ユークリッド互除法
bint ext_gcd(bint a, bint b, bint &x, bint &y);

//aとmは互いに素, a^(-1) mod m
uint64_t modinv(bint a, bint m);


//あり得る最大の引数よりも大きい数
extern const int HIGHER_MAX_ARG;

extern vector<uint64_t> pow_s;

extern uint64_t base_inv;

extern const int inf;

extern vector<vector<uint64_t>> precomputed_mul_pow_normalize;

extern vector<uint64_t> base_pow;
extern vector<uint64_t> base_inv_pow;

using hash_type = pair<uint64_t, uint64_t>;


//コマンドが実行可能化の判定はここでは行わない。
class deque_hash {
    static constexpr uint64_t mask30 = ((uint64_t) 1 << 30) - 1;
    static constexpr uint64_t mask31 = ((uint64_t) 1 << 31) - 1;
public:
    static constexpr uint64_t mask61 = ((uint64_t) 1 << 61) - 1;
    static constexpr uint64_t offset = mask61 * 7;
    static constexpr uint64_t mod(const uint64_t x) {const uint64_t y = (x >> 61) + (x & mask61);return y >= mask61 ? y - mask61 : y;}
    static constexpr uint64_t mul(const uint64_t x, const uint64_t y) {
        const uint64_t xh = x >> 31, xl = x & mask31;
        const uint64_t yh = y >> 31, yl = y & mask31;
        const uint64_t z = xl * yh + xh * yl;
        const uint64_t zh = z >> 30, zl = z & mask30;
        return (((xh * yh) << 1) + zh + (zl << 31) + xl * yl);
    }

    /*@formatter:on*/

    uint64_t all_hash = 0;
    RingBuffer500 ring_buf;

    //todo 高速化できる
    void swap_top() {
        int v0 = ring_buf[0];
        int v1 = ring_buf[1];
        pop_front();
        pop_front();
        push_front(v0);
        push_front(v1);
    }

    void rotate() {
        int v0 = ring_buf[0];
        pop_front();
        push_back(v0);
    }

    void rrotate() {
        int v_back = ring_buf.back();
        pop_back();
        push_front(v_back);
    }

    static int normalize_val(int v) {
        return v + 1;
    }

    void pop_front() {
        int k = ring_buf.size() - 1;
        int v = ring_buf[0];
        my_assert(0 <= k && k <= HIGHER_MAX_ARG * 3);
        my_assert(0 <= v && v < HIGHER_MAX_ARG);
//        uint64_t old_result = mul(pow_s[k], normalize_val(v));
        uint64_t precomputed_result = precomputed_mul_pow_normalize[k][v];
//        my_assert(old_result == precomputed_result);
        all_hash = mod(all_hash + offset - precomputed_result);
        ring_buf.pop_front();
    }

    void push_front(int v) {
        int k = ring_buf.size();
        my_assert(0 <= k && k <= HIGHER_MAX_ARG * 3);
        my_assert(0 <= v && v < HIGHER_MAX_ARG);
//        uint64_t old_result = mul(pow_s[k], normalize_val(v));
        uint64_t precomputed_result = precomputed_mul_pow_normalize[k][v];
//        my_assert(old_result == precomputed_result);
        all_hash = mod(all_hash + precomputed_result);
        ring_buf.push_front(v);
    }

    void pop_back() {
        all_hash = mod(mul(mod(all_hash + offset - normalize_val(ring_buf.back())), base_inv));
        ring_buf.pop_back();
    }

    void push_back(int v) {
        all_hash = mod(mul(all_hash, base) + normalize_val(v));
        ring_buf.push_back(v);
    }

    deque_hash(const deque<int> &A) : all_hash(0) {
        for (auto &a: A) {
            push_back(a);
        }
    }

    static uint64_t merge_hash(const uint64_t &a_hash, const uint64_t &b_hash) {
        return mod(mod(mul(a_hash, pow_s[HIGHER_MAX_ARG])) + b_hash);
    }

    static uint64_t get_do_cmd_rotate_hash(
            uint64_t current_hash,
            const RingBuffer500 &ring_buf,
            int cnt
    ) {
//        uint64_t pop_front_v;
//        int k1 = ring_buf.size() - 1;
//        int v1 = ring_buf[0];
//        my_assert(0 <= k1 && k1 <= HIGHER_MAX_ARG * 3);
//        my_assert(0 <= v1 && v1 < HIGHER_MAX_ARG);
//        uint64_t precomputed_result = precomputed_mul_pow_normalize[k1][v1];
//        pop_front_v = mod(current_hash + offset - precomputed_result);

//        int k2 =


        my_assert(0 < cnt && cnt < ring_buf.size());//意味がないコマンドは与えられないものとする
        int buf_n = ring_buf.size();
        uint64_t move_sum = 0;
        {
            int k = cnt - 1;
            move_sum = precomputed_mul_pow_normalize[k][ring_buf[0]];
        }
        for (int i = 1; i < cnt; i++) {
            //ベースの桁
            int k = cnt - 1 - i;
            uint64_t v = precomputed_mul_pow_normalize[k][ring_buf[i]];
            move_sum = mod(move_sum + v);
        }
        uint64_t move_sum_shift = mul(move_sum, base_pow[buf_n - cnt]);
        uint64_t result_hash = mod(mul(mod(current_hash + offset - move_sum_shift), base_pow[cnt]) + move_sum);
        return result_hash;
    }

    static uint64_t get_do_cmd_rev_rotate_hash(
            uint64_t current_hash,
            const RingBuffer500 &ring_buf,
            int cnt
    ) {
        my_assert(0 < cnt && cnt < ring_buf.size());//意味がないコマンドは与えられないものとする
        int buf_n = ring_buf.size();
        uint64_t move_sum = 0;
        {
            int k = cnt - 1;
            move_sum = precomputed_mul_pow_normalize[k][ring_buf[buf_n - cnt]];
        }
        for (int i = 1; i < cnt; i++) {
            //ベースの桁
            int k = cnt - 1 - i;
            uint64_t v = precomputed_mul_pow_normalize[k][ring_buf[buf_n - cnt + i]];
            move_sum = mod(move_sum + v);
        }
        uint64_t move_sum_shift = mul(move_sum, base_pow[buf_n - cnt]);
        uint64_t result_hash = mod(mul(mod(current_hash + offset - move_sum), base_inv_pow[cnt]) + move_sum_shift);
        return result_hash;
    }
};

extern struct set_pow_len set_pow_len_v;


void test_deque_hash();

//deque_hash ro(list);
//lcpならsa := saffix_arrayでO(1) 構成O(N log(n)^2)

//hashを持つ場合set<uint64_t>等にする必要がある事に注意

//とりあえず、state併用してやることにする
//コマンドが実行できるかは別で行う
class StateHash {
public:
    deque_hash A;
    deque_hash B;
//    int turn;
    int N;
//    cmd_list_t cmd_list;

    StateHash(State &st) : A(to_dq(st.A)), B(to_dq(st.B)), N(st.current_N) {

    }

    StateHash(deque_hash &a, deque_hash &b) : A(a), B(b), N(a.ring_buf.size() + b.ring_buf.size()) {

    }

    void sa() {
        A.swap_top();
    }

    void sb() {
        B.swap_top();
    }

    //ターン数などを考えていないため、sa, sbをよんでも現状副作用がない
    void ss() {
        sa();
        sb();
    }

    void push(deque_hash &from, deque_hash &to) {
        int from_top = from.ring_buf[0];
        from.pop_front();
        to.push_front(from_top);
    }

    void pa() {
        push(B, A);
    }

    void pb() {
        push(A, B);
    }

    void ra() {
        A.rotate();
    }

    void rb() {
        B.rotate();
    }

    void rr() {
        ra();
        rb();
    }

    void rra() {
        A.rrotate();
    }

    void rrb() {
        B.rrotate();
    }

    void rrr() {
        rra();
        rrb();
    }

    void do_cmd(command cmd) {
        switch (cmd) {
            case SA:
                sa();
                break;
            case SB:
                sb();
                break;
            case SS:
                ss();
                break;
            case PA:
                pa();
                break;
            case PB:
                pb();
                break;
            case RA:
                ra();
                break;
            case RB:
                rb();
                break;
            case RR:
                rr();
                break;
            case RRA:
                rra();
                break;
            case RRB:
                rrb();
                break;
            case RRR:
                rrr();
                break;
            default:
                // out("switch cmd error"); // why: output.txt出力抑制のため
                my_assert(0);
                break;
        }
    }


    uint64_t hash() const {
        return deque_hash::merge_hash(A.all_hash, B.all_hash);
    }

    static pair<uint64_t, uint64_t> apply_command_to_hash(
            uint64_t a_hash, uint64_t b_hash,
            command cmd,
            State &state
    ) {
        uint64_t new_a_hash = a_hash;
        uint64_t new_b_hash = b_hash;

        switch (cmd) {
            case RA: {
                int k = state.A.size() - 1;
                int v = state.A[0];
                my_assert(0 <= k && k <= HIGHER_MAX_ARG * 3);
                my_assert(0 <= v && v < HIGHER_MAX_ARG);
                uint64_t precomputed_result = precomputed_mul_pow_normalize[k][v];
                new_a_hash = deque_hash::mod(new_a_hash + deque_hash::offset - precomputed_result);
                int v0 = state.A[0];
                new_a_hash = deque_hash::mod(deque_hash::mul(new_a_hash, base) + deque_hash::normalize_val(v0));
                break;
            }
            case RRA: {
                int v_back = state.A.back();
                new_a_hash = deque_hash::mod(deque_hash::mul(
                        deque_hash::mod(new_a_hash + deque_hash::offset - deque_hash::normalize_val(v_back)),
                        base_inv));
                int k = state.A.size() - 1;
                my_assert(0 <= k && k <= HIGHER_MAX_ARG * 3);
                my_assert(0 <= v_back && v_back < HIGHER_MAX_ARG);
                uint64_t precomputed_result = precomputed_mul_pow_normalize[k][v_back];
                new_a_hash = deque_hash::mod(new_a_hash + precomputed_result);
                break;
            }
            case RB: {
                int k = state.B.size() - 1;
                int v = state.B[0];
                my_assert(0 <= k && k <= HIGHER_MAX_ARG * 3);
                my_assert(0 <= v && v < HIGHER_MAX_ARG);
                uint64_t precomputed_result = precomputed_mul_pow_normalize[k][v];
                new_b_hash = deque_hash::mod(new_b_hash + deque_hash::offset - precomputed_result);
                int v0 = state.B[0];
                new_b_hash = deque_hash::mod(deque_hash::mul(new_b_hash, base) + deque_hash::normalize_val(v0));
                break;
            }
            case RRB: {
                int v_back = state.B.back();
                new_b_hash = deque_hash::mod(deque_hash::mul(
                        deque_hash::mod(new_b_hash + deque_hash::offset - deque_hash::normalize_val(v_back)),
                        base_inv));
                int k = state.B.size() - 1;
                my_assert(0 <= k && k <= HIGHER_MAX_ARG * 3);
                my_assert(0 <= v_back && v_back < HIGHER_MAX_ARG);
                uint64_t precomputed_result = precomputed_mul_pow_normalize[k][v_back];
                new_b_hash = deque_hash::mod(new_b_hash + precomputed_result);
                break;
            }
            case PA: {
                int from_top = state.B[0];
                int k_b = state.B.size() - 1;
                my_assert(0 <= k_b && k_b <= HIGHER_MAX_ARG * 3);
                my_assert(0 <= from_top && from_top < HIGHER_MAX_ARG);
                uint64_t precomputed_result_b = precomputed_mul_pow_normalize[k_b][from_top];
                new_b_hash = deque_hash::mod(new_b_hash + deque_hash::offset - precomputed_result_b);
                int k_a = state.A.size();
                my_assert(0 <= k_a && k_a <= HIGHER_MAX_ARG * 3);
                my_assert(0 <= from_top && from_top < HIGHER_MAX_ARG);
                uint64_t precomputed_result_a = precomputed_mul_pow_normalize[k_a][from_top];
                new_a_hash = deque_hash::mod(new_a_hash + precomputed_result_a);
                break;
            }
            case PB: {
                int from_top = state.A[0];
                int k_a = state.A.size() - 1;
                my_assert(0 <= k_a && k_a <= HIGHER_MAX_ARG * 3);
                my_assert(0 <= from_top && from_top < HIGHER_MAX_ARG);
                uint64_t precomputed_result_a = precomputed_mul_pow_normalize[k_a][from_top];
                new_a_hash = deque_hash::mod(new_a_hash + deque_hash::offset - precomputed_result_a);
                int k_b = state.B.size();
                my_assert(0 <= k_b && k_b <= HIGHER_MAX_ARG * 3);
                my_assert(0 <= from_top && from_top < HIGHER_MAX_ARG);
                uint64_t precomputed_result_b = precomputed_mul_pow_normalize[k_b][from_top];
                new_b_hash = deque_hash::mod(new_b_hash + precomputed_result_b);
                break;
            }
            default:
                // out("apply_command_to_hash: unsupported command"); // why: output.txt出力抑制のため
                my_assert(0);
                break;
        }

        return {new_a_hash, new_b_hash};
    }

    static pair<uint64_t, uint64_t> apply_command_to_hash_PA(
        uint64_t a_hash, uint64_t b_hash,
        int push_value,
        int a_size,
        int b_size
    ) {
        uint64_t new_a_hash = a_hash;
        uint64_t new_b_hash = b_hash;

        int k_b = b_size - 1;
        my_assert(0 <= k_b && k_b <= HIGHER_MAX_ARG * 3);
        my_assert(0 <= push_value && push_value < HIGHER_MAX_ARG);
        uint64_t precomputed_result_b = precomputed_mul_pow_normalize[k_b][push_value];
        new_b_hash = deque_hash::mod(new_b_hash + deque_hash::offset - precomputed_result_b);

        int k_a = a_size;
        my_assert(0 <= k_a && k_a <= HIGHER_MAX_ARG * 3);
        my_assert(0 <= push_value && push_value < HIGHER_MAX_ARG);
        uint64_t precomputed_result_a = precomputed_mul_pow_normalize[k_a][push_value];
        new_a_hash = deque_hash::mod(new_a_hash + precomputed_result_a);

        return {new_a_hash, new_b_hash};
    }


    static pair<uint64_t, uint64_t> apply_command_to_hash_PB(
            uint64_t a_hash, uint64_t b_hash,
            int push_value,
            int a_size,
            int b_size
    ) {
        uint64_t new_a_hash = a_hash;
        uint64_t new_b_hash = b_hash;

        int k_a = a_size - 1;
        my_assert(0 <= k_a && k_a <= HIGHER_MAX_ARG * 3);
        my_assert(0 <= push_value && push_value < HIGHER_MAX_ARG);
        uint64_t precomputed_result_a = precomputed_mul_pow_normalize[k_a][push_value];
        new_a_hash = deque_hash::mod(new_a_hash + deque_hash::offset - precomputed_result_a);

        int k_b = b_size;
        my_assert(0 <= k_b && k_b <= HIGHER_MAX_ARG * 3);
        my_assert(0 <= push_value && push_value < HIGHER_MAX_ARG);
        uint64_t precomputed_result_b = precomputed_mul_pow_normalize[k_b][push_value];
        new_b_hash = deque_hash::mod(new_b_hash + precomputed_result_b);

        return {new_a_hash, new_b_hash};
    }

    static pair<uint64_t, uint64_t> get_do_cmd_nth_hash(
            uint64_t a_hash, uint64_t b_hash,
            command cmd,
            int cnt,
            const State &state
    ) {
        uint64_t new_a_hash = a_hash;
        uint64_t new_b_hash = b_hash;

        if (cnt == 0) {
            return {new_a_hash, new_b_hash};
        }

        switch (cmd) {
            case RA: {
                new_a_hash = deque_hash::get_do_cmd_rotate_hash(new_a_hash, state.A, cnt);
                break;
            }
            case RB: {
                new_b_hash = deque_hash::get_do_cmd_rotate_hash(new_b_hash, state.B, cnt);
                break;
            }
            case RRA: {
                new_a_hash = deque_hash::get_do_cmd_rev_rotate_hash(new_a_hash, state.A, cnt);
                break;
            }
            case RRB: {
                new_b_hash = deque_hash::get_do_cmd_rev_rotate_hash(new_b_hash, state.B, cnt);
                break;
            }
            default: {
                my_assert(false);//not implemented
                State state_copy = state;
                for (int i = 0; i < cnt; i++) {
                    tie(new_a_hash, new_b_hash) = apply_command_to_hash(new_a_hash, new_b_hash, cmd, state_copy);
                    state_copy.do_cmd(cmd);
                }
                break;
            }
        }

        return {new_a_hash, new_b_hash};
    }

    static pair<uint64_t, uint64_t> get_do_cmd_nth_hash_naive(
            uint64_t a_hash, uint64_t b_hash,
            command cmd,
            int cnt,
            const State &state
    ) {
#ifndef DEBUG
        my_assert(false);
#endif
        uint64_t new_a_hash = a_hash;
        uint64_t new_b_hash = b_hash;
        State state_copy = state;

        for (int i = 0; i < cnt; i++) {
            tie(new_a_hash, new_b_hash) = apply_command_to_hash(new_a_hash, new_b_hash, cmd, state_copy);
            state_copy.do_cmd(cmd);
        }

        return {new_a_hash, new_b_hash};
    }
};

void test_state_hash();

#endif //UNTITLED_STATEHASH_H
