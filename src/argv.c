#include <glob.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "argv.h"
#include "err.h"
#include "parse.h"

inline static int
lines(FILE *file)
{
  int lines = 0;
  char buf[VALUE_SIZE];
  while (fgets(buf, sizeof(buf), file))
  {
    if (buf[strlen(buf) - 1] == '\n')
      lines++;
  }

  rewind(file);

  return lines;
}

static inline int
argv_get_argc(const struct data_t *const data, size_t *extra)
{
  // NOLINTNEXTLINE
  int argc = 6; /* at least for cc, -c, filepath, -o; path, and nullptr */

  /* get argc */
  if (data->cc_args)
  {
    argc++;
    for (char *s = data->cc_args; (s = strchr(s, ' ')) != nullptr; s++, argc++)
      ;
  }
  if (data->flagFile)
    argc += lines(data->flagFile);

  /* to preallocate resulting .o files */
  for (char *srcdir = strtok(data->srcdirs, " "); srcdir;
       srcdir = strtok(nullptr, " "))
  {
    glob_t file_list;

    char pattern[strlen(srcdir) + SLASH + sizeof("*.") + strlen(data->ext)];

    snprintf(pattern, sizeof(pattern), "%s/*.%s", srcdir, data->ext);

    if (glob(pattern, GLOB_NOSORT | GLOB_ERR, nullptr, &file_list) != 0)
    {
      globfree(&file_list);
      ERR(NOMATCH, "Pattern was \"%s\". Extension or srcdir may be wrong",
          pattern);
    }
    *extra += file_list.gl_pathc;
    globfree(&file_list);
  }

  return argc;
}

/*
 * first_opt should point one past '-c'.
 * From first_opt and on, theres malloc'd buffers.
 * Before first_opt, theres '-c' and then a strtok'd stack-allocated buffer
 */
int
make_argv(vector_t *vec, const struct data_t *const data)
{
  size_t extra = 0;
  vec->size = argv_get_argc(data, &extra);
  FAIL(vec->size < 0);

  /* allocate */
  vec->data = malloc(sizeof(char *) * (vec->size + extra));
  FAIL_CODE(!vec->data, MALLOC, "Cant allocate argv");

  /* get args */
  vec->data[0] = data->cc;

  int i_arg = 1;
  for (char *token = strtok(data->cc_args, " "); token;
       token = strtok(nullptr, " "), ++i_arg)
    vec->data[i_arg] = token;

  vec->data[i_arg++] = "-c";

  vec->first_opt = i_arg;

  if (!data->flagFile)
    goto end;

  /* get args from flagFile */
  char *line = malloc(LARGE_ENOUGH_LINE);
  FAIL_CODE(!line, MALLOC, "Cant allocate line buffer");
  for (; (fgets(line, LARGE_ENOUGH_LINE, data->flagFile)) != nullptr; ++i_arg)
  {
    line[strcspn(line, "\n")] = '\0';
    vec->data[i_arg] = line;

    line = malloc(LARGE_ENOUGH_LINE);
    if (!line)
    {
      for (int j = 0; j < i_arg; ++j)
        free(vec->data[j]);
      free(vec->data);
      ERR(MALLOC, "Cant allocate line buffer");
    }
  }

  free(line);

end:
  const size_t out_max_len = strlen(data->builddir) + SLASH +
                             strlen(data->label) + SLASH + NAME_MAX +
                             1 /* nullptr */;
  vec->data[ARGV_OUTPATH_I] = malloc(out_max_len);
  if (!vec->data[ARGV_OUTPATH_I])
  {
    for (int i = vec->first_opt; i < ARGV_OFLAG_I; ++i)
      free(vec->data[i]);
    free(vec->data);
    ERR(MALLOC, "Cant allocate outpath");
  }
  snprintf(vec->data[ARGV_OUTPATH_I], out_max_len, "%s/%s/", data->builddir,
           data->label);
  if (access(vec->data[ARGV_OUTPATH_I], F_OK))
    FAIL_CODE(mkdir(vec->data[ARGV_OUTPATH_I], 0755), -errno,
              "Cant create directory \"%s\"", vec->data[ARGV_OUTPATH_I]);

  vec->data[ARGV_OFLAG_I] = "-o";
  vec->data[ARGV_PATH_I] = nullptr;
  vec->data[ARGV_NULL_I] = nullptr;
  return 0;
}

void
destroy_argv(vector_t **vector)
{
  vector_t *vec = *vector;
  for (; vec->first_opt < ARGV_PATH_I; ++vec->first_opt)
    free(vec->data[vec->first_opt]);
  free(vec->data[ARGV_OUTPATH_I]);
  free(vec->data);

#ifndef DEBUG
  *vector = nullptr;
#endif
}
