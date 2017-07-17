#pragma once

#include "model/standard_block.hpp"

namespace sched_bench { namespace algorithms {
using sched_bench::model::Standard_Block;
using sched_bench::model::Transaction;

inline 
Standard_Block single_thread(std::vector<Transaction> const &transactions) {
    static const uint MAX_TRANSACTIONS_PER_THREAD = 5;
    std::vector<Standard_Block::Entry> schedule;
    schedule.reserve(transactions.size());

    for (uint index = 0; index < transactions.size(); index++) {
        uint cycle = index / MAX_TRANSACTIONS_PER_THREAD;
        schedule.emplace_back(cycle, 0, transactions[index].id );
    }

    return Standard_Block(schedule);
}


}}