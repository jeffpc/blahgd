#ifndef __PIPELINE_H
#define __PIPELINE_H

#include "list.h"
#include "vars.h"

struct pipestages {
	const char *name;
	struct var_val *(*f)(struct var_val *);
};

struct pipeline {
	struct list_head pipe;
	const struct pipestages *stage;
};

extern void init_pipe_subsys();

extern struct pipeline *pipestage(char *name);
extern struct pipeline *pipeconnect(struct pipeline *a, struct pipeline *b);

#endif
