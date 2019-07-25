#pragma once
#include <rdmvalues.hpp>
#include <string>
#include <vector>
#include <list>
#include <thread>

#include <dmx.hpp>

namespace Rdm
{
    class RdmFuture
    {
    public:
	enum State { Busy, Ready, Timeout };
	unsigned char  m_buffer[256];
	volatile mutable State m_state;
	PacketDecoder  m_dec;
    public:
	RdmFuture();
        int size() const { return wait() ? m_dec.Pdl() : 0; }
	void set(unsigned char * b, const int size);
	bool wait() const;
        bool okay() const { return m_state == Ready; }

	int      StartCode() const    { return wait() ? m_dec.StartCode() : -1; }
	int      SubStartCode() const { return wait() ? m_dec.SubStartCode() : -1; }
	int      CommandClass() const { return wait() ? m_dec.CommandClass() : -1; }
	int      SubDevice() const    { return wait() ? m_dec.SubDevice() : -1; }
	int      Pid() const          { return wait() ? m_dec.Pid() : -1; }
	uint64_t Source() const       { return wait() ? m_dec.Source() : -1; }
	uint64_t Destination() const  { return wait() ? m_dec.Destination() : -1; }

	template <typename T>
	T at(const int offset) const
	    {
		if (!wait())
		{
		    printf ("wait failed\n");
		    return 0;
		}
		T t;
		printf ("at offset=%d size=%d\n", offset, int(sizeof(T)));
		if (!m_dec.PdAt(offset, t))
		    printf ("read ahead of payload (PDL=%d)\n", m_dec.Pdl()); // throw exception
		return t;
	    }

        void at(const int offset, const char * s, const int size) const
	    {
		if (!wait())
		    return;
		if (!m_dec.PdAt(offset, (char *)s, size))
		    ; // throw exception
	    }
    };

    class RdmApi;
    class RdmApiSetter
    {
	friend RdmApi;
    private:
	RdmApi & m_api;
	unsigned char m_buffer[256];
	Rdm::PacketEncoder m_encoder;
	RdmApiSetter(RdmApi & _api, const Rdm::Uid & who, const int cc, const int pid);
    public:
	~RdmApiSetter();

	RdmApiSetter &  add(const uint8_t v)
	    {
		m_encoder.addPd(v);
		return *this;
	    }

	template <typename T>
	RdmApiSetter &  add(const T v)
	    {
		m_encoder.addPd(v);
		return *this;
	    }

	template <typename T>
	RdmApiSetter &  add(const T & v, const unsigned int _size)
	    {
		m_encoder.addPd(v, _size);
		return *this;
	    }

	template <typename T>
	RdmApiSetter &  add(const T * v, const unsigned int _size)
	    {
		m_encoder.addPd(v, _size);
		return *this;
	    }

	RdmApiSetter &  add(const char * str)
	    {
		m_encoder.addPd(str);
		return *this;
	    }

	RdmApiSetter & add(const Uid & v)
	    {
		m_encoder.addPd(v);
		return *this;
	    }
    };

    class Uid;
    class RdmApi : public DmxTransceiver
    {
	friend RdmApiSetter;
    private:
	std::list<Rdm::RdmFuture*>  m_futures;
	std::vector<Rdm::Uid> m_discoveredDevices;
    public:
	RdmApi();

	Rdm::Uid myUid() const;
	void myUid(const Rdm::Uid &);

	void mute(const Rdm::Uid & whom);
	void unmute(const Rdm::Uid & whom);
	void startDiscovery(const Rdm::Uid & from, const Rdm::Uid & to);
	void startDiscovery();
        const std::vector<Rdm::Uid> devicesDiscovered() const;

        std::vector<Rdm::Uid> discoverDevices(const Rdm::Uid & from,
					      const Rdm::Uid & to);


	RdmApiSetter set(const Rdm::Uid & who, const int pid);

	// std::string rdm.get(Rdm::Uid(args[1]), args[2]).value();
	// the future makes it possible to kick start multiple requests
	// and handle them later.
	RdmFuture & get(const Rdm::Uid &, const int pid);

    protected:
	void addFuture(Rdm::RdmFuture & f);
	void commit(RdmApiSetter & s);

    public: // implement IDmxReceiver:
	// handle incomming dmx frames.
	void handle(const IDmxData&);
    };
};
