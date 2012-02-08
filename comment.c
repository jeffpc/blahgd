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

#include "config.h"
#include "post.h"
#include "comment.h"
#include "sar.h"
#include "decode.h"
#include "html.h"
#include "xattr.h"

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

static int __write(int fd, char *buf, int len)
{
	int ret;
	int offset = 0;

	while(offset < len) {
		ret = write(fd, buf+offset, len-offset);
		if (ret < 0) {
			printf("Write error! %s (%d)\n", strerror(errno), errno);
			return 1;
		}

		offset += ret;
	}
	return 0;
}

static void comment_error_log(char *fmt, ...)
{
#ifndef HAVE_FLOCK
	struct flock fl;
#endif
	char msg[4096];
	va_list args;
	int ret, len;
	int fd;

	time_t now;

	now = time(NULL);

	len = strftime(msg, 4096, "%a %b %d %H:%M:%S %Y:  ",
		       localtime(&now));

	va_start(args, fmt);
	len += vsnprintf(msg+len, 4096-len, fmt, args);
	va_end(args);

	fd = open(ERROR_LOG_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
	if (fd == -1) {
		fprintf(stderr, "%s: Failed to open log file\n", __func__);
		return;
	}

#ifdef HAVE_FLOCK
	ret = flock(fd, LOCK_EX);
#else
	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_pid = getpid();
	ret = fcntl(fd, F_SETLKW, &fl);
#endif
	if (ret == -1) {
		fprintf(stderr, "%s: Failed to lock log file\n", __func__);
		goto out;
	}

	ret = write(fd, msg, len);

out:
#ifdef HAVE_FLOCK
	flock(fd, LOCK_UN);
#else
	fl.l_type = F_UNLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_pid = getpid();
	fcntl(fd, F_SETLK, &fl);
#endif
	close(fd);
}

int write_out_comment(struct post *post, int id, struct timespec *now, char *author, char *email,
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

	time_t tmp_t;
	struct tm *tmp_tm;

	ret = load_post(id, post, 0);
	if (ret) {
		comment_error_log("Gah! %d (postid=%d)\n", ret, id);
		return 1;
	}

	if ((strlen(author) == 0) || (strlen(comment) == 0)) {
		comment_error_log("You must fill in name, and comment (postid=%d)\n", id);
		return 1;
	}

	tmp_t = now->tv_sec;
	tmp_tm = gmtime(&tmp_t);
	if (!tmp_tm)
		return 1;

	strftime(curdate, 31, "%Y-%m-%d %H:%M", tmp_tm);

	snprintf(path, FILENAME_MAX, "data/pending-comments/%d-%08lx.%08lx.%04xW",
		 id, now->tv_sec, now->tv_nsec, (unsigned) getpid());

	dirpath = strdup(path);
	newdirpath = strdup(path);
	if (!dirpath || !newdirpath) {
		comment_error_log("Eeeep...ENOMEM\n");
		return 1;
	}

	if (mkdir(dirpath, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
		comment_error_log("Ow, could not create directory: %d (%s) '%s'\n", errno, strerror(errno), dirpath);
		goto out_free;
	}

	if ((strlen(path) + strlen("/text.txt")) >= FILENAME_MAX) {
		comment_error_log("Uf...filename too long!\n");
		goto out_free;
	}

	strcat(path, "/text.txt");

	if ((fd = open(path, O_WRONLY | O_CREAT | O_EXCL,
		       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
		comment_error_log("Couldn't create file ... :( %d (%s) '%s'\n",
				  errno, strerror(errno), path);
		goto out_free;
	}

	if (__write(fd, comment, strlen(comment)) != 0)
		goto out;

	safe_setxattr(dirpath, XATTR_COMM_AUTHOR, author,
		      strlen(author));
	safe_setxattr(dirpath, XATTR_TIME, curdate, strlen(curdate));
	safe_setxattr(dirpath, XATTR_COMM_EMAIL, email,
		      strlen(email));

	if (url && url[0])
		safe_setxattr(dirpath, XATTR_COMM_URL, url, strlen(url));

	remote_addr = getenv("REMOTE_ADDR");
	if (remote_addr)
		safe_setxattr(dirpath, XATTR_COMM_IP, remote_addr,
			      strlen(remote_addr));

	newdirpath[strlen(newdirpath)-1] = '\0';

	ret = rename(dirpath, newdirpath);
	if (ret) {
		comment_error_log("Could not rename '%s' to '%s' %d (%s)\n",
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

static int save_comment(struct post *post)
{
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

#if 0
	fprintf(post->out, "author: \"%s\"\n", author_buf);
	fprintf(post->out, "email: \"%s\"\n", email_buf);
	fprintf(post->out, "url: \"%s\"\n", url_buf);
	fprintf(post->out, "comment: \"%s\"\n", comment_buf);
	fprintf(post->out, "date: %lu\n", date);
	fprintf(post->out, "id: %d\n", id);
#endif

	clock_gettime(CLOCK_REALTIME, &now);
	if ((now.tv_sec > (date+COMMENT_MAX_DELAY)) || (now.tv_sec < (date+COMMENT_MIN_DELAY))) {
		comment_error_log("Flash-gordon or geriatric was here... load:%lu comment:%lu delta:%lu postid:%d\n",
				  date, now.tv_sec, now.tv_sec - date, id);
		return 1;
	}

	/* URL decode everything */
	urldecode(comment_buf, strlen(comment_buf), comment_buf);
	urldecode(author_buf, strlen(author_buf), author_buf);
	urldecode(email_buf, strlen(email_buf), email_buf);
	urldecode(url_buf, strlen(url_buf), url_buf);

	if (strlen(email_buf) == 0) {
		comment_error_log("You must fill in name, email, and comment (postid=%d)\n", id);
		return 1;
	}

	return write_out_comment(post, id, &now, author_buf, email_buf, url_buf, comment_buf);
}

int blahg_comment()
{
	struct timespec s,e;
	struct post post;
	int res;

	clock_gettime(CLOCK_REALTIME, &s);

	post.out = stdout;
	post.id  = 0;
	post.time.tm_year = 0;
	post.time.tm_mon = 0;
	post.time.tm_mday = 1;

	fprintf(post.out, "Content-Type: text/html\n\n");

	res = save_comment(&post);

	if (res && !post.title)
		post.title = "Erorr";

	html_header(&post);
	html_save_comment(&post, res);
	html_sidebar(&post);
	html_footer(&post);

	if (res)
		post.title = NULL; // NOTE: This might actually leak some memory
	destroy_post(&post);

	clock_gettime(CLOCK_REALTIME, &e);

	fprintf(post.out, "<!-- time to render: %ld.%09ld seconds -->\n", (int)e.tv_sec-s.tv_sec,
		e.tv_nsec-s.tv_nsec+((e.tv_sec-s.tv_sec) ? 1000000000 : 0));

	return 0;
}
