#include <dmx.hpp>
#include <rdmvalues.hpp>
#include "dmx4linux2interface.hpp"
#include "dmx4linux2.hpp"
#include "constdmxdata.hpp"


#include <iostream>
#include <iomanip>
#include <thread>

#include <unistd.h>
#include <assert.h>
#include <string.h>


static void buildDiscoveryRequest(Rdm::PacketEncoder & e,
				  const uint64_t source,
				  const uint64_t from,
				  const uint64_t to)
{
  e.Source(source)
    .Destination(0xffffffffffffL)
    .CommandClass(Rdm::DISCOVERY_COMMAND)
    .Pid(Rdm::DISC_UNIQUE_BRANCH)
    .addPd(from, 6)
    .addPd(to, 6)
    .finalize();
}



class DmxInterface : public DmxTransceiver
{
public: // IDmxReceiver
  void handle(const IDmxData & f)
  {
    std::cout << "DmxInterface: frame received" << std::endl;
    dump(f);
  }
  void emit(const IDmxData & f)
  {
    handleTx(f);
  }
};

int main ()
{
  Dmx4Linux2Device dmx4linux2;
  Dmx4Linux2Interface dmx (dmx4linux2);

  if (!dmx4linux2.open("/dev/dmx-card0"))
    {
      perror("open dmx device");
      return 1;
    }

  DmxInterface master;
  master.registerReceiver(dmx);
  dmx.registerReceiver(master);
  volatile bool run = true;
  std::thread dmx4linux_thread([&]()
			       {
				   while(run)
				       dmx.handleIngressData();
			       }
			       );
#if 1

  {
    unsigned char buffer[256+2];
    memset(buffer, 0, sizeof(Rdm::Header));

    // Rdm::ResponderContext ctx;
    // Rdm::Message::DiscoveryRequest(ctx, source, from, to);
    {
      {
	Rdm::PacketEncoder e(buffer,sizeof(buffer));
	buildDiscoveryRequest(e,
			      0x123400000001L,
			      0x000000000001L,
			      //0xFFFFFFFFFFFEL);
			      0x123400001000L);
	master.emit(ConstDmxData((const void *)&buffer, e.size()));
      }

      {
	const int size = Rdm::PacketEncoder(buffer,sizeof(buffer))
	  .Source(0x123400000001L)
	  .Destination(0x123400000815L)
	  .CommandClass(Rdm::DISCOVERY_COMMAND)
	  .Pid(Rdm::DISC_MUTE)
	  .finalize().size();
	master.emit(ConstDmxData((const void *)&buffer, size));
      }

      {
	Rdm::PacketEncoder e(buffer,sizeof(buffer));
	buildDiscoveryRequest(e,
			      0x123400000001L,
			      0x000000000001L,
			      0xFFFFFFFFFFFEL);
	master.emit(ConstDmxData((const void *)&buffer, e.size()));
      }

    }
  }
#endif

  sleep(1);
  run = false;
  dmx4linux_thread.join();
  return 0;
}
