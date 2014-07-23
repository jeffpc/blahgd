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

extern void init_pipe_subsys();

extern struct pipestage *pipestage_alloc(char *name);
extern struct pipeline *pipestage_to_pipeline(struct pipestage *stage);
extern void pipeline_append(struct pipeline *line, struct pipestage *stage);
extern void pipeline_destroy(struct pipeline *line);

#endif
