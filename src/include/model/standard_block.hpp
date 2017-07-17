#pragma once

#include "transaction.hpp"
#include "util/scope_profile.hpp"

namespace sched_bench { namespace model {

struct Standard_Block {
    struct Entry {
        Entry(uint _cycle, uint _thread, Transaction::Id _tid) 
            : cycle(_cycle)
            , thread(_thread)
            , tid(_tid) 
        {}

        uint cycle;
        uint thread;
        Transaction::Id tid;

        friend bool operator<( Entry const &l, Entry const &r ) {
            if (l.cycle < r.cycle) {
                return true;
            } else if (l.cycle == r.cycle) {
                return l.thread < r.thread;
            } 

            return false;
        }
    };

    std::vector<Entry> transactions;

    Standard_Block ( std::vector<Entry> const &_transactions)
        : transactions(std::move(_transactions))
    {
        SCOPE_PROFILE("Sort Transaction Schedule");
        std::sort(transactions.begin(), transactions.end());
    }

    struct Dispatcher {
        struct Span {
            typedef std::vector<Entry>::const_iterator iterator;
            Span(uint _cycle, uint _thread, iterator _begin, iterator _end ) 
                : cycle(_cycle)
                , thread(_thread)
                , begin(_begin)
                , end(_end)
            {

            }

            uint cycle;
            uint thread;
            iterator begin;
            iterator end;
        };


        Standard_Block const &block;
        std::vector<Span> spans;

        uint next_span;
        uint current_cycle;
        uint outstanding_threads;

        std::vector<Transaction::Id> next() {
            static auto empty = std::vector<Transaction::Id>();
            auto span_to_ids = [](Entry const &e, uint) -> Transaction::Id {
                return e.tid;
            };

            if (next_span < spans.size()) {
                auto const &span = spans[next_span];
                if (span.cycle == current_cycle || outstanding_threads == 0) {
                    current_cycle = span.cycle;
                    outstanding_threads++;
                    next_span++;
                    return util::map(span.begin, span.end, span_to_ids);
                }
            }

            return empty;
        }

        void finalize(std::vector<Transaction::Id> const &) {
            outstanding_threads--;
        }

        bool empty() {
            return next_span >= spans.size();
        }
    };

    static Dispatcher create_dispatcher(Standard_Block const &block) {
        std::vector<Dispatcher::Span> spans;
        spans.reserve(block.transactions.size());
        uint current_cycle = block.transactions.front().cycle;
        uint current_thread = block.transactions.front().thread;
        auto start = block.transactions.cbegin();
        for (auto iter = start; iter != block.transactions.cend(); ++iter) {
            auto const &e = *iter;
            if (e.cycle != current_cycle || e.thread != current_thread) {
                spans.emplace_back(current_cycle, current_thread, start, iter);
                start = iter;
                current_cycle = e.cycle;
                current_thread = e.thread;
            }
        }
        spans.emplace_back(current_cycle, current_thread, start, block.transactions.cend());

        spans.shrink_to_fit();
        return Dispatcher {block, spans, 0, 0, 0};
    }
};

} // model 
} // sched_bench