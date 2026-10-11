#pragma once

#include "util.h"

#define ARGV_NULL_I vec->size - 1
#define ARGV_OUTPATH_I vec->size - 2
#define ARGV_OFLAG_I vec->size - 3
#define ARGV_PATH_I vec->size - 4

#define SLASH 1

typedef struct vector_t
{
  char **data;
  int size;
  int first_opt;
} vector_t;

int
make_argv(vector_t *ret, const struct data_t *const data);

void
destroy_argv(vector_t **vec);
