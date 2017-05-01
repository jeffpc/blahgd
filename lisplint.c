/*
 * Copyright (c) 2015-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <stdlib.h>

#include <jeffpc/jeffpc.h>
#include <jeffpc/error.h>
#include <jeffpc/sexpr.h>
#include <jeffpc/str.h>
#include <jeffpc/val.h>

#include "utils.h"
#include "config.h"

static char *prog;

static bool (*checker)(const char *fname, struct val *);
static bool verbose = false;

#define error(fname, ...)				\
	do {						\
		fprintf(stderr, "%s: ", (fname));	\
		fprintf(stderr, __VA_ARGS__);		\
	} while (0)

static bool check_type(const char *fname, struct val *lv,
		       const char *varname, enum val_type type,
		       bool mandatory)
{
	struct val *v;

	v = sexpr_alist_lookup_val(lv, varname);
	if (!v) {
		if (mandatory)
			error(fname, "missing '%s'\n", varname);
		return !mandatory;
	}

	if (v->type != type) {
		error(fname, "'%s' has wrong type: got %u, expected %u\n",
		      varname, v->type, type);
		val_putref(v);
		return false;
	}

	val_putref(v);
	return true;
}

static bool check_post(const char *fname, struct val *lv)
{
	struct val *fmtval;
	uint64_t fmt;
	bool ret;

	/*
	 * check fmt presence & correctness
	 */
	fmtval = sexpr_alist_lookup_val(lv, "fmt");
	if (!fmtval) {
		error(fname, "missing 'fmt'\n");
		return false;
	}

	if (fmtval->type != VT_INT) {
		error(fname, "fmt is not an int\n");
		val_putref(fmtval);
		return false;
	}

	fmt = fmtval->i;

	val_putref(fmtval);

	/*
	 * Depending on the fmt, check other parts of the metadata
	 */
	switch (fmt) {
	case 1:
	case 2:
	case 3:
		ret = check_type(fname, lv, "time", VT_STR, true);
		ret = check_type(fname, lv, "title", VT_STR, true) && ret;
		ret = check_type(fname, lv, "tags", VT_CONS, false) && ret;
		ret = check_type(fname, lv, "cats", VT_CONS, false) && ret;
		ret = check_type(fname, lv, "comments", VT_CONS, false) && ret;
		ret = check_type(fname, lv, "listed", VT_BOOL, false) && ret;
		return ret;
	case 4:
		return true;
	default:
		error(fname, "invalid format version\n");
		return false;
	}
}

static bool check_comment(const char *fname, struct val *lv)
{
	bool ret;

	ret = check_type(fname, lv, "author", VT_STR, true);
	ret = check_type(fname, lv, "email", VT_STR, true) && ret;
	ret = check_type(fname, lv, "time", VT_STR, true) && ret;
	ret = check_type(fname, lv, "ip", VT_STR, false) && ret;
	ret = check_type(fname, lv, "url", VT_STR, false) && ret;
	ret = check_type(fname, lv, "moderated", VT_BOOL, true) && ret;

	return ret;
}

static bool check_config(const char *fname, struct val *lv)
{
	bool ret;

	ret = check_type(fname, lv, CONFIG_SCGI_PORT, VT_INT, false);
	ret = check_type(fname, lv, CONFIG_SCGI_THREADS, VT_INT, false) && ret;
	ret = check_type(fname, lv, CONFIG_HTML_INDEX_STORIES, VT_INT, false) && ret;
	ret = check_type(fname, lv, CONFIG_FEED_INDEX_STORIES, VT_INT, false) && ret;
	ret = check_type(fname, lv, CONFIG_COMMENT_MAX_THINK, VT_INT, false) && ret;
	ret = check_type(fname, lv, CONFIG_COMMENT_MIN_THINK, VT_INT, false) && ret;
	ret = check_type(fname, lv, CONFIG_COMMENT_CAPTCHA_A, VT_INT, false) && ret;
	ret = check_type(fname, lv, CONFIG_COMMENT_CAPTCHA_B, VT_INT, false) && ret;
	ret = check_type(fname, lv, CONFIG_DATA_DIR, VT_STR, false) && ret;
	ret = check_type(fname, lv, CONFIG_WEB_DIR, VT_STR, false) && ret;
	ret = check_type(fname, lv, CONFIG_BASE_URL, VT_STR, false) && ret;
	ret = check_type(fname, lv, CONFIG_BUG_BASE_URL, VT_STR, false) && ret;
	ret = check_type(fname, lv, CONFIG_WIKI_BASE_URL, VT_STR, false) && ret;
	ret = check_type(fname, lv, CONFIG_PHOTO_BASE_URL, VT_STR, false) && ret;
	ret = check_type(fname, lv, CONFIG_TAGCLOUD_MIN_SIZE, VT_INT, false) && ret;
	ret = check_type(fname, lv, CONFIG_TAGCLOUD_MAX_SIZE, VT_INT, false) && ret;
	ret = check_type(fname, lv, CONFIG_LATEX_BIN, VT_STR, false) && ret;
	ret = check_type(fname, lv, CONFIG_DVIPS_BIN, VT_STR, false) && ret;
	ret = check_type(fname, lv, CONFIG_CONVERT_BIN, VT_STR, false) && ret;
	ret = check_type(fname, lv, CONFIG_WORDPRESS_CATEGORIES, VT_CONS, false) && ret;

	return ret;
}

static bool onefile(const char *fname, char *ibuf, size_t len)
{
	struct val *lv;
	bool ret;

	lv = sexpr_parse(ibuf, len);
	if (IS_ERR(lv)) {
		fprintf(stderr, "Error parsing %s: %s\n",
			fname, xstrerror(PTR_ERR(lv)));
		return false;
	}

	if (verbose) {
		sexpr_dump_file(stderr, lv, false);
		fprintf(stderr, "\n");
	}

	ret = checker(fname, lv);

	val_putref(lv);

	return ret;
}

static void usage(void)
{
	fprintf(stderr, "Usage: %s {-p | -c | -C} [-v] <filename> ...\n", prog);
	exit(2);
}

int main(int argc, char **argv)
{
	char *in;
	int i;
	int result;
	char opt;

	prog = argv[0];

	result = 0;

	ASSERT0(putenv("UMEM_DEBUG=default,verbose"));

	while ((opt = getopt(argc, argv, "pcCv")) != -1) {
		switch (opt) {
			case 'p':
				/* post */
				checker = check_post;
				break;
			case 'c':
				/* comment */
				checker = check_comment;
				break;
			case 'C':
				/* config */
				checker = check_config;
				break;
			case 'v':
				verbose = true;
				break;
			default:
				usage();
				break;
		}
	}

	if (optind == argc)
		usage();

	if (checker == NULL)
		usage();

	for (i = optind; i < argc; i++) {
		in = read_file(argv[i]);
		if (IS_ERR(in)) {
			fprintf(stderr, "%s: cannot read: %s\n",
				argv[i], xstrerror(PTR_ERR(in)));
			result++;
			continue;
		}

		if (!onefile(argv[i], in, strlen(in)))
			result++;

		free(in);
	}

	if (!result)
		printf("OK.\n");
	else
		fprintf(stderr, "%d failed files\n", result);

	return !!result;
}
