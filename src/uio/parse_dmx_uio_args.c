#include "uio.h"

#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <getopt.h>


int find_uio_by_name(char *devname, const char * fdt_name)
{
    int search_uio (int index,const char*uio,const char*name,void*user)
    {
      printf ("[%d] uio:%s, name:%s, user:%s\n", index, uio, name, (char*)user);
	if (strcmp(name, user))
	    return -1;
	strcat(devname, uio);
	return 0;
    }
    return iterate_uio(search_uio, (void*)fdt_name);
}

static int isnumber(const char * s)
{
    return strspn(s,"0123456789")==strlen(s);
    // return s[strspn(s,"0123456789")]==0;
}


int parse_uio_args(int argc,
		   char **argv,
		   char *uio_name,
		   int *portno,
		   const char * fdt_name)
{
  printf ("parse_uio_args(argc:%d, ..., fdt_name:%s)\n", argc, fdt_name);

    if (argc==2) // one parameter
    {
        if (isnumber(argv[1]) && (find_uio_by_name(uio_name, fdt_name)==0))
	{
	    *portno = atoi(argv[1]);
	    return 0;
	}
	strcpy(uio_name, argv[1]);
	return 0;
    }
    else if (argc==3) // two parameters
    {
	// num num | name num
	if (!isnumber(argv[2]))
	    return 1;
	*portno = atoi(argv[2]);

	if (isnumber(argv[1]))
	{
	    sprintf(uio_name, "/dev/uio%d", atoi(argv[1]));
	    return 0;
	}
	strcpy(uio_name, argv[1]);
	return 0;
    }
    else if(argc < 2)
    {
	if (find_uio_by_name(uio_name, fdt_name)==0)
	    return 0;
    }
    return 1;
}






int parse_dmx_uio_args2(int argc,
			char **argv,
			char *uio_name,
			int *portno)
{
  int c;
  int digit_optind = 0;

  while (1)
    {
      int this_option_optind = optind ? optind : 1;

      int option_index = 0;

      static struct option long_options[] = {
	{"device",  required_argument, 0, 'D'},
	{"port",    required_argument, 0, 'p'},
	{0,         0,                 0,  0 }
      };

      c = getopt_long(argc, argv, "D:p:",
		      long_options, &option_index);
      if (c == -1)
	break;

      switch (c)
	{
	case 'D':
	  strcpy(uio_name, optarg);
	  break;
	case 'p':
	  *portno = strtol(optarg, 0, 0);
	  break;

	default:
	  return -1;
	}
    }
  return optind;
}



int parse_dmx_uio_args(int argc,
		       char **argv,
		       char *uio_name,
		       int *portno)
{
  return parse_uio_args(argc, argv, uio_name, portno, "dmxllg@40000000");
}
