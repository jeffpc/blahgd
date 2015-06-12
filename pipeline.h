/*
 * Copyright (c) 2013-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#ifndef __PIPELINE_H
#define __PIPELINE_H

#include <sys/list.h>

#include "vars.h"

struct pipestageinfo {
	const char *name;
	struct val *(*f)(struct val *);
};

struct pipestage {
	list_node_t pipe;
	const struct pipestageinfo *stage;
};

struct pipeline {
	list_t pipe;
};

extern void init_pipe_subsys(void);

extern struct pipestage *pipestage_alloc(char *name);
extern struct pipeline *pipestage_to_pipeline(struct pipestage *stage);
extern void pipeline_append(struct pipeline *line, struct pipestage *stage);
extern void pipeline_destroy(struct pipeline *line);

#endif
