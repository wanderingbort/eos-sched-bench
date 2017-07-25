#pragma once

#include "model/standard_block.hpp"
#include "util/functional.hpp"
#include "util/scope_profile.hpp"

namespace sched_bench { namespace algorithms {
using sched_bench::model::Standard_Block;
using sched_bench::model::Transaction;

inline 
Standard_Block thread_conflicts(std::vector<Transaction> const &transactions) {
    auto next_power_of_two = [](uint input) -> uint {
        if (input == 0) {
            return 0;
        }

        uint result = input;
        result--;
        result |= result >> 1;
        result |= result >> 2;
        result |= result >> 4;
        result |= result >> 8;
        result |= result >> 16;
        result++;
        return result;
    };

    static uint const MAX_TXS_PER_THREAD = 4;
    static std::hash<Account::Id::storage_type> hasher;
    uint HASH_SIZE = std::max<uint>(4096, next_power_of_two(transactions.size() / 8));
    std::vector<boost::optional<uint>> assigned_threads(HASH_SIZE);
    
    std::vector<Standard_Block::Entry> schedule;
    schedule.reserve(transactions.size());

    auto init = [](std::vector<Transaction> const &transactions) {
        SCOPE_PROFILE("Init Current");
        auto current = util::map<>(transactions.begin(), transactions.end(), [](Transaction const &t, uint index)-> Transaction const * {
            return &t;
        });

        return current;
    };

    auto current = init(transactions);
    std::vector<Transaction const *> postponed;
    postponed.reserve(transactions.size());

    std::vector<uint> txs_per_thread;
    txs_per_thread.reserve(HASH_SIZE);

    int cycle = 0;
    bool scheduled = true;
    while( scheduled ) {
        SCOPE_PROFILE("Process Cycle:", cycle);
        scheduled = false;
        uint next_thread = 0;
        {
            SCOPE_PROFILE("Scan for Transactions");
            for( auto t : current ) {
                auto assigned_to = boost::optional<uint>();
                bool postpone = false;
                
                for( const auto& a : t->accounts ) {
                    uint hash_index = hasher(a.as_numeric()) % HASH_SIZE;
                    if( assigned_to && assigned_threads[hash_index] && assigned_to != assigned_threads[hash_index] ) {
                        postpone = true;
                        postponed.push_back(t);
                        break;
                    }

                    if (assigned_threads[hash_index]) {
                        assigned_to = assigned_threads[hash_index];
                    }
                }
                if( !postpone ) {
                    if (!assigned_to) {
                        assigned_to = boost::make_optional<uint>(next_thread++);
                        txs_per_thread.resize(next_thread);
                    } 
                    
                    if (txs_per_thread[*assigned_to] < MAX_TXS_PER_THREAD ) {
                        for( const auto& a : t->accounts ) {
                            uint hash_index = hasher(a.as_numeric()) % HASH_SIZE;
                            assigned_threads[hash_index] = assigned_to;
                        }

                        txs_per_thread[*assigned_to]++;
                        schedule.emplace_back(cycle, *assigned_to, t->id);
                        scheduled = true;
                    } else {
                        postponed.push_back(t);
                    }
                }
            }
        }

        {
            SCOPE_PROFILE("Reset Tracking");
            current.resize(0);
            txs_per_thread.resize(0);
            assigned_threads.resize(0); assigned_threads.resize(HASH_SIZE);
            std::swap( current, postponed );
            ++cycle;        
        }
    } 
    return Standard_Block(schedule);
}


}}