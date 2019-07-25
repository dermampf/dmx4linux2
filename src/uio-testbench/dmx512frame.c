#include "dmx512_port.h"


//-- move this to dmx512_rdm.c, as this is more or less all RDM stuff

int dmx512frame_is_rdm(struct dmx512frame * f)
{
	return f->startcode == 0xCC;
}



int dmx512frame_update_rdm_flags(struct dmx512frame * f)
{
	if (dmx512frame_is_rdm(f))
	{
		f->flags |= DMX512_FLAG_IS_RDM;
		if (dmx512frame_is_rdm_discover(f))
			f->flags |= DMX512_FLAG_IS_RDM_DISC;
		else
			f->flags &= ~DMX512_FLAG_IS_RDM_DISC;
	}
	else
		f->flags &= ~(DMX512_FLAG_IS_RDM|DMX512_FLAG_IS_RDM_DISC);
		
}


