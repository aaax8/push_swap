//
//

#ifndef UNTITLED_TIMEKEEPERDOUBLE_H
#define UNTITLED_TIMEKEEPERDOUBLE_H
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <utility>
#include <random>
#include <assert.h>
#include <math.h>
#include <chrono>
#include <queue>
#include <algorithm>
#include <iostream>
#include <functional>
// 時間をDouble型で管理し、経過時間も取り出せるクラス
class TimeKeeperDouble {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    double time_threshold_;

    double now_time_ = 0;

public:
    // 時間制限をミリ秒単位で指定してインスタンスをつくる。
    TimeKeeperDouble(const double time_threshold)
            : start_time_(std::chrono::high_resolution_clock::now()),
              time_threshold_(time_threshold) {
    }

    // 経過時間をnow_time_に格納する。
    void setNowTime() {
        auto diff = std::chrono::high_resolution_clock::now() - this->start_time_;
        this->now_time_ = std::chrono::duration_cast<std::chrono::microseconds>(diff).count() * 1e-3; // ms
    }

    // 経過時間をnow_time_に取得する。
    double getNowTime() const {
        return this->now_time_;
    }

    // インスタンス生成した時から指定した時間制限を超過したか判定する。
    bool isTimeOver() const {
        return now_time_ >= time_threshold_;
    }
};

#endif //UNTITLED_TIMEKEEPERDOUBLE_H
