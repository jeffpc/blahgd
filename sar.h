#ifndef __SAR_H
#define __SAR_H

#include "post.h"

struct repltab_entry {
	char what[16];
	void (*f)(struct post*);
};

extern void sar(struct post *post, char *ibuf, int size,
		struct repltab_entry *repltab);

struct repltab_entry *repltab_html;

#endif
