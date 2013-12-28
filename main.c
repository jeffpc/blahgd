#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <syslog.h>

#include "main.h"
#include "decode.h"
#include "post.h"
#include "error.h"
#include "utils.h"
#include "render.h"
#include "sidebar.h"
#include "pipeline.h"

static char *nullterminate(char *s)
{
	while(*s && (*s != '&'))
		s++;

	*s = '\0';

	return s + 1;
}

static void parse_qs(char *qs, struct req *req)
{
	struct qs *args = &req->args;
	char *tmp;
	char *end;

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

	if (!qs)
		return;

	end = qs + strlen(qs);
	tmp = qs;

	while (end > tmp) {
		int *iptr = NULL;
		char **cptr = NULL;
		int len;

		if (!strncmp(qs, "p=", 2)) {
			iptr = &args->p;
			len = 2;
		} else if (!strncmp(qs, "paged=", 6)) {
			iptr = &args->paged;
			len = 6;
		} else if (!strncmp(qs, "m=", 2)) {
			iptr = &args->m;
			len = 2;
		} else if (!strncmp(qs, "cat=", 4)) {
			cptr = &args->cat;
			len = 4;
		} else if (!strncmp(qs, "tag=", 4)) {
			cptr = &args->tag;
			len = 4;
		} else if (!strncmp(qs, "feed=", 5)) {
			cptr = &args->feed;
			len = 5;
		} else if (!strncmp(qs, "comment=", 8)) {
			iptr = &args->comment;
			len = 8;
		} else if (!strncmp(qs, "preview=", 8)) {
			iptr = &args->preview;
			len = 8;
		} else if (!strncmp(qs, "xmlrpc=", 7)) {
			iptr = &args->xmlrpc;
			len = 7;
		} else if (!strncmp(qs, "admin=", 6)) {
			iptr = &args->admin;
			len = 6;
		} else {
			args->page = PAGE_MALFORMED;
			return;
		}

		qs += len;
		tmp = nullterminate(qs);

		if (iptr) {
			*iptr = atoi(qs);
		} else if (cptr) {
			urldecode(qs, strlen(qs), qs);
			*cptr = qs;
		}

		qs = tmp;
	}

	if (args->xmlrpc)
		args->page = PAGE_XMLRPC;
	else if (args->comment)
		args->page = PAGE_COMMENT;
	else if (args->feed)
		args->page = PAGE_FEED;
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

static void req_init(struct req *req)
{
	req->dump_latency = true;
	req->start = gettime();
	req->body  = NULL;
	req->fmt   = "html";
	INIT_LIST_HEAD(&req->headers);

	req->status = 200;

	vars_init(&req->vars);

	VAR_SET_STR(&req->vars, "baseurl", xstrdup(BASE_URL));
	VAR_SET_INT(&req->vars, "now", gettime());

	VAR_SET(&req->vars, "posts", VAL_ALLOC(VT_LIST));
}

static void req_destroy(struct req *req)
{
	struct header *cur, *tmp;

	vars_destroy(&req->vars);

	printf("Status: %u\n", req->status);

	list_for_each_entry_safe(cur, tmp, &req->headers, list) {
		printf("%s: %s\n", cur->name, cur->val);

		free(cur->name);
		free(cur->val);
		free(cur);
	}

	printf("\n%s\n", req->body);

	if (req->dump_latency) {
		uint64_t delta;

		delta = gettime() - req->start;

		printf("<!-- time to render: %lu.%09lu seconds -->\n",
		       delta / 1000000000UL,
		       delta % 1000000000UL);
	}

	free(req->body);
}

void req_head(struct req *req, char *name, char *val)
{
	struct header *cur, *tmp;

	list_for_each_entry_safe(cur, tmp, &req->headers, list) {
		if (!strcmp(cur->name, name)) {
			free(cur->val);
			list_del(&cur->list);
			goto set;
		}
	}

	cur = malloc(sizeof(struct header));
	ASSERT(cur);

	cur->name = xstrdup(name);

set:
	cur->val  = xstrdup(val);
	list_add_tail(&cur->list, &req->headers);
}

int main(int argc, char **argv)
{
	struct req request;
	int ret;

	openlog("blahg", LOG_NDELAY | LOG_PID, LOG_LOCAL0);

	init_var_subsys();
	init_pipe_subsys();

	req_init(&request);

	parse_qs(getenv("QUERY_STRING"), &request);

#ifdef USE_XMLRPC
	req_head(&request, "X-Pingback", BASE_URL "/?xmlrpc=1");
#endif

	switch (request.args.page) {
		case PAGE_ARCHIVE:
			ret = blahg_archive(&request, request.args.m,
					    request.args.paged);
			break;
		case PAGE_CATEGORY:
			ret = blahg_category(&request, request.args.cat,
					     request.args.paged);
			break;
		case PAGE_TAG:
			ret = blahg_tag(&request, request.args.tag,
					request.args.paged);
			break;
		case PAGE_COMMENT:
			ret = blahg_comment(&request);
			break;
		case PAGE_FEED:
			ret = blahg_feed(&request, request.args.feed,
					 request.args.p);
			break;
		case PAGE_INDEX:
			ret = blahg_index(&request, request.args.paged);
			break;
		case PAGE_STORY:
			ret = blahg_story(&request, request.args.p,
					  request.args.preview == PREVIEW_SECRET);
			break;
#if 0
#ifdef USE_XMLRPC
		case PAGE_XMLRPC:
			return blahg_pingback();
#endif
#endif
		case PAGE_ADMIN:
			ret = blahg_admin(&request);
			break;
		default:
			// FIXME: send $SCRIPT_URL, $PATH_INFO, and $QUERY_STRING via email
			ret = R404(&request, NULL);
			break;
	}

	req_destroy(&request);

	return ret;
}
