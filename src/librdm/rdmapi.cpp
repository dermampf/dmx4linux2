#include <rdmapi.hpp>

#include <rdmvalues.hpp>
#include <constdmxdata.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <unistd.h> // usleep

using namespace Rdm;

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


// m_rdm.set(you, PID_XXX).add().add().add().add();
RdmApiSetter::RdmApiSetter(RdmApi & _api, const Rdm::Uid & who, const int cc, const int pid)
    : m_api(_api),
      m_encoder(m_buffer,sizeof(m_buffer))
{
    m_encoder
	.Destination(who)
	.CommandClass(cc)
	.Pid(pid);
}

RdmApiSetter::~RdmApiSetter()
{
    m_api.commit(*this);
}




RdmFuture::RdmFuture()
    : m_state(Busy),
      m_dec(m_buffer, sizeof(m_buffer))
{
}

void RdmFuture::set(unsigned char * b, const int size)
{
  const int s = (size < sizeof(m_buffer)) ? size : sizeof(m_buffer);

  //std::cout << "RdmFuture::set(size=" << std::dec << size << ") s=" << s << std::endl;
  //dump(b, size);

  memcpy(m_buffer, b, s);
  m_state = Ready;
}

bool RdmFuture::wait() const
{
    if (m_state == Busy)
    {
	int timeout_ms = 1000;
	while ((m_state == Busy) && (timeout_ms-- > 0))
	    usleep(1000);
	if (m_state == Busy)
	    m_state = Timeout;
    }
    return m_state == Ready;
}


RdmApi::RdmApi ()
{
    m_discoveredDevices = std::vector<Rdm::Uid>(
	{
	  0x4D530000AAAA,
	  0x4D5300000005
	} );
}

Rdm::Uid RdmApi::myUid() const
{
    return Rdm::Uid(0x123400000001L);
}

void RdmApi::myUid(const Rdm::Uid & _uid)
{
    (void)_uid;
}

void RdmApi::mute(const Rdm::Uid & whom)
{
    static unsigned char buffer[256+2];
    const int size = Rdm::PacketEncoder(buffer,sizeof(buffer))
	.Source(myUid())
	.Destination(whom)
	.CommandClass(Rdm::DISCOVERY_COMMAND)
	.Pid(Rdm::DISC_MUTE)
	.finalize().size();

    if (whom == Rdm::Uid::broadcast())
	handleTx(ConstDmxData((const void *)buffer, size));
    else
    {
	Rdm::RdmFuture f;
	addFuture(f);
	handleTx(ConstDmxData((const void *)buffer, size));
	f.wait();
    }
}

void RdmApi::unmute(const Rdm::Uid & whom)
{
    static unsigned char buffer[256+2];
    const int size = Rdm::PacketEncoder(buffer,sizeof(buffer))
	.Source(myUid())
	.Destination(whom)
	.CommandClass(Rdm::DISCOVERY_COMMAND)
	.Pid(Rdm::DISC_UN_MUTE)
	.finalize().size();

    if (whom == Rdm::Uid::broadcast())
	handleTx(ConstDmxData((const void *)buffer, size));
    else
    {
	Rdm::RdmFuture f;
	addFuture(f);
	handleTx(ConstDmxData((const void *)buffer, size));
	f.wait();
    }
}

/*
 * Make a binary search over the search space
 * to find existing device.
 */
std::vector<Rdm::Uid> RdmApi::discoverDevices(const Rdm::Uid & from,
					      const Rdm::Uid & to)
{
    std::vector<Rdm::Uid> discoveredDevices;

    typedef std::pair<uint64_t,uint64_t> UidRange_t;
    std::list<UidRange_t>  searchSpace;
    searchSpace.push_back(UidRange_t(from,to));

    static unsigned char buffer[256+2];

    while (searchSpace.size() > 0)
    {
	UidRange_t a = searchSpace.front();
	searchSpace.pop_front();
	
	//-- issue discover request and wait for reply
	memset(buffer, 0, sizeof(Rdm::Header));
	Rdm::PacketEncoder e(buffer,sizeof(buffer));
	buildDiscoveryRequest(e, myUid(), a.first, a.second);
	Rdm::RdmFuture f;
	addFuture(f);
	handleTx(ConstDmxData((const void *)buffer, e.size()));
	if (!f.wait())
	    continue; // timeout, do nothing

	if ((f.StartCode()==Rdm::SC_RDM) &&
	    (f.SubStartCode() == Rdm::SC_SUB_MESSAGE) &&
	    (f.CommandClass() == Rdm::DISCOVERY_COMMAND_RESPONSE) &&
	    (f.Pid() == Rdm::DISC_UNIQUE_BRANCH))
	{
	    if (f.Destination() == myUid())
	    {
		if (f.Source() == Rdm::Uid::broadcast())
		{
		    std::cout << std::hex << a.first << ".." << a.second
			      << " collision -> split" << std::endl;
		    searchSpace.push_back(UidRange_t(a.first, (a.first+a.second)/2-1 ));
		    searchSpace.push_back(UidRange_t((a.first+a.second)/2, a.second));
		}
		else if (f.Source() == Rdm::Uid::any())
		{
		    std::cout << std::hex << a.first << ".." << a.second
			      << " timeout" << std::endl;
		}
		else
		{
		    /* If we got a reply from one device, then
		       * mute the device and search again in the
		       * search space in which we found this item.
		       */
		  std::cout << "  found:" << std::hex << f.Source() << std::endl;
		    if (std::find(discoveredDevices.begin(),
				  discoveredDevices.end(),
				  Rdm::Uid(f.Source())) == discoveredDevices.end())
			discoveredDevices.push_back(Rdm::Uid(f.Source()));
		    mute(Rdm::Uid(f.Source()));
		      searchSpace.push_back(a);
		}
	    }
	}
    }
    return discoveredDevices;
}


void RdmApi::startDiscovery(const Rdm::Uid & from, const Rdm::Uid & to)
{
#if 1
    m_discoveredDevices = discoverDevices(from, to);
#else
    if (m_discoverOngoing)
	return;
    m_discoverThread.join();
    volatile bool m_discoverOngoing;
    std::thread m_discoverThread;
    m_discoverThread = std::thread([from,to,&m_discoveredDevices,&m_discoverOngoing]()
				   {
				       m_discoverOngoing = true;
				       m_discoveredDevices = discoverDevices(from, to);
				       m_discoverOngoing = false;
				   }
	);
#endif
}


void RdmApi::startDiscovery()
{
    startDiscovery(Rdm::Uid(0x000000000001L), Rdm::Uid(0xFFFFFFFFFFFEL));
}

const std::vector<Rdm::Uid> RdmApi::devicesDiscovered() const
{
    return m_discoveredDevices;
};

RdmApiSetter RdmApi::set(const Rdm::Uid & who, const int pid)
{
  #if 0
    std::cout << "RdmApi::set("
	      << std::hex << uint64_t(who)
	      << ","
	      << std::hex << pid // what
	      << ")"
	      << std::endl;
    #endif
    return RdmApiSetter(*this, who, Rdm::SET_COMMAND, pid);
}

void RdmApi::commit(RdmApiSetter & s)
{
  //    std::cout << "RdmApi::commit" << std::endl;
    s.m_encoder
	.Source(myUid())
	.TransactionNumber(0)
	.PortId(0)
	.MessageCount(0)
	.SubDevice(0)
	.finalize();
    //dump(s.m_buffer, s.m_encoder.size());
    handleTx(ConstDmxData((const void *)s.m_buffer, s.m_encoder.size()));
}

// std::string rdm.get(Rdm::Uid(args[1]), args[2]).value();
// the future makes it possible to kick start multiple requests
// and handle them later.
//--
// The simple implementation for now just waits for the reply and then returns the future with the value allready set.
RdmFuture & RdmApi::get(const Rdm::Uid & who, const int pid)
{
#if 0
    std::cout << "RdmApi::get("
	      << std::hex << uint64_t(who)
	      << ","
	      << pid
	      << ") => RdmFuture()"
	      << std::endl;
#endif
    static unsigned char buffer[256+2];
    const int size = Rdm::PacketEncoder(buffer,sizeof(buffer))
	.Source(myUid())
	.Destination(who)
	.CommandClass(Rdm::GET_COMMAND)
	.Pid(pid)
	.TransactionNumber(0)
	.PortId(0)
	.MessageCount(0)
	.SubDevice(0)
	.finalize().size();

    Rdm::RdmFuture * f = new Rdm::RdmFuture(); // !!!!TODO: This is a memory leak. Need some smart pointer machanism.
    // The returned object must be the master. If it is removed, the instance in the list must be removed.
    addFuture(*f);
    handleTx(ConstDmxData((const void *)buffer, size));
    return *f;
}

void RdmApi::handle(const IDmxData & _data)
{
  //std::cout << "RdmApi::handle" << std::endl;
  //dump(_data);

    std::list<Rdm::RdmFuture*>::iterator needle = m_futures.end();
    /*
    for (std::list<Rdm::RdmFuture*>::iterator it = m_futures.begin();
	 it != m_futures.end();
	 ++it)
    {
	if (found)
	    needle = it;
    }
    */

    // just use the first one received. Best first guess.
    needle = m_futures.begin();
    
    if (needle != m_futures.end())
    {
	if (*needle)
	    (*needle)->set((unsigned char *)_data.data(), _data.size());
	m_futures.erase(needle);
    }
    if (m_futures.size() > 0)
	std::cout << "unexpected data in futures:" << m_futures.size() << std::endl;
    m_futures.clear();
}

void RdmApi::addFuture(Rdm::RdmFuture & f)
{
    m_futures.push_back(&f);
}
