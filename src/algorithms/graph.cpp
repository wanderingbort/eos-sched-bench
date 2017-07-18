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

static auto initialize_tracking(std::vector<Transaction> const &transactions){
    SCOPE_PROFILE_FUNCTION();
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

static auto calculate_account_degree_map(std::map<Account::Id, Account_tracker> const &account_trackers) {
    SCOPE_PROFILE_FUNCTION();
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

Graph graph_by_account_degree(std::vector<Transaction> const &transactions)
{
    auto maps = initialize_tracking(transactions);
    auto transactions_by_id = maps.first;
    auto account_trackers = maps.second;
    auto accounts_by_degree = calculate_account_degree_map(account_trackers);

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

struct Account_tracker_v2 {
    Account_tracker_v2(Account::Id const &_a_id, std::vector<Transaction const *> _transactions) 
        : a_id(_a_id)
        , transactions(std::move(_transactions))
        , previous(nullptr)
        , degree(transactions.size())
    {
    }

    Account::Id a_id;
    std::vector<Transaction const *> transactions;
    Transaction const *previous;
    uint degree;
};

struct Account_degree_entry {
    uint degree;
    Account_tracker_v2 const *tracker;

    // sort higher degrees to the front of arrays
    friend bool operator<( Account_degree_entry const &l, Account_degree_entry const &r ) {
        return l.degree > r.degree;
    };
};

static auto initialize_tracking_v2(std::vector<Transaction> const &transactions){
    SCOPE_PROFILE_FUNCTION();

    // create per-account buckets of transaction Ids
    std::map<Account::Id, Account_tracker_v2> account_trackers;
    for (auto const &t: transactions) {
        for (auto const &a_id : t.accounts) {
            if (account_trackers.find(a_id) == account_trackers.end()) {
                account_trackers.emplace(a_id, Account_tracker_v2(a_id, {&t}));
            } else {
                account_trackers.at(a_id).transactions.emplace_back(&t);
                account_trackers.at(a_id).degree++;
            }
        }
    }
    return account_trackers;
};

Graph graph_by_account_degree_v2(std::vector<Transaction> const &transactions) {
    auto account_trackers = initialize_tracking_v2(transactions);
    
    auto calculate_sorted_degree_array = [](decltype(account_trackers) const & trackers){
        SCOPE_PROFILE("Initialize Sorted Array");
        std::vector<Account_degree_entry> results;
        results.reserve(trackers.size());
        for (auto &e: trackers) {
            auto &tracker = e.second;
            results.emplace_back(Account_degree_entry {tracker.degree, &tracker});
        }

        std::sort(results.begin(), results.end());
        return results;
    };

    auto sorted_accounts = calculate_sorted_degree_array(account_trackers);

    std::cout << "Highest Degree: " << sorted_accounts.front().degree << std::endl;

    Graph result;

    // iteratively calculate the graph
    while (sorted_accounts.front().degree > 0) {
        SCOPE_PROFILE("Process Highest Degree");
        // select 1 transaction from an account of the highest degree and create an node
        auto highest_degree_account = sorted_accounts.front().tracker;
        uint highest_degree = sorted_accounts.front().degree;
        auto selected_transaction = highest_degree_account->transactions[highest_degree - 1];

        // link the node to any "previous" transactions on all the accounts it references
        int num_previous = 0;
        for(auto const ref_a_id: selected_transaction->accounts) {
            auto &ref_tracker = account_trackers.at(ref_a_id);

            // remove this tracker from its current degree bucket
            uint degree = ref_tracker.degree;
            
            // swap this entry to the end of the range of equal degree if it is not already the last
            auto degree_range = std::equal_range(sorted_accounts.begin(), sorted_accounts.end(), Account_degree_entry {degree, nullptr});
            auto last_of_degree = degree_range.second - 1;
            for (auto i = degree_range.first; i != last_of_degree; ++i) {
                if (i->tracker == &ref_tracker) {
                    std::iter_swap(i, last_of_degree);
                    break;
                }
            }

            last_of_degree->degree--;
            ref_tracker.degree--;

            // if this transaction was not the last (now ignored due to degree) swap it with the last
            if (ref_tracker.transactions[ref_tracker.degree] != selected_transaction) {
                for (uint f_idx = 0; f_idx < ref_tracker.degree; f_idx++) {
                    if (ref_tracker.transactions[f_idx] == selected_transaction) {
                        std::swap(ref_tracker.transactions[f_idx], ref_tracker.transactions[ref_tracker.degree]);
                        break;
                    }
                }                
            }
 
            // store this transaction  as "previous" for all of the accounts it references
            if (ref_tracker.previous) {
                result.links.emplace(ref_tracker.previous->id, selected_transaction->id);
                num_previous++;
            }

            ref_tracker.previous = selected_transaction;
        }

        // if none of the referenced accounts have a previous transaction, this node is a root
        if (num_previous == 0) {
            result.roots.emplace_back(selected_transaction->id);
        }

        // loop until there are no more accounts.
    }

    return result;
}
}}