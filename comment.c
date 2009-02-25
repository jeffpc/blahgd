#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
#include "post.h"
#include "sar.h"
#include "html.h"

#define SHORT_BUF_LEN		128
#define MEDIUM_BUF_LEN		256
#define LONG_BUF_LEN		(64*1024)

#define SC_IGNORE		0
#define SC_AUTHOR		1
#define SC_AUTHOR_EQ		11
#define SC_EMAIL		2
#define SC_EMAIL_EQ		12
#define SC_URL			3
#define SC_URL_EQ		13
#define SC_COMMENT		4
#define SC_COMMENT_EQ		14
#define SC_DATE			5
#define SC_DATE_EQ		15
#define SC_SUB			6
#define SC_SUB_EQ		16
#define SC_ID			7
#define SC_ID_EQ		17
#define SC_ERROR		20

void save_comment(struct post *post)
{
	char path[FILENAME_MAX];
	int in;
	char tmp;

	int state;

	char author_buf[SHORT_BUF_LEN];
	int author_len = 0;
	char email_buf[SHORT_BUF_LEN];
	int email_len = 0;
	char url_buf[MEDIUM_BUF_LEN];
	int url_len = 0;
	char comment_buf[LONG_BUF_LEN];
	int comment_len = 0;
	time_t date = 0;
	int id = 0;

	int ret;

	struct timespec now;

	author_buf[0] = '\0'; /* better be paranoid */
	email_buf[0] = '\0'; /* better be paranoid */
	url_buf[0] = '\0'; /* better be paranoid */
	comment_buf[0] = '\0'; /* better be paranoid */

	for(state = SC_IGNORE; (in = getchar()) != EOF; ) {
		tmp = in;

#if 0
		fprintf(post->out, "|'%c' %d|\n", tmp, state);
#endif

		switch(state) {
			case SC_IGNORE:
				if (tmp == 'a')
					state = SC_AUTHOR_EQ;
				if (tmp == 'e')
					state = SC_EMAIL_EQ;
				if (tmp == 'u')
					state = SC_URL_EQ;
				if (tmp == 'c')
					state = SC_COMMENT_EQ;
				if (tmp == 'd')
					state = SC_DATE_EQ;
				if (tmp == 's')
					state = SC_SUB_EQ;
				if (tmp == 'i')
					state = SC_ID_EQ;
				break;

			case SC_AUTHOR_EQ:
				state = (tmp == '=') ? SC_AUTHOR : SC_ERROR;
				break;

			case SC_EMAIL_EQ:
				state = (tmp == '=') ? SC_EMAIL : SC_ERROR;
				break;

			case SC_URL_EQ:
				state = (tmp == '=') ? SC_URL : SC_ERROR;
				break;

			case SC_COMMENT_EQ:
				state = (tmp == '=') ? SC_COMMENT : SC_ERROR;
				break;

			case SC_DATE_EQ:
				state = (tmp == '=') ? SC_DATE : SC_ERROR;
				break;

			case SC_SUB_EQ:
				state = (tmp == '=') ? SC_SUB : SC_ERROR;
				break;

			case SC_ID_EQ:
				state = (tmp == '=') ? SC_ID : SC_ERROR;
				break;

			case SC_AUTHOR:
				if (tmp == '&' || author_len == SHORT_BUF_LEN-1) {
					tmp = '\0';
					state = SC_IGNORE;
				}

				COPYCHAR(author_buf, author_len, tmp);
				break;

			case SC_EMAIL:
				if (tmp == '&' || email_len == SHORT_BUF_LEN-1) {
					tmp = '\0';
					state = SC_IGNORE;
				}

				COPYCHAR(email_buf, email_len, tmp);
				break;

			case SC_URL:
				if (tmp == '&' || url_len == MEDIUM_BUF_LEN-1) {
					tmp = '\0';
					state = SC_IGNORE;
				}

				COPYCHAR(url_buf, url_len, tmp);
				break;

			case SC_COMMENT:
				if (tmp == '&' || comment_len == LONG_BUF_LEN-1) {
					tmp = '\0';
					state = SC_IGNORE;
				}

				COPYCHAR(comment_buf, comment_len, tmp);
				break;

			case SC_DATE:
				if (tmp == '&')
					state = SC_IGNORE;
				else
					date = (date * 10) + (tmp - '0');
				break;

			case SC_SUB:
				/* ignore the submit button */
				if (tmp == '&')
					state = SC_IGNORE;
				break;

			case SC_ID:
				if (tmp == '&')
					state = SC_IGNORE;
				else
					id = (id * 10) + (tmp - '0');
				break;
		}

		if (state == SC_ERROR)
			break;
	}

	fprintf(post->out, "author: \"%s\"\n", author_buf);
	fprintf(post->out, "email: \"%s\"\n", email_buf);
	fprintf(post->out, "url: \"%s\"\n", url_buf);
	fprintf(post->out, "comment: \"%s\"\n", comment_buf);
	fprintf(post->out, "date: %lu\n", date);
	fprintf(post->out, "id: %d\n", id);

	if ((strlen(author_buf) == 0) ||
	    (strlen(email_buf) == 0) ||
	    (strlen(comment_buf) == 0)) {
		fprintf(post->out, "You must fill in name, email, and comment\n");
		return;
	}

	ret = load_post(id, post);
	if (ret) {
		fprintf(post->out, "Gah! %d (postid=%d)\n", ret, id);
		return;
	}

	clock_gettime(CLOCK_REALTIME, &now);
	if ((now.tv_sec > (date+COMMENT_MAX_DELAY)) || (now.tv_sec < (date+COMMENT_MIN_DELAY))) {
		fprintf(post->out, "Flash-gordon or geriatric was here...  %lu %lu\n", now.tv_sec, date);
		return;
	}

	snprintf(path, FILENAME_MAX, "data/pending-comments/%d-%lu%09luW",
		 id, now.tv_sec, now.tv_nsec);

	if (mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
		fprintf(post->out, "Ow, could not create directory: %d (%s)\n", errno, strerror(errno));
		return;
	}

	/*
	 * TODO:
	 * - write to data/pending-comments/....W/post.txt
	 * - set xattrs on data/pending-comments/....W
	 * - rename data/pending-comments/....W to
	 *   data/pending-comments/....
	 */
}

int main(int argc, char **argv)
{
	struct timespec s,e;
	struct post post;

	clock_gettime(CLOCK_REALTIME, &s);

	post.out = stdout;

	fprintf(post.out, "Content-Type: text/plain\n\n");

	save_comment(&post);

#if 0
	html_header(&post);
	html_story(&post);
	html_comments(&post);
	html_sidebar(&post);
	html_footer(&post);
#endif

	destroy_post(&post);

	clock_gettime(CLOCK_REALTIME, &e);

	fprintf(post.out, "<!-- time to render: %ld.%09ld seconds -->\n", (int)e.tv_sec-s.tv_sec,
		e.tv_nsec-s.tv_nsec+((e.tv_sec-s.tv_sec) ? 1000000000 : 0));

	return 0;
}
