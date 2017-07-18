#pragma once
#include <fstream>
#include <chrono>
#include <boost/format.hpp>
#include <thread>

namespace sched_bench { namespace util {

    namespace scope_profile {
        extern std::ofstream profile_trace;
        extern char const *trace_events_separator;
        extern std::chrono::steady_clock::time_point epoch;
        bool init(char const *filename);
        void add_metadata(char const *key, char const *value);
        uint get_thread_id();
        void shutdown();        

        template<typename ARG>
        void emit_sample_name(ARG arg) {
            profile_trace << arg;
        }

        template<typename ARG, typename ...ARGS>
        void emit_sample_name(ARG arg, ARGS... args) {
            profile_trace << arg;
            emit_sample_name(args...);
        }

        template<typename ...ARGS>
        void emit_sample_begin(ARGS... args) {
            auto now = std::chrono::steady_clock::now();
            uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(now - epoch).count();
            auto tid = get_thread_id();
            
            profile_trace 
                << trace_events_separator
                << boost::format { "{\"tid\":%s,\"pid\":%s,\"ts\":%d,\"ph\":\"B\",\"cat\":\"P\",\"name\":\"" }
                % tid
                % tid
                % now_us
                ;

            emit_sample_name(args...);
            profile_trace <<  "\" }";
            trace_events_separator = ",\n";
        }

        inline
        void emit_sample_end() {
            auto now = std::chrono::steady_clock::now();
            uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(now - epoch).count();
            auto tid = get_thread_id();
            profile_trace 
                << trace_events_separator
                << boost::format { "{\"tid\":%s,\"pid\":%d,\"ts\":%d,\"ph\":\"E\"}" }
                % tid
                % tid
                % now_us
                ;

            trace_events_separator = ",\n";
        }

        class Sample {
            public:
                ~Sample() {
                    emit_sample_end();
                }

                template<typename ...ARGS>
                static Sample create(ARGS... args) {
                    emit_sample_begin(args...);
                    return Sample();
                }

            protected:
                Sample() {};
    
        };

        #define MERGE_TOKENS(a,b)  a##b
        #define LABEL_SAMPLE(a) MERGE_TOKENS(__scope_profile_sample_, a)
        #define UNIQUE_SAMPLE LABEL_SAMPLE(__COUNTER__)

        #define SCOPE_PROFILE(...) auto UNIQUE_SCOPE = ::sched_bench::util::scope_profile::Sample::create(__VA_ARGS__)
        #define SCOPE_PROFILE_FUNCTION() auto UNIQUE_SCOPE = ::sched_bench::util::scope_profile::Sample::create(__FUNCTION__)
        
    }

}}