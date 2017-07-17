#include <ctime>
#include <map>
#include "util/scope_profile.hpp"

namespace sched_bench { namespace util { namespace scope_profile {

std::ofstream profile_trace;
char const *trace_events_separator = "";
std::chrono::steady_clock::time_point epoch;

static std::map<std::string, std::string> metadata;

bool init(char const *filename) {
    epoch = std::chrono::steady_clock::now();
    profile_trace.open(filename);
    // print out preamble

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    profile_trace 
        << boost::format {"{ \"traceStartTime\": \"%.24s\" ,\"traceEvents\": [\n" } 
        % std::ctime(&now);
    return true;
}

void shutdown() {
    // print out final stuffl
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    profile_trace 
        << boost::format {"]\n, \"traceEndTime\": \"%.24s\"" } % std::ctime(&now);

    for (auto const &e: metadata) {
        profile_trace << boost::format {",\n \"%s\":\"%s\""} % e.first % e.second;
    }
        
    profile_trace << "}";
    profile_trace.close();
}

void add_metadata(char const *key, char const * value) {
    metadata.emplace(key, value);    
}

}}}