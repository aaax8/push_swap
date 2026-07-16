//
//

#ifndef UNTITLED_TIMEKEEPER_H
#define UNTITLED_TIMEKEEPER_H

#include "all_include.h"

// 時間を管理するクラス
class TimeKeeper
{
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    int64_t time_threshold_;

public:
    // 時間制限をミリ秒単位で指定してインスタンスをつくる。
    TimeKeeper(const int64_t &time_threshold)
            : start_time_(std::chrono::high_resolution_clock::now()),
              time_threshold_(time_threshold)
    {
    }

    // インスタンス生成した時から指定した時間制限を超過したか判定する。
    bool isTimeOver() const
    {
        auto diff = std::chrono::high_resolution_clock::now() - this->start_time_;
        return std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() >= time_threshold_;
    }
};

double get_time_sec();

#endif //UNTITLED_TIMEKEEPER_H
