#include <vector>

#include "base/base.h"
#include "dram_controller/controller.h"
#include "dram_controller/scheduler.h"
#include "dram_controller/rowpolicy.h"

namespace Ramulator {

class OpenRowPolicy : public IRowPolicy, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IRowPolicy, OpenRowPolicy, "OpenRowPolicy", "Open Row Policy.")
  private:

  public:
    void init() override { };

    void setup(IFrontEnd* frontend, IMemorySystem* memory_system) override { };

    void update(bool request_found, ReqBuffer::iterator& req_it) override {
      // OpenRowPolicy does not need to take any actions
    };
    

};

class AdaptiveRowPolicy : public IRowPolicy, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IRowPolicy, AdaptiveRowPolicy, "AdaptiveRowPolicy", "Adaptive Row Policy.")
  private:
    IDRAM* m_dram;

    int m_PRE_req_id = -1;

    int m_cap = -1;

    int m_rank_level = -1;
    int m_bankgroup_level = -1;
    int m_bank_level = -1;
    int m_row_level = -1;
    int m_col_level = -1;
    int m_num_ranks = -1;
    int m_num_bankgroups = -1;
    int m_num_banks = -1;
    int m_num_rows = -1;
    int m_num_columns = -1;
    

    int s_num_close_reqs = 0;

    std::vector<uint64_t> m_col_accesses;
  public:
    void init() override { };

    void setup(IFrontEnd* frontend, IMemorySystem* memory_system) override {
      m_ctrl = cast_parent<IDRAMController>();
      m_dram = m_ctrl->m_dram;

      m_cap = param<int>("cap").default_val(10000000); // TODO

      m_rank_level = m_dram->m_levels("rank");
      m_bankgroup_level = m_dram->m_levels("bankgroup");
      m_bank_level = m_dram->m_levels("bank");
      m_row_level = m_dram->m_levels("row");
      m_col_level = m_dram->m_levels("column");

      m_PRE_req_id = m_dram->m_requests("close-row");

      m_num_ranks = m_dram->get_level_size("rank");
      m_num_bankgroups = m_dram->get_level_size("bankgroup");
      m_num_banks = m_dram->get_level_size("bank");
      m_num_rows = m_dram->get_level_size("row");
      m_num_columns = m_dram->get_level_size("column");

      m_col_accesses.resize(m_num_banks * m_num_bankgroups * m_num_ranks, 0);

      register_stat(s_num_close_reqs).name("num_close_reqs");
    };

    void update(bool request_found, ReqBuffer::iterator& req_it) override {
      // AdaptiveRowPolicy does not need to take any actions
      if (!request_found)
        return;

      bool is_write = req_it->type_id == Request::Type::Write;
      bool is_read = req_it->type_id == Request::Type::Read;
      
      bool is_last_column = check_if_last_column(req_it); // 你需要實作此函數
      int data_type = extract_data_type(req_it);  // 需要擴充 Request 結構以支援
      
      bool should_close_row = false;
      
      if (is_write) {
          if (data_type == 1) { // KV$
              should_close_row = true; // 一律關 row
          } else if (data_type == 0) { // weight
              if (is_last_column) should_close_row = true;
          }
      } else if (is_read) {
          if (is_last_column) should_close_row = true;
      }
    

      if (m_dram->m_command_meta(req_it->command).is_closing ||
          m_dram->m_command_meta(req_it->command).is_refreshing)  // PRE or REF
      {

        if (req_it->addr_vec[m_bankgroup_level] == -1 && req_it->addr_vec[m_bank_level] == -1) {  // all bank closes
          for (int b = 0; b < m_num_banks; b++) {
            for (int bg = 0; bg < m_num_bankgroups; bg++) {
              int rank_id = req_it->addr_vec[m_rank_level];
              int flat_bank_id = b + bg * m_num_banks + rank_id * m_num_banks * m_num_bankgroups;
              m_col_accesses[flat_bank_id] = 0;
            }
          }
        } else if (req_it->addr_vec[m_bankgroup_level] == -1) {  // same bank closes
          for (int bg = 0; bg < m_num_bankgroups; bg++) {
            int bank_id = req_it->addr_vec[m_bank_level];
            int rank_id = req_it->addr_vec[m_rank_level];
            int flat_bank_id = bank_id + bg * m_num_banks + rank_id * m_num_banks * m_num_bankgroups;
            m_col_accesses[flat_bank_id] = 0;
          }
        } else {  // single bank closes  (PRE, VRR, RDA, WRA)
          int flat_bank_id = req_it->addr_vec[m_bank_level] +
                             req_it->addr_vec[m_bankgroup_level] * m_num_banks +
                             req_it->addr_vec[m_rank_level] * m_num_banks * m_num_bankgroups;

          m_col_accesses[flat_bank_id] = 0;
        }
      } else if (m_dram->m_command_meta(req_it->command).is_accessing)  // RD or WR
      {
        int flat_bank_id = req_it->addr_vec[m_bank_level] +
                           req_it->addr_vec[m_bankgroup_level] * m_num_banks +
                           req_it->addr_vec[m_rank_level] * m_num_banks * m_num_bankgroups;

        m_col_accesses[flat_bank_id]++;

        if (should_close_row) {
          Request req(req_it->addr_vec, m_PRE_req_id);
          m_ctrl->priority_send(req);
          m_col_accesses[flat_bank_id] = 0;
          s_num_close_reqs++;
        }
      }
    };

    bool check_if_last_column(const ReqBuffer::iterator& req_it) {
        // 檢查是否為最後一個 column
        // Hint: 你可以使用 m_dram->m_command_meta(req_it->command).is_last_column 來取得是否為最後一個 column
        return req_it->addr_vec[m_col_level] == m_num_columns - 1;
        
    }

    int extract_data_type(const ReqBuffer::iterator& req_it) {
        // 從 request 中取得 data type
        // Hint: 你可以使用 req_it->data_type 來取得 data type
        return req_it->data_type;
    };

    
};

class ClosedRowPolicy : public IRowPolicy, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IRowPolicy, ClosedRowPolicy, "ClosedRowPolicy", "Close Row Policy.")
  private:
    IDRAM* m_dram;

    int m_PRE_req_id = -1;

    int m_cap = -1;

    int m_rank_level = -1;
    int m_bankgroup_level = -1;
    int m_bank_level = -1;
    int m_row_level = -1;
    int m_num_ranks = -1;
    int m_num_bankgroups = -1;
    int m_num_banks = -1;

    int s_num_close_reqs = 0;

    std::vector<uint64_t> m_col_accesses;

  public:
    void init() override { };

    void setup(IFrontEnd* frontend, IMemorySystem* memory_system) override {
      m_ctrl = cast_parent<IDRAMController>();
      m_dram = m_ctrl->m_dram;

      m_cap = param<int>("cap").default_val(10000000); // TODO

      m_rank_level = m_dram->m_levels("rank");
      m_bankgroup_level = m_dram->m_levels("bankgroup");
      m_bank_level = m_dram->m_levels("bank");
      m_row_level = m_dram->m_levels("row");

      m_PRE_req_id = m_dram->m_requests("close-row");

      m_num_ranks = m_dram->get_level_size("rank");
      m_num_bankgroups = m_dram->get_level_size("bankgroup");
      m_num_banks = m_dram->get_level_size("bank");

      m_col_accesses.resize(m_num_banks * m_num_bankgroups * m_num_ranks, 0);

      register_stat(s_num_close_reqs).name("num_close_reqs");
    };

    void update(bool request_found, ReqBuffer::iterator& req_it) override {

      if (!request_found)
        return;

      if (m_dram->m_command_meta(req_it->command).is_closing ||
          m_dram->m_command_meta(req_it->command).is_refreshing)  // PRE or REF
      {

        if (req_it->addr_vec[m_bankgroup_level] == -1 && req_it->addr_vec[m_bank_level] == -1) {  // all bank closes
          for (int b = 0; b < m_num_banks; b++) {
            for (int bg = 0; bg < m_num_bankgroups; bg++) {
              int rank_id = req_it->addr_vec[m_rank_level];
              int flat_bank_id = b + bg * m_num_banks + rank_id * m_num_banks * m_num_bankgroups;
              m_col_accesses[flat_bank_id] = 0;
            }
          }
        } else if (req_it->addr_vec[m_bankgroup_level] == -1) {  // same bank closes
          for (int bg = 0; bg < m_num_bankgroups; bg++) {
            int bank_id = req_it->addr_vec[m_bank_level];
            int rank_id = req_it->addr_vec[m_rank_level];
            int flat_bank_id = bank_id + bg * m_num_banks + rank_id * m_num_banks * m_num_bankgroups;
            m_col_accesses[flat_bank_id] = 0;
          }
        } else {  // single bank closes  (PRE, VRR, RDA, WRA)
          int flat_bank_id = req_it->addr_vec[m_bank_level] +
                             req_it->addr_vec[m_bankgroup_level] * m_num_banks +
                             req_it->addr_vec[m_rank_level] * m_num_banks * m_num_bankgroups;

          m_col_accesses[flat_bank_id] = 0;
        }
      } else if (m_dram->m_command_meta(req_it->command).is_accessing)  // RD or WR
      {
        int flat_bank_id = req_it->addr_vec[m_bank_level] +
                           req_it->addr_vec[m_bankgroup_level] * m_num_banks +
                           req_it->addr_vec[m_rank_level] * m_num_banks * m_num_bankgroups;

        m_col_accesses[flat_bank_id]++;

        if (m_col_accesses[flat_bank_id] >= m_cap) {
          Request req(req_it->addr_vec, m_PRE_req_id);
          m_ctrl->priority_send(req);
          m_col_accesses[flat_bank_id] = 0;
          s_num_close_reqs++;
        }
      }
    };
};

}       // namespace Ramulator
