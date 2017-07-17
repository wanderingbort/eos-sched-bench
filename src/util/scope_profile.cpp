#include <ctime>
#include "util/scope_profile.hpp"

namespace sched_bench { namespace util { namespace scope_profile {

std::ofstream profile_trace;
char const *trace_events_separator = "";
std::chrono::steady_clock::time_point epoch;

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
        << boost::format {"]\n, \"traceEndTime\": \"%.24s\" }" } 
        % std::ctime(&now);
    profile_trace.close();
}

}}}