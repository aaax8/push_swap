#pragma once

#include "QueryCommon.h"
#include "YQAliases.h"

// Test用の、QueryCommonの比較粒度を落とすための表現
struct RoughQuery {
    CommonPushType push_type;
    int tar_v;

    bool operator==(const RoughQuery& rhs) const {
        return push_type == rhs.push_type && tar_v == rhs.tar_v;
    }

    bool operator!=(const RoughQuery& rhs) const {
        return !(*this == rhs);
    }
};

inline RoughQuery to_rough_query(const QueryCommon& query) {
    return {query.get_push_type(), query.get_tar_v()};
}

inline my_vector<RoughQuery> to_rough_queries(const QRIList& queries) {
    my_vector<RoughQuery> ret;
    ret.reserve(queries.size());
    for (const auto& qri : queries) {
        ret.push_back(to_rough_query(qri.get_query()));
    }
    return ret;
}
