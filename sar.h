#ifndef __SAR_H
#define __SAR_H

#include "post.h"

struct repltab_entry {
	char what[16];
	void (*f)(struct post*, void*);
};

extern void sar(struct post *post, void *data, char *ibuf,
		int size, struct repltab_entry *repltab);

extern struct repltab_entry *repltab_story_html;
extern struct repltab_entry *repltab_comm_html;
extern struct repltab_entry *repltab_cat_html;
extern struct repltab_entry *repltab_arch_html;

extern char* up_month_strs[12];

#define COPYCHAR(ob, oi, c)	do { \
					ob[oi] = c; \
					oi++; \
				} while(0)

#endif
