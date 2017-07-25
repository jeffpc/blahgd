/*
 * Copyright (c) 2014-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <unistd.h>
#include <sys/systeminfo.h>
#include <sys/inttypes.h>
#include <pthread.h>

#include <jeffpc/atomic.h>
#include <jeffpc/mem.h>

#include "req.h"
#include "utils.h"
#include "sidebar.h"
#include "render.h"
#include "qstring.h"
#include "static.h"
#include "debug.h"
#include "version.h"

static struct mem_cache *req_cache;

void init_req_subsys(void)
{
	req_cache = mem_cache_create("req-cache", sizeof(struct req), 0);
	ASSERT(!IS_ERR(req_cache));
}

struct req *req_alloc(void)
{
	return mem_cache_alloc(req_cache);
}

void req_free(struct req *req)
{
	if (!req)
		return;

	mem_cache_free(req_cache, req);
}

static void __vars_set_social(struct vars *vars)
{
	if (config.twitter_username)
		vars_set_str(vars, "twitteruser",
			     str_getref(config.twitter_username));
	if (config.twitter_description)
		vars_set_str(vars, "twitterdesc",
			     str_getref(config.twitter_description));
}

void req_init(struct req *req)
{
	/* state */
	req->dump_latency = true;

	vars_init(&req->vars);
	vars_set_str(&req->vars, "generatorversion", STATIC_STR(version_string));
	vars_set_str(&req->vars, "baseurl", str_getref(config.base_url));
	vars_set_int(&req->vars, "now", gettime());
	vars_set_int(&req->vars, "captcha_a", config.comment_captcha_a);
	vars_set_int(&req->vars, "captcha_b", config.comment_captcha_b);
	vars_set_array(&req->vars, "posts", NULL, 0);
	__vars_set_social(&req->vars);

	req->fmt = NULL;

	/* request */
	req->request_qs = nvl_alloc();
	ASSERT(req->request_qs);

	/* response */
	/* (nothing) */
}

static void calculate_content_length(struct req *req)
{
	size_t content_length;
	char tmp[64];

	/* if body length is 0, we automatically figure out the length */
	if (!req->scgi->response.bodylen)
		req->scgi->response.bodylen = strlen(req->scgi->response.body);

	/* construct latency comment */
	if (!req->dump_latency) {
		req->latency_comment[0] = '\0';
	} else {
		uint64_t delta;

		delta = gettime() - req->scgi->scgi_stats.read_body_time;

		snprintf(req->latency_comment, sizeof(req->latency_comment),
			"\n<!-- time to render: %"PRIu64".%09"PRIu64" seconds -->\n",
		       delta / 1000000000UL,
		       delta % 1000000000UL);
	}

	/* calculate the content-length */
	content_length = req->scgi->response.bodylen + strlen(req->latency_comment);

	snprintf(tmp, sizeof(tmp), "%zu", content_length);

	req_head(req, "Content-Length", tmp);
}

void req_output(struct req *req)
{
	calculate_content_length(req);
}

static void nvl_set_time(struct nvlist *nvl, const char *name, uint64_t ts)
{
	if (ts)
		nvl_set_int(nvl, name, ts);
	else
		nvl_set_null(nvl, name);
}

static void log_request(struct req *req)
{
	struct scgi *scgi = req->scgi;
	char fname[FILENAME_MAX];
	char hostname[128];
	struct nvlist *logentry;
	struct nvlist *tmp;
	struct buffer *buf;
	uint64_t now;
	int ret;

	now = gettime();

	sysinfo(SI_HOSTNAME, hostname, sizeof(hostname));

	snprintf(fname, sizeof(fname), "%s/requests/%"PRIu64".%09"PRIu64"-%011u",
		 str_cstr(config.data_dir), now / 1000000000llu,
		 now % 1000000000llu, scgi->id);

	/*
	 * allocate a log entry & store some misc info
	 */
	logentry = nvl_alloc();
	if (!logentry)
		goto err;
	nvl_set_int(logentry, "time-stamp", now);
	nvl_set_int(logentry, "pid", getpid());
	nvl_set_str(logentry, "hostname", STR_DUP(hostname));

	/*
	 * store the request
	 */
	tmp = nvl_alloc();
	if (!tmp)
		goto err_free;
	nvl_set_int(tmp, "id", scgi->id);
	nvl_set_nvl(tmp, "headers", nvl_getref(scgi->request.headers));
	nvl_set_nvl(tmp, "query-string", nvl_getref(req->request_qs));
	nvl_set_str(tmp, "body", STR_DUP(scgi->request.body));
	nvl_set_str(tmp, "fmt", STR_DUP(req->fmt));
	nvl_set_int(tmp, "file-descriptor", scgi->fd);
	nvl_set_int(tmp, "thread-id", (uint64_t) pthread_self());
	nvl_set_nvl(logentry, "request", tmp);

	/*
	 * store the response
	 */
	tmp = nvl_alloc();
	if (!tmp)
		goto err_free;
	nvl_set_int(tmp, "status", scgi->response.status);
	nvl_set_nvl(tmp, "headers", nvl_getref(scgi->response.headers));
	nvl_set_int(tmp, "body-length", scgi->response.bodylen);
	nvl_set_bool(tmp, "dump-latency", req->dump_latency);
	nvl_set_int(tmp, "write-errno", req->write_errno);
	nvl_set_nvl(logentry, "response", tmp);

	/*
	 * store the stats
	 */
	tmp = nvl_alloc();
	if (!tmp)
		goto err_free;
	nvl_set_time(tmp, "conn-selected", scgi->conn_stats.selected_time);
	nvl_set_time(tmp, "conn-accepted", scgi->conn_stats.accepted_time);
	nvl_set_time(tmp, "conn-dequeued", scgi->conn_stats.dequeued_time);
	nvl_set_time(tmp, "scgi-read-header", scgi->scgi_stats.read_header_time);
	nvl_set_time(tmp, "scgi-read-body", scgi->scgi_stats.read_body_time);
	nvl_set_time(tmp, "scgi-compute", scgi->scgi_stats.compute_time);
	nvl_set_time(tmp, "scgi-write-header", scgi->scgi_stats.write_header_time);
	nvl_set_time(tmp, "scgi-write-body", scgi->scgi_stats.write_body_time);
	nvl_set_nvl(logentry, "stats", tmp);

	/*
	 * store the options
	 */
	tmp = nvl_alloc();
	if (!tmp)
		goto err_free;
	nvl_set_int(tmp, "index-stories", req->opts.index_stories);
	nvl_set_nvl(logentry, "options", tmp);

	/* serialize */
	buf = nvl_pack(logentry, NVF_JSON);
	if (IS_ERR(buf))
		goto err_free;

	ret = write_file(fname, buffer_data(buf), buffer_used(buf));
	if (ret)
		goto err_free_buf;

	buffer_free(buf);

	nvl_putref(logentry);

	return;

err_free_buf:
	buffer_free(buf);

err_free:
	nvl_putref(logentry);

err:
	DBG("Failed to log request");
}

void req_destroy(struct req *req)
{
	log_request(req);

	vars_destroy(&req->vars);

	nvl_putref(req->request_qs);
}

void req_head(struct req *req, const char *name, const char *val)
{
	int ret;

	ret = nvl_set_str(req->scgi->response.headers, name, STR_DUP(val));
	ASSERT0(ret);
}

static bool select_page(struct req *req)
{
	struct qs *args = &req->args;
	const struct nvpair *cur;
	struct str *uri;

	args->page = PAGE_INDEX;
	args->p = -1;
	args->paged = -1;
	args->m = -1;
	args->admin = 0;
	args->comment = 0;
	args->cat = NULL;
	args->tag = NULL;
	args->feed = NULL;
	args->preview = 0;

	uri = nvl_lookup_str(req->scgi->request.headers, DOCUMENT_URI);
	ASSERT(!IS_ERR(uri));

	switch (get_uri_type(uri)) {
		case URI_STATIC:
			/* static file */
			args->page = PAGE_STATIC;
			return true;
		case URI_DYNAMIC:
			/* regular dynamic request */
			break;
		case URI_BAD:
			/* bad, bad request */
			return false;
	}

	nvl_for_each(cur, req->request_qs) {
		const char *name, *val;
		const char **cptr;
		int *iptr;

		iptr = NULL;
		cptr = NULL;

		name = nvpair_name(cur);
		val = pair2str(cur);

		if (!strcmp(name, "p")) {
			iptr = &args->p;
		} else if (!strcmp(name, "paged")) {
			iptr = &args->paged;
		} else if (!strcmp(name, "m")) {
			iptr = &args->m;
		} else if (!strcmp(name, "cat")) {
			cptr = &args->cat;
		} else if (!strcmp(name, "tag")) {
			cptr = &args->tag;
		} else if (!strcmp(name, "feed")) {
			cptr = &args->feed;
		} else if (!strcmp(name, "comment")) {
			iptr = &args->comment;
		} else if (!strcmp(name, "preview")) {
			iptr = &args->preview;
		} else if (!strcmp(name, "admin")) {
			iptr = &args->admin;
		} else {
			return false;
		}

		if (iptr)
			*iptr = atoi(val);
		else if (cptr)
			*cptr = val;
	}

	if (args->comment)
		args->page = PAGE_COMMENT;
	else if (args->tag)
		args->page = PAGE_TAG;
	else if (args->cat)
		args->page = PAGE_CATEGORY;
	else if (args->m != -1)
		args->page = PAGE_ARCHIVE;
	else if (args->p != -1)
		args->page = PAGE_STORY;
	else if (args->admin)
		args->page = PAGE_ADMIN;

	return true;
}

/*
 * Switch the request's content type to match the output format.
 * Furthermore, tweak the opts a bit based on what page we're going to be
 * serving.
 *
 * Note: We totally ignore the possibility of static files because we'll
 * override everything later on when we have a better idea of what we're
 * dealing with exactly.  So, after this function is through, it'll look
 * like we're dealing with a html page that's not PAGE_INDEX/PAGE_STORY.
 */
static bool switch_content_type(struct req *req)
{
	const char *fmt = req->args.feed;
	int page = req->args.page;

	const char *content_type;
	int index_stories;

	if (!fmt) {
		/* no feed => OK, use html */
		fmt = "html";
		content_type = "text/html";
		index_stories = config.html_index_stories;
	} else if (!strcmp(fmt, "atom")) {
		content_type = "application/atom+xml";
		index_stories = config.feed_index_stories;
	} else if (!strcmp(fmt, "rss2")) {
		content_type = "application/rss+xml";
		index_stories = config.feed_index_stories;
	} else {
		/* unsupported feed type */
		return false;
	}

	switch (page) {
		case PAGE_INDEX:
		case PAGE_STORY:
			break;

		/* for everything else, we have only HTML */
		default:
			if (strcmp(fmt, "html"))
				return false;
			break;
	}

	/* let the template engine know */
	req->fmt = fmt;

	/* let the client know */
	req_head(req, "Content-Type", content_type);

	/* set limits */
	req->opts.index_stories = index_stories;

	return true;
}

void convert_headers(struct nvlist *headers,
		     const struct nvl_convert_info *table)
{
	static const struct nvl_convert_info generic_table[] = {
		{ .name = CONTENT_LENGTH,	.tgt_type = NVT_INT, },
		{ .name = REMOTE_PORT,		.tgt_type = NVT_INT, },
		{ .name = SERVER_PORT,		.tgt_type = NVT_INT, },
		{ .name = NULL, },
	};

	nvl_convert(headers, generic_table);
	nvl_convert(headers, table);
}

int req_dispatch(struct req *req)
{
	struct str *qs;

	qs = nvl_lookup_str(req->scgi->request.headers, QUERY_STRING);
	if (IS_ERR(qs))
		return R404(req, NULL); /* FIXME: this should be R400 */

	if (parse_query_string(req->request_qs, str_cstr(qs))) {
		str_putref(qs);
		return R404(req, NULL); /* FIXME: this should be R400 */
	}

	str_putref(qs);

	if (!select_page(req))
		return R404(req, NULL);

	/*
	 * If we got a feed format, we'll be switching (most likely) to it
	 */
	if (!switch_content_type(req))
		return R404(req, "{error_unsupported_feed_fmt}");

	switch (req->args.page) {
		case PAGE_STATIC:
			return blahg_static(req);
		case PAGE_ARCHIVE:
			return blahg_archive(req, req->args.m,
					     req->args.paged);
		case PAGE_CATEGORY:
			return blahg_category(req, req->args.cat,
					      req->args.paged);
		case PAGE_TAG:
			return blahg_tag(req, req->args.tag,
					 req->args.paged);
		case PAGE_COMMENT:
			return blahg_comment(req);
		case PAGE_INDEX:
			return blahg_index(req, req->args.paged);
		case PAGE_STORY:
			return blahg_story(req, req->args.p,
					   req->args.preview == PREVIEW_SECRET);
		case PAGE_ADMIN:
			return blahg_admin(req);
		default:
			// FIXME: send $SCRIPT_URL, $PATH_INFO, and $QUERY_STRING via email
			return R404(req, NULL);
	}
}

int R404(struct req *req, char *tmpl)
{
	tmpl = tmpl ? tmpl : "{404}";

	req_head(req, "Content-Type", "text/html");

	req->scgi->response.status = SCGI_STATUS_NOTFOUND;
	req->fmt = "html";

	vars_scope_push(&req->vars);

	sidebar(req);

	req->scgi->response.body = render_page(req, tmpl);

	return 0;
}

int R301(struct req *req, const char *url)
{
	DBG("status 301 (url: '%s')", url);

	req_head(req, "Content-Type", "text/html");
	req_head(req, "Location", url);

	req->scgi->response.status = SCGI_STATUS_REDIRECT;
	req->fmt = "html";

	vars_scope_push(&req->vars);

	vars_set_str(&req->vars, "redirect", STR_DUP(url));

	req->scgi->response.body = render_page(req, "{301}");

	return 0;
}
