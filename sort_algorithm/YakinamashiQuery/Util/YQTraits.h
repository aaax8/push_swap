//// filepath: sort_algorithm/YakinamashiQuery/YQTraits.h
//#pragma once
//
//#include <type_traits>
//#include "QueryCommon.h"
//#include "QueryCommonRI.h"
//
//
//// Concept: A2BQuery or B2AQuery only
//template<typename Q>
//concept ABPushQuery = is_same_v<Q, A2BQuery> || is_same_v<Q, B2AQuery>;
//
//template<class Query>
//struct QueryTypeTraits;
//
//template<>
//struct QueryTypeTraits<A2BQuery> {
//    using RIType = A2BQueryRI;
//    static constexpr PastPushType push_type = PastPushType::A2B;
//};
//
//template<>
//struct QueryTypeTraits<B2AQuery> {
//    using RIType = B2AQueryRI;
//    static constexpr PastPushType push_type = PastPushType::B2A;
//};
//
//template<>
//struct QueryTypeTraits<A2AQuery> {
//    using RIType = A2AQueryRI;
//    static constexpr PastPushType push_type = PastPushType::A2A;
//};
//
//
//// Convenient alias
//template<typename Query>
//using GetQueryRI = typename QueryTypeTraits<Query>::RIType;
//
//template<class Query>
//constexpr PastPushType GetPushType = QueryTypeTraits<Query>::push_type;
