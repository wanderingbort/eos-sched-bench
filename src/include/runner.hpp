#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <fstream>
#include <vector>
#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>
#include "model/transaction.hpp"
#include "util/functional.hpp"
#include "util/scope_profile.hpp"


namespace sched_bench {
using namespace model;

struct Runner {

    struct Results {
        double duration_ms;
        double runtime_est_ms;
        uint transactions_retired;

        bool valid;
        std::string error_message;
    };

    struct Config {
        // transaction generation params
        uint transaction_count;
        double transaction_cost_ms_mean;
        double transaction_cost_ms_stddev;        
        double account_popularity_mean;
        double account_popularity_stddev;
        std::vector<double> pct_transactions_per_scope_count;

        // analysis
        uint thread_count;

        template<typename OP>
        void emit_properties(OP op) const {
            auto scope_dist_strs = util::map<>(pct_transactions_per_scope_count, [](const double &d, uint) -> std::string {
                return std::string {(boost::format{"%0.04f"} % d).str()};
            });

            op("transactionCount", std::to_string(transaction_count).c_str());
            op("scopePopularityMean", (boost::format{"%0.04f"} % account_popularity_mean).str().c_str() );
            op("scopePopularityStddev", (boost::format{"%0.04f"} % account_popularity_stddev).str().c_str() );
            op("scopePopularityMean", (boost::format{"%0.04f"} % account_popularity_mean).str().c_str() );
            op("threadCount", std::to_string(thread_count).c_str() );
            op("transactionCostMean", (boost::format{"%0.04f"} % transaction_cost_ms_mean).str().c_str() );
            op("transactionCostStddev", (boost::format{"%0.04f"} % transaction_cost_ms_stddev).str().c_str() );
            op("scopeDegreeDist", (boost::format{"[%s]"} % boost::algorithm::join(scope_dist_strs, ", ")).str().c_str() );
        }
    };

    static std::vector<Transaction> generate_transactions(Config const &config);
    static std::map<Transaction::Id, double> generate_costs(std::vector<Transaction> const &transactions, Config const &config);

    template<typename SCHED_FN>
    static Results execute_one(std::vector<Transaction> const &transactions, std::map<Transaction::Id, double> const &costs, Config const &config, char const *fn_name, SCHED_FN fn) {
        SCOPE_PROFILE("Execute:", fn_name);
        Results results;
        results.valid = true;
        results.transactions_retired = 0;

        std::cout << boost::format {"Scheduling[%s]\n"} % fn_name;
        auto run_schedule_fn = [](Results &results, SCHED_FN fn, std::vector<Transaction> const &transactions) {
            SCOPE_PROFILE("Schedule");
            auto sched_start = std::chrono::steady_clock::now();
            auto const block = fn(transactions);
            auto sched_end = std::chrono::steady_clock::now();
            results.duration_ms = (std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1, 1000>>>(sched_end - sched_start)).count();
            return block;
        };

        auto block = run_schedule_fn(results, fn, transactions);

        std::cout << boost::format {"Validating/Estimating[%s]\n"} % fn_name;
        {
            SCOPE_PROFILE("Validate/Estimate");
            // validate and estimate
            std::ofstream trace_file((boost::format{"%s.trace"} % fn_name).str());
            std::vector<uint> idle_threads = util::map<>(std::vector<uint>(config.thread_count), [](uint const &, uint i) -> uint { return i; });
            std::reverse(idle_threads.begin(), idle_threads.end());
            double now = 0;
            auto dispatcher = decltype(block)::create_dispatcher(block);
            auto dispatch = dispatcher.next();
            std::multimap<double, std::pair<uint, std::pair<uint, decltype(dispatch)>>> working_threads;
            uint t_id = 0;
            char const *sep = "";

            trace_file << "{ \"traceEvents\": [\n";
            bool done=false;
            while(!done) {

                // get the cost of the sequential transactions
                double cost = util::reduce<>(dispatch.begin(), dispatch.end(), 0.0, [&](Transaction::Id const &id, double cost) -> double {
                    return cost + costs.at(id);
                });

                // find/assign to a thread 
                if (!idle_threads.empty() && !dispatch.empty()) {
                    uint thread_id = idle_threads.back();
                    idle_threads.pop_back();
                    working_threads.emplace(std::make_pair(now + cost, std::make_pair(thread_id, std::make_pair(t_id, dispatch))));

                    trace_file 
                        << sep
                        << boost::format { "{ \"tid\": %d, \"pid\": %d, \"ts\": %d, \"ph\": \"B\", \"cat\": \"EOSThread\", \"name\": \"Dispatch:%d\", \"args\" : { \"transactions\" : [" }
                        % thread_id
                        % thread_id
                        % std::llrint(std::floor(now * 1000.0))
                        % t_id ; 

                    char const *t_sep = "";
                    for (auto const &id: dispatch) {
                        trace_file << t_sep << id.as_numeric();
                        t_sep = ",";
                    }

                    trace_file << "] }}";
                    t_id++;
                    sep = ",\n";

                    // grab the next 
                    dispatch = dispatcher.next();
                } else if (!working_threads.empty()) {
                    // forward time to clear some jobs
                    auto iter = working_threads.begin();
                    auto next_complete = *iter;
                    working_threads.erase(iter);
                    uint thread_id = next_complete.second.first;
                    auto completed_dispatch = next_complete.second.second.second;
                    auto completed_dispatch_id = next_complete.second.second.first;
                    double completed_time = next_complete.first;

                    results.transactions_retired += completed_dispatch.size();

                    dispatcher.finalize(completed_dispatch);
                    idle_threads.push_back(thread_id);
                    now = completed_time;

                    trace_file 
                        << sep
                        << boost::format { "{ \"tid\": %d, \"pid\": %d, \"ts\": %d, \"ph\": \"E\", \"cat\": \"EOSThread\", \"name\": \"Dispatch:%d\" }" }
                        % thread_id
                        % thread_id
                        % std::llrint(std::floor(now * 1000.0))
                        % completed_dispatch_id ; 

                    // if our last dispatch was empty, 
                    if (dispatch.empty()) {
                        dispatch = dispatcher.next();
                    }
                } else if (working_threads.empty()) {
                    // done processing jobs
                    if(!dispatcher.empty()) {
                        results.valid = false;
                        results.error_message = "DEADLOCK: all threads are idle but the dispather is not empty";
                    }

                    done = true;
                }
            }

            if (results.valid) {
                results.runtime_est_ms = now;
            } else {
                results.runtime_est_ms = 0.0;
            }

            trace_file 
                << boost::format { "\n], \"schedulerName\":\"%s\", \"estimatedRuntimeMs\": %f, \"schedulerTimeMs\": %f, \"retiredTransactons\": %d" }
                % fn_name
                % results.runtime_est_ms
                % results.duration_ms
                % results.transactions_retired;

            config.emit_properties([&](char const *k, char const *v) {
                trace_file << boost::format {",\n\"%s\":\"%s\""} % k % v;
            });

            trace_file << "\n}";
                
            trace_file.close();
        }
        
        return results;
    }

    template<typename SCHED_FN>
    static std::vector<Results> execute_all(std::vector<Transaction> const &transactions, std::map<Transaction::Id, double> const &costs, Config const &config, char const *fn_name, SCHED_FN fn) {
        return std::vector<Results>({execute_one(transactions, costs, config, fn_name, fn)});
    }

    template<typename SCHED_FN, typename ...ARGS >
    static std::vector<Results> execute_all(std::vector<Transaction> const &transactions, std::map<Transaction::Id, double> const &costs, Config const &config, char const *fn_name, SCHED_FN fn, ARGS... args) {
        std::vector<Results> results = execute_all(transactions, costs, config, args...);
        results.push_back(execute_one(transactions, costs, config, fn_name, fn));
        return results;
    }

    template<typename ...ARGS >
    static std::vector<Results> execute( Config const &config, ARGS... args) {
        SCOPE_PROFILE("MAIN EXECUTE");
        std::cout 
            << "Config:\n"
            << "  Generation:\n"
            << boost::format {"    Transaction Count: %d\n"} % config.transaction_count
            << boost::format {"    Scope Popularity: %0.04f avg (%0.04f std dev)\n"} % config.account_popularity_mean % config.account_popularity_stddev
            << boost::format {"    Scope Degree Distribution:\n"};

        for (uint degree = 0; degree< config.pct_transactions_per_scope_count.size(); degree++) {
            std::cout << boost::format { "      %d: %0.04f\n" } % (degree + 1) % config.pct_transactions_per_scope_count[degree];
        }

        std::cout
            << "  Analysis:\n"
            << boost::format {"    Simulated Thread Count: %d\n"} % config.thread_count
            << boost::format {"    Transaction Cost: %0.04f avg (%0.04f std dev)\n"} % config.transaction_cost_ms_mean % config.transaction_cost_ms_stddev;

        std::cout << "=====================================\n";

        config.emit_properties(util::scope_profile::add_metadata);
        
        // generate transactions
        auto const transactions = generate_transactions(config);
        auto const costs = generate_costs(transactions, config);

        // execute all schedulers
        auto results = execute_all(transactions, costs, config, args...);
        std::reverse(results.begin(), results.end());
        return results;
    }
};
}