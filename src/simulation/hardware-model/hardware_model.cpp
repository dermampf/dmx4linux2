// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
// https://github.com/grg/verilator/blob/master/test_c/sim_main.cpp
// https://zipcpu.com/blog/2017/06/21/looking-at-verilator.html
/*
 * make the interrupt a semaphore, that is given if the interrupt is triggered. Let a thread run if the interrupt is trigered.
 */
#include "hardware_model.h"

#include <stdlib.h>
#include "Vtop.h"
#include <verilated.h>
#include <verilated_vcd_c.h>
#include <list>

#include "memoryrequestqueue.hpp"

static vluint64_t g_simulationTime_ns = 1;
static vluint64_t simulationTime_ns()
{
  return g_simulationTime_ns;
}
double sc_time_stamp ()
{
  return simulationTime_ns();
  // Called by $time in Verilog
  // converts to double, to match
  // what SystemC does
}


class BinarySignal
{
private:
  vluint8_t   oldValue;
  vluint8_t & value;
public:
  BinarySignal(vluint8_t & source)
    : value(source)
  {
  }
  bool posedge() const
  {
    return value==1 && oldValue==0;
  }
  bool negedge() const
  {
    return value==0 && oldValue==1;
  }
  void update()
  {
    oldValue = value;
  }
  bool operator () () const
  {
    return value;
  }
};

class SimulationModuleI
{
public:
  virtual void controlOutputs() = 0;
  virtual void handleInputs() = 0;
};


class InterruptHandler  : public SimulationModuleI
{
  vluint8_t &  clk_i;
  vluint8_t &  reset_i;
  vluint8_t &  irq;
  std::vector<std::function<void()>> irqHandlers;
  std::thread m_irqThread;
  bool m_irqReq = false;
  bool m_run = true;

public:
  InterruptHandler(
                   vluint8_t &  _clk_i,
                   vluint8_t &  _reset_i,
                   vluint8_t &  _irq
                   )
    : clk_i(_clk_i),
      reset_i(_reset_i),
      irq(_irq)
  {
    irqHandlers.resize(1);
    m_irqThread = std::thread
      ( [this]()
        {
          while(m_run)
            {
              if (m_irqReq)
                {
                  m_irqReq = false;
                  if (irqHandlers[0])
                    irqHandlers[0]();
                }
              else
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
      );
  }

  virtual ~InterruptHandler()
  {
    m_run = false;
    m_irqThread.join();
  }

  void controlOutputs()
  {
  }

  void handleInputs()
  {
    if (clk_i && irq)
      m_irqReq = true;
  }

  static std::shared_ptr<InterruptHandler> create(Vtop * tb)
  {
    std::shared_ptr<InterruptHandler> irqhandler = std::make_shared<InterruptHandler>
      (
       tb->clk,
       tb->reset,
       tb->o_irq
       );
    return irqhandler;
  }

  void bindInterrupt (int irqno, std::function<void()> handler)
  {
    if (irqno < irqHandlers.size())
      irqHandlers[irqno] = handler;
  }
};


class DmxBus  : public SimulationModuleI
{
  vluint8_t &  dmx_txen;
  vluint8_t &  dmx_tx;
  vluint8_t &  dmx_rx;
  vluint8_t &  dmx_led;
public:
  DmxBus(
         vluint8_t &  _dmx_txen,
         vluint8_t &  _dmx_tx,
         vluint8_t &  _dmx_rx,
         vluint8_t &  _dmx_led
         )
    :  dmx_txen(_dmx_txen),
       dmx_tx(_dmx_tx),
       dmx_rx(_dmx_rx),
       dmx_led(_dmx_led)
  {
  }
  void controlOutputs()
  {
    dmx_rx = dmx_tx;
  }
  void handleInputs()
  {
  }

  static std::shared_ptr<DmxBus> create(Vtop * tb)
    {
     std::shared_ptr<DmxBus> dmxbus = std::make_shared<DmxBus>
     (
      tb->dmx_txen,
      tb->dmx_tx,
      tb->dmx_rx,
      tb->dmx_led
      );
     return dmxbus;
    }
};



class WishboneMaster : public SimulationModuleI
{
private:
  BinarySignal clk_i;
  vluint8_t &  reset_i;
  vluint8_t &  wb_cyc;
  vluint8_t &  wb_stb;
  vluint8_t &  wb_we;
  vluint8_t &  wb_sel;
  vluint32_t & wb_addr;
  vluint32_t & wb_wdata;
  vluint8_t &  wb_stall;
  vluint8_t &  wb_ack;
  vluint8_t &  wb_err;
  vluint32_t & wb_rdata;

  MemoryRequestPtr   pendingRequest;
  MemoryRequestQueue requestQueue;

  int state = 0;
  
public:
  WishboneMaster(
                 vluint8_t  & _clk_i,
                 vluint8_t  & _reset_i,
                 vluint8_t  & _wb_cyc,
                 vluint8_t  & _wb_stb,
                 vluint8_t  & _wb_we,
                 vluint8_t  & _wb_sel,
                 vluint32_t & _wb_addr,
                 vluint32_t & _wb_wdata,
                 vluint8_t  & _wb_stall,
                 vluint8_t  & _wb_ack,
                 vluint8_t  & _wb_err,
                 vluint32_t & _wb_rdata
                 )
    : clk_i(_clk_i),
      reset_i(_reset_i),
      wb_cyc(_wb_cyc),
      wb_stb(_wb_stb),
      wb_we(_wb_we),
      wb_sel(_wb_sel),
      wb_addr(_wb_addr),
      wb_wdata(_wb_wdata),
      wb_stall(_wb_stall),
      wb_ack(_wb_ack),
      wb_err(_wb_err),
      wb_rdata(_wb_rdata)
  {
    wb_cyc = 0;
    wb_stb = 0;
    wb_rdata = 0xdeadbeef;
  }

  static std::shared_ptr<WishboneMaster> create(Vtop * tb)
  {
    std::shared_ptr<WishboneMaster> wbmaster = std::make_shared<WishboneMaster>
      (
       tb->clk,
       tb->reset,
       tb->wb_cyc,
       tb->wb_stb,
       tb->wb_we,
       tb->wb_sel,
       tb->wb_addr,
       tb->wb_wdata,
       tb->wb_stall,
       tb->wb_ack,
       tb->wb_err,
       tb->wb_rdata
       );
    return wbmaster;
  }

  
  /*
   * Has some signals to control the wishbone bus.
   * Has a queue on the requester side.
   */
  void controlOutputs();
  void handleInputs();


  void onWriteError()
  {
    std::cout << std::dec << "[" << simulationTime_ns() << "] write Error" << std::endl;
    if (pendingRequest)
      {
        pendingRequest->setResult(1);
        pendingRequest.reset();
      }
  }
  void onWriteAck()
  {
    if (pendingRequest)
      {
        pendingRequest->setResult(0);
        pendingRequest.reset();
      }
  }
  void onReadError()
  {
    std::cout << std::dec << "[" << simulationTime_ns() << "] read Error" << std::endl;
    if (pendingRequest)
      {
        pendingRequest->setResult(0);
        pendingRequest.reset();
      }
  }
  void onReadAck()
  {
    if (pendingRequest)
      {
        pendingRequest->setResult(wb_rdata);
        pendingRequest.reset();
      }
  }

  void queue(MemoryRequestPtr r)
  {
    requestQueue.queue(r);
  }

  void writeMemory(uint32_t address,
                   uint8_t  mask,
                   uint32_t data)
  {
    requestQueue.writeMemory(address, mask, data);
  }

  uint32_t readMemory(uint32_t address,
                      uint8_t  mask)
  {
    return requestQueue.readMemory(address, mask);
  }

};

void WishboneMaster::controlOutputs()
{
  if (clk_i.posedge())
    {
      if (reset_i)
        {
          wb_cyc = 0;
          wb_stb = 0;
          state = 0;
        }
      else if (state==0) // Idle
        {
          if (!requestQueue.empty())
            {
              pendingRequest = requestQueue.dequeueDontWait();
              if (pendingRequest->isWrite())
                {
                  wb_cyc = 1;
                  wb_stb = 1;
                  wb_sel = pendingRequest->dataMask();
                  wb_we = 1;
                  wb_addr = pendingRequest->address();
                  wb_wdata = pendingRequest->data();
                  state = 1; // write
                }
              else // isRead
                {
                  wb_cyc = 1;
                  wb_stb = 1;
                  wb_sel = pendingRequest->dataMask();
                  wb_we = 0;
                  wb_addr = pendingRequest->address();
                  state = 2; // read
                }
            }
        }
    }
}

void WishboneMaster::handleInputs()
{
  if (clk_i.posedge())
    {
      switch (state)
        {
        case 0: // Idle
          break;

        case 1: // Write
          if (wb_err)
            {
              onWriteError();
              wb_cyc = 0;
              wb_stb = 0;
              wb_we = 0;
              state = 3; // Terminate Request
            }
          else if (wb_ack)
            {
              onWriteAck();
              wb_cyc = 0;
              wb_stb = 0;
              wb_we = 0;
              state = 3; // Terminate Request
            }
          break;

        case 2: // Read
          if (wb_err)
            {
              onReadError();
              wb_cyc = 0;
              wb_stb = 0;
              wb_we = 0;
              state = 3; // Terminate Request
            }
          else if (wb_ack)
            {
              onReadAck();
              wb_cyc = 0;
              wb_stb = 0;
              wb_we = 0;
              state = 3; // Terminate Request
            }
          break;

        case 3: // Terminate
          state = 4; // Idle
          break;
        case 4: // Terminate2
          state = 0; // Idle
          break;
        }
    }

  clk_i.update();
}


class SimulationToplevel
{
public:
  std::mutex              modulesLock;
  std::vector<std::shared_ptr<SimulationModuleI>> modules;
  Vtop *m_tb = 0;
  VerilatedVcdC * trace = 0;
  bool m_run = true;
  std::thread m_busHandler;

public:
  SimulationToplevel()
  {
    // Create an instance of our module under test
    m_tb = new Vtop;
    if (!trace && getenv("HARDWARE_MODEL_VDC_FILENAME"))
      {
        trace = new VerilatedVcdC;
        tb()->trace(trace, 99);
        trace->open(getenv("HARDWARE_MODEL_VDC_FILENAME"));
      }
    start();
  }

  ~SimulationToplevel()
  {
    stop();
    m_busHandler.join();
  }

  Vtop *tb() { return m_tb; }
  bool run() const { return m_run; }
  void stop() { m_run = false; }
  void start()
  {
    m_busHandler = std::thread( [this](){this->simulationLoop();} );
  }

  void addModule (std::shared_ptr<SimulationModuleI> m)
  {
    std::unique_lock<std::mutex> lk(modulesLock);
    modules.push_back(m);
  }

  void simulationLoop()
  {
    tb()->reset = 1;
    tb()->clk = 0;
    // Tick the clock until we are done
    while(run() && !Verilated::gotFinish())
      {
        // change all inputs to the toplevel module  here.

        {
          std::unique_lock<std::mutex> lk(modulesLock);
          for (auto & m : modules)
            m->controlOutputs();
        }

        tb()->eval();
        if (trace)
          {
            trace->dump(simulationTime_ns());
            trace->flush();
          }
      
        // read all outputs from the toplevel module here
        {
          std::unique_lock<std::mutex> lk(modulesLock);
          for (auto & m : modules)
            m->handleInputs();
        }
           
        if (tb()->clk == 1)
          tb()->reset = 0;
        tb()->clk = tb()->clk ? 0 : 1;

        g_simulationTime_ns += 5;
      }
  }
};


class SimulatedDmxUart
{
private:
  SimulationToplevel  sim;
  std::shared_ptr<WishboneMaster> wbmaster;
  std::shared_ptr<InterruptHandler> irqhandler;
  std::shared_ptr<DmxBus> dmxbus;
public:
  SimulatedDmxUart()
  {
    wbmaster = WishboneMaster::create(sim.tb());
    sim.addModule (wbmaster);

    irqhandler = InterruptHandler::create(sim.tb());
    sim.addModule (irqhandler);

    dmxbus = DmxBus::create(sim.tb());
    sim.addModule (dmxbus);
  }

  void writeMemory(uint32_t address,
                   uint8_t  mask,
                   uint32_t data)
  {
    wbmaster->writeMemory(address, mask, data);
  }

  uint32_t readMemory(uint32_t address,
                      uint8_t  mask)
  {
    return wbmaster->readMemory(address, mask);
  }

  void bindInterrupt (int irqno, std::function<void()> handler)
  {
    irqhandler->bindInterrupt(irqno, handler);
  }
};




/* C binding for the UART simulation */

static SimulatedDmxUart * g_dmxuart = 0;
extern "C" void wbWriteMemory(uint32_t address,
                   uint8_t  mask,
                   uint32_t data)
{
  if (g_dmxuart)
    g_dmxuart->writeMemory(address, mask, data);
}

extern "C" uint32_t wbReadMemory(uint32_t address,
                      uint8_t  mask)
{
  return g_dmxuart ? g_dmxuart->readMemory(address, mask) : 0;
}


extern "C" void wbStartup(int argc, char **argv)
{
  if (g_dmxuart == 0)
    {
      Verilated::traceEverOn(true);
      Verilated::commandArgs(argc, argv);
      g_dmxuart = new SimulatedDmxUart();
    }
}

extern "C" void wbCleanup()
{
  if (g_dmxuart == 0)
    {
      SimulatedDmxUart *t = g_dmxuart;
      g_dmxuart = 0;
      delete t;
    }
}

extern "C" void wbBindInterrupt(int irqno,
                                void (*irqfunc)(int irqno, void *arg),
                                void * arg)
{
  if (g_dmxuart && irqfunc)
    {
      g_dmxuart->bindInterrupt
        (
         irqno,
         [irqfunc, irqno, arg]()
         {
           irqfunc(irqno, arg);
         }
         );
    }
}

void wbCppBindInterrupt(int irqno,
                        std::function<void()> handler
                        )
{
  if (g_dmxuart)
    {
      g_dmxuart->bindInterrupt(irqno,handler);
    }
}

