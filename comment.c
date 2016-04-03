/*
 * Copyright (c) 2009-2016 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

#include <jeffpc/error.h>
#include <jeffpc/atomic.h>
#include <jeffpc/sexpr.h>
#include <jeffpc/str.h>
#include <jeffpc/io.h>

#include "req.h"
#include "sidebar.h"
#include "render.h"
#include "utils.h"
#include "config.h"
#include "comment.h"
#include "decode.h"
#include "post.h"
#include "debug.h"

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

static struct str *prep_meta_sexpr(const char *author, const char *email,
				   const char *curdate, const char *ip,
				   const char *url)
{
	struct val *lv;
	struct str *str;

	/*
	 * We're looking for a list looking something like:
	 *
	 * '((author . "<author>")
	 *   (email . "<email>")
	 *   (time . "<time>")
	 *   (ip . "<ip>")
	 *   (url . "<url>")
	 *   (moderated . #f))
	 */

	/* FIXME: this is *really* ugly... there has to be a better way */
	lv = VAL_ALLOC_CONS(
	       VAL_ALLOC_CONS(VAL_ALLOC_SYM_CSTR("author"), VAL_DUP_CSTR((char *) author)),
	       VAL_ALLOC_CONS(
	         VAL_ALLOC_CONS(VAL_ALLOC_SYM_CSTR("email"), VAL_DUP_CSTR((char *) email)),
	         VAL_ALLOC_CONS(
	           VAL_ALLOC_CONS(VAL_ALLOC_SYM_CSTR("time"), VAL_DUP_CSTR((char *) curdate)),
	           VAL_ALLOC_CONS(
	             VAL_ALLOC_CONS(VAL_ALLOC_SYM_CSTR("ip"), VAL_DUP_CSTR((char *) ip)),
		     VAL_ALLOC_CONS(
	               VAL_ALLOC_CONS(VAL_ALLOC_SYM_CSTR("url"), VAL_DUP_CSTR((char *) url)),
		       VAL_ALLOC_CONS(
	                 VAL_ALLOC_CONS(VAL_ALLOC_SYM_CSTR("moderated"), VAL_ALLOC_BOOL(false)),
	                 NULL))))));

	str = sexpr_dump(lv, false);

	val_putref(lv);

	return str;
}

const char *write_out_comment(struct req *req, int id, char *author,
			      char *email, char *url, char *comment)
{
	static atomic_t nonce;

	char basepath[FILENAME_MAX];
	char dirpath[FILENAME_MAX];
	char textpath[FILENAME_MAX];
	char lisppath[FILENAME_MAX];

	char curdate[32];
	char *remote_addr; /* yes, this is a pointer */
	int ret;
	struct str *meta;

	uint64_t now, now_nsec;
	time_t now_sec;
	struct tm *now_tm;

	nvlist_t *post;

	if (strlen(email) == 0) {
		DBG("You must fill in email (postid=%d)", id);
		return MISSING_EMAIL;
	}

	if (strlen(author) == 0) {
		DBG("You must fill in name (postid=%d)", id);
		return MISSING_NAME;
	}

	if (strlen(comment) == 0) {
		DBG("You must fill in comment (postid=%d)", id);
		return MISSING_CONTENT;
	}

	post = get_post(req, id, NULL, false);
	if (!post) {
		DBG("Gah! %d (postid=%d)", -1, id);
		return GENERIC_ERR_STR;
	}

	now = gettime();
	now_sec  = now / 1000000000UL;
	now_nsec = now % 1000000000UL;
	now_tm = gmtime(&now_sec);
	if (!now_tm) {
		DBG("Ow, gmtime() returned NULL");
		return INTERNAL_ERR;
	}

	strftime(curdate, 31, "%Y-%m-%d %H:%M", now_tm);

	snprintf(basepath, FILENAME_MAX, "%s/pending-comments/%d-%08lx.%08"PRIx64".%05x",
		 str_cstr(config.data_dir), id, now_sec, now_nsec,
		 atomic_inc(&nonce));

	snprintf(dirpath,  FILENAME_MAX, "%sW", basepath);
	snprintf(textpath, FILENAME_MAX, "%s/text.txt", dirpath);
	snprintf(lisppath, FILENAME_MAX, "%s/meta.lisp", dirpath);

	ASSERT3U(strlen(dirpath),  <, FILENAME_MAX - 1);
	ASSERT3U(strlen(textpath), <, FILENAME_MAX - 1);
	ASSERT3U(strlen(lisppath), <, FILENAME_MAX - 1);

	ret = xmkdir(dirpath, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	if (ret) {
		DBG("Ow, could not create directory: %d (%s) '%s'", ret,
		    xstrerror(ret), dirpath);
		return INTERNAL_ERR;
	}

	ret = write_file(textpath, comment, strlen(comment));
	if (ret) {
		DBG("Couldn't write file ... :( %d (%s) '%s'",
		    ret, xstrerror(ret), textpath);
		return INTERNAL_ERR;
	}

	remote_addr = nvl_lookup_str(req->request_headers, REMOTE_ADDR);

	meta = prep_meta_sexpr(author, email, curdate, remote_addr, url);
	if (!meta) {
		DBG("failed to prep lisp meta data");
		return INTERNAL_ERR;
	}

	ret = write_file(lisppath, meta->str, str_len(meta));

	str_putref(meta);

	if (ret) {
		DBG("Couldn't write file ... :( %d (%s) '%s'",
		    ret, xstrerror(ret), lisppath);
		return INTERNAL_ERR;
	}

	ret = xrename(dirpath, basepath);
	if (ret) {
		DBG("Could not rename '%s' to '%s' %d (%s)",
		    dirpath, basepath, ret, xstrerror(ret));
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
		DBG("Missing user agent...");
		return USERAGENT_MISSING;
	}

	author_buf[0] = '\0'; /* better be paranoid */
	email_buf[0] = '\0'; /* better be paranoid */
	url_buf[0] = '\0'; /* better be paranoid */
	comment_buf[0] = '\0'; /* better be paranoid */

	if (!req->request_body) {
		DBG("missing req. body");
		return INTERNAL_ERR;
	}

	in = req->request_body;

	for (state = SC_IGNORE; *in; in++) {
		tmp = *in;

#if 0
		DBG("|'%c' %d|", tmp, state);
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
	DBG("author: \"%s\"", author_buf);
	DBG("email: \"%s\"", email_buf);
	DBG("url: \"%s\"", url_buf);
	DBG("comment: \"%s\"", comment_buf);
	DBG("date: %lu", date);
	DBG("id: %d", id);
	DBG("captcha: %lu", captcha);
#endif

	now = gettime();
	deltat = now - date;

	if (nonempty) {
		DBG("User filled out supposedly empty field... postid:%d", id);
		return GENERIC_ERR_STR;
	}

	if ((deltat > 1000000000ull * config.comment_max_think) ||
	    (deltat < 1000000000ull * config.comment_min_think)) {
		DBG("Flash-gordon or geriatric was here... load:%"PRIu64
		    " comment:%"PRIu64" delta:%"PRIu64" postid:%d",
		    date, now, deltat, id);
		return GENERIC_ERR_STR;
	}

	if (captcha != (config.comment_captcha_a + config.comment_captcha_b)) {
		DBG("Math illiterate was here... got:%"PRIu64
		    " expected:%"PRIu64" postid:%d", captcha,
		    config.comment_captcha_a + config.comment_captcha_b,
		    id);
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
		vars_set_str(&req->vars, "comment_error_str", errmsg);
		// FIXME: __store_title(&req->vars, "Error");
	} else {
		tmpl = "{comment_saved}";
	}

	req->body = render_page(req, tmpl);
	return 0;
}
