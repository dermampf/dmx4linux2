#include "dmx4linux2interface.hpp"
#include "dmx4linux2.hpp"
#include <linux/dmx512/dmx512frame.h>
#include <dmx.hpp>
#include <string.h>
#include <unistd.h>

#include <iostream>

namespace Dmx4Linux2
{
    class DmxData : public IDmxData
    {
    private:
	const void * m_data;
	const int    m_size;
    public:
	DmxData(const dmx512frame & _frame)
	    : m_data(_frame.data), m_size(_frame.payload_size + 1) { }
	int          size() const { return m_size; }
	const void * data() const { return m_data; }
    };
};

Dmx4Linux2Interface::Dmx4Linux2Interface (Dmx4Linux2Device & _device)
  : m_device(_device)
{
}
  
void Dmx4Linux2Interface::handle(const IDmxData & f)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    std::cout << "Dmx4Linux2Interface::handle(size:" << f.size() << ") t=" << std::dec << (uint64_t(ts.tv_sec)*1000*1000 + ts.tv_nsec/1000) << "us"
	      << std::endl;
    struct dmx512frame dmxframe;
    memset (&dmxframe, 0, sizeof(dmxframe));
    dmxframe.port = 0;
    dmxframe.payload_size = f.size() - 1;
    memcpy(dmxframe.data, f.data(), f.size());
    m_device.write(dmxframe);
}


#include <signal.h>
#include <poll.h>

void Dmx4Linux2Interface::handleIngressData()
{
    const int timeoutMilliseconds = 1000;

    struct dmx512frame dmxframe;
    struct pollfd fds;
    m_device.addToPoll(fds);
    const int n = poll(&fds, 1, timeoutMilliseconds);
    // std::cout << "poll returned: " << n << std::endl;
    if (n > 0)
    {
	if (fds.revents&POLLIN)
	{
	    if (m_device.read(dmxframe))
		if (dmxframe.port == 0)
		    handleTx(Dmx4Linux2::DmxData(dmxframe));
	    usleep(1000);
	    // poll in dmx4linux2 driver does not work properly at the moment.
	}
    }
}
