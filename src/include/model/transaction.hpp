#pragma once
#include "util/numeric_id.hpp"
#include "account.hpp"

namespace sched_bench { namespace model {

struct Transaction {
    typedef util::Numeric_id<Transaction> Id;
    
    Transaction(Id _id, std::vector<Account::Id> _accounts )
        : id(_id)
        , accounts(std::move(_accounts))
    {
    }

    Id id;
    std::vector<Account::Id> accounts;
};

}}
