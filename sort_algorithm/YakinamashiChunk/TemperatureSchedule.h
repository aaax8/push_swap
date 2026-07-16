// sort_algorithm/YakinamashiChunk/TemperatureSchedule.h
#ifndef TEMPERATURE_SCHEDULE_H
#define TEMPERATURE_SCHEDULE_H

#include "../../all_include.h"

namespace YakinamashiChunk {

// //x 線形冷却スケジュール
// 初期温度から最終温度まで、経過時間に比例して線形に冷却
class TemperatureSchedule {
private:
    double initial_temp;  // 初期温度
    double final_temp;    // 最終温度
    double total_time;    // 総時間（秒）
    
public:
    // //x コンストラクタ
    TemperatureSchedule(double init_t, double final_t, double total_t)
        : initial_temp(init_t), final_temp(final_t), total_time(total_t) {
        my_assert(init_t > 0.0);
        my_assert(final_t >= 0.0);
        my_assert(init_t >= final_t);
        my_assert(total_t > 0.0);
    }
    
    // //x 経過時間から現在温度を計算（線形冷却）
    // T(t) = initial_temp - (initial_temp - final_temp) * (t / total_time)
    double get_temperature(double progress) const {
        my_assert(0.0 <= progress && progress <= 1.0 + 0.001);
        
        // 時間超過時は最終温度
        if (progress >= 1.0) {
            return final_temp;
        }
        
        double temp = initial_temp + (final_temp - initial_temp) * progress;
        return temp;
    }
};

}  // namespace YakinamashiChunk

#endif
