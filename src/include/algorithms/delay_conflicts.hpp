#pragma once

#include "model/standard_block.hpp"
#include "util/functional.hpp"
#include "util/scope_profile.hpp"

namespace sched_bench { namespace algorithms {
using sched_bench::model::Standard_Block;
using sched_bench::model::Transaction;

inline 
Standard_Block delay_conflicts(std::vector<Transaction> const &transactions) {
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

    int cycle = 0;
    std::vector<bool>  used(1024*1024);
    bool scheduled = true;
    while( scheduled ) {
        SCOPE_PROFILE("Process Cycle:", cycle);
        scheduled = false;
        int thread = 0;
        {
            SCOPE_PROFILE("Scan for Transactions");
            for( auto t : current ) {
                bool u = false;
                for( const auto& a : t->accounts ) {
                    if( used[a.as_numeric()%used.size()] ) {
                        u = true;
                        postponed.push_back(t);
                        break;
                    }
                }
                if( !u ) {
                    for( const auto& a : t->accounts ) {
                        used[a.as_numeric()%used.size()] = true;
                    }

                    schedule.emplace_back(cycle, thread++, t->id);
                    scheduled = true;
                }
            }
        }

        {
            SCOPE_PROFILE("Reset Tracking");
            current.resize(0);
            used.resize(0); used.resize(1024*1024);
            std::swap( current, postponed );
            ++cycle;        
        }
    } 
    return Standard_Block(schedule);
}


}}