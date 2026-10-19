#include "main_info.h"

static int s_Argc = 0;
static const char **s_Argv = 0;

void
arg_init(int argc, char **argv)
{
  s_Argc = argc;
  s_Argv = argv;
}

int
get_argc()
{
  return s_Argc;
}

char **
get_argv()
{
  return s_Argv;
}
