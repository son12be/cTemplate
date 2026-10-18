#pragma once

#include <stdio.h>

#include "misc.h"

/* TODO
 * make it all malloc'd instead
 * of guessing appropiate sizes
 */
struct data_t
{
	char cc[SMALLER_VALUE_SIZE * 2];
	char cc_args[VALUE_SIZE];
	char srcdirs[VALUE_SIZE];
	char builddir[SMALL_VALUE_SIZE];
	char name[SMALL_VALUE_SIZE];
	char ext[SMALLER_VALUE_SIZE];
  char pre_cc[VALUE_SIZE];
	char *label;
	FILE *flagFile;
	unsigned int threads;
};

int
parse_template(struct data_t *data);
