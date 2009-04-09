#ifndef __HTML_H
#define __HTML_H

#include "post.h"

extern void html_story(struct post *post);
extern void html_index(struct post *post);
extern void html_archive(struct post *post, int archid);
extern void html_category(struct post *post, char *catname);
extern void html_comments(struct post *post);
extern void html_sidebar(struct post *post);
extern void html_header(struct post *post);
extern void html_footer(struct post *post);

extern void feed_index(struct post *post, char *fmt);
extern void feed_header(struct post *post, char *fmt);
extern void feed_footer(struct post *post, char *fmt);

#endif
