#include <atomic>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <map>
#include <iostream>
#include "util/scope_profile.hpp"

namespace sched_bench { namespace util { namespace scope_profile {

static std::map<std::string, std::string> metadata;

static std::thread *output_thread;
static std::atomic_bool done(false);

static std::atomic_uint write_record_offset(0);
static Sample_record *Sample_records = nullptr;
static uint Max_sample_records = 0;

void output_thread_entry(char const *filename, std::chrono::steady_clock::time_point epoch) {
    std::chrono::milliseconds sleep_time {15};
    auto start_date = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::FILE* temp_file = std::tmpfile();
    uint record_offset = 0;

    while(true) {

        uint record_index = record_offset % Max_sample_records;
        volatile Sample_record *check_record = Sample_records + record_index;
        if (check_record->type == Sample_record_type::INVALID) {
            if (done.load()) {
                break;
            } else {
                std::this_thread::sleep_for(sleep_time);
                continue;
            }
        }
        record_offset++;
        auto &ring_record = Sample_records[record_index];
        std::fwrite(&ring_record, sizeof(ring_record), 1, temp_file);        
        ring_record.type = Sample_record_type::INVALID;
    }
    auto end_date = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    // print out the json tracefile using our binary temp file
    std::ofstream profile_trace;
    profile_trace.open(filename);
    // print out preamble

    profile_trace 
        << boost::format {"{ \"traceStartTime\": \"%.24s\" ,\"traceEvents\": [\n" } 
        % std::ctime(&start_date);

    char const *sep = "";
    std::rewind(temp_file);
    Sample_record record;
    while (std::fread(&record, sizeof(record), 1, temp_file) != 0) {
        uint64_t record_time = std::chrono::duration_cast<std::chrono::microseconds>(record.tp - epoch).count();
        profile_trace 
            << sep
            << boost::format { "{\"tid\":%s,\"pid\":%s,\"ts\":%d,\"cat\":\"P\"," }
            % record.tid
            % record.tid
            % record_time
            ;

        if (record.type == Sample_record_type::BEGIN) {
            profile_trace << "\"ph\":\"B\",\"name\":\"" << record.name << "\"}";
        } else if (record.type == Sample_record_type::END) {
            profile_trace << "\"ph\":\"E\"}";
        }

        sep = ",\n";
    }

    profile_trace 
        << boost::format {"]\n, \"traceEndTime\": \"%.24s\"" } % std::ctime(&end_date);

    for (auto const &e: metadata) {
        profile_trace << boost::format {",\n \"%s\":\"%s\""} % e.first % e.second;
    }
        
    profile_trace << "}";
    profile_trace.close();
}

bool init(char const *filename, uint num_sample_records) {
    auto epoch = std::chrono::steady_clock::now();
    Max_sample_records = num_sample_records;
    Sample_records = new Sample_record[num_sample_records];
    output_thread = new std::thread(output_thread_entry, filename, epoch);
    return true;
}

Sample_record &get_record() {
    uint offset = write_record_offset.fetch_add(1);
    uint index = offset % Max_sample_records;

    volatile Sample_record *check_record = Sample_records + index;
    while (check_record->type != Sample_record_type::INVALID) {
        // spin lock waiting for the I/O thread to catch up
    }

    return Sample_records[index];
}

void shutdown() {
    done.store(true);
    std::cout << "Finalizing Trace Data\n";
    output_thread->join();
    delete(Sample_records);
    delete(output_thread);
}

void add_metadata(char const *key, char const * value) {
    metadata.emplace(key, value);    
}

static std::atomic_uint next_thread_id(0);
thread_local boost::optional<uint> this_thread_id;

uint get_thread_id() {
    if (!this_thread_id) {
        uint t_id = next_thread_id.fetch_add(1);
        this_thread_id = boost::optional<uint>(t_id);
    }

    return *this_thread_id;
}
            

}}}