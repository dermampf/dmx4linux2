#pragma once

#include <dmx.hpp>
#include <vector>

class VectorBufferDmxData : public IDmxData
{
  std::vector<unsigned char> m_buffer;
public:
  int          size() const                 { return m_buffer.size(); }
  const void * data() const                 { return m_buffer.data(); }
  void size(const int newSize)              { m_buffer.resize(newSize); }
  unsigned char & operator [] (const int i) { return m_buffer[i]; }
  unsigned char * data()                    { return m_buffer.data(); }
};
