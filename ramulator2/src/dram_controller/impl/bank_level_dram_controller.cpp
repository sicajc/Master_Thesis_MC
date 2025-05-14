#include "base/base.h"
#include "base/type.h"
#include "dram_controller/controller.h"
#include "memory_system/memory_system.h"
#include <cmath>
#include <iostream>
#include <fstream>

namespace Ramulator
{

  class BankLevelController final : public IDRAMController,
                                    public Implementation
  {
    //
    RAMULATOR_REGISTER_IMPLEMENTATION(IDRAMController, BankLevelController,
                                      "BankLevelController", "A generic DRAM controller.");

  private:
    std::deque<Request> pending;       // A queue for read requests that are about to
                                       // finish (callback after RL)
    std::deque<Request> read_order_q;  // A queue for recording the order of
                                       // in-coming read requests
    std::deque<Request> write_order_q; // A queue for recording the order of
                                       // in-coming write requests

    ReqBuffer m_active_buffer;   // Buffer for requests being served. This has the
                                 // highest priority
    ReqBuffer m_priority_buffer; // Buffer for high-priority requests (e.g.,
                                 // maintenance like refresh).
    ReqBuffer m_read_buffer;     // Read request buffer
    ReqBuffer m_write_buffer;    // Write request buffer
    ReqBuffer m_unified_buffer;
    ReqBuffer m_command_generator_delay_event_queue; // Used to simulate the delay of bank FSM and issue fifo
    
    ReqBuffer m_issue_fifo;

    int m_bank_addr_idx = -1;
    IMemorySystem *m_memory_system;

    float m_wr_low_watermark;
    float m_wr_high_watermark;
    bool m_is_write_mode = false;

    Clk_t m_sample_time = 0;
    Clk_t m_command_generator_delay = 0;
    int m_interval_served_requests = 0;
    int s_total_served_requests = 0;

    size_t s_row_hits = 0;
    size_t s_row_misses = 0;
    size_t s_row_conflicts = 0;
    size_t s_read_row_hits = 0;
    size_t s_read_row_misses = 0;
    size_t s_read_row_conflicts = 0;
    size_t s_write_row_hits = 0;
    size_t s_write_row_misses = 0;
    size_t s_write_row_conflicts = 0;

    size_t m_num_cores = 0;
    std::vector<size_t> s_read_row_hits_per_core;
    std::vector<size_t> s_read_row_misses_per_core;
    std::vector<size_t> s_read_row_conflicts_per_core;

    size_t s_num_read_reqs = 0;
    size_t s_num_write_reqs = 0;
    size_t s_num_other_reqs = 0;
    size_t s_queue_len = 0;
    size_t s_read_queue_len = 0;
    size_t s_write_queue_len = 0;
    size_t s_priority_queue_len = 0;
    float s_queue_len_avg = 0;
    float s_read_queue_len_avg = 0;
    float s_write_queue_len_avg = 0;
    float s_priority_queue_len_avg = 0;

    size_t s_read_latency = 0;
    float s_avg_read_latency = 0;

    float s_average_bandwidth = 0;
    float s_peak_bandwidth = 0;
    float s_worst_bandwidth = INFINITY;

    std::string bandwidth_record_file_dir = "bandwidth_statistics.txt";

  public:
    std::vector<float> bandwidth_sequence; // Vector to store bandwidth values

    void init() override
    {
      m_command_generator_delay = param<Clk_t>("command_generator_delay")
                                      .desc("The delay of the command generator.")
                                      .default_val(1);
      m_sample_time = param<Clk_t>("sample_time")
                          .desc("The time interval to sample the statistics.")
                          .default_val(2000);

      bandwidth_record_file_dir = param<std::string>("bandwidth_record_file")
                                      .desc("The file to record the bandwidth statistics.")
                                      .default_val("../cmd_records/bandwidth_statistics.txt");
      
      // m_unified_buffer.max_size = 1; // Trace here, something is inccorect here
      // m_command_generator_delay_event_queue.max_size = 1;
      m_unified_buffer.set_queue_size(2);
      m_command_generator_delay_event_queue.set_queue_size(2); 


      m_scheduler = create_child_ifce<IScheduler>();
      m_refresh = create_child_ifce<IRefreshManager>();
      m_rowpolicy = create_child_ifce<IRowPolicy>();

      if (m_config["plugins"])
      {
        YAML::Node plugin_configs = m_config["plugins"];
        for (YAML::iterator it = plugin_configs.begin();
             it != plugin_configs.end(); ++it)
        {
          m_plugins.push_back(create_child_ifce<IControllerPlugin>(*it));
        }
      }
    };

    void setup(IFrontEnd *frontend, IMemorySystem *memory_system) override
    {
      m_dram = memory_system->get_ifce<IDRAM>();
      m_bank_addr_idx = m_dram->m_levels("bank");
      m_priority_buffer.max_size = 512 * 3 + 32;

      // Add this to extract the memory system interfaces
      m_memory_system = memory_system;

      m_num_cores = frontend->get_num_cores();

      s_read_row_hits_per_core.resize(m_num_cores, 0);
      s_read_row_misses_per_core.resize(m_num_cores, 0);
      s_read_row_conflicts_per_core.resize(m_num_cores, 0);

      m_logger = Logging::create_logger("GenericDRAMControllerLog_" +
                                        std::to_string(m_channel_id));

      register_stat(s_row_hits).name("row_hits_{}", m_channel_id);
      register_stat(s_row_misses).name("row_misses_{}", m_channel_id);
      register_stat(s_row_conflicts).name("row_conflicts_{}", m_channel_id);
      register_stat(s_read_row_hits).name("read_row_hits_{}", m_channel_id);
      register_stat(s_read_row_misses).name("read_row_misses_{}", m_channel_id);
      register_stat(s_read_row_conflicts)
          .name("read_row_conflicts_{}", m_channel_id);
      register_stat(s_write_row_hits).name("write_row_hits_{}", m_channel_id);
      register_stat(s_write_row_misses).name("write_row_misses_{}", m_channel_id);
      register_stat(s_write_row_conflicts)
          .name("write_row_conflicts_{}", m_channel_id);

      for (size_t core_id = 0; core_id < m_num_cores; core_id++)
      {
        register_stat(s_read_row_hits_per_core[core_id])
            .name("read_row_hits_core_{}", core_id);
        register_stat(s_read_row_misses_per_core[core_id])
            .name("read_row_misses_core_{}", core_id);
        register_stat(s_read_row_conflicts_per_core[core_id])
            .name("read_row_conflicts_core_{}", core_id);
      }

      register_stat(s_num_read_reqs).name("num_read_reqs_{}", m_channel_id);
      register_stat(s_num_write_reqs).name("num_write_reqs_{}", m_channel_id);
      register_stat(s_num_other_reqs).name("num_other_reqs_{}", m_channel_id);
      register_stat(s_queue_len).name("queue_len_{}", m_channel_id);
      register_stat(s_read_queue_len).name("read_queue_len_{}", m_channel_id);
      register_stat(s_write_queue_len).name("write_queue_len_{}", m_channel_id);
      register_stat(s_priority_queue_len)
          .name("priority_queue_len_{}", m_channel_id);
      register_stat(s_queue_len_avg).name("queue_len_avg_{}", m_channel_id);
      register_stat(s_read_queue_len_avg)
          .name("read_queue_len_avg_{}", m_channel_id);
      register_stat(s_write_queue_len_avg)
          .name("write_queue_len_avg_{}", m_channel_id);
      register_stat(s_priority_queue_len_avg)
          .name("priority_queue_len_avg_{}", m_channel_id);

      register_stat(s_read_latency).name("read_latency_{}", m_channel_id);
      register_stat(s_avg_read_latency).name("avg_read_latency_{}", m_channel_id);

      // register_stat(s_average_bandwidth).name("average_bandwidth");
      // register_stat(s_peak_bandwidth).name("peak_bandwidth");
      // register_stat(s_worst_bandwidth).name("worst_bandwidth");
    };

    bool send(Request &req) override
    {
      req.final_command = m_dram->m_request_translations(req.type_id);

      switch (req.type_id)
      { // Statistics
      case Request::Type::Read:
      {
        s_num_read_reqs++;
        break;
      }
      case Request::Type::Write:
      {
        s_num_write_reqs++;
        break;
      }
      default:
      {
        s_num_other_reqs++;
        break;
      }
      }

      // Else, enqueue them to corresponding buffer based on request type id
      bool is_success = false;
      bool raw_exist = false;
      req.arrive = m_clk;

      // Simply enqueue the request to the same unified buffer, since we are now at bank level controller
      if(m_unified_buffer.size() == 0 && m_unified_buffer.is_full() == false)
      {
        if ((req.type_id == Request::Type::Read || req.type_id == Request::Type::Write) )
        {
          is_success = m_unified_buffer.enqueue(req);
        }
        else
        {
          throw std::runtime_error("Invalid request type!");
        }
      }

      if (!is_success)
      {
        // We could not enqueue the request
        req.arrive = -1;
        return false;
      }

      return true;
    };

    bool priority_send(Request &req) override
    {
      req.final_command = m_dram->m_request_translations(req.type_id);

      bool is_success = false;
      is_success = m_unified_buffer.enqueue(req);
      return is_success;
    }

    void tick() override
    {
      m_clk++;

      // Update statistics
      s_queue_len += m_read_buffer.size() + m_write_buffer.size() +
                     m_priority_buffer.size() + pending.size();
      s_read_queue_len += m_read_buffer.size() + pending.size();
      s_write_queue_len += m_write_buffer.size();
      s_priority_queue_len += m_priority_buffer.size();

      // 1. Serve completed reads, this would call the call_back function
      serve_completed_reads();

      // This enters the refresh cycle
      m_refresh->tick();
     

      // 2. Try to find a request to serve.
      ReqBuffer::iterator req_it;
      ReqBuffer *buffer = nullptr;
      bool request_found = schedule_request(req_it, buffer);

       
      // 2.1 Take row policy action
      m_rowpolicy->update(request_found, req_it); // The row policy actions
      

      // 3. Update all plugins
      for (auto plugin : m_plugins)
      {
        plugin->update(request_found, req_it);
      }

      // 4. Finally, issue the commands to serve the request
      if (request_found)
      {
        // If we find a real request to serve
        if (req_it->is_stat_updated == false)
        {
          update_request_stats(req_it);
        }

        m_dram->issue_command(req_it->command, req_it->addr_vec);
        m_refresh->tick(req_it);

        // If we are issuing the last command, set depart clock cycle and move the
        // request to the pending queue
        if (req_it->command == req_it->final_command)
        {
          if (req_it->type_id == Request::Type::Read)
          {
            req_it->depart = m_clk + m_dram->m_read_latency;
            pending.push_back(*req_it);
          }
          else if (req_it->type_id == Request::Type::Write)
          {
            // TODO: Add code to update statistics
          }
          buffer->remove(req_it); // These two removes the dram request from the selected buffer
        }
        else
        {
          if (m_dram->m_command_meta(req_it->command).is_opening)
          {
            m_active_buffer.enqueue(*req_it);
            buffer->remove(req_it);
          }
        }
        
      }

      // Bandwidth calculation
      if (m_clk % m_sample_time == 0 && m_clk != 0)
      {
        float _bandwidth = float(m_interval_served_requests * 128) / float(m_sample_time);

        if (_bandwidth > 0)
        {
          s_peak_bandwidth = std::max(s_peak_bandwidth, _bandwidth);
          s_worst_bandwidth = std::min(s_worst_bandwidth, _bandwidth);
        }

        // Record the bandwidth value
        bandwidth_sequence.push_back(_bandwidth);

        m_interval_served_requests = 0;
      }
    };

  private:
    /**
     * @brief    Helper function to check if a request is hitting an open row
     * @details
     *
     */
    bool is_row_hit(ReqBuffer::iterator &req)
    {
      return m_dram->check_rowbuffer_hit(req->final_command, req->addr_vec);
    }
    /**
     * @brief    Helper function to check if a request is opening a row
     * @details
     *
     */
    bool is_row_open(ReqBuffer::iterator &req)
    {
      return m_dram->check_node_open(req->final_command, req->addr_vec);
    }

    /**
     * @brief
     * @details
     *
     */
    void update_request_stats(ReqBuffer::iterator &req)
    {
      req->is_stat_updated = true;

      if (req->type_id == Request::Type::Read)
      {
        if (is_row_hit(req))
        {
          s_read_row_hits++;
          s_row_hits++;
          if (req->source_id != -1)
            s_read_row_hits_per_core[req->source_id]++;
        }
        else if (is_row_open(req))
        {
          s_read_row_conflicts++;
          s_row_conflicts++;
          if (req->source_id != -1)
            s_read_row_conflicts_per_core[req->source_id]++;
        }
        else
        {
          s_read_row_misses++;
          s_row_misses++;
          if (req->source_id != -1)
            s_read_row_misses_per_core[req->source_id]++;
        }
      }
      else if (req->type_id == Request::Type::Write)
      {
        if (is_row_hit(req))
        {
          s_write_row_hits++;
          s_row_hits++;
        }
        else if (is_row_open(req))
        {
          s_write_row_conflicts++;
          s_row_conflicts++;
        }
        else
        {
          s_write_row_misses++;
          s_row_misses++;
        }
      }
    }

    /**
     * @brief    Helper function to serve the completed read requests
     * @details
     * This function is called at the beginning of the tick() function.
     * It checks the pending queue to see if the top request has received data
     * from DRAM. If so, it finishes this request by calling its callback and
     * poping it from the pending queue.
     */
    void serve_completed_reads()
    {
      if (pending.size())
      {
        // Check the first pending request
        auto &req = pending[0];
        // Since depart is the added delay needed for issuing the req
        if (req.depart <= m_clk)
        {
          // Request received data from dram
          if (req.depart - req.arrive > 1)
          {
            // Check if this requests accesses the DRAM or is being forwarded.
            // TODO add the stats back
            s_read_latency += req.depart - req.arrive;

            // if (req.callback) { // This callback notifies the front ends that
            // the request is done display the req addr and the clk If the request
            // comes from outside (e.g., processor), call its callback std::cerr
            // << "Request served at Clk=" << m_clk
            //           << ", Addr=" << req.addr << ", Type=" << req.type_id
            //           << std::endl;
            // req.callback(req); // First make it execute the callback normally

            // m_logger->debug("Request served at Clk={}, Addr={}, Type={}", m_clk,
            //                 req.addr, req.type_id);
            m_memory_system->ordered_receive(req);
          }
          // Finally, remove this request from the pending queue
          pending.pop_front();
          m_interval_served_requests++;
          s_total_served_requests++;
        }
      };
    };

    /**
     * @brief    Checks if we need to switch to write mode
     *
     */
    void set_write_mode()
    {
      if (!m_is_write_mode) // in read mode
      {
        if ((m_write_buffer.size() > m_wr_high_watermark * m_write_buffer.max_size) || m_read_buffer.size() == 0)
        {
          m_is_write_mode = true;
        }
      }
      else
      {
        if (m_write_buffer.size() == 0 && m_read_buffer.size() != 0) // Issue until the write buffer is empty once entering the write mode during this condition
        {
          m_is_write_mode = false;
        }
      }
    };

    /**
     * @brief    Checks if we need to switch to write mode
     *
     */
    void set_write_mode_raw() {

    };

    /**
     * @brief    Helper function to find a request to schedule from the buffers.
     *
     */
    bool schedule_request(ReqBuffer::iterator &req_it, ReqBuffer *&req_buffer)
    { 
      // Update timing information in event_queue
      if(m_command_generator_delay_event_queue.size() != 0){
        for (auto it = m_command_generator_delay_event_queue.begin(); it != m_command_generator_delay_event_queue.end(); it++){
          if(it->request_issue_delay > 0){
            it->request_issue_delay--;
          }
        }
      }

      // Take one request from the request buffer
      // Updates its delay information to mimics the hardware behaviour and delays
      // Pop the request only if it reaches the count down to zero
      if(m_unified_buffer.size() != 0){
        req_it = m_unified_buffer.begin();
        req_it->request_issue_delay = m_command_generator_delay;
        bool event_enqueue_success;
        
        if(m_command_generator_delay_event_queue.is_full()==false)
          event_enqueue_success =  m_command_generator_delay_event_queue.enqueue(*req_it);
        else
          event_enqueue_success = false;
        
        // If enquing is a success, remove the request from the unified buffer
        if(event_enqueue_success){
          m_unified_buffer.remove(req_it);
        }
      }

      bool request_found = false;
      // 2.1    First, check the act buffer to serve requests that are already
      // activating (avoid useless ACTs)
      if (req_it = m_scheduler->get_best_request(m_active_buffer);
          req_it != m_active_buffer.end())
      {
        if (m_dram->check_ready(req_it->command, req_it->addr_vec))
        {
          request_found = true;
          req_buffer = &m_active_buffer;
        }
      }

      // 2.2    If no requests can be scheduled from the act buffer, check the
      // rest of the buffers
      if (!request_found)
      {
        // 2.2.1    We first check the priority buffer to prioritize e.g.,
        // maintenance requests
        if (m_priority_buffer.size() != 0) // Wait until the unified buffer is empty to schedule to request
        {
          req_buffer = &m_priority_buffer;
          req_it = m_priority_buffer.begin();
          req_it->command = m_dram->get_preq_command(req_it->final_command, req_it->addr_vec);

          request_found = m_dram->check_ready(req_it->command, req_it->addr_vec);
          if (!request_found & m_priority_buffer.size() != 0)
          {
            return false;
          }
        }

        // 2.2.1    If no request to be scheduled in the priority buffer, check
        // the read and write buffers.
        if (!request_found)
        { 
          // Not unified buffer any more, extract from the bank_fsm_issue_fifo_event_buffer
          auto &buffer = m_command_generator_delay_event_queue;
          
          if (req_it = m_scheduler->get_best_request(buffer);req_it != buffer.end() && req_it->request_issue_delay == 0) {
            request_found = m_dram->check_ready(req_it->command, req_it->addr_vec);
            req_buffer = &buffer;
          }
        }
      }

      // 2.3 If we find a request to schedule, we need to check if it will close
      // an opened row in the active buffer.
      if (request_found)
      {
        if (m_dram->m_command_meta(req_it->command).is_closing)
        {
          auto &rowgroup = req_it->addr_vec;
          for (auto _it = m_active_buffer.begin(); _it != m_active_buffer.end();_it++)
          {
            auto &_it_rowgroup = _it->addr_vec;
            bool is_matching = true;
            for (int i = 0; i < m_bank_addr_idx + 1; i++)
            {
              if (_it_rowgroup[i] != rowgroup[i] && _it_rowgroup[i] != -1 &&
                  rowgroup[i] != -1)
              {
                is_matching = false;
                break;
              }
            }
            if (is_matching)
            {
              request_found = false;
              break;
            }
          }
        }
      }

      return request_found;
    }

    void finalize() override
    {
      s_avg_read_latency = (float)s_read_latency / (float)s_num_read_reqs;
      // s_average_bandwidth = float(128) / float(s_avg_read_latency); // In bytes

      s_queue_len_avg = (float)s_queue_len / (float)m_clk;
      s_read_queue_len_avg = (float)s_read_queue_len / (float)m_clk;
      s_write_queue_len_avg = (float)s_write_queue_len / (float)m_clk;
      s_priority_queue_len_avg = (float)s_priority_queue_len / (float)m_clk;

      // Write bandwidth sequence to a file
      // std::ofstream outfile(bandwidth_record_file_dir);
      // for (const auto &bw : bandwidth_sequence)
      // {
      //   outfile << bw << std::endl;
      // }
      // outfile.close();

      return;
    }
  };

} // namespace Ramulator