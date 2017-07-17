#pragma once

namespace sched_bench { namespace util {

template<typename TAG>
class Numeric_id {
public:
    typedef uint32_t storage_type;

    explicit Numeric_id(storage_type v)
        :value(v)
    {
    }

    Numeric_id(Numeric_id<TAG> const &) = default;
    Numeric_id<TAG>& operator= (Numeric_id<TAG> const &) = default;

    friend bool operator==( Numeric_id<TAG> const &l, Numeric_id<TAG>const &r ) {
        return l.value == r.value;
    };

    friend bool operator!=( Numeric_id<TAG> const &l, Numeric_id<TAG>const &r ) {
        return !(l == r);
    };

    friend bool operator<( Numeric_id<TAG> const &l, Numeric_id<TAG>const &r ) {
        return l.value < r.value;
    };

    storage_type as_numeric() const { 
        return value; 
    }
private:
    storage_type value;
};

}}
