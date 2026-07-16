//QueriesValidator.h

#pragma once
#include "QueryCommonRI.h"
#include <stdexcept>
#include "QueriesState.h"

namespace QueriesValidator {
//    QueriesState &state;
//public:
//    explicit QueriesValidator(QueriesState &state) : state(state) {}

    //x
    inline void validate_qi(int qi_idx, int queries_size) {
        my_assert(0 <= qi_idx && qi_idx < queries_size);
    }

   inline void validate_to_qi(int to_qi, int queries_size) {
       my_assert(0 <= to_qi && to_qi <= queries_size);
   }
    // 他のバリデーションもここに追加できます


};
