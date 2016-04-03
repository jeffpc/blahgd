/*
 * Copyright (c) 2009-2016 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <jeffpc/error.h>

#include "post.h"
#include "req.h"
#include "config.h"
#include "sidebar.h"
#include "parse.h"
#include "render.h"
#include "utils.h"

struct archive_filter_args {
	unsigned int start;
	unsigned int end;
};

/* return true iff post->time is in [start,end) */
static bool archive_filter(struct post *post, void *private)
{
	struct archive_filter_args *args = private;

	return (post->time >= args->start) && (post->time < args->end);
}

static void __load_posts(struct req *req, int page, int archid)
{
	const unsigned int posts_per_page = req->opts.index_stories;
	struct post *posts[posts_per_page];
	int nposts;

	if (!archid) {
		/* regular index */
		nposts = index_get_posts(posts, NULL, false, NULL, NULL,
					 page * posts_per_page,
					 posts_per_page);
	} else {
		/* archive index */
		struct archive_filter_args filter_args;
		struct tm start;
		struct tm end;

		memset(&start, 0, sizeof(start));

		start.tm_year = (archid / 100) - 1900;
		start.tm_mon  = (archid % 100) - 1;
		start.tm_mday = 1;

		end = start;
		end.tm_mon++;
		if (end.tm_mon >= 12) {
			end.tm_mon = 1;
			end.tm_year++;
		}

		filter_args.start = mktime(&start);
		filter_args.end   = mktime(&end);

		fprintf(stderr, "%s %u %u\n", __func__, filter_args.start, filter_args.end);

		nposts = index_get_posts(posts, NULL, false, archive_filter,
					 &filter_args, page * posts_per_page,
					 posts_per_page);
	}

	load_posts(req, posts, nposts, nposts == posts_per_page);
}

static void __store_title(struct vars *vars, char *title)
{
	vars_set_str(vars, "title", title);
}

static void __store_pages(struct vars *vars, int page)
{
	vars_set_int(vars, "prevpage", page + 1);
	vars_set_int(vars, "curpage",  page);
	vars_set_int(vars, "nextpage", page - 1);
}

static void __store_archid(struct vars *vars, int archid)
{
	vars_set_int(vars, "archid", archid);
}

int blahg_index(struct req *req, int page)
{
	page = max(page, 0);

	__store_title(&req->vars, "Blahg");
	__store_pages(&req->vars, page);

	sidebar(req);

	vars_scope_push(&req->vars);

	__load_posts(req, page, 0);

	req->body = render_page(req, "{index}");
	return 0;
}

static int validate_arch_id(int arch)
{
	int y = arch / 100;
	int m = arch % 100;

	return (m >= 1) && (m <= 12) && (y >= 1970) && (y < 2100);
}

int blahg_archive(struct req *req, int m, int page)
{
	static const char *months[12] = {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November",
		"December",
	};

	char nicetitle[32];

	if (!validate_arch_id(m))
		m = 197001;

	snprintf(nicetitle, sizeof(nicetitle), "%d &raquo; %s", m / 100,
		 months[(m % 100) - 1]);

	req_head(req, "Content-Type", "text/html");

	page = max(page, 0);

	__store_title(&req->vars, nicetitle);
	__store_pages(&req->vars, page);
	__store_archid(&req->vars, m);

	sidebar(req);

	vars_scope_push(&req->vars);

	__load_posts(req, page, m);

	req->body = render_page(req, "{archive}");
	return 0;
}
