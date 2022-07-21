#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
   retrieve a command line argument. arguments should start with "-"
   or "--" or "+" but this is not requiered. The command line is
   permuted, with all found parameters moved to the end and argc
   lowered. A parameter can have an optional argument, this is
   controlled with the paramhasarg variable. Found arguments are
   returned with paramarg.

   @param argc address of argc from main. The variable is decremented by the strings found.
   @param argv argv array from main. The array is permuted, found string moved to end
   @param param parameter string which should be found
   @param paramhasarg look for an additional argument after param
   @param paramarg string of additional argument. Returns the argument string, NULL otherwise.

   @return -1 if param was not found. 0 if param was found. 1 if param
   was found and additional argument was found. -2 if paramters are errorneous
   (NULL strings etc.)
*/
int DMXgetarg(int *argc, const char **argv, const char *param, const int paramhasarg, const char **paramarg)
{
  int skip=0;
  int i=1, j;
  int ret=-1;
  const char *parambak=NULL;

  /* check input */
  if(argc==NULL || argv==NULL || param==NULL)
    return -2;

  if(*argc<2)
    return -2;

  if(strlen(param)<1)
    return -2;

  if(paramhasarg && !paramarg)
    return -2;

  /* search for param */
  while(i<*argc)
    {
      if(!strcmp(argv[i], param))
	{
	  skip=1;
	  ret=0;
	  parambak=argv[i];
	  break;
	}
      i++;
    }

  /* no param found */
  if(ret<0 && skip==0)
    return ret;

  if(paramhasarg && i+1<*argc)
    {
      *paramarg=argv[i+1];
      ret=1;
      skip=2;
    }

  /* move processed strings to end of argv */
  for(j=i; j<*argc-skip; j++)
    argv[j]=argv[j+skip];
  if(j<*argc)
    argv[j++]=parambak;
  if(j<*argc && paramhasarg)
    argv[j]=*paramarg;

  *argc-=skip;

  return ret;
}

