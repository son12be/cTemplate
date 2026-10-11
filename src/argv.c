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
make_argv(vector_t *vec, const struct data_t *const data)
{
  vec->size = 6; /* at least for cc, -c, filepath, -o; path, and NULL */

  /* get argc */
  for(char *s = data->cc; (s = strchr(s, ' ')) != NULL; s++, vec->size++);
  if(data->flagFile)
  {
    vec->size += lines(data->flagFile);
    if(vec->size < 0)
      ERR(-errno, "Failure counting flagfile lines");
  }

  /* allocate */
  vec->data = malloc(sizeof(char*) * vec->size);
  if(!vec->data)
    ERR(MALLOC, "Cant allocate argv");

  /* get args from data->cc */
  int i_arg = 0;
  for(char *token = strtok(data->cc, " "); token; token = strtok(NULL, " "), ++i_arg)
    vec->data[i_arg] = token;

  vec->data[i_arg++] = "-c";

  vec->first_opt = i_arg;

  if(!data->flagFile)
    goto end;

  /* get args from flagFile */
  char *line = malloc(VALUE_SIZE / 2);
  if(!line)
    ERR(MALLOC, "Cant allocate line buffer");
  for(; (fgets(line, VALUE_SIZE / 2, data->flagFile)) != NULL; ++i_arg)
  {
    line[strcspn(line, "\n")] = '\0';
    vec->data[i_arg] = line;

    line = malloc(VALUE_SIZE);
    if(!line)
    {
      for(int j = 0; j < i_arg; ++j)
        free(vec->data[j]);
      free(vec->data);
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
  vec->data[ARGV_OUTPATH_I] = malloc(maxLenSz);
  if(!vec->data[ARGV_OUTPATH_I])
  {
    for(int i = vec->first_opt; i < ARGV_OFLAG_I; ++i)
      free(vec->data[i]);
    free(vec->data);
    ERR(MALLOC, "Cant allocate outpath");
  }
  snprintf(vec->data[ARGV_OUTPATH_I], maxLenSz, "%s/%s/", data->builddir, data->label);
  if(access(vec->data[ARGV_OUTPATH_I], F_OK))
    FAIL_CODE(mkdir(vec->data[ARGV_OUTPATH_I], 0755), -errno, "Cant create directory \"%s\"", vec->data[ARGV_OUTPATH_I]);

  vec->data[ARGV_OFLAG_I] = "-o";
  vec->data[ARGV_PATH_I] = NULL;
  vec->data[ARGV_NULL_I] = NULL;
  return 0;
}

void
destroy_argv(vector_t **vector)
{
  vector_t *vec = *vector;
  for(; vec->first_opt < ARGV_PATH_I; ++vec->first_opt)
    free(vec->data[vec->first_opt]);
  free(vec->data[ARGV_OUTPATH_I]);

  *vector = NULL;
}

