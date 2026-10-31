#pragma once

#include "argv.h"

int
exec_cmd(const char *const cmd);

void
sigchld_handler(int);

int
should_compile(const char *const source_path, const char *const object_path, const int comp_force);

int
exec_cc(vector_t *vec);

int
run_pst_cc_bin(const char *const path, const char *const args, 
    const char *const env);
