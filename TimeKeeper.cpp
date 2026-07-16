//
//

#include "TimeKeeper.h"

double get_time_sec(){
    return static_cast<double>(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count())/1000000000;
}
