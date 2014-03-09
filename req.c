#include <unistd.h>

#include "req.h"
#include "utils.h"
#include "sidebar.h"
#include "render.h"

static void req_init(struct req *req, enum req_via via)
{
	/* state */
	req->dump_latency = true;
	req->start = gettime();

	vars_init(&req->vars);
	vars_set_str(&req->vars, "baseurl", BASE_URL);
	vars_set_int(&req->vars, "now", gettime());
	vars_set_int(&req->vars, "captcha_a", COMMENT_CAPTCHA_A);
	vars_set_int(&req->vars, "captcha_b", COMMENT_CAPTCHA_B);
	vars_set_nvl_array(&req->vars, "posts", NULL, 0);

	req->fmt   = NULL;

	/* request */
	req->via = via;
	req->request_headers = NULL;
	req->request_body = NULL;
	req->request_qs = nvl_alloc();

	/* response */
	req->status = 200;
	req->headers = nvl_alloc();
	req->body  = NULL;
}

void req_init_cgi(struct req *req)
{
	req_init(req, REQ_CGI);

	req->out = stdout;
}

void req_init_scgi(struct req *req, int fd)
{
	req_init(req, REQ_SCGI);

	req->scgi.fd = fd;
	req->out = fdopen(fd, "wb");

	ASSERT(req->out);
}

void req_output(struct req *req)
{
	nvpair_t *header;

	fprintf(req->out, "Status: %u\n", req->status);

	for (header = nvlist_next_nvpair(req->headers, NULL);
	     header;
	     header = nvlist_next_nvpair(req->headers, header))
		fprintf(req->out, "%s: %s\n", nvpair_name(header), pair2str(header));

	fprintf(req->out, "\n%s\n", req->body);

	if (req->dump_latency) {
		uint64_t delta;

		delta = gettime() - req->start;

		fprintf(req->out, "<!-- time to render: %lu.%09lu seconds -->\n",
		       delta / 1000000000UL,
		       delta % 1000000000UL);
	}
}

void req_destroy(struct req *req)
{
	vars_destroy(&req->vars);

	nvlist_free(req->headers);

	free(req->request_body);
	nvlist_free(req->request_headers);
	nvlist_free(req->request_qs);

	switch (req->via) {
		case REQ_CGI:
			break;
		case REQ_SCGI:
			fclose(req->out);
			close(req->scgi.fd);
			break;
	}

	free(req->body);
}

void req_head(struct req *req, const char *name, const char *val)
{
	nvl_set_str(req->headers, name, val);
}

static void select_page(struct req *req)
{
	struct qs *args = &req->args;
	nvpair_t *cur;

	args->page = PAGE_INDEX;
	args->p = -1;
	args->paged = -1;
	args->m = -1;
	args->admin = 0;
	args->xmlrpc = 0;
	args->comment = 0;
	args->cat = NULL;
	args->tag = NULL;
	args->feed = NULL;
	args->preview = 0;

	for (cur = nvlist_next_nvpair(req->request_qs, NULL);
	     cur;
	     cur = nvlist_next_nvpair(req->request_qs, cur)) {
		char *name, *val;;
		char **cptr;
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
		} else if (!strcmp(name, "xmlrpc")) {
			iptr = &args->xmlrpc;
		} else if (!strcmp(name, "admin")) {
			iptr = &args->admin;
		} else {
			args->page = PAGE_MALFORMED;
			return;
		}

		if (iptr)
			*iptr = atoi(val);
		else if (cptr)
			*cptr = val;
	}

	if (args->xmlrpc)
		args->page = PAGE_XMLRPC;
	else if (args->comment)
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
}

static bool switch_content_type(struct req *req)
{
	char *fmt = req->args.feed;
	int page = req->args.page;

	const char *content_type;
	int index_stories;

	if (!fmt) {
		/* no feed => OK, use html */
		fmt = "html";
		content_type = "text/html";
		index_stories = HTML_INDEX_STORIES;
	} else if (!strcmp(fmt, "atom")) {
		content_type = "application/atom+xml";
		index_stories = FEED_INDEX_STORIES;
	} else if (!strcmp(fmt, "rss2")) {
		content_type = "application/rss+xml";
		index_stories = FEED_INDEX_STORIES;
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

int req_dispatch(struct req *req)
{
	select_page(req);

	/*
	 * If we got a feed format, we'll be switching (most likely) to it
	 */
	if (!switch_content_type(req))
		return R404(req, "{error_unsupported_feed_fmt}");

	switch (req->args.page) {
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
#ifdef USE_XMLRPC
		case PAGE_XMLRPC:
			return blahg_pingback();
#endif
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

	LOG("status 404 (tmpl: '%s')", tmpl);

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
	LOG("status 301 (url: '%s')", url);

	req_head(req, "Content-Type", "text/html");
	req_head(req, "Location", url);

	req->status = 301;
	req->fmt    = "html";

	vars_scope_push(&req->vars);

	vars_set_str(&req->vars, "redirect", xstrdup(url));

	req->body = render_page(req, "{301}");

	return 0;
}
