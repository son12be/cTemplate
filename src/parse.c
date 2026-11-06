#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "err.h"
#include "main_info.h"
#include "misc.h"
#include "parse.h"

struct tuple_t
{
  const char *key;
  char **value;
  bool required;
};

const struct tuple_t confTuples[] = {
    {"SRCDIRS", &data.srcdirs, true},
    {"BUILDDIR", &data.builddir, true},
    {"EXT", &data.ext, true},
    {"CC", &data.cc, true},
    {"NAME", &data.name, true},
    {"ARGS", &data.cc_args, false},
    {"PRE_CC", &data.pre_cc, false},
    {"PST_CC", &data.pst_cc, false},
    {"RUN_ARGS", &data.run_args, false},
    {"RUN_ENV", &data.run_env, false},
};

void
destroy_data()
{
  for (size_t i = 0; i < sizeof(confTuples) / sizeof(confTuples[0]); ++i)
  {
    if (*confTuples[i].value)
      free(*confTuples[i].value);
  }
}

static inline int
actual_parse(const FILE *template, int cur_line)
{
  bool label_hit_pending = data.label != nullptr;

  char line[LARGE_ENOUGH_LINE];
  while ((fgets(line, sizeof(line), template)) != nullptr)
  {
    cur_line++;
    char *key = line;
    trim_ws(&key);

    if (key[0] == '\0' || key[0] == '#')
      continue;

    /* found another label, time to stop */
    if (strstr(key, ":\0"))
    {
      if (label_hit_pending)
      {
        return 0;
      }
      else
      {
        label_hit_pending = true;
        continue;
      }
    }

    // line_cc[strcspn(line_cc, "\n")] = '\0'; // isspace considers '\n' as
    // space

    /* find the whitespace between key and value */
    char *value = strchr(key, ' ');
    FAIL_CODE(!value, BAD_FORMAT, "At line %i. Key \"%s\" should have a value",
              cur_line, key);

    value++;

    *(value - 1) = '\0';
    trim_ws(&value);

    /* handle special cases */
    if (streq(key, "FLAGFILE"))
    {
      if (data.flagFile)
      {
        WARN("Key \"FLAGFILE\" was already specified");
        fclose(data.flagFile);
      }

      data.flagFile = fopen(value, "r");
      FAIL_CODE(!data.flagFile, CANT_OPEN,
                "At line %i. Cant open flagfile \"%s\"", cur_line, value);
      continue;
    }
    else if (streq(key, "SUBDIR"))
    {
      char *arg = strchr(value, ' ');
      if (arg)
      {
        *arg = '\0';
        arg++;
      }
      else
      {
        arg = "";
      }

      const char *const dir = value;

      char cmd[sizeof(line) - strlen(dir) + strlen(get_argv()[0]) + 1];
      snprintf(cmd, sizeof(cmd), "%s %s", get_argv()[0], arg);

      char pwd[PATH_MAX];
      getcwd(pwd, sizeof(pwd));

      trim_ws(&dir);
      FAIL_CODE(chdir(dir) < 0, -errno,
                "In SUBDIR: Cant cd into \"%s\". At line %i", dir, cur_line);

      LOG("Entering subdir \"%s\" at line %i...", dir, cur_line);
      FAIL(exec_cmd(cmd));

      chdir(pwd);

      continue;
    }

    int found = 0;
    for (unsigned int i = 0; i < sizeof(confTuples) / sizeof(confTuples[0]);
         ++i)
    {
      if (!streq(key, confTuples[i].key))
        continue;

      if (*confTuples[i].value)
      {
        WARN("Key \"%s\" was already specified", key);
        free(*confTuples[i].value);
      }

      const size_t len = strlen(value) + 1;
      *confTuples[i].value = malloc(len);

      snprintf(*confTuples[i].value, len, "%s", value);

      found = 1;
      break;
    }

    if (!found)
    {
      WARN("Key \"%s\" unrecognized", key);
    }
  }
  FAIL_CODE(errno, -errno, "Failed while parsing template");

  return 0;
}

static inline int
find_label(const FILE *template, const char *const label)
{
  rewind(template);
  int cur_line = 0;
  if (!label)
    goto first_label;

  const size_t label_len = strlen(label);
  for (char line[LARGE_ENOUGH_LINE];
       fgets(line, sizeof(line), template) != NULL;)
  {
    cur_line++;
    if (strncmp(line, label, label_len) == 0)
      if (line[label_len] == ':')
        return cur_line;
  }

  return -1;

first_label:
  for (char line[LARGE_ENOUGH_LINE];
       fgets(line, sizeof(line), template) != NULL;)
  {
    cur_line++;
    char *colon = strrchr(line, ':');
    if (!colon)
      continue;

    return cur_line;
  }

  return 0;
}

int
parse_template()
{
  const FILE *template = fopen(TEMPLATE_FILENAME, "r");
  if (!template)
    ERR(CANT_OPEN, "Template file (%s)", TEMPLATE_FILENAME);

  int cur_line = find_label(template, "global");
  if (cur_line >= 0)
  {
    FAIL_GOTO(actual_parse(template, cur_line) < 0, err);
  }

  cur_line = find_label(template, data.label);
  if (cur_line >= 0)
  {
    FAIL_GOTO(actual_parse(template, cur_line) < 0, err);
  }
  else
  {
    ERR_NR(NOMATCH, "Cant find label \"%s\" in TEMPLATE", data.label);
    goto err;
  }

  fclose(template);

  for (unsigned int i = 0; i < sizeof(confTuples) / sizeof(struct tuple_t); ++i)
  {
    if (confTuples[i].required && !*confTuples[i].value)
      ERR(BAD_FORMAT, "A value for key \"%s\" is required", confTuples[i].key);
  }

  return 0;

err:
  fclose(template);
  return -1;
}
