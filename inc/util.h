#pragma once

#include <semaphore.h>

#include "argv.h"

extern sem_t sem;

#define OPTCOMP_FORCE  (1u << 0)
#define OPTRUN         (1u << 1)
#define OPTWAIT        (1u << 2)
#define OPTQUIET       (1u << 3)

#define streq(A, B) (strcmp(A, B) == 0)

int
strends(const char *A, const char *B);

char *
trim_ws(const char **s);

void
sigchld_handler(int);

int
should_compile(const char *const source_path, const char *const object_path, const int comp_force);

/* move to compile.* or argv.* */
int
exec_cc(vector_t *vec);
