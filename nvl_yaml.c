/*
 * Copyright (c) 2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <yaml.h>

#include "nvl.h"
#include "error.h"
#include "utils.h"

/* enclosing type */
enum enctype {
	ET_MAP,
	ET_SEQ,
};

struct state {
	enum enctype type;

	/* map */
	nvlist_t *nvl;
	bool key;
	yaml_event_t ykey; /* saved key */

	/* seq */
	ulong_t nvals;
	char *vals[30];
};

/*
 * Parse yaml data blob.  The yaml blob *must* be a mapping.
 *
 */
nvlist_t *nvl_from_yaml(const char *yamlstr, size_t len)
{
	struct state stack[10];
	int idx;
	yaml_parser_t parser;

	if (!yaml_parser_initialize(&parser))
		goto err;

	yaml_parser_set_input_string(&parser, (unsigned char *) yamlstr, len);

	idx = -1;

	for (;;) {
		yaml_event_t e;

		if (!yaml_parser_parse(&parser, &e))
			goto err_parser;

		if (e.type == YAML_STREAM_END_EVENT)
			break;

		switch (e.type) {
			case YAML_SCALAR_EVENT:
				if (stack[idx].type == ET_MAP) {
					if (stack[idx].key) {
						stack[idx].ykey = e;
						stack[idx].key = false;
					} else {
						const char *key = (const char *) stack[idx].ykey.data.scalar.value;
						const char *val = (const char *) e.data.scalar.value;

						nvl_set_str(stack[idx].nvl, key, val);
						stack[idx].key = true;
					}
				} else if (stack[idx].type == ET_SEQ) {
					const char *val = (const char *) e.data.scalar.value;

					stack[idx].vals[stack[idx].nvals++] = xstrdup(val);
				} else {
					ASSERT(0);
				}
				break;
			case YAML_SEQUENCE_START_EVENT:
				idx++;
				stack[idx].type = ET_SEQ;
				stack[idx].nvals = 0;
				break;
			case YAML_SEQUENCE_END_EVENT:
				idx--;
				nvl_set_str_array(stack[idx].nvl,
						  (const char *) stack[idx].ykey.data.scalar.value,
						  stack[idx + 1].vals,
						  stack[idx + 1].nvals);
				break;
			case YAML_MAPPING_START_EVENT:
				idx++;
				stack[idx].type = ET_MAP;
				stack[idx].nvl = nvl_alloc();
				stack[idx].key = true;
				fprintf(stderr, "nvl = %p\n", stack[idx].nvl);
				break;
			case YAML_MAPPING_END_EVENT: {
				nvlist_t *nvl = stack[idx].nvl;

				idx--;

				if (idx >= 0)
					nvl_set_nvl(stack[idx].nvl,
						    (const char *) stack[idx].ykey.data.scalar.value,
						    nvl);
				break;
			}
			default:
				break;
		}
	}


	yaml_parser_delete(&parser);

	return stack[0].nvl;

err_parser:
	yaml_parser_delete(&parser);

err:
	return NULL;
}
