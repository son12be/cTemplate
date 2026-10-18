#include <linux/limits.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#include "compile.h"
#include "err.h"
#include "util.h"
#include "parse.h"

/* bro meson is so much better wtf */

int
main(int argc, char **argv)
{
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
        if(data.threads == 0)
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

  if(parse_template(&data) < 0)
    goto err;

  if(data.pre_cc)
  {
    pid_t pid = fork();
    if(pid < 0)
    {
      ERR_NR(-errno, "Cant fork to execute \"%s\"", data.pre_cc);
      goto err;
    } else if(pid == 0)
    {
      execlp("/bin/sh", "/bin/sh", "-c", data.pre_cc, NULL);

      ERR_NR(-errno, "Cant exec \"%s\"", data.pre_cc);
      goto err;
    } else 
    {
      int chld_rc;
      waitpid(pid, &chld_rc, 0);
      if(WIFEXITED(chld_rc) && WEXITSTATUS(chld_rc) != 0)
      {
        ERR_NR(CHLD_ERR, "\"%s\" exited with code %i", data.pre_cc, WEXITSTATUS(chld_rc));
        goto err;
      }
    }
  }

  // printf("%s %s %s %s %s %i\n", data.srcdirs, data.incdirs, data.builddir, data.ext, data.cc, data.threads);
  if(compile(&data, flags) < 0)
    goto err;

  if(flags & OPTRUN)
  {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "./%s/%s/%s", data.builddir, data.label, data.name);
    printf("Executing \"%s\"...\n", path);
    execlp(path, path);

    ERR_NR(-errno, "Cant exec \"%s\"", data.pre_cc);
    goto err;
  }

  destroy_data(&data);
  return 0;

err:
  destroy_data(&data);
  return report_err();
}
