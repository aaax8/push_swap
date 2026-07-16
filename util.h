
//
//

#ifndef UNTITLED_UTIL_H
#define UNTITLED_UTIL_H

#include "all_include.h"


// debug_vector.h

#include <vector>
#include <stdexcept>
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <filesystem>

#include <bitset>
#include <source_location>

extern std::ofstream output_file;
extern std::filesystem::path output_dir;
extern std::filesystem::path output_file_path;

inline void assert_catch() {
    ;
}
#ifdef DEBUG

#define my_assert(b) \
    do { \
        if (!(b)) {  \
            assert_catch();         \
            std::cerr << "[ASSERT FAILED] " << __FILE__ << ":" << __LINE__ \
                      << " - Expression: " << #b << std::endl; \
            std::abort(); \
        } \
    } while(0)

#else

#define my_assert(b) ((void)0)

#endif

// ----------- 通常の vector<T> 用 -----------------
template<typename T>
class debug_vector : public std::vector<T> {
    using base = std::vector<T>;
public:
    using base::base;

    debug_vector(const base& rhs) : base(rhs) {}
    debug_vector(std::initializer_list<T> init) : base(init) {}

    debug_vector& operator=(const base& rhs) {
        base::operator=(rhs);
        return *this;
    }

    debug_vector& operator=(std::initializer_list<T> init) {
        base::operator=(init);
        return *this;
    }

    const T& operator[](size_t i) const {
        if (i >= base::size()) {
            my_assert(0);
            std::cerr << "[debug_vector] Out of range read: "
                << i << " >= " << base::size() << std::endl;
            std::abort();
        }
        return base::operator[](i);
    }

    T& operator[](size_t i) {
        if (i >= base::size()) {
            my_assert(0);
            std::cerr << "[debug_vector] Out of range write: "
                << i << " >= " << base::size() << std::endl;
            std::abort();
        }
        return base::operator[](i);
    }

    void resize(size_t new_sz, const T& v) {
        base::resize(new_sz, v);
    }

    void resize(size_t new_sz) {
        if (new_sz <= base::size()) {
            base::erase(base::begin() + new_sz, base::end());
        } else {
            // why: DefaultConstructibleでない型は縮小のみサポート
            if constexpr (std::is_default_constructible_v<T>) {
                base::resize(new_sz);
            } else {
                my_assert(false);
            }
        }
    }
};


// ----------- vector<bool> 用部分特殊化 -----------------
template<>
class debug_vector<bool> : public std::vector<bool> {
    using base = std::vector<bool>;

public:
    using base::base;

    // vector<bool5>::reference はプロキシ型
    typename base::reference operator[](size_t i) {
        if (i >= base::size()) {
            my_assert(0);
            std::cerr << "[debug_vector<bool>] Out of range write: "
                << i << " >= " << base::size() << std::endl;
            std::abort();
        }
        return base::operator[](i);
    }

    typename base::const_reference operator[](size_t i) const {
        if (i >= base::size()) {
            my_assert(0);
            std::cerr << "[debug_vector<bool>] Out of range read: "
                << i << " >= " << base::size() << std::endl;
            std::abort();
        }
        return base::operator[](i);
    }
};

#ifdef DEBUG
template<class T> using my_vector = debug_vector<T>;
#else 
template<class T> using my_vector = std::vector<T>;
#endif


#ifdef DEBUG
// my_bitset: bitset with bounds-checked access (asserts on out-of-range)
template <size_t N>
class my_bitset : public std::bitset<N> {
    using base = std::bitset<N>;
public:
    using base::base;

    // 責務: 全 bit を true にする。
    // 必要な理由: bitset mask 構築で std::bitset と同じ bset.set() 呼び出しを使うため。
    my_bitset& set() {
        return static_cast<my_bitset&>(base::set());
    }

    // 責務: std::bitset<N> の値を my_bitset<N> へ代入する。
    // 必要な理由: shift や OR の戻り値が std::bitset<N> になる箇所でも my_bitset に保持するため。
    my_bitset& operator=(const base &rhs) {
        base::operator=(rhs);
        return *this;
    }

    // Bounds-checked set
    my_bitset& set(size_t pos, bool val = true) {
        my_assert(pos < N);
        return static_cast<my_bitset&>(base::set(pos, val));
    }
    // Bounds-checked reset
    my_bitset& reset(size_t pos) {
        my_assert(pos < N);
        return static_cast<my_bitset&>(base::reset(pos));
    }

    // Bounds-checked reset
    my_bitset& reset() {
        return static_cast<my_bitset&>(base::reset());
    }

    // Bounds-checked test
    bool test(size_t pos) const {
        my_assert(pos < N);
        return base::test(pos);
    }
    // Bounds-checked operator[]
    bool operator[](size_t pos) const {
        my_assert(pos < N);
        return base::operator[](pos);
    }
    // Bounds-checked reference for non-const []
    typename base::reference operator[](size_t pos) {
        my_assert(pos < N);
        return base::operator[](pos);
    }
};
#else
template <size_t N>
using my_bitset = std::bitset<N>;
#endif

template<class T>
T sum(const vector<T>& vec) {
    T ret = 0;
    for (auto& v : vec) {
        ret += v;
    }
    return ret;
}


template<class List>
List get_sorted(const List& list) {
    List ret = list;
    std::sort(ret.begin(), ret.end());
    return ret;
};

template<class T, size_t N>
ostream& operator<<(ostream& os, const array<T, N>& rhs) {
    for (auto& v : rhs) {
        os << v << " ";
    }
    os << endl;
    return os;
}

template<class T, class U>
ostream& operator<<(ostream& os, const tuple<T, U>& rhs) {
    auto [a, b] = rhs;
    os << "(" << a << ", " << b << ")";
    return os;
}

template<class T, class U, class V>
ostream& operator<<(ostream& os, const tuple<T, U, V>& rhs) {
    auto [a, b, c] = rhs;
    os << "(" << a << ", " << b << ", " << c << ")";
    return os;
}

template<class T, class U>
ostream& operator<<(ostream& os, const pair<T, U>& p) {
    os << "(" << p.first << ", " << p.second << ")";
    return os;
}

template<class T>
ostream& operator<<(ostream& os, const vector<T>& rhs) {
    for (auto& v : rhs) {
        os << v << " ";
    }
    os << endl;
    return os;
}

template<class T>
ostream& operator<<(ostream& os, const deque<T>& rhs) {
    for (auto& v : rhs) {
        os << v << " ";
    }
    //    os << endl;
    return os;
}

template<size_t N>
std::ostream& operator<<(std::ostream& os, const std::bitset<N>& bs) {
    os << "{";
    bool first = true;
    for (size_t i = 0; i < N; ++i) {
        if (bs.test(i)) {
            if (!first) os << ", ";
            first = false;
            os << i;
        }
    }
    os << "}";
    return os;
}

template<class T>
ostream& operator<<(ostream& os, const set<T>& rhs) {
    for (auto& v : rhs) {
        os << v << " ";
    }
    os << endl;
    return os;
}


extern std::ofstream output_file;

// 責務: out_file が通知済みの path を保持する。
// 必要な理由: テンプレート関数ごとに static set が分かれて、同じ file_path の通知が重複するのを避けるため。
inline std::set<std::string> &out_file_notice_paths() {
    static std::set<std::string> paths;
    return paths;
}

// 責務: file_path ごとに初回だけ、標準出力へファイル出力先を通知する。
// 必要な理由: 調査ログがファイルへ出ていることを実行時に忘れないようにするため。
inline void notice_out_file_path(const std::string &file_path) {
    std::set<std::string> &paths = out_file_notice_paths();
    if (paths.find(file_path) != paths.end()) return;
    std::cout << "writing output to " << file_path << std::endl;
    paths.insert(file_path);
}

// 責務: out_file の file_path が現在開いている output_file と同じか判定する。
// 必要な理由: 既存 out から out_file へ委譲した時も、同じファイルを別 ofstream で同時に開かないようにするため。
inline bool is_open_output_file_path(const std::string &file_path) {
    return output_file.is_open() && file_path == output_file_path.string();
}

// 責務: 指定 file_path へ空行を append 出力する。
// 必要な理由: 既存 out() の空行出力も out_file 経由へ統一するため。
inline void out_file(const std::string &file_path) {
    notice_out_file_path(file_path);
    if (is_open_output_file_path(file_path)) {
        output_file << std::endl;
        return;
    }
    std::ofstream file(file_path, std::ios::app);
    file << std::endl;
}

// 責務: 指定 file_path へ head だけを 1 行 append 出力する。
// 必要な理由: 既存 out(head) と同じ 1 行出力を、初回通知つきファイル出力で使うため。
template<class T>
void out_file(const std::string &file_path, const T &head) {
    notice_out_file_path(file_path);
    if (is_open_output_file_path(file_path)) {
        output_file << head << std::endl;
        return;
    }
    std::ofstream file(file_path, std::ios::app);
    file << head << std::endl;
}

// 責務: 指定 file_path へ複数引数を空白区切りで 1 行 append 出力する。
// 必要な理由: 既存 out(head, tail...) と同じ呼び心地で調査ログを追加するため。
template<class T, class... U>
void out_file(const std::string &file_path, const T &head, const U&... tail) {
    notice_out_file_path(file_path);
    if (is_open_output_file_path(file_path)) {
        output_file << head;
        ((output_file << " " << tail), ...);
        output_file << std::endl;
        return;
    }
    std::ofstream file(file_path, std::ios::app);
    file << head;
    ((file << " " << tail), ...);
    file << std::endl;
}

template<class T>
void out(T head) {
    if (output_file.is_open()) {
        out_file(output_file_path.string(), head);
    }
    else {
        cout << head << endl;
    }
}

template<class T, class... U>
void out(T head, U ... tail) {
    if (output_file.is_open()) {
        out_file(output_file_path.string(), head, tail...);
    }
    else {
        cout << head << " ";
        out(tail...);
    }
}

inline void clear_file() {
    if (!output_file.is_open()) return;

    auto path = output_file_path;   // ファイル名込みのパスを保存
    output_file.close();

    // 中身を空にして開き直す（truncate）
    output_file.open(path, std::ios::out | std::ios::trunc);

    if (!output_file) {
        std::cerr << "failed to clear output file: " << path << std::endl;
        std::abort();
    }
}

inline std::ofstream &output2_stream() {
    static std::ofstream output2_file;
    return output2_file;
}

inline void ensure_output2_open() {
    std::ofstream &ofs = output2_stream();
    if (ofs.is_open()) return;
    if (output_dir.empty()) return;
    std::filesystem::create_directories(output_dir);
    ofs.open(output_dir / "output2.txt", std::ios::app);
    if (!ofs.is_open()) {
        std::cerr << "failed to open output2.txt" << std::endl;
        std::abort();
    }
}

template<class T>
void out2_impl(T head) {
    output2_stream() << head << endl;
}

template<class T, class... U>
void out2_impl(T head, U ... tail) {
    output2_stream() << head << " ";
    out2_impl(tail...);
}

template<class... U>
void out2(U ... values) {
    ensure_output2_open();
    out2_impl(values...);
}

inline void out2() {
    ensure_output2_open();
    output2_stream() << endl;
}

inline void clear_output2_file() {
    std::ofstream &ofs = output2_stream();
    if (ofs.is_open()) ofs.close();
    if (output_dir.empty()) return;
    std::filesystem::create_directories(output_dir);
    ofs.open(output_dir / "output2.txt", std::ios::trunc);
    if (!ofs.is_open()) {
        std::cerr << "failed to clear output2.txt" << std::endl;
        std::abort();
    }
}

inline std::ofstream &output3_stream() {
    static std::ofstream output3_file;
    return output3_file;
}

inline void ensure_output3_open() {
    std::ofstream &ofs = output3_stream();
    if (ofs.is_open()) return;
    if (output_dir.empty()) return;
    std::filesystem::create_directories(output_dir);
    ofs.open(output_dir / "output3.txt", std::ios::app);
    if (!ofs.is_open()) {
        std::cerr << "failed to open output3.txt" << std::endl;
        std::abort();
    }
}

template<class T>
void out3_impl(T head) {
    output3_stream() << head << endl;
}

template<class T, class... U>
void out3_impl(T head, U ... tail) {
    output3_stream() << head << " ";
    out3_impl(tail...);
}

template<class... U>
void out3(U ... values) {
    ensure_output3_open();
    out3_impl(values...);
}

inline void clear_output3_file() {
    std::ofstream &ofs = output3_stream();
    if (ofs.is_open()) ofs.close();
    if (output_dir.empty()) return;
    std::filesystem::create_directories(output_dir);
    ofs.open(output_dir / "output3.txt", std::ios::trunc);
    if (!ofs.is_open()) {
        std::cerr << "failed to clear output3.txt" << std::endl;
        std::abort();
    }
}

void out();

template<class List>
void out_with_idx(const List& list, int ignore_val) {
    for (int i = 0; i < list.size(); i++) {
        if (list[i] == ignore_val)continue;
        out("(", i, ", ", list[i], ")");
    }
}


template<class R, class V>
//圧縮結果, [i] := i番目に大きい値
pair<R, vector<int>> compress(V a) {
    map<int, int> comp_idx;
    vector<int> vals(a.size());
    R ret(a.size());
    for (auto& v : a) {
        comp_idx[v];
    }
    int id = 0;
    for (auto& [_, v] : comp_idx) {
        v = id++;
    }
    for (int i = 0; i < a.size(); i++) {
        ret[i] = comp_idx[a[i]];

        vals[comp_idx[a[i]]] = a[i];
    }
    return { ret, vals };
}

template<class R, class V>
//ある値が、何番目の値かの辞書を返す
pair<R, vector<int>> get_compress_dict(V a) {
    map<int, int> comp_idx;
    vector<int> vals(a.size());
    for (auto& v : a) {
        comp_idx[v];
    }
    int id = 0;
    R res;
    for (auto& [av, v] : comp_idx) {
        v = id;
        res[av] = id;
        id++;
    }
    for (int i = 0; i < a.size(); i++) {
        vals[comp_idx[a[i]]] = a[i];
    }
    return { res, vals };
}


vector<int> arg_to_vec(int argc, char* argv[]);

std::filesystem::path get_project_root(int argc, char* argv[]);


int mod(int a, int mod_v);

//0~MAX_N-1でできているか
template<class T>
bool is_permutation(const T& list, int N) {
    if (list.size() != N) {
        return false;
    }
    vector<bool> exist(N);
    for (auto& v : list) {
        if (!(0 <= v && v < N)) {
            return false;
        }
        if (exist[v]) {
            return false;
        }
        exist[v] = true;
    }
    return true;
}

template<class T, class U>
bool chmi(T& lhs, const U& rhs) {
    if (lhs > rhs) {
        lhs = rhs;
        return true;
    }
    else {
        return false;
    }
}

template<class T, class U>
bool chma(T& lhs, const U& rhs) {
    if (lhs < rhs) {
        lhs = rhs;
        return true;
    }
    else {
        return false;
    }
}

inline int u0(int v) {
    return max(0, v);
}

inline void validate_sorted(const my_vector<int>& idxs) {
#ifdef DEBUG
    for (int i = 1; i < (int)idxs.size(); i++) {
        my_assert(idxs[i - 1] < idxs[i]);
    }
#endif
}

bool bget(int mask, int i);

int bit(int i);

template<size_t N, class T>
void out_array(T a[N]) {
    for (int i = 0; i < N; i++) {
        cout << a[i] << " ";
    }
    cout << endl;
}

extern std::mt19937 engine;
void set_my_rand_seed(uint32_t seed);
void set_my_rand_seed(uint32_t seed);

int my_popcount(int i);

bool is_sorted_of_ring(int a, int b, int c);

bool is_sorted_of_ring_eq(int a, int b, int c);

template<size_t N>
int pop_cnt(const bitset<N>& bset, int j) {
    int a = bset.count();
    int b = (bset >> j).count();
    return a - b;
}

template<size_t N>
int pop_cnt(const bitset<N>& bset, int i, int j) {
    if (i >= j)return 0;
    int a = (bset >> i).count();
    int b = (bset >> j).count();
    return a - b;
}

#ifdef DEBUG
template<size_t N>
int pop_cnt(const my_bitset<N>& bset, int j) {
    int a = bset.count();
    int b = (bset >> j).count();
    return a - b;
}

template<size_t N>
int pop_cnt(const my_bitset<N>& bset, int i, int j) {
    if (i >= j)return 0;
    int a = (bset >> i).count();
    int b = (bset >> j).count();
    return a - b;
}
#endif

int my_rand(int b);

int my_rand(int a, int b);

int rand_except(int a, int b);

// a <= x < b の整数で c を除く
int rand_except(int a, int b, int c);

template<typename T, size_t N>
class vec_array {
private:
    array<T, N> data_;
    size_t size_;

    // 責務: 要素追加前に容量を確認し、超過時は呼び出し元 file/line/function を表示して停止する。
    // 必要な理由: 固定配列化した箇所で容量不足が起きたとき、どの push_back 呼び出しが原因か追えるようにするため。
    void assert_can_add(const std::source_location &loc) const {
        if (size_ < N) return;
        std::cerr << "[vec_array overflow] capacity=" << N
                  << " size=" << size_
                  << " file=" << loc.file_name()
                  << " line=" << loc.line()
                  << " function=" << loc.function_name()
                  << std::endl;
        my_assert(false);
        std::abort();
    }

public:
    vec_array() : data_{}, size_(0) {}

    void push_back(const T& value, const std::source_location &loc = std::source_location::current()) {
        assert_can_add(loc);
        data_[size_] = value;
        size_++;
    }

    // 責務: rvalue の要素を vec_array 末尾へ move 代入で追加する。
    // 必要な理由: vector から vec_array へ置き換えた箇所で、move 可能な値を余分に copy しないため。
    void push_back(T&& value, const std::source_location &loc = std::source_location::current()) {
        assert_can_add(loc);
        data_[size_] = std::move(value);
        size_++;
    }

    template<typename... Args>
    void emplace_back(Args&&... args) {
        assert_can_add(std::source_location::current());
        data_[size_] = T(std::forward<Args>(args)...);
        size_++;
    }

    T& operator[](size_t i) {
        my_assert(i < size_);
        return data_[i];
    }

    const T& operator[](size_t i) const {
        my_assert(i < size_);
        return data_[i];
    }

    size_t size() const {
        return size_;
    }

    // 責務: 現在の保持要素数が 0 か返す。
    // 必要な理由: vector から vec_array へ自然置換した箇所の empty() 判定を維持するため。
    bool empty() const {
        return size_ == 0;
    }

    void clear() {
        size_ = 0;
    }

    void resize_small(int new_sz){
        my_assert(size_ >= new_sz);
        size_ = new_sz;
    }

    T& back() {
        my_assert(size_ > 0);
        return data_[size_ - 1];
    }

    const T& back() const {
        my_assert(size_ > 0);
        return data_[size_ - 1];
    }

    void pop_back() {
        my_assert(size_ > 0);
        size_--;
    }

    // 責務: 指定位置へ 1 要素を挿入し、後続要素を 1 つ後ろへずらす。
    // 必要な理由: vector から vec_array へ置換した split point 列で、既存の sorted unique 挿入処理を維持するため。
    T* insert(T *pos, const T &value, const std::source_location &loc = std::source_location::current()) {
        assert_can_add(loc);
        my_assert(data_.data() <= pos && pos <= data_.data() + size_);
        const size_t insert_idx = static_cast<size_t>(pos - data_.data());
        for (size_t i = size_; i > insert_idx; i--) {
            data_[i] = std::move(data_[i - 1]);
        }
        data_[insert_idx] = value;
        size_++;
        return data_.data() + insert_idx;
    }

    T* begin() {
        return data_.data();
    }

    const T* begin() const {
        return data_.data();
    }

    T* end() {
        return data_.data() + size_;
    }

    const T* end() const {
        return data_.data() + size_;
    }
};

template<typename T, size_t N>
ostream& operator<<(ostream& os, const vec_array<T, N>& rhs) {
    for (size_t i = 0; i < rhs.size(); i++) {
        os << rhs[i] << " ";
    }
    os << endl;
    return os;
}

#endif //UNTITLED_UTIL_H
