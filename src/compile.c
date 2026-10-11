#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <glob.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "err.h"
#include "compile.h"
#include "argv.h"
#include "util.h"

sem_t sem;

static inline int
compile_srcdir(const char *const srcdir, const struct data_t *data, vector_t *vec, int flags)
{
  glob_t file_list;
  char pattern[
    strlen(srcdir) + SLASH +
    sizeof("*.") +
    strlen(data->ext)];

  snprintf(pattern, sizeof(pattern), "%s/*.%s", srcdir, data->ext);

  /* TODO maybe store rc? */
  FAIL_CODE(glob(pattern, GLOB_NOSORT | GLOB_ERR, NULL, &file_list) != 0,
      NOMATCH, "Pattern was \"%s\"", pattern);

  /* loop through returned paths */
  const char *outpath_end = strrchr(vec->data[ARGV_OUTPATH_I], '/') + 1;
  for(unsigned int i = 0; i < file_list.gl_pathc; ++i)
  {
    vec->data[ARGV_PATH_I] = file_list.gl_pathv[i];

    struct stat st;
    if(stat(vec->data[ARGV_PATH_I], &st) < 0)
    {
      ERR_NR(CANT_OPEN, "Cant get stat struct for file \"%s\"", vec->data[ARGV_PATH_I]);
      goto err;
    }
    snprintf(outpath_end, NAME_MAX, "%s.o", basename(vec->data[ARGV_PATH_I]));

    int changed = should_compile(
                      vec->data[ARGV_PATH_I],
                      vec->data[ARGV_OUTPATH_I],
                      flags & OPTCOMP_FORCE);

    FAIL_GOTO(changed < 0, err);
    if(!changed)
      continue;

    if(exec_cc(vec) < 0)
      goto err;
  }

  /* BUG
   * Remove all files of a label, then build with that label
   * First run will fail, as not all .o files are spotted by glob(3p)
   * Second run will spot them and build properly
   */
  const int dir_fd = open(srcdir, 0);
  FAIL_CODE(dir_fd < 0, -errno, "Cant open srcdir \"%s\"", srcdir);
  FAIL_CODE(fdatasync(dir_fd) < 0, -errno, "Cant sync \"%s\"", srcdir); /* should we make it a warn? */

  globfree(&file_list);

  return 0;

err:
  globfree(&file_list);
  return -1;
}

int
compile(struct data_t *data, const int flags)
{
  FAIL_CODE(sem_init(&sem, 0, data->threads) < 0, CANT_OPEN, "Cant open semaphore");

  if(!data->label)
    data->label = "";

  vector_t vector;
  FAIL(make_argv(&vector, data) < 0);
  vector_t *vec = &vector;

  const struct sigaction sig = { .sa_handler = sigchld_handler };
  sigaction(SIGCHLD, &sig, NULL);

  printf("Compiler: %s\nCompiling with flags:", vec->data[0]);
  for(int i = vec->first_opt; i < ARGV_PATH_I; ++i)
    printf(" %s", vec->data[i]);
  printf("\n");

  /* loop through specified srcdirs */
  for(char *srcdir = strtok(data->srcdirs, " "); srcdir; srcdir = strtok(NULL, " "))
  {
    if(compile_srcdir(srcdir, data, vec, flags) < 0)
    {
      destroy_argv(&vec);
      return -1;
    }
    waitpid(-1, NULL, 0);
  }

  /* move eveything before vec->first_opt by one to overwrite '-c' */
  memmove(vec->data + 1, vec->data, sizeof(char*) * (vec->first_opt - 1));
  vec->data++; vec->size--; vec->first_opt--;

  /* Fuck it, simplier than having to deal with what to do with PATH */
  vec->data[ARGV_PATH_I] = "-Wl,--as-needed";

  glob_t file_list;
  char pattern[
    strlen(data->builddir) + SLASH +
    strlen(data->label) + SLASH +
    sizeof("*.o")];

  snprintf(pattern, sizeof(pattern), "%s/%s/*.o", data->builddir, data->label);
  if(glob(pattern, GLOB_NOSORT | GLOB_ERR, NULL, &file_list) != 0)
  {
    destroy_argv(&vec);
    globfree(&file_list);
    ERR(NOMATCH, "Pattern was \"%s\"", pattern);
  }

  int bytes = sizeof(char*) * (vec->size + file_list.gl_pathc + 1);
  char **argv_cc = malloc(bytes);
  memcpy(argv_cc, vec->data, sizeof(char*) * vec->size);

  snprintf(strrchr(argv_cc[ARGV_OUTPATH_I], '/') + 1, NAME_MAX, "%s", data->name);

  memcpy(argv_cc + ARGV_NULL_I, file_list.gl_pathv, sizeof(char*) * (file_list.gl_pathc + 1));
  vec->size += file_list.gl_pathc;

#ifdef DEBUG
  for(int i = 0; i < ARGV_NULL_I; ++i)
    printf("%s ", argv_cc[i]);
  printf("\n");
#else
  printf("LD [%s]\n", vec->data[ARGV_OUTPATH_I - file_list.gl_pathc]);
#endif

  const pid_t pid = fork();
  if(pid < 0)
  {
    ERR(-errno, "fork() failed");
  } else if(pid == 0)
  {
    execvp(argv_cc[0], argv_cc);

    /* FIXME
     * 2 mem leaks here
     * Gotta free vec->data
     */
    ERR_NR(-errno, "Cant exec \"%s\"", argv_cc[0]);
    vec->size -= file_list.gl_pathc;
    destroy_argv(&vec);
    globfree(&file_list);
  } else
  {
    vec->size -= file_list.gl_pathc;
    destroy_argv(&vec);
    globfree(&file_list);

    waitpid(-1, NULL, WNOHANG);

    return 0;
  }


  return -1;
}
