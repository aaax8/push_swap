//
//
#include "all_include.h"
#include "StateHash.h"

const uint64_t base = 61520083;


bint ext_gcd(bint a, bint b, bint &x, bint &y)
{
    if (b == 0)
    {
        x = 1;
        y = 0;
        return a;
    }
    bint q = a / b;
    bint g = ext_gcd(b, a-q*b, x, y);
    bint z = x-q*y;
    x = y;
    y = z;
    return g;
}

uint64_t modinv(bint a, bint m)
{
    bint x, y;
    ext_gcd(a, m, x, y);
    x %= m;
    if (x < 0)
        x += m;
    return x;
}

const int HIGHER_MAX_ARG = 1000;

constexpr int pow_len = HIGHER_MAX_ARG * 3;

vector<uint64_t> pow_s(pow_len + 1);

uint64_t base_inv = modinv(base, ((uint64_t) 1 << 61) - 1);

const int inf = 1<<30;

vector<vector<uint64_t>> precomputed_mul_pow_normalize(pow_len + 1, vector<uint64_t>(HIGHER_MAX_ARG));

vector<uint64_t> base_pow(pow_len + 1);
vector<uint64_t> base_inv_pow(pow_len + 1);

struct set_pow_len {
    set_pow_len() {
        pow_s[0] = 1;
        for (std::size_t i = 1; i <= pow_len; i++) { pow_s[i] = deque_hash::mod(deque_hash::mul(pow_s[i - 1], base)); }
        
        base_pow[0] = 1;
        for (std::size_t i = 1; i <= pow_len; i++) {
            base_pow[i] = deque_hash::mod(deque_hash::mul(base_pow[i - 1], base));
        }
        
        base_inv_pow[0] = 1;
        for (std::size_t i = 1; i <= pow_len; i++) {
            base_inv_pow[i] = deque_hash::mod(deque_hash::mul(base_inv_pow[i - 1], base_inv));
        }
        
        for (int k = 0; k <= pow_len; k++) {
            for (int v = 0; v < HIGHER_MAX_ARG; v++) {
                int normalized_v = v + 1;
                precomputed_mul_pow_normalize[k][v] = deque_hash::mul(pow_s[k], normalized_v);
            }
        }
    }
} set_pow_len_v;

void test_deque_hash(){
    deque<int>q;
    deque_hash roll(q);
    auto p_hash =[&](){
        // out(roll.all_hash); // why: output.txt出力抑制のため
    };
    // out("start"); // why: output.txt出力抑制のため
    roll.push_front(1);p_hash();
    roll.push_front( 2);p_hash();
    roll.push_front( 3);p_hash();
    roll.push_front( 4);p_hash();
    roll.pop_front();p_hash();
    roll.pop_back();p_hash();
    roll.push_back(1);p_hash();
    roll.push_front(4);p_hash();
    // out("----"); // why: output.txt出力抑制のため
    {
        // out("3421 hash"); // why: output.txt出力抑制のため
        roll.swap_top();p_hash();
        deque_hash h3421({3,4,2,1});
        // out(h3421.all_hash); // why: output.txt出力抑制のため
    }
    {
        // out("4213 hash"); // why: output.txt出力抑制のため
        roll.rotate();p_hash();
        deque_hash h({4,2,1,3});
        // out(h.all_hash); // why: output.txt出力抑制のため
    }
    {
        // out("3421 hash"); // why: output.txt出力抑制のため
        roll.rrotate();p_hash();
        deque_hash h({3,4,2,1});
        // out(h.all_hash); // why: output.txt出力抑制のため
    }

}
