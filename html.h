#ifndef __HTML_H
#define __HTML_H

#include "post.h"

extern void html_story(struct post *post);
extern void html_index(struct post *post);
extern void html_archive(struct post *post, int archid);
extern void html_comments(struct post *post);
extern void html_sidebar(struct post *post);
extern void html_header(struct post *post);
extern void html_footer(struct post *post);

#endif
