#ifndef DMX_GETARG_H
#define DMX_GETARG_H
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
#ifdef __cplusplus
extern "C" {
#endif

int DMXgetarg(int *argc, const char **argv, const char *param, const int paramhasarg, const char **paramarg);

#ifdef __cplusplus
};
#endif

#endif
