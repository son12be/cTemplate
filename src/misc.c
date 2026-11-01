#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "err.h"

#include "misc.h"

int
exec_cmd(const char *const cmd)
{
  LOG("Executing \"%s\"", cmd);
  pid_t pid = fork();
  if (pid < 0)
  {
    ERR(-errno, "Cant fork to execute \"%s\"", cmd);
  }
  else if (pid == 0)
  {
    shell_exec(cmd);

    ERR_NR(-errno, "Cant execute \"%s\"", cmd);
    report_err();
    exit(-1);
  }
  WAITPID
  {
    ERR(CHLD_ERR, "\"%s\" returned %i", cmd, WEXITSTATUS(chld_rc));
  }

  return 0;
}

int
should_compile(const char *const source_path, const char *const object_path,
               const uint8_t flags)
{
  if (flags & (OPTCOMP_FORCE | OPTDEL) || access(object_path, F_OK) != 0)
    return 1;

  struct stat st;
  time_t source_modtime;
  time_t object_modtime;

  FAIL_CODE(stat(source_path, &st) < 0, CANT_OPEN,
            "Cant open stat struct for path \"%s\"", source_path);
  source_modtime = st.st_mtime;

  FAIL_CODE(stat(object_path, &st) < 0, CANT_OPEN,
            "Cant open stat struct for path \"%s\"", object_path);
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
  for (int i = 0; i < ARGV_NULL_I; ++i)
    printf("%s ", vec->data[i]);
  printf("\n");
#else
  printf("CC [%s]\n", vec->data[ARGV_OUTPATH_I]);
#endif

  sem_wait(&sem);
  pid_t pid = fork();
  if (pid < 0)
  {
    ERR(-errno, "Cant fork");
  }
  else if (pid == 0) /* child */
  {
    execvp(vec->data[0], vec->data);

    /* if exec returns, then it failed */
    ERR_NR(-errno, "Cant execute \"%s\"", vec->data[0]);
    report_err();
    exit(errno); /* Children shall never return */
  }

  return 0;
}

int
run_pst_cc_bin(const char *const path, const char *const args,
               const char *const env)
{
  if (env)
  {
    for (char *tok = strtok(env, " "); tok; tok = strtok(nullptr, " "))
    {
      char *equal = strchr(tok, '=');
      if (!equal)
      {
        WARN("In RUN_ENV: token \"%s\" must have an equal sign", tok);
        continue;
      }

      *equal = '\0';
      char *val = equal + 1;

      setenv(tok, val, 1);
    }
  }

  const size_t len = strlen(path) + (args ? strlen(args) + 1 : 0) + 1;

  char cmd[len];
  if (args)
    snprintf(cmd, len, "%s %s", path, args);
  else
    snprintf(cmd, len, "%s", path);

  return exec_cmd(cmd);
}

int
strends(const char *A, const char *B)
{
  if (!A || !B)
    return 0;

  char *c = strrchr(A, B[0]);
  return c && (strcmp(c, B) == 0);
}

char *
trim_ws(const char **s)
{
  while (isspace((unsigned char)**s))
    (*s)++;

  char *end = strchr(*s, '\0');
  end--;
  while (end > *s && isspace((unsigned char)*end))
  {
    *end = '\0';
    end--;
  }

  return *s;
}

void
sigchld_handler(int)
{
  sem_post(&sem);
}

int
shell_exec(char *cmd)
{
  execlp("/bin/sh", "/bin/sh", "-c", cmd, nullptr);

  return -1;
}
