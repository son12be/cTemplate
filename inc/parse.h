#pragma once

#include <stdio.h>

struct data_t
{
  char *cc;
  char *cc_args;
  char *srcdirs;
  char *builddir;
  char *name;
  char *ext;
  char *pre_cc;
  char *label;
  FILE *flagFile;
  unsigned int threads;
};

int
parse_template(struct data_t *data);

void
destroy_data(struct data_t *data);
