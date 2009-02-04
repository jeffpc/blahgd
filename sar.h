#ifndef __SAR_H
#define __SAR_H

#include "post.h"

struct repltab_entry {
	char what[16];
	void (*f)(struct post*, struct comment*);
};

extern void sar(struct post *post, struct comment *comm, char *ibuf,
		int size, struct repltab_entry *repltab);

extern struct repltab_entry *repltab_html;
extern struct repltab_entry *repltab_comm_html;

#endif
