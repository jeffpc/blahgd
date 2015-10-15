/*
 * Copyright (c) 2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include "config.h"
#include "file_cache.h"
#include "lisp.h"

/*
 * The actual config
 */
struct config config;

static struct str *exampledotcom;

static void config_load_scgi_port(struct val *lv)
{
	uint64_t tmp;
	bool found;

	tmp = lisp_alist_lookup_int(lv, CONFIG_SCGI_PORT, &found);

	if (found && (tmp < 65536))
		config.scgi_port = tmp;
	else
		config.scgi_port = DEFAULT_SCGI_PORT;
}

static void config_load_u64(struct val *lv, const char *vname,
			    uint64_t *ret, uint64_t def)
{
	uint64_t tmp;
	bool found;

	tmp = lisp_alist_lookup_int(lv, vname, &found);

	if (found)
		*ret = tmp;
	else
		*ret = def;
}

static void config_load_url(struct val *lv, const char *vname,
			    struct str **ret)
{
	struct str *s;

	s = lisp_alist_lookup_str(lv, vname);

	if (s)
		*ret = s;
	else
		*ret = str_getref(exampledotcom);
}

static void config_load_str(struct val *lv, const char *vname,
			    struct str **ret, const char *def)
{
	struct str *s;

	s = lisp_alist_lookup_str(lv, vname);

	if (s)
		*ret = s;
	else
		*ret = STR_DUP(def);
}

static void config_load_scgi_threads(struct val *lv)
{
	config_load_u64(lv, CONFIG_SCGI_THREADS, &config.scgi_threads,
			DEFAULT_SCGI_THREADS);

	/* we need at least one thread */
	if (!config.scgi_threads)
		config.scgi_threads = DEFAULT_SCGI_THREADS;
}

int config_load(const char *fname)
{
	struct val *lv;
	char *raw;

	srand(time(NULL));

	exampledotcom = STR_DUP("http://example.com");

	if (fname) {
		raw = read_file(fname);
		if (IS_ERR(raw))
			return PTR_ERR(raw);

		lv = parse_lisp_cstr(raw);

		free(raw);

		if (!lv)
			return EINVAL;
	} else {
		lv = NULL;
	}

	config_load_scgi_port(lv);
	config_load_scgi_threads(lv);
	config_load_u64(lv, CONFIG_COMMENT_MAX_THINK, &config.comment_max_think,
			DEFAULT_COMMENT_MAX_THINK);
	config_load_u64(lv, CONFIG_COMMENT_MIN_THINK, &config.comment_min_think,
			DEFAULT_COMMENT_MIN_THINK);
	config_load_u64(lv, CONFIG_COMMENT_CAPTCHA_A, &config.comment_captcha_a,
			rand());
	config_load_u64(lv, CONFIG_COMMENT_CAPTCHA_B, &config.comment_captcha_b,
			rand());
	config_load_url(lv, CONFIG_BASE_URL, &config.base_url);
	config_load_url(lv, CONFIG_BUG_BASE_URL, &config.bug_base_url);
	config_load_url(lv, CONFIG_WIKI_BASE_URL, &config.wiki_base_url);
	config_load_url(lv, CONFIG_PHOTO_BASE_URL, &config.photo_base_url);
	config_load_u64(lv, CONFIG_TAGCLOUD_MIN_SIZE, &config.tagcloud_min_size,
			DEFAULT_TAGCLOUD_MIN_SIZE);
	config_load_u64(lv, CONFIG_TAGCLOUD_MAX_SIZE, &config.tagcloud_max_size,
			DEFAULT_TAGCLOUD_MAX_SIZE);
	config_load_str(lv, CONFIG_LATEX_BIN, &config.latex_bin,
			DEFAULT_LATEX_BIN);
	config_load_str(lv, CONFIG_DVIPNG_BIN, &config.dvipng_bin,
			DEFAULT_DVIPNG_BIN);

	val_putref(lv);

	printf("config.scgi_port = %u\n", config.scgi_port);
	printf("config.scgi_threads = %"PRIu64"\n", config.scgi_threads);
	printf("config.comment_max_think = %"PRIu64"\n", config.comment_max_think);
	printf("config.comment_min_think = %"PRIu64"\n", config.comment_min_think);
	printf("config.comment_captcha_a = %"PRIu64"\n", config.comment_captcha_a);
	printf("config.comment_captcha_b = %"PRIu64"\n", config.comment_captcha_b);
	printf("config.base_url = %s\n", str_cstr(config.base_url));
	printf("config.wiki_base_url = %s\n", str_cstr(config.wiki_base_url));
	printf("config.bug_base_url = %s\n", str_cstr(config.bug_base_url));
	printf("config.photo_base_url = %s\n", str_cstr(config.photo_base_url));
	printf("config.tagcloud_min_size = %"PRIu64"\n", config.tagcloud_min_size);
	printf("config.tagcloud_max_size = %"PRIu64"\n", config.tagcloud_max_size);
	printf("config.latex_bin = %s\n", str_cstr(config.latex_bin));
	printf("config.dvipng_bin = %s\n", str_cstr(config.dvipng_bin));

	return 0;
}
