#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <syslog.h>

#include "main.h"
#include "decode.h"
#include "post.h"
#include "error.h"
#include "utils.h"
#include "render.h"
#include "sidebar.h"
#include "pipeline.h"
#include "req.h"
#include "qstring.h"

static void parse_qs(struct req *req)
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

int main(int argc, char **argv)
{
	struct req req;
	int ret;

	openlog("blahg", LOG_NDELAY | LOG_PID, LOG_LOCAL0);

	init_val_subsys();
	init_pipe_subsys();

	req_init_cgi(&req);

	parse_query_string(req.request_qs, getenv("QUERY_STRING"));

	parse_qs(&req);

#ifdef USE_XMLRPC
	req_head(&req, "X-Pingback", BASE_URL "/?xmlrpc=1");
#endif

	/*
	 * If we got a feed format, we'll be switching (most likely) to it
	 */
	if (!switch_content_type(&req)) {
		ret = R404(&req, "{error_unsupported_feed_fmt}");
		goto out;
	}

	switch (req.args.page) {
		case PAGE_ARCHIVE:
			ret = blahg_archive(&req, req.args.m,
					    req.args.paged);
			break;
		case PAGE_CATEGORY:
			ret = blahg_category(&req, req.args.cat,
					     req.args.paged);
			break;
		case PAGE_TAG:
			ret = blahg_tag(&req, req.args.tag,
					req.args.paged);
			break;
		case PAGE_COMMENT:
			ret = blahg_comment(&req);
			break;
		case PAGE_INDEX:
			ret = blahg_index(&req, req.args.paged);
			break;
		case PAGE_STORY:
			ret = blahg_story(&req, req.args.p,
					  req.args.preview == PREVIEW_SECRET);
			break;
#if 0
#ifdef USE_XMLRPC
		case PAGE_XMLRPC:
			return blahg_pingback();
#endif
#endif
		case PAGE_ADMIN:
			ret = blahg_admin(&req);
			break;
		default:
			// FIXME: send $SCRIPT_URL, $PATH_INFO, and $QUERY_STRING via email
			ret = R404(&req, NULL);
			break;
	}

out:
	req_output(&req);
	req_destroy(&req);

	return ret;
}
