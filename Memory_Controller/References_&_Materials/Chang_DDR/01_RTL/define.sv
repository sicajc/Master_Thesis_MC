//`define PATTERN_DISP_ON

//command defination {cke,cs_n,ras_n,cas_n,we_n}
`define  CMD_POWER_UP       5'b01111
`define  CMD_LOAD_MODE      5'b10000
`define  CMD_REFRESH        5'b10001
`define  CMD_PRECHARGE      5'b10010
`define  CMD_ACTIVE         5'b10011
`define  CMD_WRITE          5'b10100
`define  CMD_READ           5'b10101
`define  CMD_ZQ_CALIBRATION 5'b10110
`define  CMD_NOP            5'b10111
`define  CMD_DESELECT       5'b11111
`define  CMD_POWER_DOWN     5'b01111
`define  CMD_SELF_REFLESH	  5'b00001


//command state naming
`define FSM_WIDTH1      5
`define FSM_POWER_UP    `FSM_WIDTH1'd0
`define FSM_WAIT_TXPR   `FSM_WIDTH1'd1
`define FSM_ZQ          `FSM_WIDTH1'd2
`define FSM_LMR0        `FSM_WIDTH1'd3
`define FSM_LMR1        `FSM_WIDTH1'd4
`define FSM_LMR2        `FSM_WIDTH1'd5
`define FSM_LMR3        `FSM_WIDTH1'd6
`define FSM_WAIT_TMRD   `FSM_WIDTH1'd7
`define FSM_WAIT_TDLLK  `FSM_WIDTH1'd8
`define FSM_IDLE        `FSM_WIDTH1'd9
`define FSM_READY       `FSM_WIDTH1'd10
`define FSM_ACTIVE      `FSM_WIDTH1'd11
`define FSM_POWER_D     `FSM_WIDTH1'd12
`define FSM_REF         `FSM_WIDTH1'd13
`define FSM_WRITE       `FSM_WIDTH1'd14
`define FSM_READ        `FSM_WIDTH1'd15
`define FSM_PRE         `FSM_WIDTH1'd16
`define FSM_WAIT_TRRD   `FSM_WIDTH1'd17
`define FSM_WAIT_TCCD   `FSM_WIDTH1'd18
`define FSM_DLY_WRITE   `FSM_WIDTH1'd19
`define FSM_DLY_READ    `FSM_WIDTH1'd20
`define FSM_WAIT_TRCD   `FSM_WIDTH1'd21
`define FSM_WAIT_TRTW   `FSM_WIDTH1'd22
`define FSM_WAIT_OUT_F  `FSM_WIDTH1'd23
`define FSM_WAIT_TWTR   `FSM_WIDTH1'd24
`define FSM_WAIT_TRTP   `FSM_WIDTH1'd25
`define FSM_WAIT_TW     `FSM_WIDTH1'd26
`define FSM_WAIT_TRP    `FSM_WIDTH1'd27
`define FSM_WAIT_TRAS   `FSM_WIDTH1'd28
`define FSM_WAIT_TRC    `FSM_WIDTH1'd29

//bank state naming
`define FSM_WIDTH2    5
`define B_INITIAL     `FSM_WIDTH2'd0
`define B_IDLE        `FSM_WIDTH2'd1
`define B_ACTIVE      `FSM_WIDTH2'd2
`define B_ACT_CHECK   `FSM_WIDTH2'd3
`define B_WRITE       `FSM_WIDTH2'd4
`define B_READ        `FSM_WIDTH2'd5
`define B_WRITE_CHECK `FSM_WIDTH2'd6
`define B_READ_CHECK  `FSM_WIDTH2'd7
`define B_PRE         `FSM_WIDTH2'd8
`define B_PRE_CHECK   `FSM_WIDTH2'd9
`define B_ACT_STANDBY `FSM_WIDTH2'd10


//dqs control state naming
`define FSM_WIDTH3       4
`define D_IDLE           `FSM_WIDTH3'd0
`define D_WAIT_CL_WRITE  `FSM_WIDTH3'd1
`define D_WAIT_CL_READ   `FSM_WIDTH3'd2
`define D_WRITE1         `FSM_WIDTH3'd3
`define D_WRITE2         `FSM_WIDTH3'd4
`define D_WRITE_F        `FSM_WIDTH3'd5
`define D_READ1          `FSM_WIDTH3'd6
`define D_READ2          `FSM_WIDTH3'd7
`define D_READ_F         `FSM_WIDTH3'd8

//dq control state naming
`define DQ_IDLE          2'd0
`define DQ_OUT           2'd1

/****************************************
time paramemters
*****************************************/
//`define CLK_DEFINE 2000 //3000ps
`define CLK_DEFINE 3 //3ns

//define latency cycles
`define POWER_UP_LATENCY 14
`define CYCLE_TXPR 81
`define CYCLE_TMRD 9  //tMRD = 4 cycles   (4-1) * 3 <- LMR0~LMR3 total waiting time
`define CYCLE_TDLLK 512
`define CYCLE_TRCD 4  //tRCD = 5 cycles, new timing 11000(ps) / 3000(ps) = 4
`define CYCLE_TRC  8 //tRC = 17 cycles, new timing 23000(ps) / 3000(ps) = 8
`define CYCLE_TCCD 4  //tCCD = 4 cycles, same
`define CYCLE_TCL  5  //tCL = CAS Latency, new timing is 14000/3000 = 5
`define CYCLE_TCWL 5  //tCWL = CAS write Latency
`define CYCLE_TWR  5  //tWR = Write Recovery
`define CYCLE_TAL  0  //tAL = Additional Latency  set AL = CL-2  CYCLE_TAL = AL - 1
`define CYCLE_TRRD 4  //tRRD = Active BANK A to Active BANK B min. latency round((7500ps/3000ps))=3
`define CYCLE_TFAW 15 //tFAW = Four Bank Active window (45000ps/3000ps)=15
`define CYCLE_TRTP 4  //tRTP = Read to precharge command delay round((7500ps/3000ps))=3
`define CYCLE_TRP  3  //tRP = precharge period round((13500ps/3000ps))=5, new timing is 7000/3000 = 3
`define CYCLE_TRAS 12 //tRAS = active-to-precharge the same bank latency.  round(36000ps/3000ps)=12
`define CYCLE_TOTAL_WL 5   //CWL + AL
`define CYCLE_TOTAL_RL 5   //CL + AL

`define CYCEL_ODT_OFF 5+0-2  //CWL + AL - 2
`define CYCEL_ODT_ON  5+0-2  //CWL + AL - 2

`define CYCLE_TWTR  4 //write to read command latency : round((7500ps/3000ps))=3
`define CYCLE_TRTW  `CYCLE_TOTAL_RL+`CYCLE_TCCD+2-(`CYCLE_TOTAL_WL)
                     //read to write command latency : RL + tCCD + 2*tCK - WL

/*
//define latency cycles
`define CYCLE_TXPR 81
`define CYCLE_TMRD 9  //tMRD = 4 cycles   (4-1) * 3 <- LMR0~LMR3 total waiting time
`define CYCLE_TDLLK 512
`define CYCLE_TRCD 9  //tRCD = 9 cycles
`define CYCLE_TRC  33 //tRC = 33 cycles
`define CYCLE_TCCD 4  //tCCD = 4 cycles
`define CYCLE_TCL  9  //tCL = CAS Latency
`define CYCLE_TCWL 7  //tCWL = CAS write Latency
`define CYCLE_TWR  10  //tWR = Write Recovery
`define CYCLE_TAL  0  //tAL = Additional Latency  set AL = CL-2  CYCLE_TAL = AL - 1
`define CYCLE_TRRD 4  //tRRD = Active BANK A to Active BANK B min. latency (6000ps/1500ps)=4
`define CYCLE_TFAW 20 //tFAW = Four Bank Active window (30000ps/1500ps)=20
`define CYCLE_TRTP 5  //tRTP = Read to precharge command delay (7500ps/1500ps)=5
`define CYCLE_TRP  9  //tRP = precharge period (13500ps/1500ps)=9
`define CYCLE_TRAS 24 //tRAS = active-to-precharge the same bank latency.  round(35000ps/1500ps)=24
`define CYCLE_TOTAL_WL 7   //CWL + AL
`define CYCLE_TOTAL_RL 9   //CL + AL

`define CYCEL_ODT_OFF 7+0-2  //CWL + AL - 2
`define CYCEL_ODT_ON  7+0-2  //CWL + AL - 2

`define CYCLE_TWTR  5 //write to read command latency : (7500ps/1500ps)=5
`define CYCLE_TRTW  `CYCLE_TOTAL_RL+`CYCLE_TCCD+2-(`CYCLE_TOTAL_WL)
                     //read to write command latency : RL + tCCD + 2*tCK - WL
*/


/****************************************
 mode register configuration
*****************************************/

//---------MODE Register 0------------------------------------
`define BURST_LENGTH   2'b10  // on-the-fly via A12
`define BURST_TYPE     1'b0   // Sequential
`define CAS_LATENCY    3'b001 // CAS = 5
`define DLL_RESET      1'b1   // DLL_RESET on
`define WRITE_RECOVERY 3'b001 // write recovery time : 5
`define PRECHARGE_PD   1'b0   // DLL off


`define MR0_CONFIG {1'b0,`PRECHARGE_PD,`WRITE_RECOVERY,`DLL_RESET,1'b0,`CAS_LATENCY,`BURST_TYPE,1'b0,`BURST_LENGTH}
//------------------------------------------------------------
//---------MODE Register 1------------------------------------
`define DLL_ENABLE        1'b0  //Enable

`define ODS_M1  1'b1
`define ODS_M5  1'b0        //32 Ohm

`define ADD_LATENCY  2'b00  // additive latency = 0
`define WRITE_LEVEL  1'b0   // disable

`define Rtt_M2 1'b1
`define Rtt_M6 1'b0
`define Rtt_M9 1'b0   // Rtt_nom : Non_writes-->60 Ohm; Writes-->60 Ohm

`define TDQS   1'b0 //Disable
`define Q_OFF  1'b0 //Disable

`define MR1_CONFIG {1'b0,`Q_OFF,`TDQS,1'b0,`Rtt_M9,1'b0,`WRITE_LEVEL,`Rtt_M6,`ODS_M5,`ADD_LATENCY,`Rtt_M2,`ODS_M1,`DLL_ENABLE}

//------------------------------------------------------------
//---------MODE Register 2------------------------------------
`define CAS_WRITE_LATENCY 3'b000 // CWL = 5 clock cycles
`define AUTO_SELF_REFRESH 1'b0   // Disable : Manual
`define SELF_REFRESH_TEMP 1'b0   // Normal (0~85 �XC)
`define DYNAMIC_ODT       2'b01  // Rtt_wr = RZQ / 4

`define MR2_CONFIG {3'b000,`DYNAMIC_ODT,1'b0,`SELF_REFRESH_TEMP,`AUTO_SELF_REFRESH,`CAS_WRITE_LATENCY,3'b000}
//------------------------------------------------------------
//---------MODE Register 3------------------------------------

`define MR3_CONFIG 0     // set by default
//------------------------------------------------------------


/*******************************************************
bit width definations
********************************************************/
`define DM_BITS    2
`define BA_BITS    3
`define DQ_BITS    128
`define DQS_BITS   2
`define ROW_BITS   16
`define COL_BITS   4
`define ADDR_BITS  `COL_BITS+`ROW_BITS


`define USER_COMMAND_BITS 31
`define MEM_CTR_COMMAND_BITS 29

// Schedule command defination, the physical IO FSM controlled by current bank state and counters
`define ATCMD_NOP        4'd0
`define ATCMD_READ       4'd1
`define ATCMD_WRITE      4'd2
`define ATCMD_POWER_D    4'd3
`define ATCMD_POWER_U    4'd4
`define ATCMD_REFRESH    4'd5
`define ATCMD_ACTIVE     4'd6
`define ATCMD_PRECHARGE  4'd7

`define BANK_IDLE      2'd0
`define BANK_ACTIVE    2'd1
`define BANK_PRECHARGE 2'd2


`define ISSUE_BUF_PTR_SIZE 4
`define ISSUE_BUF_SIZE 8

`define ISU_FIFO_WIDTH 4+`ADDR_BITS+`BA_BITS //{command , addr , bank}
                           //[20:17]   [16:3] [2:0]

`define OUT_FIFO_WIDTH  2 //{read/write,Burst_Length} ;

`define WDATA_FIFO_WIDTH `DQ_BITS*8+1//{wdata,burst_length}
//------------------------------
//for bank FSM process
//------------------------------
`define PROC_NO    3'd0
`define PROC_WRITE 3'd1
`define PROC_READ  3'd2

`define STALL_WIDTH 5 //{power_d,pre,read,write,act}

//------------------------------
//for cmd_scheduler
//------------------------------
`define BA_PROC_CMD_WIDTH 3
`define BA_INFO_WIDTH `FSM_WIDTH2+`ADDR_BITS+3//`FSM_WIDTH2+14+3 //{ba_state,addr,process_cmd}