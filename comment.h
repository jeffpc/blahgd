#ifndef __COMMENT_H
#define __COMMENT_H

extern int write_out_comment(struct post *post, int id, struct timespec *now,
			     char *author, char *email, char *url, char *comment);

#endif
