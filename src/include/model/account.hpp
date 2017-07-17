#pragma once
#include "util/numeric_id.hpp"

namespace sched_bench { namespace model {

struct Account {
    typedef util::Numeric_id<Account> Id;

    Account(Id _id, double _popularity) 
        : id(_id)
        , popularity(_popularity)
    {
    }

    Id id;
    double popularity;
};

}};
