// human_review_begin: checker 単体で 42 checker 互換の判定処理を持つため。
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
// human_review_end

// human_review_begin: stdin の命令名を型で扱い、不正命令を判定しやすくするため。
enum class CheckerCmd {
    Sa,
    Sb,
    Ss,
    Pa,
    Pb,
    Ra,
    Rb,
    Rr,
    Rra,
    Rrb,
    Rrr,
};
// human_review_end

// human_review_begin: checker の A/B スタックをまとめ、操作適用の引数を短く保つため。
// Stacks stores the current checker state so each operation can update A and B together.
struct Stacks {
    std::deque<int> a;
    std::deque<int> b;
};
// human_review_end

// human_review_begin: checker の小さな処理を分離し、main の分岐を読みやすくするため。
bool split_single_arg(const std::string &arg, std::vector<std::string> &tokens);
bool parse_args(int argc, char **argv, std::deque<int> &a);
bool parse_value(const std::string &token, int &value);
bool parse_cmd(const std::string &line, CheckerCmd &cmd);
void apply_cmd(Stacks &stacks, CheckerCmd cmd);
bool is_sorted(const std::deque<int> &a);
// human_review_end

// human_review_begin: argc==2 の "3 2 1" 形式を通常の引数列へ変換するため。
// split_single_arg accepts the quoted push_swap form used by many 42 checkers.
bool split_single_arg(const std::string &arg, std::vector<std::string> &tokens)
{
    std::istringstream iss(arg);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return !tokens.empty();
}
// human_review_end

// human_review_begin: argv の整数検証と重複検証を checker の入力仕様としてまとめるため。
// parse_args builds stack A from argv while rejecting invalid integers and duplicates.
bool parse_args(int argc, char **argv, std::deque<int> &a)
{
    std::vector<std::string> tokens;
    if (argc == 2) {
        if (!split_single_arg(argv[1], tokens)) {
            return false;
        }
    } else {
        for (int i = 1; i < argc; ++i) {
            tokens.emplace_back(argv[i]);
        }
    }

    std::unordered_set<int> seen;
    for (const std::string &token : tokens) {
        int value = 0;
        if (!parse_value(token, value) || seen.count(value) != 0) {
            return false;
        }
        seen.insert(value);
        a.push_back(value);
    }
    return true;
}
// human_review_end

// human_review_begin: 42 checker と同じく int 範囲の整数だけを受理するため。
// parse_value rejects non-digit tokens, partial parses, and values outside int range.
bool parse_value(const std::string &token, int &value)
{
    if (token.empty()) {
        return false;
    }
    std::size_t pos = 0;
    if (token[pos] == '+' || token[pos] == '-') {
        ++pos;
    }
    if (pos == token.size()) {
        return false;
    }
    for (; pos < token.size(); ++pos) {
        if (token[pos] < '0' || token[pos] > '9') {
            return false;
        }
    }

    errno = 0;
    char *end = nullptr;
    const long parsed = std::strtol(token.c_str(), &end, 10);
    if (errno == ERANGE || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}
// human_review_end

// human_review_begin: stdin の各行を 11 種類の有効命令だけに制限するため。
// parse_cmd converts one input line into a checker command or reports invalid input.
bool parse_cmd(const std::string &line, CheckerCmd &cmd)
{
    if (line == "sa") {
        cmd = CheckerCmd::Sa;
    } else if (line == "sb") {
        cmd = CheckerCmd::Sb;
    } else if (line == "ss") {
        cmd = CheckerCmd::Ss;
    } else if (line == "pa") {
        cmd = CheckerCmd::Pa;
    } else if (line == "pb") {
        cmd = CheckerCmd::Pb;
    } else if (line == "ra") {
        cmd = CheckerCmd::Ra;
    } else if (line == "rb") {
        cmd = CheckerCmd::Rb;
    } else if (line == "rr") {
        cmd = CheckerCmd::Rr;
    } else if (line == "rra") {
        cmd = CheckerCmd::Rra;
    } else if (line == "rrb") {
        cmd = CheckerCmd::Rrb;
    } else if (line == "rrr") {
        cmd = CheckerCmd::Rrr;
    } else {
        return false;
    }
    return true;
}
// human_review_end

// human_review_begin: 有効命令を A/B スタックへ適用し、要素不足時は no-op にするため。
// apply_cmd implements the push_swap operation semantics used by checker.
void apply_cmd(Stacks &stacks, CheckerCmd cmd)
{
    auto swap_top = [](std::deque<int> &stack) {
        if (stack.size() >= 2) {
            std::swap(stack[0], stack[1]);
        }
    };
    auto push_top = [](std::deque<int> &dst, std::deque<int> &src) {
        if (!src.empty()) {
            dst.push_front(src.front());
            src.pop_front();
        }
    };
    auto rotate = [](std::deque<int> &stack) {
        if (stack.size() >= 2) {
            const int value = stack.front();
            stack.pop_front();
            stack.push_back(value);
        }
    };
    auto reverse_rotate = [](std::deque<int> &stack) {
        if (stack.size() >= 2) {
            const int value = stack.back();
            stack.pop_back();
            stack.push_front(value);
        }
    };

    switch (cmd) {
        case CheckerCmd::Sa:
            swap_top(stacks.a);
            break;
        case CheckerCmd::Sb:
            swap_top(stacks.b);
            break;
        case CheckerCmd::Ss:
            swap_top(stacks.a);
            swap_top(stacks.b);
            break;
        case CheckerCmd::Pa:
            push_top(stacks.a, stacks.b);
            break;
        case CheckerCmd::Pb:
            push_top(stacks.b, stacks.a);
            break;
        case CheckerCmd::Ra:
            rotate(stacks.a);
            break;
        case CheckerCmd::Rb:
            rotate(stacks.b);
            break;
        case CheckerCmd::Rr:
            rotate(stacks.a);
            rotate(stacks.b);
            break;
        case CheckerCmd::Rra:
            reverse_rotate(stacks.a);
            break;
        case CheckerCmd::Rrb:
            reverse_rotate(stacks.b);
            break;
        case CheckerCmd::Rrr:
            reverse_rotate(stacks.a);
            reverse_rotate(stacks.b);
            break;
    }
}
// human_review_end

// human_review_begin: 最終状態が昇順かを checker の OK/KO 判定に使うため。
// is_sorted checks whether stack A is strictly ascending after duplicate validation.
bool is_sorted(const std::deque<int> &a)
{
    for (std::size_t i = 1; i < a.size(); ++i) {
        if (a[i - 1] > a[i]) {
            return false;
        }
    }
    return true;
}
// human_review_end

// human_review_begin: checker の入力検証、命令適用、最終 OK/KO 出力を統括するため。
// main follows the 42 checker contract expected by push_swap testers.
int main(int argc, char **argv)
{
    if (argc < 2) {
        return 0;
    }

    Stacks stacks;
    if (!parse_args(argc, argv, stacks.a)) {
        std::cerr << "Error\n";
        return 1;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        CheckerCmd cmd;
        if (!parse_cmd(line, cmd)) {
            std::cerr << "Error\n";
            return 1;
        }
        apply_cmd(stacks, cmd);
    }

    if (stacks.b.empty() && is_sorted(stacks.a)) {
        std::cout << "OK\n";
    } else {
        std::cout << "KO\n";
    }
    return 0;
}
// human_review_end
