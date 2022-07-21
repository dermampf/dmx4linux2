int find_uio_by_name(char *devname, const char * fdt_name);
int parse_dmx_uio_args(int argc, char **argv, char *uio_name, int *portno);
int parse_uio_args(int argc,
		   char **argv,
		   char *uio_name,
		   int *portno,
		   const char * fdt_name);
int parse_dmx_uio_args2(int argc,
			char **argv,
			char *uio_name,
			int *portno);
