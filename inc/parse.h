#pragma once

#include <stdio.h>

#define TEMPLATE_FILENAME "TEMPLATE"
#define LARGE_ENOUGH_LINE 1024
#define VALUE_SIZE 256

struct data_t
{
  char *cc;
  char *cc_args;
  char *srcdirs;
  char *builddir;
  char *name;
  char *ext;
  char *pre_cc;
  char *pst_cc;
  char *label;
  FILE *flagFile;
  unsigned int threads;
};

int
parse_template(struct data_t *data);

void
destroy_data(struct data_t *data);
