#pragma once

#include <semaphore.h>

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

int
shell_exec(char *cmd);
