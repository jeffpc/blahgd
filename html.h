#ifndef __HTML_H
#define __HTML_H

#include "post.h"

extern void html_story(struct post_old *post);
extern void html_save_comment(struct post_old *post, int notsaved);
extern void html_archive(struct post_old *post, int archid);
extern void html_tag(struct post_old *post, char *tagname, char *bydir,
		     int numstories);
extern void html_comments(struct post_old *post);
extern void html_sidebar(struct post_old *post);

extern void feed_index(struct post_old *post, char *fmt, int limit);
extern void feed_header(struct post_old *post, char *fmt);
extern void feed_footer(struct post_old *post, char *fmt);

#endif
