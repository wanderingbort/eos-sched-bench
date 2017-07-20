#include <iostream>
#include <set>
#include "algorithms/graph.hpp"
#include <boost/optional.hpp>
#include "util/scope_profile.hpp"


namespace sched_bench {  namespace algorithms {
using namespace sched_bench::model;

struct Account_tracker {
    Account_tracker() {
    }
    Account_tracker(std::set<Transaction::Id> _transactions) 
        : transactions(std::move(_transactions))
    {
    }

    std::set<Transaction::Id> transactions;
    boost::optional<Transaction::Id> previous;
};


Graph graph_by_account_degree(std::vector<Transaction> const &transactions)
{
    auto init_maps = [](std::vector<Transaction> const &transactions){
        SCOPE_PROFILE("Map Transactions By ID");
        // create a map of id -> transaction
        std::map<Transaction::Id, Transaction const *> transactions_by_id;
        
        // create per-account buckets of transaction Ids
        std::map<Account::Id, Account_tracker> account_trackers;
        for (auto const &t: transactions) {
            for (auto const &a_id : t.accounts) {
                if (account_trackers.find(a_id) == account_trackers.end()) {
                    account_trackers.emplace(a_id, Account_tracker({t.id}));
                } else {
                    account_trackers[a_id].transactions.insert(t.id);
                }
            }

            transactions_by_id.emplace(t.id, &t);
        }
        return std::make_pair(transactions_by_id, account_trackers);
    };

    auto maps = init_maps(transactions);
    auto transactions_by_id = maps.first;
    auto account_trackers = maps.second;
    
    auto init_account_degrees = [](std::map<Account::Id, Account_tracker> const &account_trackers) {
        SCOPE_PROFILE("Calculate Account Degrees");
        std::map<int, std::set<Account::Id>> accounts_by_degree;
        for(auto const &entry: account_trackers) {
            auto a_id = entry.first;
            auto &tracker = entry.second;

            int degree = tracker.transactions.size();
            if (accounts_by_degree.find(degree) == accounts_by_degree.end()) {
                accounts_by_degree.emplace(degree, std::set<Account::Id>({a_id}));
            } else {
                accounts_by_degree[degree].insert(a_id);
            }
        }

        return accounts_by_degree;
    };

    auto accounts_by_degree = init_account_degrees(account_trackers);

    std::cout << "Highest Degree: " << (*accounts_by_degree.rbegin()).first << std::endl;

    Graph result;

    // iteratively calculate the graph
    while (!account_trackers.empty()) {
        SCOPE_PROFILE("Process Highest Degree");
        // select 1 transaction from an account of the highest degree and create an node
        auto &highest_degree_accounts = (*accounts_by_degree.rbegin()).second;
        auto &selected_tracker = account_trackers[*highest_degree_accounts.begin()];
        auto &selected_transaction = *transactions_by_id[*selected_tracker.transactions.begin()];

        // link the node to any "previous" transactions on all the accounts it references
        int num_previous = 0;
        for(auto const ref_a_id: selected_transaction.accounts) {
            auto &ref_tracker = account_trackers[ref_a_id];

            // remove this tracker from its current degree bucket
            int degree = ref_tracker.transactions.size();
            auto &degree_set = accounts_by_degree[degree];
            degree_set.erase(ref_a_id);
            if (degree_set.size() == 0) {
                accounts_by_degree.erase(degree);
            }

            // remove this transaction and store it as "previous" for all of the accounts it references
            ref_tracker.transactions.erase(selected_transaction.id);
            if (ref_tracker.previous) {
                result.links.emplace(*ref_tracker.previous, selected_transaction.id);
                num_previous++;
            }

            ref_tracker.previous = selected_transaction.id;

            // if this account has no more transactions, remove its tracker
            if (ref_tracker.transactions.size() == 0) {
                account_trackers.erase(ref_a_id);
            } else {
                int degree = ref_tracker.transactions.size();
                // otherwise, re-add it to the degree buckets
                if (accounts_by_degree.find(degree) == accounts_by_degree.end()) {
                    accounts_by_degree.emplace(degree, std::set<Account::Id>({ref_a_id}));
                } else {
                    accounts_by_degree[degree].insert(ref_a_id);
                }
            }
        }

        // if none of the referenced accounts have a previous transaction, this node is a root
        if (num_previous == 0) {
            result.roots.emplace_back(selected_transaction.id);
        }

        // loop until there are no more accounts.
    }

    return result;
   
}

Graph graph_by_hash_conflict(std::vector<Transaction> const &transactions) {
    static const uint HASH_SIZE = 4096;
    std::vector<Transaction const *> prev_hash(HASH_SIZE);
    Graph result;
    result.roots.reserve(transactions.size());

    std::vector<Transaction::Id> previous;
    previous.reserve(64);

    for (auto const &t: transactions) {
        for (auto const &a : t.accounts ) {
            uint hash_index = a.as_numeric() % HASH_SIZE;

            auto &prev = prev_hash.at(hash_index);
            if (prev != nullptr && prev != &t) {
                previous.emplace_back(prev->id);
            }
            prev = &t;
        }

        if (previous.size() == 0) {
            // list this transaction as a root
            result.roots.emplace_back(t.id);
        } else {
            // list the de-duplicated previous transaction IDs as links
            std::sort(previous.begin(), previous.end());
            boost::optional<Transaction::Id> dupe_check;
            
            for (auto const &p_id: previous) {
                if (!dupe_check || p_id != *dupe_check) {
                    result.links.emplace(p_id, t.id);
                }

                dupe_check = boost::make_optional(p_id);
            }
            previous.clear();
        }
    }


    return result;
}

}}