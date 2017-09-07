/*
 * Copyright (c) 2009-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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
#include <jeffpc/io.h>
#include <jeffpc/qstring.h>

#include "req.h"
#include "sidebar.h"
#include "render.h"
#include "utils.h"
#include "config.h"
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

/*
 * We use one-letter field names because of historic reasons.  The short
 * names help a bit with transfer sizes, but mostly serve to trivially
 * obfuscate the form from spammers.
 *
 * These defines give reasonable names to the field names.
 */
#define COMMENT_AUTHOR		"a"
#define COMMENT_EMAIL		"e"
#define COMMENT_URL		"u"
#define COMMENT_COMMENT		"c"
#define COMMENT_DATE		"d"
#define COMMENT_ID		"i"
#define COMMENT_CAPTCHA		"v"
#define COMMENT_EMPTY		"x"

static const struct nvl_convert_info comment_convert[] = {
	{ .name = COMMENT_DATE,		.tgt_type = NVT_INT, },
	{ .name = COMMENT_ID,		.tgt_type = NVT_INT, },
	{ .name = COMMENT_CAPTCHA,	.tgt_type = NVT_INT, },
	{ .name = NULL, },
};

static struct str *prep_meta_sexpr(struct str *author, struct str *email,
				   struct str *curdate, struct str *ip,
				   struct str *url)
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

	lv = sexpr_args_to_list(6,
				VAL_ALLOC_CONS(str_cast_to_val(STATIC_STR("author")), str_cast_to_val(author)),
				VAL_ALLOC_CONS(str_cast_to_val(STATIC_STR("email")), str_cast_to_val(email)),
				VAL_ALLOC_CONS(str_cast_to_val(STATIC_STR("time")), str_cast_to_val(curdate)),
				VAL_ALLOC_CONS(str_cast_to_val(STATIC_STR("ip")), str_cast_to_val(ip)),
				VAL_ALLOC_CONS(str_cast_to_val(STATIC_STR("url")), str_cast_to_val(url)),
				VAL_ALLOC_CONS(str_cast_to_val(STATIC_STR("moderated")), VAL_ALLOC_BOOL(false)));

	str = sexpr_dump(lv, false);

	val_putref(lv);

	return str;
}

static const char *write_out_comment(struct req *req, int id,
				     struct str *email,
				     struct str *author,
				     struct str *url,
				     struct str *comment)
{
	static atomic_t nonce;

	const char *err;

	char basepath[FILENAME_MAX];
	char dirpath[FILENAME_MAX];
	char textpath[FILENAME_MAX];
	char lisppath[FILENAME_MAX];

	char curdate[32];
	int ret;
	struct str *meta;

	uint64_t now, now_nsec;
	time_t now_sec;
	struct tm *now_tm;

	struct nvlist *post;

	post = get_post(req, id, NULL, false);
	if (!post) {
		DBG("Gah! %d (postid=%d)", -1, id);
		err = GENERIC_ERR_STR;
		goto err;
	}
	nvl_putref(post);

	now = gettime();
	now_sec  = now / 1000000000UL;
	now_nsec = now % 1000000000UL;
	now_tm = gmtime(&now_sec);
	if (!now_tm) {
		DBG("Ow, gmtime() returned NULL");
		err = INTERNAL_ERR;
		goto err;
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
		err = INTERNAL_ERR;
		goto err;
	}

	ret = write_file(textpath, str_cstr(comment), str_len(comment));
	if (ret) {
		DBG("Couldn't write file ... :( %d (%s) '%s'",
		    ret, xstrerror(ret), textpath);
		err = INTERNAL_ERR;
		goto err;
	}

	meta = prep_meta_sexpr(author, email, STR_DUP(curdate),
			       nvl_lookup_str(req->scgi->request.headers,
					      SCGI_REMOTE_ADDR),
			       url);
	if (!meta) {
		DBG("failed to prep lisp meta data");
		err = INTERNAL_ERR;
		goto err;
	}

	ret = write_file(lisppath, str_cstr(meta), str_len(meta));

	str_putref(meta);

	if (ret) {
		DBG("Couldn't write file ... :( %d (%s) '%s'",
		    ret, xstrerror(ret), lisppath);
		err = INTERNAL_ERR;
		goto err;
	}

	ret = xrename(dirpath, basepath);
	if (ret) {
		DBG("Could not rename '%s' to '%s' %d (%s)",
		    dirpath, basepath, ret, xstrerror(ret));
		err = INTERNAL_ERR;
		goto err;
	}

	return NULL;

err:
	str_putref(author);
	str_putref(email);
	str_putref(url);
	str_putref(comment);

	return err;
}

static const char *get_postid(struct nvlist *qs, int *id_r)
{
	uint64_t id;
	int ret;

	ret = nvl_lookup_int(qs, COMMENT_ID, &id);
	if (ret) {
		DBG("failed to look up id: %s", xstrerror(ret));
		return GENERIC_ERR_STR;
	}

	if (id > INT_MAX) {
		DBG("postid:%"PRIu64" > INT_MAX", id);
		return GENERIC_ERR_STR;
	}

	*id_r = id;

	return NULL;
}

static const char *spam_check(int id, struct nvlist *qs)
{
	uint64_t now, date;
	uint64_t captcha;
	uint64_t deltat;
	struct str *str;
	bool nonempty;
	int ret;

	/*
	 * Check empty field.
	 */
	str = nvl_lookup_str(qs, COMMENT_EMPTY);
	if (IS_ERR(str)) {
		DBG("failed to look up captcha (postid:%u): %s", id,
		    xstrerror(PTR_ERR(str)));
		return GENERIC_ERR_STR;
	}

	nonempty = str_len(str) != 0;
	str_putref(str);

	if (nonempty) {
		DBG("User filled out supposedly empty field... postid:%u", id);
		return GENERIC_ERR_STR;
	}

	/*
	 * Check think time.
	 */
	ret = nvl_lookup_int(qs, COMMENT_DATE, &date);
	if (ret) {
		DBG("failed to look up date (postid:%u): %s", id,
		    xstrerror(ret));
		return GENERIC_ERR_STR;
	}

	now = gettime();
	deltat = now - date;

	if ((deltat > 1000000000ull * config.comment_max_think) ||
	    (deltat < 1000000000ull * config.comment_min_think)) {
		DBG("Flash-gordon or geriatric was here... load:%"PRIu64
		    " comment:%"PRIu64" delta:%"PRIu64" postid:%u",
		    date, now, deltat, id);
		return GENERIC_ERR_STR;
	}

	/*
	 * Check captcha.
	 */
	ret = nvl_lookup_int(qs, COMMENT_CAPTCHA, &captcha);
	if (ret) {
		DBG("failed to look up captcha (postid:%u): %s", id,
		    xstrerror(ret));
		return GENERIC_ERR_STR;
	}

	if (captcha != (config.comment_captcha_a + config.comment_captcha_b)) {
		DBG("Math illiterate was here... got:%"PRIu64
		    " expected:%"PRIu64" postid:%u", captcha,
		    config.comment_captcha_a + config.comment_captcha_b,
		    id);
		return CAPTCHA_FAIL;
	}

	return NULL;
}

static const char *get_str(int id, struct nvlist *nvl, const char *name,
			   const char *dbg_name, const char *err,
			   struct str **str_r)
{
	struct str *str;

	str = nvl_lookup_str(nvl, name);
	if (IS_ERR(str)) {
		DBG("%s: failed to look up %s (postid=%u): %s", __func__,
		    dbg_name, id, xstrerror(PTR_ERR(str)));
		return GENERIC_ERR_STR;
	}

	if (str_len(str) == 0) {
		if (err) {
			DBG("%s: must fill in %s (postid=%u)", __func__, dbg_name,
			    id);
			str_putref(str);
			return err;
		}

		/* turn empty strings to NULL */
		str_putref(str);
		str = NULL;
	}

	*str_r = str;

	return NULL;
}

static const char *get_strings(int id,
			       struct nvlist *qs,
			       struct str **email_r,
			       struct str **author_r,
			       struct str **comment_r,
			       struct str **url_r)
{
	struct str *author;
	struct str *email;
	struct str *comment;
	struct str *url;
	const char *err;

	err = get_str(id, qs, COMMENT_EMAIL, "email", MISSING_EMAIL, &email);
	if (err)
		goto err;

	err = get_str(id, qs, COMMENT_AUTHOR, "author", MISSING_NAME, &author);
	if (err)
		goto err_email;

	err = get_str(id, qs, COMMENT_COMMENT, "comment", MISSING_CONTENT,
		      &comment);
	if (err)
		goto err_author;

	err = get_str(id, qs, COMMENT_URL, "url", NULL, &url);
	if (err)
		goto err_comment;

	*email_r = email;
	*author_r = author;
	*comment_r = comment;
	*url_r = url;

	return NULL;

err_comment:
	str_putref(comment);

err_author:
	str_putref(author);

err_email:
	str_putref(email);

err:
	return err;
}

static const char *save_comment(struct req *req)
{
	struct nvlist *qs;
	struct str *author;
	struct str *email;
	struct str *url;
	struct str *comment;
	const char *err;
	int ret;
	int id;

	if (nvl_exists_type(req->scgi->request.headers, SCGI_HTTP_USER_AGENT,
			    NVT_STR)) {
		DBG("Missing user agent...");
		return USERAGENT_MISSING;
	}

	if (nvl_exists_type(req->scgi->request.headers, SCGI_REMOTE_ADDR,
			    NVT_STR)) {
		DBG("Missing remote addr...");
		return INTERNAL_ERR;
	}

	if (!req->scgi->request.body) {
		DBG("missing req. body");
		return INTERNAL_ERR;
	}

	qs = nvl_alloc();
	if (!qs) {
		DBG("failed to allocate nvlist");
		return INTERNAL_ERR;
	}

	err = GENERIC_ERR_STR;

	ret = qstring_parse(qs, req->scgi->request.body);
	if (ret) {
		DBG("failed to parse comment: %s", xstrerror(ret));
		goto err;
	}

	ret = nvl_convert(qs, comment_convert, false);
	if (ret) {
		DBG("Failed to convert nvlist types: %s", xstrerror(ret));
		goto err;
	}

	err = get_postid(qs, &id);
	if (err)
		goto err;

	err = spam_check(id, qs);
	if (err)
		goto err;

	err = get_strings(id, qs, &email, &author, &comment, &url);
	if (err)
		goto err;

	/* consumes all the string refs */
	err = write_out_comment(req, id, email, author, url, comment);

err:
	nvl_putref(qs);
	return err;
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
		vars_set_str(&req->vars, "comment_error_str", STR_DUP(errmsg));
		// FIXME: __store_title(&req->vars, "Error");
	} else {
		tmpl = "{comment_saved}";
	}

	req->scgi->response.body = render_page(req, tmpl);

	return 0;
}
