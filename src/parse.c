#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "misc.h"
#include "err.h"
#include "parse.h"
#include "util.h"

struct tuple_t
{
	const char *key;
	void *value;
  uint8_t required;
};

#define GET_CONF_TABLE \
	struct tuple_t confTuples[] = \
	{ \
		{ "SRCDIRS", &data->srcdirs, 1 }, \
		{ "BUILDDIR", &data->builddir, 1 }, \
		{ "EXT", &data->ext, 1 }, \
		{ "CC", &data->cc, 1 }, \
		{ "ARGS", &data->cc_args, 0 }, \
		{ "NAME", &data->name, 1 }, \
		{ "PRE_CC", &data->pre_cc, 0 }, \
	};


void
destroy_data(struct data_t *data)
{
  GET_CONF_TABLE;
  for(size_t i = 0; i < sizeof(confTuples) / sizeof(confTuples[0]); ++i)
  {
    if(*((char**)confTuples[i].value))
      free(*((char**)confTuples[i].value));
  }
}

static inline int
actual_parse(struct data_t *const data, const FILE *template, int cur_line, struct tuple_t *const confTuples, const unsigned int n)
{
	int label_hit = data->label ? 1 : 0;
	char line[VALUE_SIZE];
	while((fgets(line, sizeof(line), template)) != NULL)
	{
		cur_line++;
		char *line_cc = line;
		trim_ws(&line_cc);

		if(line_cc[0] == '\0' || line_cc[0] == '#')
			continue;

		/* found another label, time to stop */
		if(strstr(line_cc, ":\0"))
		{
			if(label_hit)
				return 0;
			else
			{
				label_hit = 1;
				continue;
			}
		}

		// line_cc[strcspn(line_cc, "\n")] = '\0'; // isspace considers '\n' as space

		/* find the whitespace between key and value */
		char *value = strchr(line_cc, ' ');
		FAIL_CODE(!value || *(++value) == '\0', BAD_FORMAT, "At line %i. Key should have a value", cur_line);

		*(value - 1) = '\0';
		trim_ws(&value);

		/* handle special cases */
		if(streq(line_cc, "FLAGFILE"))
		{
			data->flagFile = fopen(value, "r");
			FAIL_CODE(!data->flagFile, CANT_OPEN,  "At line %i. Cant open flagfile \"%s\"", cur_line, value);
			continue;
		}

		for(unsigned int i = 0; i < n; ++i)
		{
			if(!streq(line_cc, confTuples[i].key))
				continue; 

      const size_t len = strlen(value) + 1;
      *((char**)confTuples[i].value) = malloc(len);

			snprintf(*((char**)confTuples[i].value), len, "%s", value);
		}
	}
	FAIL_CODE(errno, -errno, "Failed while parsing template");
	return 0;
}

static inline int
find_label(const FILE *template, const char *const label)
{
	rewind(template);
	int cur_line = 0;
	if(!label)
		goto first_label;

	const int label_len = strlen(label);
	for(char line[VALUE_SIZE]; fgets(line, sizeof(line), template) != NULL; )
	{
		cur_line++;
		if(strncmp(line, label, label_len) == 0)
			if(line[label_len] == ':')
				return cur_line;
	}

	return -1;

first_label:
	for(char line[VALUE_SIZE]; fgets(line, sizeof(line), template) != NULL; )
	{
		cur_line++;
		char *colon = strrchr(line, ':');
		if(!colon)
			continue;

		return cur_line;
	}

	return 0;
}

/* NOTE
 * Line count breaks if the line is larger than VALUE_SIZE - 1
 */
int
parse_template(struct data_t *data)
{
	const FILE *template = fopen(TEMPLATE_FILENAME, "r");
	if(!template)
		ERR(CANT_OPEN, "Template file (%s)", TEMPLATE_FILENAME);

  GET_CONF_TABLE;

	int cur_line = 0;
	if((cur_line = find_label(template, "global")) >= 0)
	{
		// if(!data->label) data->label = "global";
		FAIL_GOTO(actual_parse(data, template, cur_line, confTuples, sizeof(confTuples) / sizeof(struct tuple_t)) < 0, err);
	}

	if((cur_line = find_label(template, data->label)) >= 0)
	{
		FAIL_GOTO(actual_parse(data, template, cur_line, confTuples, sizeof(confTuples) / sizeof(struct tuple_t)) < 0, err);
	} else 
	{
		ERR_NR(NOMATCH, "Cant find label \"%s\" in TEMPLATE", data->label);
		goto err;
	}
	
	fclose(template);

	for(unsigned int i = 0; i < sizeof(confTuples) / sizeof(struct tuple_t); ++i)
	{
		if(confTuples[i].required && !*((char**)confTuples[i].value))
			ERR(BAD_FORMAT, "A value for key \"%s\" is required", confTuples[i].key);
	}

	return 0;

err:
	fclose(template);
	return -1;
}
