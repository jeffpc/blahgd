#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "post.h"
#include "sar.h"

static void echo_postid(struct post *post, struct comment *comm)
{
	fprintf(post->out, "%d", post->id);
}

static char* up_month_strs[12] = {
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December",
};

static void echo_title(struct post *post, struct comment *comm)
{
	fprintf(post->out, "%s", post->title);
}

static void echo_posttime(struct post *post, struct comment *comm)
{
	fprintf(post->out, "%02d:%02d", post->time.tm_hour,
		post->time.tm_min);
}

static void echo_postdate(struct post *post, struct comment *comm)
{
	fprintf(post->out, "%s %d, %04d",
		up_month_strs[post->time.tm_mon], post->time.tm_mday,
		1900+post->time.tm_year);
}

static void echo_comment_count(struct post *post, struct comment *comm)
{
	char path[FILENAME_MAX];
	struct dirent *de;
	DIR *dir;
	int count;

	snprintf(path, FILENAME_MAX, "data/posts/%d/comments", post->id);

	dir = opendir(path);
	if (!dir)
		return;

	count = 0;
	while((de = readdir(dir))) {
		if (!strcmp(de->d_name, ".") ||
		    !strcmp(de->d_name, ".."))
			continue;

		count++;
	}

	closedir(dir);

	fprintf(post->out, "%d", count);
}

static struct repltab_entry __repltab_html[] = {
	{"POSTID",	echo_postid},
	{"POSTDATE",	echo_postdate},
	{"POSTTIME",	echo_posttime},
	{"TITLE",	echo_title},
	{"COMCOUNT",	echo_comment_count},
	{"",		NULL},
};

static void echo_comment_id(struct post *post, struct comment *comm)
{
	fprintf(post->out, "%d", comm->id);
}

static void echo_comment_author(struct post *post, struct comment *comm)
{
	fprintf(post->out, "%s", comm->author);
}

static void echo_comment_time(struct post *post, struct comment *comm)
{
	fprintf(post->out, "%02d:%02d", comm->time.tm_hour,
		comm->time.tm_min);
}

static void echo_comment_date(struct post *post, struct comment *comm)
{
	fprintf(post->out, "%s %d, %04d",
		up_month_strs[comm->time.tm_mon], comm->time.tm_mday,
		1900+comm->time.tm_year);
}

static struct repltab_entry __repltab_comm_html[] = {
	{"POSTID",	echo_postid},
	{"POSTDATE",	echo_postdate},
	{"POSTTIME",	echo_posttime},
	{"TITLE",	echo_title},
	{"COMCOUNT",	echo_comment_count},
	{"COMID",	echo_comment_id},
	{"COMAUTHOR",	echo_comment_author},
	{"COMDATE",	echo_comment_date},
	{"COMTIME",	echo_comment_time},
	{"",		NULL},
};

struct repltab_entry *repltab_html = __repltab_html;
struct repltab_entry *repltab_comm_html = __repltab_comm_html;

static int invoke_repl(struct post *post, struct comment *comm, char *cmd,
		       struct repltab_entry *repltab)
{
	int i;

	if (!repltab)
		return 1;

	for(i=0; repltab[i].f; i++) {
		if (strcmp(cmd, repltab[i].what))
			continue;

		repltab[i].f(post, comm);
		return 0;
	}

	return 1;
}

#define SAR_NORMAL	0
#define SAR_WAIT1	1
#define SAR_WAIT2	2
#define SAR_SPECIAL	3
#define SAR_ERROR	4

#define COPYCHAR(ob, oi, c)	do { \
					ob[oi] = c; \
					oi++; \
				} while(0)

void sar(struct post *post, struct comment *comm, char *ibuf, int size,
	 struct repltab_entry *repltab)
{
	char obuf[size];
	char cmd[16];
	int iidx, oidx, cidx;
	int state = SAR_NORMAL;
	char tmp;

	for(iidx = oidx = cidx = 0; (iidx < size) && (state != SAR_ERROR); iidx++) {
		tmp = ibuf[iidx];
#if 0
		fprintf(post->out, "\n| cur '%c', state %d| ", tmp, state);
#endif

		switch(state) {
			case SAR_NORMAL:
				if (tmp == '@')
					state = SAR_WAIT1;
				else
					COPYCHAR(obuf, oidx, tmp);
				break;

			case SAR_WAIT1:
				if (tmp == '@') {
					state = SAR_SPECIAL;
					COPYCHAR(obuf, oidx, '\0');
					fprintf(post->out, obuf);
					oidx = 0;
				} else {
					state = SAR_NORMAL;
					COPYCHAR(obuf, oidx, '@');
					COPYCHAR(obuf, oidx, tmp);
				}
				break;

			case SAR_SPECIAL:
				if (tmp != '@' && cidx < 16)
					COPYCHAR(cmd, cidx, tmp);
				else if (tmp != '@' && cidx >= 16)
					state = SAR_ERROR;
				else
					state = SAR_WAIT2;
				break;

			case SAR_WAIT2:
				if (tmp != '@')
					state = SAR_ERROR;
				else {
					COPYCHAR(cmd, cidx, '\0');
					if (invoke_repl(post, comm, cmd, repltab))
						fprintf(post->out, "@@%s@@", cmd);
					cidx = 0;
					state = SAR_NORMAL;
				}
				break;
		}
	}

	if (state == SAR_ERROR)
		fprintf(post->out, "\n====================\nSAR error!\n");

	if (oidx) {
		COPYCHAR(obuf, oidx, '\0');
		fprintf(post->out, obuf);
	}
}
