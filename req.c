#include <unistd.h>
#include <sys/inttypes.h>

#include "req.h"
#include "utils.h"
#include "sidebar.h"
#include "render.h"
#include "qstring.h"
#include "static.h"

static void req_init(struct req *req, enum req_via via)
{
	req->stats.req_init = gettime();

	/* state */
	req->dump_latency = true;

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
	req->status  = 200;
	req->headers = nvl_alloc();
	req->body    = NULL;
	req->bodylen = 0;

#ifdef USE_XMLRPC
	req_head(req, "X-Pingback", BASE_URL "/?xmlrpc=1");
#endif
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

	req->stats.req_output = gettime();

	/* return status */
	fprintf(req->out, "Status: %u\n", req->status);

	/* write out the headers */
	for (header = nvlist_next_nvpair(req->headers, NULL);
	     header;
	     header = nvlist_next_nvpair(req->headers, header))
		fprintf(req->out, "%s: %s\n", nvpair_name(header), pair2str(header));

	/* separate headers from body */
	fprintf(req->out, "\n");

	/* if body length is 0, we automatically figure out the length */
	if (!req->bodylen)
		req->bodylen = strlen(req->body);

	/* write out the body */
	fwrite(req->body, 1, req->bodylen, req->out);

	/* when requested, write out the latency for this operation */
	if (req->dump_latency) {
		uint64_t delta;

		delta = req->stats.req_output - req->stats.req_init;

		fprintf(req->out, "\n<!-- time to render: %"PRIu64".%09"PRIu64" seconds -->\n",
		       delta / 1000000000UL,
		       delta % 1000000000UL);
	}

	req->stats.req_done = gettime();
}

void req_destroy(struct req *req)
{
	req->stats.destroy = gettime();

	vars_destroy(&req->vars);

	nvlist_free(req->headers);

	free(req->request_body);
	nvlist_free(req->request_headers);
	nvlist_free(req->request_qs);

	switch (req->via) {
		case REQ_SCGI:
			fflush(req->out);
			fclose(req->out);
			/*
			 * NOTE: do *not* close the fd itself since fclose
			 * already closed it for us
			 */
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
	args->xmlrpc = 0;
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
			return false;
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

void convert_headers(nvlist_t *headers, const struct convert_header_info *table)
{
	static const struct convert_header_info generic_table[] = {
		{ .name = CONTENT_LENGTH,	.type = DATA_TYPE_UINT64, },
		{ .name = REMOTE_PORT,		.type = DATA_TYPE_UINT64, },
		{ .name = SERVER_PORT,		.type = DATA_TYPE_UINT64, },
		{ .name = NULL, },
	};

	uint64_t intval;
	char *str;
	int ret;
	int i;

	/*
	 * If the table we're not processing is the generic table, recurse
	 * once to get those conversions out of the way.
	 */
	if (table != generic_table)
		convert_headers(headers, generic_table);

	/*
	 * If we weren't given a table, we have nothing to do other than the
	 * generic conversion (see above)
	 */
	if (!table)
		return;

	for (i = 0; table[i].name; i++) {
		nvpair_t *pair;

		ret = nvlist_lookup_nvpair(headers, table[i].name, &pair);
		if (ret)
			continue;

		if (nvpair_type(pair) != DATA_TYPE_STRING)
			continue;

		ret = nvpair_value_string(pair, &str);
		if (ret)
			continue;

		switch (table[i].type) {
			case DATA_TYPE_UINT64:
				ret = str2u64(str, &intval);
				if (!ret)
					nvl_set_int(headers, table[i].name,
						    intval);
				break;
			default:
				ASSERT(0);
		}
	}
}

int req_dispatch(struct req *req)
{
	parse_query_string(req->request_qs,
			   nvl_lookup_str(req->request_headers,
					  QUERY_STRING));

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

	vars_set_str(&req->vars, "redirect", url);

	req->body = render_page(req, "{301}");

	return 0;
}
