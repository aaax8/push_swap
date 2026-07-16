#include "all_include.h"
#include <fstream>
#include <filesystem>
#include "util.h"

std::ofstream output_file;
std::filesystem::path output_dir;
std::filesystem::path output_file_path;

void out() {
    if (output_file.is_open()) {
        out_file(output_file_path.string());
    } else {
        cout << "" << endl;
    }
}

vector<int> arg_to_vec(int argc, char *argv[]) {
    int size = argc - 1;
    vector<int> ret(size);
    for (int i = 0; i < size; i++) {
        ret[i] = atoi(argv[i + 1]);
    }
    return ret;
}

std::filesystem::path get_project_root(int argc, char *argv[]) {
    std::filesystem::path default_root = ".";

    if (argc > 0) {
        std::filesystem::path exe_path = std::filesystem::absolute(argv[0]);
        std::filesystem::path project_root = exe_path.parent_path();

        while (project_root != project_root.root_path() &&
               !std::filesystem::exists(project_root / "main.cpp") &&
               !std::filesystem::exists(project_root / "CMakeLists.txt")) {
            project_root = project_root.parent_path();
        }

        if (std::filesystem::exists(project_root / "main.cpp") ||
            std::filesystem::exists(project_root / "CMakeLists.txt")) {
            return project_root;
        }
    }

    return default_root;
}

int mod(int a, int mod_v) {
    return ((a % mod_v) + mod_v) % mod_v;
}

bool bget(int mask, int i) {
    return (mask >> i) & 1;
}

int bit(int i) {
    return (1 << i);
}

std::random_device seed_gen;
std::mt19937 engine(seed_gen());
std::mt19937 rng(std::random_device{}());

void set_my_rand_seed(uint32_t seed) {
    rng.seed(seed);
}

int my_popcount(int i) {
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    i = (i + (i >> 4)) & 0x0f0f0f0f;
    i = i + (i >> 8);
    i = i + (i >> 16);
    return i & 0x3f;
}

//a
bool is_sorted_of_ring(int a, int b, int c) {
#ifdef DEBUG
    my_assert(a != b && b != c && a != c);
#endif
    return (a < b && b < c) || (b < c && c < a) || (c < a && a < b);
}

bool is_sorted_of_ring_eq(int a, int b, int c) {
    return (a <= b && b <= c) || (b <= c && c <= a) || (c <= a && a <= b);
}

int my_rand(int a, int b) {
    my_assert(0 <= a && a < b);
    std::uniform_int_distribution<int> dist(a, b - 1);
    return dist(rng);
}

int my_rand(int b) {
    my_assert(b > 0);
    return my_rand(0, b);
}

int rand_except(int a, int b) {
    std::uniform_int_distribution<int> dist(a, b - 1);
    return dist(rng);
}

// a <= x < b の乱数で c を除く
int rand_except(int a, int b, int c) {
    assert(a < b);

    if (b - a <= 1) {
        assert(a != c);
        return a;
    }

    int val;
    do {
        val = std::uniform_int_distribution<int>(a, b - 1)(rng);
    } while (val == c);
    return val;
}
