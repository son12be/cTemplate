#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#include <libgen.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "argv.h"
#include "compile.h"
#include "err.h"
#include "misc.h"
#include "parse.h"

sem_t sem;

static inline int
compile_srcdir(const char *const srcdir, const struct data_t *data,
               vector_t *vec, uint8_t flags)
{
  int comp_n = 0;

  glob_t file_list;
  char pattern[strlen(srcdir) + SLASH + sizeof("*.") + strlen(data->ext)];

  snprintf(pattern, sizeof(pattern), "%s/*.%s", srcdir, data->ext);

  /* TODO maybe store rc? */
  FAIL_CODE(glob(pattern, GLOB_NOSORT | GLOB_ERR, nullptr, &file_list) != 0,
            NOMATCH, "Pattern was \"%s\"", pattern);

  /* loop through returned paths */
  const char *outpath_end = strrchr(vec->data[ARGV_OUTPATH_I], '/') + 1;
  for (unsigned int i = 0; i < file_list.gl_pathc; ++i)
  {
    vec->data[ARGV_PATH_I] = file_list.gl_pathv[i];

    struct stat st;
    if (stat(vec->data[ARGV_PATH_I], &st) < 0)
    {
      ERR_NR(CANT_OPEN, "Cant get stat struct for file \"%s\"",
             vec->data[ARGV_PATH_I]);
      goto err;
    }
    snprintf(outpath_end, NAME_MAX, "%s.o", basename(vec->data[ARGV_PATH_I]));

    int changed = should_compile(vec->data[ARGV_PATH_I],
                                 vec->data[ARGV_OUTPATH_I], flags);

    FAIL_GOTO(changed < 0, err);
    if (!changed)
      continue;

    if (exec_cc(vec) < 0)
      goto err;

    comp_n++;
  }

  /* BUG
   * Remove all files of a label, then build with that label
   * First run will fail, as not all .o files are spotted by glob(3p)
   * Second run will spot them and build properly
   * Should be fixed by 4d64a20; not sure tho
   */
  const int dir_fd = open(srcdir, 0);
  if (dir_fd < 0 || fdatasync(dir_fd) < 0)
  {
    WARN("Cant sync dir \"%s\": %s", srcdir, STRERROR);
  }

  globfree(&file_list);

  return comp_n;

err:
  globfree(&file_list);
  return -1;
}

int
compile(struct data_t *data, const uint8_t flags)
{
  FAIL_CODE(sem_init(&sem, 0, data->threads) < 0, CANT_OPEN,
            "Cant open semaphore. Val was %i", data->threads);

  if (!data->label)
    data->label = "";

  vector_t vector;
  FAIL(make_argv(&vector, data) < 0);
  vector_t *vec = &vector;

  const struct sigaction sig = {.sa_handler = sigchld_handler};
  sigaction(SIGCHLD, &sig, nullptr);

  if (!(flags & OPTQUIET))
  {
    printf("Compiler: %s\nCompiling with flags:", vec->data[0]);
    for (int i = vec->first_opt; i < ARGV_PATH_I; ++i)
      printf(" %s", vec->data[i]);
    printf("\n");
  }

  if (flags & OPTDEL)
  {
    char pattern[strlen(data->builddir) + SLASH + strlen(data->label) + SLASH +
                 sizeof("*.o")];
    snprintf(pattern, sizeof(pattern), "%s/%s/*.o", data->builddir,
             data->label);

    glob_t globlist;
    glob(pattern, GLOB_ERR | GLOB_NOSORT, nullptr, &globlist);

    for (size_t i = 0; i < globlist.gl_pathc; ++i)
    {
      unlink(globlist.gl_pathv[i]);
    }

    if (globlist.gl_pathv)
      globfree(&globlist);
  }

  int comp_n = 0;

  /* loop through specified srcdirs */
  for (char *srcdir = strtok(data->srcdirs, " "); srcdir;
       srcdir = strtok(nullptr, " "))
  {
    int rc = compile_srcdir(srcdir, data, vec, flags);
    if (rc < 0)
    {
      destroy_argv(&vec);
      return -1;
    }

    comp_n += rc;
  }
  for (int chld_pid, chld_rc; (chld_pid = waitpid(-1, &chld_rc, WNOHANG)) > -1;)
  {
    if (WIFEXITED(chld_rc) && WEXITSTATUS(chld_rc) != 0)
    {
      destroy_argv(&vec);
      ERR(CHLD_ERR, "A child returned %i", WEXITSTATUS(chld_rc));
    }
  }

  if (comp_n == 0)
  {
    LOG("Nothing to do");
    destroy_argv(&vec);
    return 0;
  }

  /* move eveything by one to overwrite '-c' */
  /* dont modify vec->data, as that would have to adjust */
  /* for that when freeing it */
  memmove(vec->data + vec->first_opt - 1, vec->data + vec->first_opt,
          sizeof(char *) * (vec->size - vec->first_opt));
  vec->size--;
  vec->first_opt--;

  /* Fuck it, simplier than having to deal with what to do with PATH */
  vec->data[ARGV_PATH_I] = "-Wl,--as-needed";

  glob_t file_list;
  char pattern[strlen(data->builddir) + SLASH + strlen(data->label) + SLASH +
               sizeof("*.o")];

  snprintf(pattern, sizeof(pattern), "%s/%s/*.o", data->builddir, data->label);
  if (glob(pattern, GLOB_NOSORT | GLOB_ERR, nullptr, &file_list) != 0)
  {
    destroy_argv(&vec);
    globfree(&file_list);
    ERR(NOMATCH, "Pattern was \"%s\"", pattern);
  }

  snprintf(strrchr(vec->data[ARGV_OUTPATH_I], '/') + 1, NAME_MAX, "%s",
           data->name);

  memcpy(vec->data + ARGV_NULL_I, file_list.gl_pathv,
         sizeof(char *) * (file_list.gl_pathc + 1));
  vec->size += file_list.gl_pathc; // NOLINT size cant be uint

#ifdef DEBUG
  for (int i = 0; i < ARGV_NULL_I; ++i)
    printf("%s ", vec->data[i]);
  printf("\n");
#else
  printf("LD [%s]\n", vec->data[ARGV_OUTPATH_I - file_list.gl_pathc]);
#endif

  const pid_t pid = fork();
  if (pid < 0)
  {
    ERR_NR(-errno, "fork() failed");
  }
  else if (pid == 0)
  {
    execvp(vec->data[0], vec->data);

    ERR_NR(-errno, "Cant exec \"%s\"", vec->data[0]);
    report_err();
    exit(-1);
  }
  else
  {
    WAITPID
    {
      ERR_NR(CHLD_ERR, "\"%s\" returned %i", vec->data[0],
             WEXITSTATUS(chld_rc));
    }
  }

  vec->size -= file_list.gl_pathc; // NOLINT size cant be uint
  destroy_argv(&vec);
  globfree(&file_list);

  return get_err();
}
