#ifndef LOADSTORE_STALL_TRACE_H
#define LOADSTORE_STALL_TRACE_H
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>

#include "base/exception.h"
#include "base/request.h"
#include "base/type.h"
#include "frontend/frontend.h"

namespace Ramulator
{
  class LoadStoreStallCore
  {
    friend class LoadStoreStallTrace;

  private:
    struct Trace
    {
      bool is_write;
      Addr_t addr;
      int stall_cycles;
      int type; // 0: weight, 1: KV$
    };

    std::vector<Trace> m_trace;

    bool m_waiting_for_request = false;
    int m_current_stall_cycles = 0;
    bool m_is_debug = false;

    size_t m_trace_length = 0;
    size_t m_curr_trace_idx = 0;

    size_t m_trace_count = 0;

    int m_core_id = 0;
    size_t m_num_expected_traces = 0;
    size_t m_num_retired_traces  = 0;

    Clk_t m_clk = 0;
    Clk_t m_clock_ratio = 1;

    IMemorySystem *m_memory_system;

    std::string m_returned_trace_file_path_str;
    // a return trace file
    std::ofstream m_returned_trace_file;

    // Bandwidth trace
    std::ofstream m_bandwidth_trace_file;

  public:
    // callback
    std::function<void(Request &)> m_callback;

    LoadStoreStallCore(int clk_ratio, int core_id, size_t num_expected_traces, std::string trace_path_str
      ,std::string returned_trace_path_str,bool is_debug,int bandwidth_sample_time_interval, int read_datapath_width,std::string bandwidth_trace_file_path);

    void tick();

    void receive(Request &req);

    void connect_memory_system(IMemorySystem *memory_system);

    float get_peak_bandwidth();

    float get_worst_bandwidth();

    float get_average_bandwidth();

  private:
    void init_trace(const std::string &file_path_str, const std::string &returned_trace_path_str);

    // TODO: FIXME
    bool is_finished();
  };
} // namespace Ramulator

#endif // LOADSTORE_STALL_TRACE_H