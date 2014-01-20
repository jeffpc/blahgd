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
	struct stat statbuf;
	char *buf;
	char *out;
	int ret;
	int fd;

	snprintf(path, sizeof(path), "templates/%s/%s.tmpl", req->fmt, tmpl);

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		LOG("template (%s) open error", path);
		return NULL;
	}

	out = NULL;

	ret = fstat(fd, &statbuf);
	if (ret == -1) {
		LOG("fstat failed");
		goto out_close;
	}

	buf = mmap(NULL, statbuf.st_size + 4096, PROT_READ, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		LOG("mmap failed");
		goto out_close;
	}

	out = render_page(req, buf);

	munmap(buf, statbuf.st_size + 4096);

out_close:
	close(fd);

	return out;
}
