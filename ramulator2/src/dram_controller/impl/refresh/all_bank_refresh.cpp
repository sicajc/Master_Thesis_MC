#include <vector>

#include "base/base.h"
#include "dram_controller/controller.h"
#include "dram_controller/refresh.h"

namespace Ramulator {

class AllBankRefresh : public IRefreshManager, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IRefreshManager, AllBankRefresh, "AllBank", "All-Bank Refresh scheme.")
  private:
    Clk_t m_clk = 0;
    IDRAM* m_dram;
    IDRAMController* m_ctrl;

    int m_dram_org_levels = -1;
    int m_num_ranks = -1;

    int m_nrefi = -1;
    int m_ref_req_id = -1;
    Clk_t m_next_refresh_cycle = -1;

    int m_num_refresh_blocks = -1;

  public:
    void init() override {
      m_ctrl = cast_parent<IDRAMController>();
    };

    void setup(IFrontEnd* frontend, IMemorySystem* memory_system) override {

      m_num_refresh_blocks = param<int>("num_refresh_blocks").default_val(4); // Number of refresh blocks

      m_dram = m_ctrl->m_dram;

      m_dram_org_levels = m_dram->m_levels.size();
      m_num_ranks = m_dram->get_level_size("rank");

      m_nrefi = m_dram->m_timing_vals("nREFI"); //Gets the Refresh interval from dram device
      m_ref_req_id = m_dram->m_requests("all-bank-refresh"); // Gets the request id for all bank refresh

      m_next_refresh_cycle = m_nrefi; // Use the refresh interval as next refresh cycle
    };
    void tick(ReqBuffer::iterator& req_it){};

    void tick() {
      m_clk++;

      if (m_clk == m_next_refresh_cycle) {
        m_next_refresh_cycle += m_nrefi;
        for (int r = 0; r < m_num_ranks; r++) {
          std::vector<int> addr_vec(m_dram_org_levels, -1);
          addr_vec[0] = m_ctrl->m_channel_id;
          addr_vec[1] = r;
          Request req(addr_vec, m_ref_req_id);

          bool is_success = m_ctrl->priority_send(req);
          if (!is_success) {
            throw std::runtime_error("Failed to send refresh!");
          }
        }
      }
    };

};

class WriteUpdateRefresh : public IRefreshManager, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IRefreshManager, WriteUpdateRefresh, "PartialRefresh", "Write Update Refresh scheme.")
  private:
    Clk_t m_clk = 0;
    IDRAM* m_dram;
    IDRAMController* m_ctrl;

    int m_dram_org_levels = -1;
    int m_dram_row_level = -1;
    int m_num_ranks = -1;
    int m_num_bankgroups = -1;
    int m_num_banks = -1;
    int m_num_rows = -1;

    int refresh_counter;

    int m_nrefi = -1;
    int m_ref_req_id = -1;
    Clk_t m_next_refresh_cycle = -1;

    int m_num_refresh_blocks = param<int>("num_refresh_blocks").default_val(4);
    int m_num_refresh_rows = -1; // Number of refresh rows per bank group
    int* partial_refrest_pointer = new int[m_num_refresh_blocks]();

  public:
    void init() override {
      m_ctrl = cast_parent<IDRAMController>();
    };

    void setup(IFrontEnd* frontend, IMemorySystem* memory_system) override {

      //m_num_refresh_blocks = param<int>("num_refresh_blocks").default_val(4); // Number of refresh blocks

      m_dram = m_ctrl->m_dram;
      refresh_counter = 0;

      m_dram_org_levels = m_dram->m_levels.size();
      m_dram_row_level = m_dram->m_levels("row");
      m_num_ranks = m_dram->get_level_size("rank");
      m_num_bankgroups = m_dram->get_level_size("bankgroup");
      m_num_banks = m_dram->get_level_size("bank");
      m_num_rows = m_dram->get_level_size("row");
      
      m_num_refresh_rows = m_num_rows / m_num_refresh_blocks; // Number of refresh rows per bank group
      if(m_num_refresh_rows == 0) {
        throw std::runtime_error("Number of refresh rows cannot be zero!");
      }

      
      partial_refrest_pointer = new int[m_num_refresh_blocks]();
      

      m_nrefi = m_dram->m_timing_vals("nREFI"); //Gets the Refresh interval from dram device
      m_ref_req_id = m_dram->m_requests("all-bank-refresh"); // Gets the request id for all bank refresh

      m_next_refresh_cycle = m_nrefi; // Use the refresh interval as next refresh cycle
    };

    void tick(){
      m_clk++;

      if (m_clk == m_next_refresh_cycle) {
        m_next_refresh_cycle += m_nrefi;
        for (int r = 0; r < m_num_ranks; r++) {
          if((refresh_counter % m_num_refresh_rows) <= partial_refrest_pointer[refresh_counter/m_num_refresh_rows]) {  
            std::vector<int> addr_vec(m_dram_org_levels, -1);
            addr_vec[0] = m_ctrl->m_channel_id;
            addr_vec[1] = r;
            Request req(addr_vec, m_ref_req_id);

            bool is_success = m_ctrl->priority_send(req);
            if (!is_success) {
              throw std::runtime_error("Failed to send refresh!");
            }
          }
        }

        refresh_counter++;

        if(refresh_counter == m_num_rows) {
          refresh_counter = 0;
        }
      }
    };

    void tick(ReqBuffer::iterator& req_it) {

      
      if(((req_it->addr_vec[m_dram_row_level]) % m_num_refresh_rows) > partial_refrest_pointer[(req_it->addr_vec[m_dram_row_level])/m_num_refresh_rows]) {
        partial_refrest_pointer[(req_it->addr_vec[m_dram_row_level])/m_num_refresh_rows] = req_it->addr_vec[m_dram_row_level] % m_num_refresh_rows;
      }
      

    };

};

}       // namespace Ramulator
