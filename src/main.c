#include <linux/limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#include "compile.h"
#include "err.h"
#include "util.h"
#include "parse.h"
#include "misc.h"
#include "main_info.h"

/* bro meson is so much better wtf */

int
main(int argc, char **argv)
{
  arg_init(argc, argv);

  struct data_t data = 
  {
    .srcdirs = 0,
    .builddir = 0,
    .ext = 0,
    .cc = 0,
    .cc_args = 0,
    .pre_cc = 0,
    .threads = 1,
    .flagFile = NULL,
  };

  uint8_t flags = 0u;
  int opt;
  while((opt = getopt(argc, argv, "j:frl:q")) != -1)
  {
    switch(opt)
    {
      case 'j':
        data.threads = atoi(optarg);
        if(data.threads < 1)
        {
          fprintf(stderr, "\"%s\" is not a valid argument for -j \n", optarg);
          return -1;
        }
        break;
      case 'f':
        flags |= OPTCOMP_FORCE;
        break;
      case 'r':
        flags |= OPTRUN;
        break;
      case 'l':
        data.label = optarg;
        break;
      case 'q':
        flags |= OPTQUIET;
        break;
      case '?':
        return -1;
        break;
    }
  }

  FAIL_GOTO(parse_template(&data) < 0, err);

  if(data.pre_cc)
    FAIL_GOTO(exec_cmd(data.pre_cc) < 0, err);

  FAIL_GOTO(compile(&data, flags) < 0, err);

  if(data.pst_cc)
    FAIL_GOTO(exec_cmd(data.pst_cc) < 0, err);

  if(flags & OPTRUN)
  {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "./%s/%s/%s", data.builddir, data.label, data.name);

    FAIL_GOTO(exec_cmd(path) < 0, err);
  }

  destroy_data(&data);
  return 0;

err:
  destroy_data(&data);
  return report_err();
}
