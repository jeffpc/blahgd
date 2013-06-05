#ifndef __PIPELINE_H
#define __PIPELINE_H

#include "list.h"
#include "vars.h"

struct pipestages {
	const char *name;
	struct val *(*f)(struct val *);
};

struct pipeline {
	struct list_head pipe;
	const struct pipestages *stage;
};

extern void init_pipe_subsys();

extern struct pipeline *pipestage(char *name);
extern void pipeline_destroy(struct list_head *pipelist);

#endif
