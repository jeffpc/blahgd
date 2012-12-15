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
#include "db.h"

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

static void __load_posts(struct req *req, int page)
{
	sqlite3_stmt *stmt;
	int ret;

	req->u.index.nposts = 0;

	open_db();
	SQL(stmt, "SELECT id FROM posts ORDER BY time DESC LIMIT ? OFFSET ?");
	SQL_BIND_INT(stmt, 1, HTML_INDEX_STORIES);
	SQL_BIND_INT(stmt, 2, page * HTML_INDEX_STORIES);
	SQL_FOR_EACH(stmt) {
		int postid;

		postid = SQL_COL_INT(stmt, 0);

		if (load_post(postid, &req->u.index.posts[req->u.index.nposts++]))
			continue;
	}
}

int blahg_index(struct req *req, int page)
{
	req_head(req, "Content-Type: text/html");

	page = max(page, 0);

	__load_posts(req, page);

	return render_page(req, "{index}");
}
