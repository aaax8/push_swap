//
//

#ifndef UNTITLED_DEBUG_H
#define UNTITLED_DEBUG_H


//debugしたくないときは、以下の行をコメントアウトする
//#define DEBUG

#include <iostream>
#include <cstring>

template <class T>
void deb_one(const char* name, const T& value) {
    std::cout << name << " = " << value;
}

// ===== 終端（これが無かった）=====
inline void deb_impl(const char*) {
    // 何もしない
}
// 再帰で可変長処理
template <class T, class... Args>
void deb_impl(const char* names, const T& value, const Args&... args) {
    // 次のカンマを探す
    const char* comma = strchr(names, ',');
    if (!comma) {
        deb_one(names, value);
    } else {
        std::cout.write(names, comma - names);
        std::cout << " = " << value << ", ";
        deb_impl(comma + 1, args...);
    }
}

template <class... Args>
void deb_func(const char* names, const Args&... args) {
    deb_impl(names, args...);
    std::cout << '\n';
}

// マクロ
#define deb(...) deb_func(#__VA_ARGS__, __VA_ARGS__)

#endif //UNTITLED_DEBUG_H
