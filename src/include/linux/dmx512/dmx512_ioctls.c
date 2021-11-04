#include "dmx512_ioctls.h"
#include <sys/ioctl.h>

/*
 * The caller needs to free 'matchcode' if 0 is returned.
 */
static int dmx512_get_matchcode(int fd,
				int index,
				unsigned int * size,
				char ** matchcode)
{
	int ret;
	struct dmx512_rxfilter_info info;
	memset(&info, 0, sizeof(info));
	info.matchcodes_index = index;
	ret = ioctl(fd, DMX512_IOCTL_GET_PORT_FILTER, &info);
	if (ret)
		return ret;
	info.matchcode = malloc(info.matchcodes_size);
	ret = ioctl(fd, DMX512_IOCTL_GET_PORT_FILTER, &info);
	if (ret) {
		free(info.matchcode);
		return ret;
	}
	*size = info.matchcode_size;
	*matchcode = info.matchcode;
	return 0;
}

/*
 * Set index to -1 to append the matchcode.
 */
static int dmx512_set_matchcode(int fd,
				int index,
				unsigned int size,
				char * matchcode)
{
	int ret;
	struct dmx512_rxfilter_info info;
	info.matchcodes_index = index;
	info.matchcode_size = size;
	info.matchcode = matchcode;
	return ioctl(fd, DMX512_IOCTL_GET_PORT_FILTER, &info);
}
