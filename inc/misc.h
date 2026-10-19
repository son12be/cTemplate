#pragma once

#include "parse.h"
#include "argv.h"

int
pre_cc(struct data_t *data);

void
sigchld_handler(int);

int
should_compile(const char *const source_path, const char *const object_path, const int comp_force);

int
exec_cc(vector_t *vec);
