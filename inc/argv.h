#pragma once

#include "util.h"

#define ARGV_NULL_I argc - 1
#define ARGV_OUTPATH_I argc - 2
#define ARGV_OFLAG_I argc - 3
#define ARGV_PATH_I argc - 4

#define SLASH 1

int
make_argv(char ***ret, const struct data_t *const data, int *const argc, int *const first_opt);

void
destroy_argv(char ***argv, int argc, int first_opt);
