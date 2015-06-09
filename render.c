#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "render.h"
#include "parse.h"
#include "error.h"
#include "template_cache.h"

char *render_page(struct req *req, char *str)
{
	struct parser_output x;

	x.req   = req;
	x.post  = NULL;
	x.input = str;
	x.len   = strlen(str);
	x.pos   = 0;

	x.cond_stack_use = -1;

	tmpl_lex_init(&x.scanner);
	tmpl_set_extra(&x, x.scanner);

	ASSERT(tmpl_parse(&x) == 0);

	tmpl_lex_destroy(x.scanner);

	return x.output;
}

char *render_template(struct req *req, const char *tmpl)
{
	char path[FILENAME_MAX];
	struct str *raw;
	char *out;

	snprintf(path, sizeof(path), "templates/%s/%s.tmpl", req->fmt, tmpl);

	raw = template_cache_get(path);
	if (!raw)
		return NULL;

	out = render_page(req, raw->str);

	str_putref(raw);

	return out;
}
