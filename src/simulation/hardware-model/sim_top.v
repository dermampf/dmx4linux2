module top(
           clk,
           reset,
           // wishbone bus
           wb_cyc,
           wb_stb,
           wb_we,
           wb_addr,
           wb_wdata,
           wb_sel,
           wb_stall,
           wb_ack,
           wb_rdata,
           wb_err,

           // interrupt
           o_irq,

           // dmx in and out
           dmx_tx, dmx_txen, dmx_rx, dmx_led
           );

   input clk;
   input reset;

  //== Slave connections:
   input           wb_cyc;
   input           wb_stb;
   input           wb_we;
   input [28-1:0]  wb_addr;
   input [32-1:0]  wb_wdata;
   input [4-1:0]   wb_sel;
   output          wb_stall;
   output          wb_ack;
   output [32-1:0] wb_rdata;
   output          wb_err;
   
   output o_irq;

   output [0:0] dmx_tx;
   output [0:0] dmx_txen;
   input  [0:0] dmx_rx;
   output [0:0] dmx_led;


   // place uart here
   uart_top dmxuart0
     (
      .wb_clk_i(clk),
      // Wishbone signals
      
      .wb_rst_i(reset),
      .wb_adr_i(wb_addr[2:0]),
      .wb_dat_i(wb_wdata[7:0]),
      .wb_dat_o(wb_rdata[7:0]),
      .wb_we_i(wb_we),
      .wb_stb_i(wb_stb),
      .wb_cyc_i(wb_cyc),
      .wb_ack_o(wb_ack),
      .wb_sel_i(wb_sel),
      .int_o(o_irq),
      // UART      signals
      // serial input/output
      .stx_pad_o(dmx_tx),
      .srx_pad_i(dmx_rx),
      // modem signals
      .rts_pad_o(dmx_txen),
      .cts_pad_i(),
      .dtr_pad_o(dmx_led),
      .dsr_pad_i(),
      .ri_pad_i(),
      .dcd_pad_i()
`ifdef UART_HAS_BAUDRATE_OUTPUT
      , .baud_o()
`endif
      );
   assign wb_stall = 0;
   assign wb_err = 0;
   assign wb_rdata[31:8] = 0;

endmodule
