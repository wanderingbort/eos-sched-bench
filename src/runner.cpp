#include <iostream>
#include <set>

#include "runner.hpp"
#include "model/account.hpp"

using namespace sched_bench;
static int calculate_num_accounts(int index, std::vector<double> const &weights, int max_transactions) {
    double sum_weights = 0.0;
    for (int i = weights.size() - 1; i >= 0; --i) {
        sum_weights += weights[i];
        int cutoff = std::lrint(std::ceil(sum_weights * (double)max_transactions));
        if (index < cutoff) {
            return i + 1;
        }
    } 

    return 1;
}

static int calculate_target_account_coverage(std::vector<double> const &weights) {
    double result = 0.0;
    for (int i = 0; i < weights.size(); i++) {
        result += weights[i] * (double)(i + 1);
    }

    return result;
}


std::vector<Transaction> 
Runner::generate_transactions(Config const &config) {
    SCOPE_PROFILE_FUNCTION();
    // randoms
    std::random_device rdev;
    std::mt19937 prng(rdev());
    std::normal_distribution<> pop_dist(config.account_popularity_mean, config.account_popularity_stddev);
    
    // generate accounts
    std::vector<Account::Id> account_bag;
    std::vector<Account> accounts;
    double account_coverage = 0.0;
    double const target_coverage = calculate_target_account_coverage(config.pct_transactions_per_scope_count);
    double max_popularity = 0.0;
    int account_id = 0;
    while(account_coverage < target_coverage) {
        // if (account_id % 25 == 0) {
        //     std::cout << "Generating Account " << account_id << std::endl;
        // }
        double popularity = std::max(0.00001, pop_dist(prng));
        accounts.emplace_back(Account::Id(account_id), popularity);
        uint32_t add_to_bag = std::lrint(std::ceil(popularity * (double)config.transaction_count));
        account_bag.insert(account_bag.cend(), add_to_bag, Account::Id(account_id));

        account_coverage += popularity;
        account_id++;
        max_popularity = std::max(max_popularity, popularity);
    }

    // std::cout << "Max popularity: " << max_popularity << std::endl;

    // shuffle the account bag
    // std::cout << "Account bag size: " << account_bag.size() << std::endl;
    std::shuffle(account_bag.begin(), account_bag.end(), prng);

    // generate transactions
    std::vector<Transaction> transactions;
    transactions.reserve(config.transaction_count);
    for (int i = 0; i < config.transaction_count; ++i) {
        // if (i % (config.transaction_count >> 3) == 0) {
        //     std::cout << "Generating Transaction " << i << std::endl;
        // }

        int num_accounts = calculate_num_accounts(i,config.pct_transactions_per_scope_count, config.transaction_count);

        std::set<Account::Id> referenced_accounts;
        
        auto iter = account_bag.rbegin();
        while (referenced_accounts.size() < num_accounts && iter != account_bag.rend()) {
            // find the next account, not in the current set
            auto const &id = *iter;
            if (referenced_accounts.find(id) == referenced_accounts.end()) {
                referenced_accounts.insert(id);
                account_bag.erase(std::next(iter).base());
            }

            ++iter;
        }

        if (referenced_accounts.size() > 0) {
            transactions.emplace_back(Transaction::Id(i), std::vector<Account::Id>(referenced_accounts.begin(), referenced_accounts.end()));
        }

    }

    std::shuffle(transactions.begin(), transactions.end(), prng);
    return transactions;
}

std::map<Transaction::Id, double> 
Runner::generate_costs(std::vector<Transaction> const &transactions, Config const &config) {
    SCOPE_PROFILE_FUNCTION();
    // randoms
    std::random_device rdev;
    std::mt19937 prng(rdev());
    std::normal_distribution<> cost_dist(config.transaction_cost_ms_mean, config.transaction_cost_ms_stddev);

    std::map<Transaction::Id, double> costs;
    for (auto const &t: transactions) {
        costs.emplace(t.id, std::max(0.001, cost_dist(prng)));
    }

    return costs;
}


