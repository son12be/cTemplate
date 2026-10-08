#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#include "argv.h"
#include "err.h"

inline static int
lines(FILE *file)
{
  int lines = 0;
  char buf[VALUE_SIZE];
  while(fgets(buf, sizeof(buf), file))
    lines++;

  rewind(file);

  return lines;
}

/*
 * first_opt should point one past '-c'.
 * From first_opt and on, theres malloc'd buffers.
 * Before first_opt, theres '-c' and then a strtok'd stack-allocated buffer
 */
int
make_argv(char ***ret, const struct data_t *const data, int *const argc, int *const first_opt)
{
  *argc = 6; /* at least for cc, -c, filepath, -o; path, and NULL */

  /* get argc */
  for(char *s = data->cc; (s = strchr(s, ' ')) != NULL; s++, (*argc)++);
  if(data->flagFile)
  {
    *argc += lines(data->flagFile);
    if(*argc < 0)
      ERR(-errno, "Failure counting flagfile lines");
  }

  /* allocate */
  *ret = malloc(sizeof(char*) * *argc);
  if(!*ret)
    ERR(MALLOC, "Cant allocate argv");

  /* get args from data->cc */
  int i_arg = 0;
  for(char *token = strtok(data->cc, " "); token; token = strtok(NULL, " "), ++i_arg)
    (*ret)[i_arg] = token;

  (*ret)[i_arg++] = "-c";

  *first_opt = i_arg;

  if(!data->flagFile)
    goto end;

  /* get args from flagFile */
  char *line = malloc(VALUE_SIZE / 2);
  if(!line)
    ERR(MALLOC, "Cant allocate line buffer");
  for(; (fgets(line, VALUE_SIZE / 2, data->flagFile)) != NULL; ++i_arg)
  {
    line[strcspn(line, "\n")] = '\0';
    (*ret)[i_arg] = line;

    line = malloc(VALUE_SIZE);
    if(!line)
    {
      for(int j = 0; j < i_arg; ++j)
        free((*ret)[j]);
      free(*ret);
      ERR(MALLOC, "Cant allocate line buffer");
    }
  }

  free(line);


end:
  const int maxLenSz =
    strlen(data->builddir) + SLASH +
    strlen(data->label) + SLASH +
    NAME_MAX +
    1 /* NULL */;
  (*ret)[*ARGV_OUTPATH_I] = malloc(maxLenSz);
  if(!*ret)
  {
    for(int i = *first_opt; i < *ARGV_OFLAG_I; ++i)
      free((*ret)[i]);
    free(*ret);
    ERR(MALLOC, "Cant allocate outpath");
  }
  snprintf((*ret)[*ARGV_OUTPATH_I], maxLenSz, "%s/%s/", data->builddir, data->label);
  if(access((*ret)[*ARGV_OUTPATH_I], F_OK))
    FAIL_CODE(mkdir((*ret)[*ARGV_OUTPATH_I], 0755), -errno, "Cant create directory \"%s\"", (*ret)[*ARGV_OUTPATH_I]);

  (*ret)[*ARGV_OFLAG_I] = "-o";
  (*ret)[*ARGV_PATH_I] = NULL;
  (*ret)[*ARGV_NULL_I] = NULL;
  return 0;
}

void
destroy_argv(char ***argv, int argc, int first_opt)
{
  for(; first_opt < ARGV_PATH_I; ++first_opt)
    free((*argv)[first_opt]);
  free((*argv)[ARGV_OUTPATH_I]);
  *argv = NULL;
}

