#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

#include "main.h"
#include "sidebar.h"
#include "render.h"

#include "config.h"
#include "comment.h"
#include "decode.h"

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

int write_out_comment(struct req *req, int id, char *author, char *email,
		      char *url, char *comment)
{
	char curdate[32];
	char path[FILENAME_MAX];
	char *dirpath = NULL;
	char *newdirpath = NULL;
	char *remote_addr; /* yes, this is a pointer */
	int ret;
	int result = 1;
	int fd;

	uint64_t now, now_nsec;
	time_t now_sec;
	struct tm *now_tm;

	ret = load_post(req, id, NULL);
	if (ret) {
		LOG("Gah! %d (postid=%d)", ret, id);
		return 1;
	}

	if ((strlen(author) == 0) || (strlen(comment) == 0)) {
		LOG("You must fill in name, and comment (postid=%d)", id);
		return 1;
	}

	now = gettime();
	now_sec  = now / 1000000000UL;
	now_nsec = now % 1000000000UL;
	now_tm = gmtime(&now_sec);
	if (!now_tm)
		return 1;

	strftime(curdate, 31, "%Y-%m-%d %H:%M", now_tm);

	snprintf(path, FILENAME_MAX, "data/pending-comments/%d-%08lx.%08lx.%05xW",
		 id, now_sec, now_nsec, (unsigned) getpid());

	dirpath = strdup(path);
	newdirpath = strdup(path);
	if (!dirpath || !newdirpath) {
		LOG("Eeeep...ENOMEM");
		return 1;
	}

	if (mkdir(dirpath, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
		LOG("Ow, could not create directory: %d (%s) '%s'", errno, strerror(errno), dirpath);
		goto out_free;
	}

	if ((strlen(path) + strlen("/text.txt")) >= FILENAME_MAX) {
		LOG("Uf...filename too long!");
		goto out_free;
	}

	strcat(path, "/text.txt");

	if ((fd = open(path, O_WRONLY | O_CREAT | O_EXCL,
		       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
		LOG("Couldn't create file ... :( %d (%s) '%s'",
				  errno, strerror(errno), path);
		goto out_free;
	}

	if (xwrite(fd, comment, strlen(comment)) != 0)
		goto out;

#if 0
	safe_setxattr(dirpath, XATTR_COMM_AUTHOR, author,
		      strlen(author));
	safe_setxattr(dirpath, XATTR_TIME, curdate, strlen(curdate));
	safe_setxattr(dirpath, XATTR_COMM_EMAIL, email,
		      strlen(email));

	if (url && url[0])
		safe_setxattr(dirpath, XATTR_COMM_URL, url, strlen(url));

	remote_addr = getenv("HTTP_X_REAL_IP");
	if (remote_addr) {
		safe_setxattr(dirpath, XATTR_COMM_IP, remote_addr,
			      strlen(remote_addr));
	} else {
		remote_addr = getenv("REMOTE_ADDR");
		if (remote_addr)
			safe_setxattr(dirpath, XATTR_COMM_IP, remote_addr,
				      strlen(remote_addr));
	}
#endif
	// FIXME: store metadata

	newdirpath[strlen(newdirpath)-1] = '\0';

	ret = rename(dirpath, newdirpath);
	if (ret) {
		LOG("Could not rename '%s' to '%s' %d (%s)",
				  dirpath, newdirpath, errno,
				  strerror(errno));
		goto out;
	}

	result = 0;

out:
	close(fd);
out_free:
	free(dirpath);
	free(newdirpath);

	return result;
}

#define COPYCHAR(ob, oi, c)	do { \
					ob[oi] = (c); \
					oi++; \
				} while(0)

static int save_comment(struct req *req)
{
	int in;
	char tmp;
	uint64_t now, deltat;

	int state;

	char author_buf[SHORT_BUF_LEN];
	int author_len = 0;
	char email_buf[SHORT_BUF_LEN];
	int email_len = 0;
	char url_buf[MEDIUM_BUF_LEN];
	int url_len = 0;
	char comment_buf[LONG_BUF_LEN];
	int comment_len = 0;
	uint64_t date = 0;
	int id = 0;

	author_buf[0] = '\0'; /* better be paranoid */
	email_buf[0] = '\0'; /* better be paranoid */
	url_buf[0] = '\0'; /* better be paranoid */
	comment_buf[0] = '\0'; /* better be paranoid */

	for(state = SC_IGNORE; (in = getchar()) != EOF; ) {
		tmp = in;

#if 0
		LOG("|'%c' %d|", tmp, state);
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

				if (tmp == '+')
					COPYCHAR(comment_buf, comment_len, ' ');
				else
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

#if 1
	LOG("author: \"%s\"", author_buf);
	LOG("email: \"%s\"", email_buf);
	LOG("url: \"%s\"", url_buf);
	LOG("comment: \"%s\"", comment_buf);
	LOG("date: %lu", date);
	LOG("id: %d", id);
#endif

	now = gettime();
	deltat = now - date;

	if ((deltat > COMMENT_MAX_DELAY) || (deltat < COMMENT_MIN_DELAY)) {
		LOG("Flash-gordon or geriatric was here... load:%lu comment:%lu delta:%lu postid:%d",
				  date, now, deltat, id);
		return 1;
	}

	/* URL decode everything */
	urldecode(comment_buf, strlen(comment_buf), comment_buf);
	urldecode(author_buf, strlen(author_buf), author_buf);
	urldecode(email_buf, strlen(email_buf), email_buf);
	urldecode(url_buf, strlen(url_buf), url_buf);

	if (strlen(email_buf) == 0) {
		LOG("You must fill in name, email, and comment (postid=%d)", id);
		return 1;
	}

	return write_out_comment(req, id, author_buf, email_buf, url_buf, comment_buf);
}

int blahg_comment(struct req *req)
{
	char *tmpl;
	int ret;

	req_head(req, "Content-Type", "text/html");

	sidebar(req);

	vars_scope_push(&req->vars);

	ret = save_comment(req);
	if (ret) {
		tmpl = "{comment_error}";
		// FIXME: __store_title(&req->vars, "Error");
	} else {
		tmpl = "{comment_save}";
	}

	req->body = render_page(req, tmpl);
	return 0;
}
