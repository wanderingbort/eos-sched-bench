#pragma once


namespace sched_bench { namespace util {

template<typename R, typename I, typename OP>
R reduce(I from, I to, R const &init, OP op) {
    R result = init;
    for (auto iter = from; iter != to; ++iter) {
        result = op(*iter, result);
    }

    return result;
}

template<typename I, typename OP>
struct map_types {
    typedef typename std::iterator_traits<I>::value_type value_type;
    typedef typename std::result_of<OP(value_type const &, uint)>::type result_value_type;
    typedef typename std::vector<result_value_type> result_type;
};

template<typename I, typename OP>
typename map_types<I,OP>::result_type
map(I from, I to, OP op) {
    typename map_types<I,OP>::result_type result;
    uint index = 0;
    for (auto iter = from; iter != to; ++index, ++iter) {
        result.push_back(op(*iter, index));
    }

    return result;
}

template<typename C, typename OP>
typename map_types<typename C::iterator,OP>::result_type
map(C const &c, OP op) {
    return map<>(c.begin(), c.end(), op);
}

}}