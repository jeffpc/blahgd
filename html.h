#ifndef __HTML_H
#define __HTML_H

#include "post.h"

extern void html_story(struct post *post);
extern void html_save_comment(struct post *post, int notsaved);
extern void html_archive(struct post *post, int archid);
extern void html_tag(struct post *post, char *tagname, char *bydir,
		     int numstories);
extern void html_comments(struct post *post);
extern void html_sidebar(struct post *post);

extern void feed_index(struct post *post, char *fmt, int limit);
extern void feed_header(struct post *post, char *fmt);
extern void feed_footer(struct post *post, char *fmt);

#endif
