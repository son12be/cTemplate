#include <libgen.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "util.h"

int
strends(const char *A, const char *B)
{
  if(!A || !B)
    return 0;

  char *c = strrchr(A, B[0]);
  return c && (strcmp(c, B) == 0);
}

char *
trim_ws(const char **s)
{
  while(isspace((unsigned char)**s))
    (*s)++;

  char *end = strchr(*s, '\0');
  end--;
  while(end > *s && isspace((unsigned char)*end))
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
  execlp("/bin/sh", "/bin/sh", "-c", cmd, NULL);

  return -1;
}
