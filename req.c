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
static atomic_t reqids;

void init_req_subsys(void)
{
	req_cache = mem_cache_create("req-cache", sizeof(struct req), 0);
	ASSERT(!IS_ERR(req_cache));
}

struct req *req_alloc(void)
{
	struct req *req;

	req = mem_cache_alloc(req_cache);

	if (req)
		req->id = atomic_inc(&reqids);

	return req;
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
			     str_cstr(config.twitter_username));
	if (config.twitter_description)
		vars_set_str(vars, "twitterdesc",
			     str_cstr(config.twitter_description));
}

static void req_init(struct req *req, enum req_via via)
{
	req->stats.req_init = gettime();

	/* state */
	req->dump_latency = true;

	vars_init(&req->vars);
	vars_set_str(&req->vars, "generatorversion", version_string);
	vars_set_str(&req->vars, "baseurl", str_cstr(config.base_url));
	vars_set_int(&req->vars, "now", gettime());
	vars_set_int(&req->vars, "captcha_a", config.comment_captcha_a);
	vars_set_int(&req->vars, "captcha_b", config.comment_captcha_b);
	vars_set_nvl_array(&req->vars, "posts", NULL, 0);
	__vars_set_social(&req->vars);

	req->fmt   = NULL;

	/* request */
	req->via = via;
	req->request_headers = NULL;
	req->request_body = NULL;
	req->request_qs = nvl_alloc();

	/* response */
	req->status  = 200;
	req->headers = nvl_alloc();
	req->body    = NULL;
	req->bodylen = 0;
}

void req_init_scgi(struct req *req, int fd)
{
	req_init(req, REQ_SCGI);

	req->fd = fd;
}

static void __req_stats(struct req *req)
{
	enum statpage pg = STATPAGE_HTTP_XXX;

	switch (req->status) {
		case 301:
			pg = STATPAGE_HTTP_301;
			break;
		case 404:
			pg = STATPAGE_HTTP_404;
			break;
		case 200:
			switch (req->args.page) {
				case PAGE_ARCHIVE:
					pg = STATPAGE_ARCHIVE;
					break;
				case PAGE_CATEGORY:
					pg = STATPAGE_CAT;
					break;
				case PAGE_TAG:
					pg = STATPAGE_TAG;
					break;
				case PAGE_COMMENT:
					pg = STATPAGE_COMMENT;
					break;
				case PAGE_INDEX:
					pg = STATPAGE_INDEX;
					break;
				case PAGE_STORY:
					pg = STATPAGE_STORY;
					break;
				case PAGE_ADMIN:
					pg = STATPAGE_ADMIN;
					break;
				case PAGE_STATIC:
					pg = STATPAGE_STATIC;
					break;
			}
			break;
		default:
			pg = STATPAGE_HTTP_XXX;
			break;
	}

	stats_update_request(pg, &req->stats);
}

static void calculate_content_length(struct req *req)
{
	size_t content_length;
	char tmp[64];

	/* if body length is 0, we automatically figure out the length */
	if (!req->bodylen)
		req->bodylen = strlen(req->body);

	/* construct latency comment */
	if (!req->dump_latency) {
		req->latency_comment[0] = '\0';
	} else {
		uint64_t delta;

		delta = req->stats.req_output - req->stats.req_init;

		snprintf(req->latency_comment, sizeof(req->latency_comment),
			"\n<!-- time to render: %"PRIu64".%09"PRIu64" seconds -->\n",
		       delta / 1000000000UL,
		       delta % 1000000000UL);
	}

	/* calculate the content-length */
	content_length = req->bodylen + strlen(req->latency_comment);

	snprintf(tmp, sizeof(tmp), "%zu", content_length);

	req_head(req, "Content-Length", tmp);
}

void req_output(struct req *req)
{
	char tmp[256];
	nvpair_t *header;
	int ret;

	req->stats.req_output = gettime();

	calculate_content_length(req);

	/* return status */
	snprintf(tmp, sizeof(tmp), "Status: %u\n", req->status);
	ret = xwrite_str(req->fd, tmp);
	if (ret)
		goto out;

	/* write out the headers */
	for (header = nvlist_next_nvpair(req->headers, NULL);
	     header;
	     header = nvlist_next_nvpair(req->headers, header)) {
		snprintf(tmp, sizeof(tmp), "%s: %s\n", nvpair_name(header),
			 pair2str(header));

		ret = xwrite_str(req->fd, tmp);
		if (ret)
			goto out;
	}

	/* separate headers from body */
	ret = xwrite_str(req->fd, "\n");
	if (ret)
		goto out;

	/* write out the body */
	ret = xwrite(req->fd, req->body, req->bodylen);
	if (ret)
		goto out;

	/* write out the latency for this operation */
	ret = xwrite_str(req->fd, req->latency_comment);

out:
	/*
	 * At this point, we're done as far as the client is concerned.  We
	 * have nothing else to send, so we make sure that we actually push
	 * all the bytes along now.
	 */
	close(req->fd);

	/*
	 * Stash away the success/failure of the above writes
	 */
	req->write_errno = -ret;

	req->stats.req_done = gettime();

	__req_stats(req);
}

static void log_request(struct req *req)
{
	char fname[FILENAME_MAX];
	char hostname[128];
	nvlist_t *logentry;
	nvlist_t *tmp;
	uint64_t now;
	size_t len;
	char *buf;
	int ret;

	now = gettime();

	sysinfo(SI_HOSTNAME, hostname, sizeof(hostname));

	snprintf(fname, sizeof(fname), "%s/requests/%"PRIu64".%09"PRIu64"-%011u",
		 str_cstr(config.data_dir), now / 1000000000llu,
		 now % 1000000000llu, req->id);

	/*
	 * allocate a log entry & store some misc info
	 */
	logentry = nvl_alloc();
	if (!logentry)
		goto err;
	nvl_set_int(logentry, "time-stamp", now);
	nvl_set_int(logentry, "pid", getpid());
	nvl_set_str(logentry, "hostname", hostname);

	/*
	 * store the request
	 */
	tmp = nvl_alloc();
	if (!tmp)
		goto err_free;
	nvl_set_int(tmp, "id", req->id);
	nvl_set_nvl(tmp, "headers", req->request_headers);
	nvl_set_nvl(tmp, "query-string", req->request_qs);
	nvl_set_str(tmp, "body", req->request_body);
	nvl_set_str(tmp, "fmt", req->fmt);
	nvl_set_int(tmp, "file-descriptor", req->fd);
	nvl_set_int(tmp, "thread-id", (uint64_t) pthread_self());
	nvl_set_nvl(logentry, "request", tmp);
	nvlist_free(tmp);

	/*
	 * store the response
	 */
	tmp = nvl_alloc();
	if (!tmp)
		goto err_free;
	nvl_set_int(tmp, "status", req->status);
	nvl_set_nvl(tmp, "headers", req->headers);
	nvl_set_int(tmp, "body-length", req->bodylen);
	nvl_set_bool(tmp, "dump-latency", req->dump_latency);
	nvl_set_int(tmp, "write-errno", req->write_errno);
	nvl_set_nvl(logentry, "response", tmp);
	nvlist_free(tmp);

	/*
	 * store the stats
	 */
	tmp = nvl_alloc();
	if (!tmp)
		goto err_free;
	nvl_set_int(tmp, "fd-conn", req->stats.fd_conn);
	nvl_set_int(tmp, "req-init", req->stats.req_init);
	nvl_set_int(tmp, "enqueue", req->stats.enqueue);
	nvl_set_int(tmp, "dequeue", req->stats.dequeue);
	nvl_set_int(tmp, "req-output", req->stats.req_output);
	nvl_set_int(tmp, "req-done", req->stats.req_done);
	nvl_set_int(tmp, "destroy", req->stats.destroy);
	nvl_set_nvl(logentry, "stats", tmp);
	nvlist_free(tmp);

	/*
	 * store the options
	 */
	tmp = nvl_alloc();
	if (!tmp)
		goto err_free;
	nvl_set_int(tmp, "index-stories", req->opts.index_stories);
	nvl_set_nvl(logentry, "options", tmp);
	nvlist_free(tmp);

	/* serialize */
	buf = NULL;
	ret = nvlist_pack(logentry, &buf, &len, NV_ENCODE_XDR, 0);
	if (ret)
		goto err_free;

	ret = write_file(fname, buf, len);
	if (ret)
		goto err_free_buf;

	free(buf);

	nvlist_free(logentry);

	return;

err_free_buf:
	free(buf);

err_free:
	nvlist_free(logentry);

err:
	DBG("Failed to log request");
}

void req_destroy(struct req *req)
{
	req->stats.destroy = gettime();

	log_request(req);

	vars_destroy(&req->vars);

	nvlist_free(req->headers);

	free(req->request_body);
	nvlist_free(req->request_headers);
	nvlist_free(req->request_qs);

	switch (req->via) {
		case REQ_SCGI:
			break;
	}

	free(req->body);
}

void req_head(struct req *req, const char *name, const char *val)
{
	nvl_set_str(req->headers, name, val);
}

static bool select_page(struct req *req)
{
	struct qs *args = &req->args;
	nvpair_t *cur;
	char *uri;

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

	uri = nvl_lookup_str(req->request_headers, DOCUMENT_URI);

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

	for (cur = nvlist_next_nvpair(req->request_qs, NULL);
	     cur;
	     cur = nvlist_next_nvpair(req->request_qs, cur)) {
		char *name, *val;
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

void convert_headers(nvlist_t *headers, const struct convert_info *table)
{
	static const struct convert_info generic_table[] = {
		{ .name = CONTENT_LENGTH,	.type = DATA_TYPE_UINT64, },
		{ .name = REMOTE_PORT,		.type = DATA_TYPE_UINT64, },
		{ .name = SERVER_PORT,		.type = DATA_TYPE_UINT64, },
		{ .name = NULL, },
	};

	nvl_convert(headers, generic_table);
	nvl_convert(headers, table);
}

int req_dispatch(struct req *req)
{
	if (parse_query_string(req->request_qs,
			       nvl_lookup_str(req->request_headers,
					      QUERY_STRING)))
		return R404(req, NULL); /* FIXME: this should be R400 */

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

	req->status = 404;
	req->fmt    = "html";

	vars_scope_push(&req->vars);

	sidebar(req);

	req->body = render_page(req, tmpl);

	return 0;
}

int R301(struct req *req, const char *url)
{
	DBG("status 301 (url: '%s')", url);

	req_head(req, "Content-Type", "text/html");
	req_head(req, "Location", url);

	req->status = 301;
	req->fmt    = "html";

	vars_scope_push(&req->vars);

	vars_set_str(&req->vars, "redirect", url);

	req->body = render_page(req, "{301}");

	return 0;
}
