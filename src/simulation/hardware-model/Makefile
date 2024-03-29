# Makefile for the simulation

all : libhardware_model.so hw_model_cmdline

UART_DIR=3rdparty/uart16550/rtl/verilog/

VERILOG_FILES=sim_top.v

VERILOG_FILES+=$(UART_DIR)uart_wb.v $(UART_DIR)uart_top.v $(UART_DIR)uart_debug_if.v \
	$(UART_DIR)uart_regs.v $(UART_DIR)uart_receiver.v $(UART_DIR)uart_sync_flops.v \
	$(UART_DIR)uart_transmitter.v $(UART_DIR)uart_tfifo.v $(UART_DIR)uart_rfifo.v $(UART_DIR)raminfr.v


CFLAGS+=-g -ggdb
CXXFLAGS+=-g -ggdb
LFLAGS+=-g -ggdb
LDFLAGS+=-g -ggdb

CFLAGS+=-fPIC
CXXFLAGS+=-fPIC

CXXFLAGS+=-Iobj_dir/ -I/usr/share/verilator/include/
CXXFLAGS+=-std=c++17

VERILATOR_CXX_FILES=/usr/share/verilator/include/verilated.cpp
VERILATOR_CXX_FILES+=/usr/share/verilator/include/verilated_vcd_c.cpp

LDLIBS+=-lpthread

VERILATOR_OBJS+=obj_dir/Vtop.o obj_dir/Vtop__Syms.o # obj_dir/Vtop__Slow.o
VERILATOR_OBJS+=obj_dir/Vtop__Trace__Slow.o
VERILATOR_OBJS+=obj_dir/Vtop__Trace.o

VERILOG_INCLUDE_PATH+=-I.

test_model : hardware_model.cpp test_model.o $(VERILATOR_OBJS) $(VERILATOR_CXX_FILES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

libhardware_model.so : hardware_model.cpp $(VERILATOR_OBJS) $(VERILATOR_CXX_FILES)
	$(CXX) -shared $(CXXFLAGS) -o $@ $^ $(LDLIBS)

obj_dir/Vtop__Trace.cpp obj_dir/Vtop__Trace__Slow.cpp obj_dir/Vtop.cpp obj_dir/Vtop__Syms.cpp: $(VERILOG_FILES) $(VERILOG_FILES)
	verilator --trace -cc -Wno-fatal $(VERILOG_INCLUDE_PATH) --top-module top $(VERILOG_FILES)

hw_model_cmdline : hw_model_cmdline.o
	$(CC) -o $@ $< -L. -lhardware_model

run : test_model
	./test_model

realclean: clean
	-rm -f test_model hw_model_cmdline *.so

clean:
	-rm -rf obj_dir
	-rm -f *~ *.o *.vdc

# See also:
# http://manpages.ubuntu.com/manpages/trusty/man1/verilator.1.html
