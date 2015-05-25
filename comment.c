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
#include <atomic.h>

#include "req.h"
#include "sidebar.h"
#include "render.h"
#include "db.h"
#include "utils.h"
#include "config.h"
#include "comment.h"
#include "decode.h"
#include "error.h"
#include "post.h"

#define INTERNAL_ERR		"Ouch!  Encountered an internal error.  " \
				"Please contact me to resolve this issue."
#define GENERIC_ERR_STR		"Somehow, your post is getting rejected. " \
				"If your issues persist, contact me to " \
				"resolve this issue."
#define _REQ_FIELDS		"The required fields are: name, email " \
				"address, the math field, and of course "\
				"the content."
#define USERAGENT_MISSING	"You need to send a user-agent."
#define CAPTCHA_FAIL		"You need to get better at math."
#define MISSING_EMAIL		"You need to supply a valid email address. " \
				_REQ_FIELDS
#define MISSING_NAME		"You do have a name, right? " _REQ_FIELDS
#define MISSING_CONTENT		"No content? " _REQ_FIELDS

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
#define SC_CAPTCHA		8
#define SC_CAPTCHA_EQ		18
#define SC_EMPTY		9
#define SC_EMPTY_EQ		19
#define SC_ERROR		99

const char *write_out_comment(struct req *req, int id, char *author,
			      char *email, char *url, char *comment)
{
	static uint32_t nonce;

	char basepath[FILENAME_MAX];
	char dirpath[FILENAME_MAX];
	char textpath[FILENAME_MAX];
	char sqlpath[FILENAME_MAX];

	char curdate[32];
	char *remote_addr; /* yes, this is a pointer */
	int ret;
	char *sql;

	uint64_t now, now_nsec;
	time_t now_sec;
	struct tm *now_tm;

	nvlist_t *post;

	if (strlen(email) == 0) {
		LOG("You must fill in email (postid=%d)", id);
		return MISSING_EMAIL;
	}

	if (strlen(author) == 0) {
		LOG("You must fill in name (postid=%d)", id);
		return MISSING_NAME;
	}

	if (strlen(comment) == 0) {
		LOG("You must fill in comment (postid=%d)", id);
		return MISSING_CONTENT;
	}

	post = load_post(req, id, NULL, false);
	if (!post) {
		LOG("Gah! %d (postid=%d)", -1, id);
		return GENERIC_ERR_STR;
	}

	now = gettime();
	now_sec  = now / 1000000000UL;
	now_nsec = now % 1000000000UL;
	now_tm = gmtime(&now_sec);
	if (!now_tm) {
		LOG("Ow, gmtime() returned NULL");
		return INTERNAL_ERR;
	}

	strftime(curdate, 31, "%Y-%m-%d %H:%M", now_tm);

	snprintf(basepath, FILENAME_MAX, DATA_DIR "/pending-comments/%d-%08lx.%08"PRIx64".%05x",
		 id, now_sec, now_nsec, atomic_inc_32_nv(&nonce));

	snprintf(dirpath,  FILENAME_MAX, "%sW", basepath);
	snprintf(textpath, FILENAME_MAX, "%s/text.txt", dirpath);
	snprintf(sqlpath,  FILENAME_MAX, "%s/meta.sql", dirpath);

	ASSERT3U(strlen(dirpath),  <, FILENAME_MAX - 1);
	ASSERT3U(strlen(textpath), <, FILENAME_MAX - 1);
	ASSERT3U(strlen(sqlpath),  <, FILENAME_MAX - 1);

	if (mkdir(dirpath, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
		LOG("Ow, could not create directory: %d (%s) '%s'", errno, strerror(errno), dirpath);
		return INTERNAL_ERR;
	}

	ret = write_file(textpath, comment, strlen(comment));
	if (ret) {
		LOG("Couldn't write file ... :( %d (%s) '%s'",
		    errno, strerror(errno), textpath);
		return INTERNAL_ERR;
	}

	remote_addr = nvl_lookup_str(req->request_headers, REMOTE_ADDR);

	sql = sqlite3_mprintf("INSERT INTO comments "
			      "(post, id, author, email, time, "
			      "remote_addr, url, moderated) VALUES "
			      "(%d, %d, %Q, %Q, %Q, %Q, %Q, 0);",
			      id, 0, author, email, curdate,
			      remote_addr, url);

	ret = write_file(sqlpath, sql, strlen(sql));

	sqlite3_free(sql);

	if (ret) {
		LOG("Couldn't write file ... :( %d (%s) '%s'",
		    errno, strerror(errno), textpath);
		return INTERNAL_ERR;
	}

	ret = rename(dirpath, basepath);
	if (ret) {
		LOG("Could not rename '%s' to '%s' %d (%s)",
		    dirpath, basepath, errno, strerror(errno));
		return INTERNAL_ERR;
	}

	return NULL;
}

#define COPYCHAR(ob, oi, c)	do { \
					ob[oi] = (c); \
					oi++; \
				} while(0)

static const char *save_comment(struct req *req)
{
	char *in;
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
	uint64_t captcha = 0;
	bool nonempty = false;
	int id = 0;

	if (!nvl_lookup_str(req->request_headers, HTTP_USER_AGENT)) {
		LOG("Missing user agent...");
		return USERAGENT_MISSING;
	}

	author_buf[0] = '\0'; /* better be paranoid */
	email_buf[0] = '\0'; /* better be paranoid */
	url_buf[0] = '\0'; /* better be paranoid */
	comment_buf[0] = '\0'; /* better be paranoid */

	if (!req->request_body)
		return INTERNAL_ERR;

	in = req->request_body;

	for (state = SC_IGNORE; *in; in++) {
		tmp = *in;

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
				if (tmp == 'v')
					state = SC_CAPTCHA_EQ;
				if (tmp == 'x')
					state = SC_EMPTY_EQ;
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

			case SC_CAPTCHA_EQ:
				state = (tmp == '=') ? SC_CAPTCHA : SC_ERROR;
				break;

			case SC_EMPTY_EQ:
				state = (tmp == '=') ? SC_EMPTY : SC_ERROR;
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

			case SC_CAPTCHA:
				if (tmp == '&')
					state = SC_IGNORE;
				else
					captcha = (captcha * 10) + (tmp - '0');
				break;

			case SC_EMPTY:
				if (tmp == '&') {
					state = SC_IGNORE;
				} else {
					nonempty = true;
					state = SC_ERROR;
				}

				break;
		}

		if (state == SC_ERROR)
			break;
	}

#if 0
	LOG("author: \"%s\"", author_buf);
	LOG("email: \"%s\"", email_buf);
	LOG("url: \"%s\"", url_buf);
	LOG("comment: \"%s\"", comment_buf);
	LOG("date: %lu", date);
	LOG("id: %d", id);
	LOG("captcha: %lu", captcha);
#endif

	now = gettime();
	deltat = now - date;

	if (nonempty) {
		LOG("User filled out supposedly empty field... postid:%d", id);
		return GENERIC_ERR_STR;
	}

	if ((deltat > COMMENT_MAX_DELAY) || (deltat < COMMENT_MIN_DELAY)) {
		LOG("Flash-gordon or geriatric was here... load:%lu comment:%lu delta:%lu postid:%d",
		    date, now, deltat, id);
		return GENERIC_ERR_STR;
	}

	if (captcha != (COMMENT_CAPTCHA_A + COMMENT_CAPTCHA_B)) {
		LOG("Math illiterate was here... got:%lu expected:%lu postid:%d",
		    captcha, COMMENT_CAPTCHA_A + COMMENT_CAPTCHA_B, id);
		return CAPTCHA_FAIL;
	}

	/* URL decode everything */
	urldecode(comment_buf, strlen(comment_buf), comment_buf);
	urldecode(author_buf, strlen(author_buf), author_buf);
	urldecode(email_buf, strlen(email_buf), email_buf);
	urldecode(url_buf, strlen(url_buf), url_buf);

	return write_out_comment(req, id, author_buf, email_buf, url_buf, comment_buf);
}

int blahg_comment(struct req *req)
{
	const char *errmsg;
	char *tmpl;

	req_head(req, "Content-Type", "text/html");

	sidebar(req);

	vars_scope_push(&req->vars);

	errmsg = save_comment(req);
	if (errmsg) {
		tmpl = "{comment_error}";
		vars_set_str(&req->vars, "comment_error_str",
			     xstrdup(errmsg));
		// FIXME: __store_title(&req->vars, "Error");
	} else {
		tmpl = "{comment_saved}";
	}

	req->body = render_page(req, tmpl);
	return 0;
}
