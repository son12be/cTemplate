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

sem_t sem;

static inline int
compile_srcdir(const char *const srcdir, const struct data_t *data, char **argv, const int argc, int flags)
{
  const DIR *dir = opendir(srcdir);
  if(!dir)
    ERR(CANT_OPEN, "Cant open srcdir \"%s\"", srcdir);

  glob_t file_list;
  char pattern[
    strlen(srcdir) + SLASH +
    sizeof("*.") +
    strlen(data->ext)];

  snprintf(pattern, sizeof(pattern), "%s/*.%s", srcdir, data->ext);

  /* TODO maybe store rc? */
  if(glob(pattern, GLOB_NOSORT | GLOB_ERR, NULL, &file_list))
  {
    closedir(dir);
    ERR(NOMATCH, "Pattern was \"%s\"", pattern);
  }

  /* loop through returned paths */
  const char *outpath_end = strrchr(argv[ARGV_OUTPATH_I], '/') + 1;
  for(unsigned int i = 0; i < file_list.gl_pathc; ++i)
  {
    argv[ARGV_PATH_I] = file_list.gl_pathv[i];

    struct stat st;
    if(stat(argv[ARGV_PATH_I], &st) < 0)
    {
      ERR_NR(CANT_OPEN, "Cant get stat struct for file \"%s\"", argv[ARGV_PATH_I]);
      goto err;
    }
    snprintf(outpath_end, NAME_MAX, "%s.o", basename(argv[ARGV_PATH_I]));

    int changed;
    FAIL_GOTO(
        (changed = should_compile(argv[ARGV_PATH_I], argv[ARGV_OUTPATH_I], flags & OPTCOMP_FORCE)) < 0,
        err);
    if(!changed)
      continue;

    if(exec_cc(argv) < 0)
      goto err;
  }

  /* BUG
   * Remove all files of a label, then build with that label
   * First run will fail, as not all .o files are spotted by glob(3p)
   * Second run will spot them and build properly
   */
  const int dir_fd = dirfd(dir);
  FAIL(fsync(dir_fd));

  closedir(dir);
  globfree(&file_list);

  return 0;

err:
  closedir(dir);
  globfree(&file_list);
  return -1;
}

int
compile(struct data_t *data, const int flags)
{
  FAIL_CODE(sem_init(&sem, 0, data->threads) < 0, CANT_OPEN, "Cant open semaphore");

  if(!data->label)
    data->label = "";

  int argc;
  int first_opt;
  char **argv;
  FAIL(make_argv(&argv, data, &argc, &first_opt) < 0);

  const struct sigaction sig = { .sa_handler = sigchld_handler };
  sigaction(SIGCHLD, &sig, NULL);

  /* loop through specified srcdirs */
  for(char *srcdir = strtok(data->srcdirs, " "); srcdir; srcdir = strtok(NULL, " "))
  {
    if(compile_srcdir(srcdir, data, argv, argc, flags) < 0)
    {
      destroy_argv(&argv, argc, first_opt);
      return -1;
    }
    waitpid(-1, NULL, 0);
  }

  /* move eveything before first_opt by one to overwrite '-c' */
  memmove(argv + 1, argv, sizeof(char*) * (first_opt - 1));
  argv++; argc--; first_opt--;

  /* Fuck it, simplier than having to deal with what to do with PATH */
  argv[ARGV_PATH_I] = "-Wl,--as-needed";

  glob_t file_list;
  char pattern[
    strlen(data->builddir) + SLASH +
    strlen(data->label) + SLASH +
    sizeof("*.o")];

  snprintf(pattern, sizeof(pattern), "%s/%s/*.o", data->builddir, data->label);
  if(glob(pattern, GLOB_NOSORT | GLOB_ERR, NULL, &file_list) != 0)
  {
    destroy_argv(&argv, argc, first_opt);
    globfree(&file_list);
    ERR(NOMATCH, "Pattern was \"%s\"", pattern);
  }

  // argv = realloc(argv, sizeof(char*) * (argc + file_list.gl_pathc + 1)); // argv[<first_opt] not malloc'd
  int bytes = sizeof(char*) * (argc + file_list.gl_pathc + 1);
  char **argv_cc = malloc(bytes);
  memcpy(argv_cc, argv, sizeof(char*) * argc);

  snprintf(strrchr(argv_cc[ARGV_OUTPATH_I], '/') + 1, NAME_MAX, "%s", data->name);

  memcpy(argv_cc + ARGV_NULL_I, file_list.gl_pathv, sizeof(char*) * (file_list.gl_pathc + 1));
  argc += file_list.gl_pathc;

  for(int i = 0; i < ARGV_NULL_I; ++i)
    printf("%s ", argv_cc[i]);
  printf("\n");

  const pid_t pid = fork();
  if(pid < 0)
  {
    ERR(-errno, "fork() failed");
  } else if(pid == 0)
  {
    execvp(argv_cc[0], argv_cc);

    /* FIXME
     * 2 mem leaks here
     * Gotta free argv
     */
    ERR_NR(-errno, "Cant exec \"%s\"", argv_cc[0]);
    argc -= file_list.gl_pathc;
    destroy_argv(&argv_cc, argc, first_opt);
    globfree(&file_list);
  } else
  {
    argc -= file_list.gl_pathc;
    destroy_argv(&argv_cc, argc, first_opt);
    globfree(&file_list);

    waitpid(-1, NULL, WNOHANG);

    return 0;
  }


  return -1;
}
