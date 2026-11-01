#include <linux/limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "compile.h"
#include "err.h"
#include "main_info.h"
#include "misc.h"
#include "parse.h"

/* bro meson is so much better wtf */

int
main(int argc, char **argv)
{
  arg_init(argc, argv);

  struct data_t data;
  memset(&data, 0, sizeof(data));
  data.threads = 1;

  uint8_t flags = 0u;
  int opt;
  while ((opt = getopt(argc, argv, "j:frdl:q")) != -1)
  {
    switch (opt)
    {
    case 'j':
      data.threads = atoi(optarg);
      if (data.threads < 1)
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
    case 'd':
      flags |= OPTDEL;
      break;
    case '?':
      return -1;
      break;
    }
  }

  FAIL_GOTO(parse_template(&data) < 0, err);

  if (data.pre_cc)
    FAIL_GOTO(exec_cmd(data.pre_cc) < 0, err);

  FAIL_GOTO(compile(&data, flags) < 0, err);

  if (data.pst_cc)
    FAIL_GOTO(exec_cmd(data.pst_cc) < 0, err);

  if (flags & OPTRUN)
  {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "./%s/%s/%s", data.builddir, data.label,
             data.name);

    FAIL_GOTO(run_pst_cc_bin(path, data.run_args, data.run_env) < 0, err);
  }

  destroy_data(&data);
  return 0;

err:
  destroy_data(&data);
  return report_err();
}
