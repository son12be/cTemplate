#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "util.h"
#include "err.h"

#include "misc.h"

int
pre_cc(struct data_t *data)
{
  if(data->pre_cc)
  {
    pid_t pid = fork();
    if(pid < 0)
    {
      ERR(-errno, "Cant fork to execute \"%s\"", data->pre_cc);
    } else if(pid == 0)
    {
      shell_exec(data->pre_cc);

      ERR_NR(-errno, "Cant execute \"%s\"", data->pre_cc);
      exit(-1);
    } else 
    {
      int chld_rc;
      waitpid(pid, &chld_rc, 0);
      if(WIFEXITED(chld_rc) && WEXITSTATUS(chld_rc) != 0)
      {
        ERR(CHLD_ERR, "\"%s\" exited with code %i", data->pre_cc, WEXITSTATUS(chld_rc));
      }
    }
  }

  return 0;
}

int
should_compile(const char *const source_path, const char *const object_path, const int comp_force)
{
  if(comp_force || access(object_path, F_OK) != 0)
    return 1;

  struct stat st;
  time_t source_modtime;
  time_t object_modtime;

  FAIL_CODE(stat(source_path, &st) < 0, CANT_OPEN, "Cant open stat struct for path \"%s\"", source_path);
  source_modtime = st.st_mtime;

  FAIL_CODE(stat(object_path, &st) < 0, CANT_OPEN, "Cant open stat struct for path \"%s\"", object_path);
  object_modtime = st.st_mtime;
  
  return source_modtime > object_modtime;
}

/*
 * Decrement sem and "fork and exec"
 * sigchld will handle incrementing sem
 */
extern inline int
exec_cc(vector_t *vec)
{
  /* i wanna end my fucking life */
  /* No idea how to do this either */

#ifdef DEBUG
  for(int i = 0; i < ARGV_NULL_I; ++i)
    printf("%s ", vec->data[i]);
  printf("\n");
#else
  printf("CC [%s]\n", vec->data[ARGV_OUTPATH_I]);
#endif

  sem_wait(&sem);
  pid_t pid = fork();
  if(pid < 0)
  {
    ERR(-errno, "Cant fork");
  } else if(pid == 0) /* child */
  {
    execvp(vec->data[0], vec->data);

    /* if exec returns, then it failed */
    ERR_NR(-errno, "Cant execute \"%s\"", vec->data[0]);
    exit(errno); /* Children shall never return */
  } else /* parent */
  {
    return 0;
  }
}
