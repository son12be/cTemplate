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
  char *run_args;
  char *run_env;
  char *label;
  FILE *flagFile;
  unsigned int threads;
};

extern struct data_t data;

int
parse_template();

void
destroy_data();
