#pragma once

#include <stdio.h>
#include <semaphore.h>

#include "misc.h"

extern sem_t sem;

#define OPTCOMP_FORCE  0b00000001
#define OPTRUN         0b00000010

#define streq(A, B) (strcmp(A, B) == 0)

enum logLevel_e
{
	LOG_ERR = 0,
	LOG_WARN = 1,
	LOG_DEBUG = 2,
};

struct data_t
{
	char cc[VALUE_SIZE];
	char srcdirs[VALUE_SIZE];
	char builddir[SMALL_VALUE_SIZE];
	char name[SMALL_VALUE_SIZE];
	char ext[SMALLER_VALUE_SIZE];
	char *label;
	FILE *flagFile;
	unsigned int threads;
};

int
strends(const char *A, const char *B);

char *
trim_ws(const char **s);

void
sigchld_handler(int);

int
should_compile(const char *const source_path, const char *const object_path, const int comp_force);

int
exec_cc(char **argv);
