#pragma once
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>

namespace sched_bench { namespace util {

    namespace scope_profile {
        enum class Sample_record_type {
            INVALID,
            BEGIN,
            END,
        };

        struct Sample_record_header {
            std::chrono::steady_clock::time_point tp;
            uint tid;
            Sample_record_type type;
        };

        struct Sample_record : public Sample_record_header {
            static const uint DESIRED_RECORD_SIZE = 256;
            static const uint MAX_NAME_LENGTH = DESIRED_RECORD_SIZE - sizeof(Sample_record_header);
            char name[MAX_NAME_LENGTH];
        };

        static_assert(sizeof(Sample_record) == Sample_record::DESIRED_RECORD_SIZE, "unexpected size of struct Sample_record");

        bool init(char const *filename, uint num_sample_record = 4096);
        void add_metadata(char const *key, char const *value);
        uint get_thread_id();
        Sample_record &get_record();
        void shutdown();        

        template<typename ARG>
        struct __format_string {
        };

        template<typename ARG>
        void record_sample_name(char *buf, uint &offset, uint capacity, ARG arg) {
            if (capacity - offset < 1) {
                return;
            }
            offset = std::min(capacity, offset + std::snprintf(buf+offset, capacity-offset, __format_string<ARG>::value, arg));
        }

        template<typename ARG, typename ...ARGS>
        void record_sample_name(char *buf, uint &offset, uint capacity, ARG arg, ARGS... args) {
            record_sample_name(buf, offset, capacity, arg);
            record_sample_name(buf, offset, capacity, args...);
        }

        template<typename ...ARGS>
        void emit_sample_begin(ARGS... args) {
            auto now = std::chrono::steady_clock::now();
            auto tid = get_thread_id();
            auto &record = get_record();
            
            uint offset = 0;
            record_sample_name(record.name, offset, Sample_record::MAX_NAME_LENGTH, args...);
            
            record.tp = now;
            record.tid = tid;
            record.type = Sample_record_type::BEGIN;
        }

        inline
        void emit_sample_end() {
            auto tid = get_thread_id();
            auto &record = get_record();
            auto now = std::chrono::steady_clock::now();
            record.tp = now;
            record.tid = tid;
            record.type = Sample_record_type::END;
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
        

        template<>
        struct __format_string<char const *> {
            static constexpr char const * value = "%s";
        };

        template<>
        struct __format_string<char *> {
            static constexpr char const * value = "%s";
        };

        template<>
        struct __format_string<uint> {
            static constexpr char const * value = "%d";
        };

        template<>
        struct __format_string<int> {
            static constexpr char const * value = "%d";
        };

        template<>
        struct __format_string<float> {
            static constexpr char const * value = "%0.04f";
        };

        template<>
        struct __format_string<double> {
            static constexpr char const * value = "%0.04f";
        };
    }

}}