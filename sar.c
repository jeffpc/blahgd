#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "post.h"
#include "sar.h"

static void echo_postid(struct post *post, void *data)
{
	fprintf(post->out, "%d", post->id);
}

char* up_month_strs[12] = {
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December",
};

static void echo_story_title(struct post *post, void *data)
{
	fprintf(post->out, "%s", post->title);
}

static void echo_posttime(struct post *post, void *data)
{
	fprintf(post->out, "%02d:%02d", post->time.tm_hour,
		post->time.tm_min);
}

static void echo_postdate(struct post *post, void *data)
{
	fprintf(post->out, "%s %d, %04d",
		up_month_strs[post->time.tm_mon], post->time.tm_mday,
		1900+post->time.tm_year);
}

static void __echo_comment_count(struct post *post, void *data, int numeric)
{
	char path[FILENAME_MAX];
	struct dirent *de;
	DIR *dir;
	int count = 0;

	snprintf(path, FILENAME_MAX, "data/posts/%d/comments", post->id);

	dir = opendir(path);
	if (!dir) {
		fprintf(post->out, numeric ? "0" : "No");
		return;
	}

	while((de = readdir(dir))) {
		if (!strcmp(de->d_name, ".") ||
		    !strcmp(de->d_name, ".."))
			continue;

		count++;
	}

	closedir(dir);

	fprintf(post->out, "%d", count);
}

static void echo_comment_count(struct post *post, void *data)
{
	__echo_comment_count(post, data, 0);
}

static void echo_comment_count_numeric(struct post *post, void *data)
{
	__echo_comment_count(post, data, 1);
}

static struct repltab_entry __repltab_story_html[] = {
	{"POSTID",	echo_postid},
	{"POSTDATE",	echo_postdate},
	{"POSTTIME",	echo_posttime},
	{"TITLE",	echo_story_title},
	{"COMCOUNT",	echo_comment_count},
	{"",		NULL},
};

static struct repltab_entry __repltab_story_numcomment_html[] = {
	{"POSTID",	echo_postid},
	{"POSTDATE",	echo_postdate},
	{"POSTTIME",	echo_posttime},
	{"TITLE",	echo_story_title},
	{"COMCOUNT",	echo_comment_count_numeric},
	{"",		NULL},
};

static void echo_comment_id(struct post *post, void *data)
{
	struct comment *comm = data;

	fprintf(post->out, "%d", comm->id);
}

static void echo_comment_author(struct post *post, void *data)
{
	struct comment *comm = data;

	fprintf(post->out, "%s", comm->author);
}

static void echo_comment_time(struct post *post, void *data)
{
	struct comment *comm = data;

	fprintf(post->out, "%02d:%02d", comm->time.tm_hour,
		comm->time.tm_min);
}

static void echo_comment_date(struct post *post, void *data)
{
	struct comment *comm = data;

	fprintf(post->out, "%s %d, %04d",
		up_month_strs[comm->time.tm_mon], comm->time.tm_mday,
		1900+comm->time.tm_year);
}

static struct repltab_entry __repltab_comm_html[] = {
	{"POSTID",	echo_postid},
	{"POSTDATE",	echo_postdate},
	{"POSTTIME",	echo_posttime},
	{"TITLE",	echo_story_title},
	{"COMCOUNT",	echo_comment_count},
	{"COMID",	echo_comment_id},
	{"COMAUTHOR",	echo_comment_author},
	{"COMDATE",	echo_comment_date},
	{"COMTIME",	echo_comment_time},
	{"",		NULL},
};

static void echo_cat_name(struct post *post, void *data)
{
	char *name = data;

	fprintf(post->out, "%s", name);
}

static struct repltab_entry __repltab_cat_html[] = {
	{"CATNAME",	echo_cat_name},
	{"",		NULL},
};

static void echo_arch_name(struct post *post, void *data)
{
	char *name = data;

	fprintf(post->out, "%s", name);
}

static void echo_arch_desc(struct post *post, void *data)
{
	char *name = data;

	fprintf(post->out, "%s %c%c%c%c", up_month_strs[atoi(name+4)-1],
		name[0], name[1], name[2], name[3]);
}

static struct repltab_entry __repltab_arch_html[] = {
	{"ARCHNAME",	echo_arch_name},
	{"ARCHDESC",	echo_arch_desc},
	{"",		NULL},
};

struct repltab_entry *repltab_story_html = __repltab_story_html;
struct repltab_entry *repltab_story_numcomment_html = __repltab_story_numcomment_html;
struct repltab_entry *repltab_comm_html = __repltab_comm_html;
struct repltab_entry *repltab_cat_html = __repltab_cat_html;
struct repltab_entry *repltab_arch_html = __repltab_arch_html;

static int invoke_repl(struct post *post, void *data, char *cmd,
		       struct repltab_entry *repltab)
{
	int i;

	if (!repltab)
		return 1;

	for(i=0; repltab[i].f; i++) {
		if (strcmp(cmd, repltab[i].what))
			continue;

		repltab[i].f(post, data);
		return 0;
	}

	return 1;
}

#define SAR_NORMAL	0
#define SAR_WAIT1	1
#define SAR_WAIT2	2
#define SAR_SPECIAL	3
#define SAR_ERROR	4

void sar(struct post *post, void *data, char *ibuf, int size,
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
					if (invoke_repl(post, data, cmd, repltab))
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