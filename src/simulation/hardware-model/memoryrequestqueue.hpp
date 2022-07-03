#pragma once

#include <iostream>
#include <thread>
#include <queue>
#include <chrono>
#include <mutex>
#include <memory>
#include <future>
#include <cstdint>


class MemoryRequest
{
private:
  uint32_t m_address;
  uint8_t  m_dataMask;
  uint32_t m_data;
  bool     m_isWrite;
  std::promise<uint32_t> m_resultPromise;

public:
  MemoryRequest(uint32_t _address, uint8_t _dataMask, uint32_t _data)
    : m_address(_address),
      m_dataMask(_dataMask),
      m_data(_data),
      m_isWrite(true)
  {
  }
  MemoryRequest(uint32_t _address, uint8_t _dataMask)
    : m_address(_address),
      m_dataMask(_dataMask),
      m_data(0),
      m_isWrite(false)
  {
  }

  uint32_t address() const  { return m_address; }
  uint8_t  dataMask() const { return m_dataMask; }
  uint32_t data() const     { return m_data; }
  bool     isWrite() const  { return m_isWrite; }

  void setResult(uint32_t result)
  {
    m_resultPromise.set_value(result);
  }
  uint32_t getResult()
  {
    std::future<uint32_t> f = m_resultPromise.get_future();
    return f.get();
  }
};
typedef std::shared_ptr<MemoryRequest>  MemoryRequestPtr;

class MemoryRequestQueue
{
private:
  std::queue<MemoryRequestPtr>  q;
  std::mutex              lock;
public:
  bool empty() const
  {
    return q.empty();
  }
  void queue(MemoryRequestPtr m)
  {
    std::unique_lock<std::mutex> lk(lock);
    q.push(m);
  }
  MemoryRequestPtr dequeueDontWait()
  {
    std::unique_lock<std::mutex> lk(lock);
    MemoryRequestPtr msg = q.front();
    q.pop();
    return msg;
  }
  void waitWhileEmpty()
  {
    while (empty())
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  MemoryRequestPtr dequeue()
  {
    waitWhileEmpty();
    return dequeueDontWait();
  }


  void writeMemory(uint32_t address,
                   uint8_t mask,
                   uint32_t value)
  {
    MemoryRequestPtr m(new MemoryRequest(address, mask, value));
    queue(m);
    m->getResult();
  }

  uint32_t readMemory(uint32_t address, uint8_t mask)
  {
    MemoryRequestPtr m(new MemoryRequest(address, mask));
    queue(m);
    return m->getResult();
  }
};
