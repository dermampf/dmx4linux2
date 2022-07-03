# Makefile for the simulation

UART_DIR=3rdparty/uart16550/rtl/verilog/

VERILOG_FILES=sim_top.v

VERILOG_FILES+=$(UART_DIR)uart_wb.v $(UART_DIR)uart_top.v $(UART_DIR)uart_debug_if.v \
	$(UART_DIR)uart_regs.v $(UART_DIR)uart_receiver.v $(UART_DIR)uart_sync_flops.v \
	$(UART_DIR)uart_transmitter.v $(UART_DIR)uart_tfifo.v $(UART_DIR)uart_rfifo.v $(UART_DIR)raminfr.v


CFLAGS+=-g -ggdb
CXXFLAGS+=-g -ggdb
LFLAGS+=-g -ggdb
LDFLAGS+=-g -ggdb

CXXFLAGS+=-Iobj_dir/ -I/usr/share/verilator/include/
CXXFLAGS+=-std=c++17

VERILATOR_CXX_FILES=/usr/share/verilator/include/verilated.cpp
VERILATOR_CXX_FILES+=/usr/share/verilator/include/verilated_vcd_c.cpp

LDLIBS+=-lpthread

VERILATOR_OBJS+=obj_dir/Vtop.o obj_dir/Vtop__Syms.o # obj_dir/Vtop__Slow.o
VERILATOR_OBJS+=obj_dir/Vtop__Trace__Slow.o
VERILATOR_OBJS+=obj_dir/Vtop__Trace.o

VERILOG_INCLUDE_PATH+=-I.

sim_top : hardware_model.cpp test_model.o $(VERILATOR_OBJS) $(VERILATOR_CXX_FILES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

obj_dir/Vtop__Trace.cpp obj_dir/Vtop__Trace__Slow.cpp obj_dir/Vtop.cpp obj_dir/Vtop__Syms.cpp: $(VERILOG_FILES) $(VERILOG_FILES)
	verilator --trace -cc -Wno-fatal $(VERILOG_INCLUDE_PATH) --top-module top $(VERILOG_FILES)

run : sim_top
	./sim_top

clean:
	-rm -rf obj_dir
	-rm -f *~ sim_top *.o *.vdc

# See also:
# http://manpages.ubuntu.com/manpages/trusty/man1/verilator.1.html
