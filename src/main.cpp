#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include "runner.hpp"
#include "algorithms/delay_conflicts.hpp"
#include "algorithms/single_thread.hpp"
#include "algorithms/graph.hpp"
#include "util/functional.hpp"
#include "util/scope_profile.hpp"

using namespace sched_bench;
namespace po = boost::program_options;

boost::optional<Runner::Config> parse_options(int argc, char *argv[]) {
    boost::optional<Runner::Config> no_config;
    Runner::Config config;
    std::string scope_dist_str;

    po::options_description desc("Options:");
    desc.add_options()
        ("help", "show this help message")
        ("transactions,t", po::value<uint>(&config.transaction_count)->default_value(1000), "The number of transactions to simulate")
        ("threads,n", po::value<uint>(&config.thread_count)->default_value(20), "The number of threads to simulate during analysis")
        ("avg-cost",po::value<double>(&config.transaction_cost_ms_mean)->default_value(0.3), "The average cost of a transaction in milliseconds")
        ("stddev-cost",po::value<double>(&config.transaction_cost_ms_stddev)->default_value(0.25), "The standard-deviation cost of a transaction in milliseconds")
        ("avg-popularity",po::value<double>(&config.account_popularity_mean)->default_value(0.01), "The average percentage of transactions that a scope appears in")
        ("stddev-popularity",po::value<double>(&config.account_popularity_stddev)->default_value(0.005), "The standard-deviation percentage of transactions that a scope appears in")
        ("scope-distribution",po::value<std::string>(&scope_dist_str)->default_value("0.1587,0.6827,0.1573,0.0013"), "a comma separated list defining what percentage of transactions reference that many scopes (as a one-based array)")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return no_config;
    }

    try {
        po::notify(vm);
    } catch(std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return no_config;
    } catch(...) {
        std::cerr << "Unknown error parsing command line!" << "\n";
        return no_config;
    }

    std::vector<std::string> scope_pct_strs;
    boost::split(scope_pct_strs, scope_dist_str, boost::is_any_of(","));
    config.pct_transactions_per_scope_count = util::map<>(scope_pct_strs, [](std::string const &str, uint index) -> double {
        return std::stod(str);
    });

    return boost::optional<Runner::Config>(config);    
}

int main(int argc, char *argv[]) {
    
    auto config = parse_options(argc, argv);
    if (!config) {
        return -1;
    }

    util::scope_profile::init("profile.trace");
    
    //print_generated(accounts, transactions);
    auto results = Runner::execute(*config, 
        "single_thread", algorithms::single_thread, 
        "graph_account_degree", algorithms::graph_by_account_degree,
        "delay_conflicts", algorithms::delay_conflicts
    );

    util::scope_profile::shutdown();
    return 0;
}

