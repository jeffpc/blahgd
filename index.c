#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "post.h"
#include "sar.h"
#include "html.h"
#include "main.h"
#include "map.h"
#include "config_opts.h"
#include "parse.h"

static char *__render(struct req *req, char *str)
{
	struct parser_output x;
	int ret;

	x.req   = req;
	x.input = str;
	x.len   = strlen(str);
	x.pos   = 0;

	tmpl_lex_init(&x.scanner);
	tmpl_set_extra(&x, x.scanner);

	ret = tmpl_parse(&x);

	tmpl_lex_destroy(x.scanner);

	return x.output;
}

char *render_template(struct req *req, const char *tmpl)
{
	char path[FILENAME_MAX];
	struct stat statbuf;
	char *buf;
	char *out;
	int ret;
	int fd;

	snprintf(path, sizeof(path), "templates/%s/%s.tmpl", req->fmt, tmpl);

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "template (%s) open error\n", path);
		return NULL;
	}

	out = NULL;

	ret = fstat(fd, &statbuf);
	if (ret == -1) {
		fprintf(stderr, "fstat failed\n");
		goto out_close;
	}

	buf = mmap(NULL, statbuf.st_size + 4096, PROT_READ, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		fprintf(stderr, "mmap failed\n");
		goto out_close;
	}

	out = __render(req, buf);

	munmap(buf, statbuf.st_size + 4096);

out_close:
	close(fd);

	return out;
}

int render_page(struct req *req, char *tmpl)
{
	int ret;

	ret = load_map(&req->map, req->fmt);
	if (ret)
		return ret;

	printf("%s\n", __render(req, tmpl));

	return ret;
}

int blahg_index(struct req *req, int page)
{
	req_head(req, "Content-Type: text/html");

	page = max(page, 0);

	return render_page(req, "{index}");
}
