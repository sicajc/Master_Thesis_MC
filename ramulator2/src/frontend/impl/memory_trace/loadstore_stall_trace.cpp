#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>

#include "base/exception.h"
#include "base/request.h"
#include "base/base.h"

#include "frontend/frontend.h"
#include "loadstore_stall_trace.h"

namespace Ramulator
{
  namespace fs = std::filesystem;

  bool   m_wait_write_req_burst = false;

  int    m_write_req_delay_cycle = 1; // Write burst cycle delay

  size_t    m_received_request_in_interval  = 0;
  size_t    m_total_received_request = 0;
  float  s_peak_bandwidth    = -1;
  float  s_worst_bandwidth   = INFINITY;
  float  s_average_bandwidth = -1;
  int    m_bandwidth_sample_time_interval = 500;
  int    m_read_datapath_width = 1024;
  std::string m_bandwidth_trace_file_path = "";

  LoadStoreStallCore::LoadStoreStallCore(int clk_ratio, int core_id, size_t num_expected_traces, std::string trace_path_str
  ,std::string returned_trace_path_str,bool is_debug,int bandwidth_sample_time_interval, int read_datapath_width,std::string bandwidth_trace_file_path)
  {
    m_is_debug = is_debug;
    m_num_expected_traces = num_expected_traces;
    m_core_id = core_id;
    m_clock_ratio = clk_ratio;
    m_callback = [this](Request &req)
    { return this->receive(req); }; // Check to see if the request comes back
    init_trace(trace_path_str, returned_trace_path_str);
    // Statistics for bandwidth analysis
    m_returned_trace_file_path_str = returned_trace_path_str;
    m_bandwidth_sample_time_interval = bandwidth_sample_time_interval;
    m_read_datapath_width = read_datapath_width; // in bits
    m_bandwidth_trace_file_path = bandwidth_trace_file_path;
  };

  void LoadStoreStallCore::tick()
  {
    m_clk++;

    if (m_current_stall_cycles > 0) // Processor core stalling
    {
      m_current_stall_cycles--;
      return;
    }

    // If the core finish executing all the traces, it no longer needs to send request
    if(this->is_finished() || (m_curr_trace_idx >= m_num_expected_traces) || (m_curr_trace_idx >= m_trace_length))
    {
      return;
    }

    // write delay
    // if(m_write_req_delay_cycle > 0 && m_wait_write_req_burst == true){
    //   m_write_req_delay_cycle--;
    //   return;
    // }

    // Send another request
    const Trace &t = m_trace[m_curr_trace_idx];

    // Exception, check if the num of expected traces is less than the actual traces
    // Size of m_trace  
    if(m_num_expected_traces > m_trace.size())
    {
      throw ConfigurationError("Number of expected traces exceed the actual number of traces");
    }


    // addr, type, data_type, core_id, callback
    Request request(t.addr, t.is_write ? Request::Type::Write : Request::Type::Read, t.type, m_core_id,m_callback);
    
    
    if(t.is_write == true && m_wait_write_req_burst == false) // If it is a write request
    {
      // start waiting for bursting
      m_wait_write_req_burst = true;
      m_write_req_delay_cycle = 4;
      return;
    } 

    m_wait_write_req_burst = false;
   

    bool request_sent = m_memory_system->send(request);

    if (request_sent)
    {
      if (t.is_write == true) // If the request is a write request, simply mark it as retired
        m_num_retired_traces++;

      m_current_stall_cycles = t.stall_cycles;
      m_curr_trace_idx++;
      m_trace_count++;
    }

    // Bandwidth statistics
    if(m_clk % m_bandwidth_sample_time_interval == 0)
    {
      // Total Received data / Time = (Data bit width*Requests)/Time
      float bandwidth = (float(m_received_request_in_interval*m_read_datapath_width)/float(8))/float(m_bandwidth_sample_time_interval); // in bytes
      if(s_peak_bandwidth < bandwidth)
        s_peak_bandwidth = bandwidth;
      if(s_worst_bandwidth > bandwidth)
        s_worst_bandwidth = bandwidth;

      m_received_request_in_interval = 0;

      // if(m_is_debug)
        // std::cerr << "Bandwidth at " << m_clk << " clk cycle is " << bandwidth << " G_bytes" << std::endl;
    }
  };

  void LoadStoreStallCore::receive(Request &req)
  {
    // print Receive the request at clk cycle addr and core id
    if(m_is_debug)
      std::cerr << req.type_id <<" request received at " << m_clk << " clk cycle addr " << req.addr << " and core id " << m_core_id << " data type " << req.data_type << " (0=weight 1=KV$)" << std::endl;

    m_waiting_for_request = false;

    // Write the request to the returned trace file in the following format
    // clk, request address, core id, data_type
    m_returned_trace_file << m_clk << " " << req.addr << " " << m_core_id << " " << req.data_type << std::endl;

    // Staistics, calculate the bandwidth, and display on the screen
    m_received_request_in_interval++;
    m_total_received_request++;

    m_num_retired_traces++;
  };

  void LoadStoreStallCore::connect_memory_system(IMemorySystem *memory_system)
  {
    m_memory_system = memory_system;
  };


  void LoadStoreStallCore::init_trace(const std::string &file_path_str, const std::string &returned_trace_path_str)
  {
    fs::path trace_path(file_path_str);
    fs::path returned_trace_path(returned_trace_path_str);

    if (!fs::exists(trace_path))
    {
      throw ConfigurationError("Trace {} does not exist!", file_path_str);
    }

    if (!fs::exists(returned_trace_path))
    {
      throw ConfigurationError("Folder for return trace {} does not exists", returned_trace_path_str);
    }

    std::ifstream trace_file(trace_path);
    if (!trace_file.is_open())
    {
      throw ConfigurationError("Trace {} cannot be opened!", file_path_str);
    }

    std::string line;
    while (std::getline(trace_file, line))
    {
      std::vector<std::string> tokens;
      tokenize(tokens, line, " ");

      // cmd addr       stall_cycles  weight(0)/KV$(1)
      // LD  0x12345678 3

      // TODO: Add line number here for better error messages
      if (tokens.size() != 4)
      {
        throw ConfigurationError("Trace {} format size invalid!", file_path_str);
      }

      bool is_write = false;
      if (tokens[0] == "LD")
      {
        is_write = false;
      }
      else if (tokens[0] == "ST")
      {
        is_write = true;
      }
      else
      {
        //std::cout << tokens[0] << std::endl;
        throw ConfigurationError("Trace {} format LD/ST invalid!", file_path_str);
      }

      Addr_t addr = -1;
      if (tokens[1].compare(0, 2, "0x") == 0 |
          tokens[1].compare(0, 2, "0X") == 0)
      {
        addr = std::stoll(tokens[1].substr(2), nullptr, 16);
      }
      else
      {
        addr = std::stoll(tokens[1]);
      }

      int stall_cycles = std::stoi(tokens[2]);
      int type = std::stoi(tokens[3]);
      if(tokens[3] != "0" && tokens[3] != "1")
      {
        throw ConfigurationError("Trace {} format type invalid!", file_path_str);
      }

      m_trace.push_back({is_write, addr, stall_cycles, type});
    }

    trace_file.close();

    m_trace_length = m_trace.size();

    // Create a returned trace file
    std::ofstream returned_trace_file(returned_trace_path / fs::path("returned_request_trace_"+std::to_string(m_core_id) + ".txt"));
    // store it to the variable
    m_returned_trace_file = std::move(returned_trace_file);

    // Create a bandwidth trace file
    std::ofstream bandwidth_trace_file(returned_trace_path / fs::path("bandwidth_trace_"+std::to_string(m_core_id) + ".txt"));
    // store it to the variable
    m_bandwidth_trace_file = std::move(bandwidth_trace_file);
  };

  float LoadStoreStallCore::get_peak_bandwidth()
  {
    return s_peak_bandwidth;
  };

  float LoadStoreStallCore::get_worst_bandwidth()
  {
    return s_worst_bandwidth;
  };

  float LoadStoreStallCore::get_average_bandwidth()
  {
    return s_average_bandwidth;
  };

  
  // TODO: FIXME
  bool LoadStoreStallCore::is_finished()
    { 
      // Total Received data / Time = (Data bit width*Requests)/Time
      s_average_bandwidth = float(float(m_total_received_request * m_read_datapath_width)/float(8))/float(m_clk); // in bytes
      // If the core retired enough request, it is finished
      return m_num_retired_traces>=m_num_expected_traces;
    };
}; // namespace Ramulator