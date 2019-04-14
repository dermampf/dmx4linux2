/*
/dev/dxm512/cards    - all cards.
/dev/dxm512/cardX    - only this card.
/dev/dxm512/cardX-portY  - only this port on this card.
It does not matter which one is opened. They are all handled the same way,
except that for /dev/cardX a filter on that card is set and that for /dev/cardXportY a filter on one port of a card is set.
You can achive tha same with IOCTL_DMX512_FILTER_PORT, but that you can request specific ports even from different cards.

The same can be done on the dmx512netdev.
Each port has a unique id that is used to address it within the frame.
The card is not mentioned within the dmxframe and is only used as information
to the application and to handle the ownership, as the ports are destroyed once
the cards that the ports belong to are removed.
*/

/*
 * ALl ports, device, ... share one common id space.
 * An object can be uniqely be identified by it's id.
 */

IOCTL_DMX512_ENUM_DEVICES
struct dmx512_device_entry
{
    char name[MAX_NAME];
    unsigned int deviceid;
};
struct ioctl_dmx512_devicelist
{
    int device_count;
    dmx512_device_entry * devices; // if devices is set to 0 this call only returns the number of devices in device_count.
};


IOCTL_DMX512_ENUM_PORTS
struct dmx512_port_entry
{
    char name[MAX_NAME];
    unsigned int portid;
};
struct ioctl_dmx512_devicelist
{
    unsigned int device_id; // 0 for any device else it's that specific device-id.
    int port_count;
    dmx512_port_entry * ports; // if ports is set to 0 this call only returns the number of ports in port_count.
};

IOCTL_DMX512_SET_PORT_FILTER
IOCTL_DMX512_GET_PORT_FILTER
struct ioctl_dmx512_port_filter
{
    unsigned int port_count;
    unsigned int * port_ids; // If port_ids is set to 0 on IOCTL_DMX512_GET_PORT_FILTER the number of ports is writen to port_count.
};

// port_count of 0 on IOCTL_DMX512_SET_PORT_FILTER clears all port filters.
// With no port filter set all frames are received.

/*
  Can we somehow model mergers and splitters within this api?
*/
