#pragma once
#include <algorithm>
#include <map>
#include "model/transaction.hpp"

namespace sched_bench { namespace algorithms {

using model::Transaction;

struct Graph
{
    std::vector<Transaction::Id> roots;
    std::multimap<Transaction::Id, Transaction::Id> links;

    struct Dispatcher
    {
        Graph const &graph;
        std::map<Transaction::Id, uint> unmet_dependencies;
        std::vector<Transaction::Id> ready;

        std::vector<Transaction::Id> next() {
            static auto empty = std::vector<Transaction::Id>();
            if (ready.size() > 0)
            {
                auto const &id = ready.back();
                ready.pop_back();
                return std::vector<Transaction::Id> {{id}};
            }
            else
            {
                return empty;
            }
        }

        void finalize(std::vector<Transaction::Id> const &dispatch) {
            for (auto const &t: dispatch) {
                auto links = graph.links.equal_range(t);
                for (auto l = links.first; l != links.second; ++l) {
                    unmet_dependencies[l->second]--;
                    if (unmet_dependencies[l->second] == 0) {
                        ready.push_back(l->second);
                        unmet_dependencies.erase(l->second);
                    }
                }
            }
        }

        bool empty() {
            return ready.empty() && unmet_dependencies.empty();
        }
    };

    static Dispatcher create_dispatcher(Graph const &block) {
        auto ready = std::vector<Transaction::Id>(block.roots.begin(), block.roots.end());
        std::map<Transaction::Id, uint> unmet_dependencies;
        for (auto i = block.links.begin(); i != block.links.end(); ++i) {
            unmet_dependencies[i->second]++;
        }

        return Dispatcher {block, unmet_dependencies, ready};
    }
};

Graph graph_by_account_degree(std::vector<Transaction> const &transactions);
Graph graph_by_hash_conflict(std::vector<Transaction> const &transactions);


}}