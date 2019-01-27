/*
 * Copyright (c) 2015-2019 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <jeffpc/sexpr.h>

#include "config.h"
#include "utils.h"
#include "file_cache.h"
#include "debug.h"

/*
 * The actual config
 */
struct config config;

static struct str exampledotcom = STR_STATIC_INITIALIZER("example.com");

static void config_load_u64(struct val *lv, const char *vname,
			    uint64_t *ret, uint64_t def)
{
	uint64_t tmp;
	bool found;

	tmp = sexpr_alist_lookup_int(lv, vname, &found);

	if (found)
		*ret = tmp;
	else
		*ret = def;
}

static void config_load_url(struct val *lv, const char *vname,
			    struct str **ret)
{
	struct str *s;

	s = sexpr_alist_lookup_str(lv, vname);

	if (s)
		*ret = s;
	else
		*ret = str_getref(&exampledotcom);
}

static void config_load_str(struct val *lv, const char *vname,
			    struct str **ret, const char *def)
{
	struct str *s;

	s = sexpr_alist_lookup_str(lv, vname);

	if (s)
		*ret = s;
	else if (def)
		*ret = STR_DUP(def);
	else
		*ret = NULL;
}

static void config_load_list(struct val *lv, const char *vname,
			     struct val **ret)
{
	*ret = sexpr_alist_lookup_val(lv, vname);
}

static void config_load_scgi_port(struct val *lv)
{
	uint64_t tmp;

	config_load_u64(lv, CONFIG_SCGI_PORT, &tmp, DEFAULT_SCGI_PORT);

	if (tmp < 65536)
		config.scgi_port = tmp;
	else
		config.scgi_port = DEFAULT_SCGI_PORT;
}

static void config_load_scgi_threads(struct val *lv)
{
	uint64_t tmp;

	config_load_u64(lv, CONFIG_SCGI_THREADS, &tmp, 0);

	/* zero threads means 1/CPU; the taskq code expects -1 for that */
	if (!tmp)
		config.scgi_threads = -1;
	else if (tmp > INT_MAX)
		config.scgi_threads = INT_MAX;
	else
		config.scgi_threads = tmp;
}

int config_load(const char *fname)
{
	struct val *lv;
	char *raw;

	srand(time(NULL));

	if (fname) {
		raw = read_file(fname);
		if (IS_ERR(raw))
			return PTR_ERR(raw);

		lv = sexpr_parse_cstr(raw);

		free(raw);

		if (IS_ERR(lv))
			return PTR_ERR(lv);
	} else {
		lv = NULL;
	}

	config_load_scgi_port(lv);
	config_load_scgi_threads(lv);
	config_load_u64(lv, CONFIG_HTML_INDEX_STORIES, &config.html_index_stories,
			DEFAULT_HTML_INDEX_STORIES);
	config_load_u64(lv, CONFIG_FEED_INDEX_STORIES, &config.feed_index_stories,
			DEFAULT_FEED_INDEX_STORIES);
	config_load_u64(lv, CONFIG_COMMENT_MAX_THINK, &config.comment_max_think,
			DEFAULT_COMMENT_MAX_THINK);
	config_load_u64(lv, CONFIG_COMMENT_MIN_THINK, &config.comment_min_think,
			DEFAULT_COMMENT_MIN_THINK);
	config_load_u64(lv, CONFIG_COMMENT_CAPTCHA_A, &config.comment_captcha_a,
			rand());
	config_load_u64(lv, CONFIG_COMMENT_CAPTCHA_B, &config.comment_captcha_b,
			rand());
	config_load_str(lv, CONFIG_DATA_DIR, &config.data_dir, DEFAULT_DATA_DIR);
	config_load_str(lv, CONFIG_WEB_DIR, &config.web_dir, DEFAULT_WEB_DIR);
	config_load_url(lv, CONFIG_BASE_URL, &config.base_url);
	config_load_url(lv, CONFIG_BUG_BASE_URL, &config.bug_base_url);
	config_load_url(lv, CONFIG_WIKI_BASE_URL, &config.wiki_base_url);
	config_load_url(lv, CONFIG_PHOTO_BASE_URL, &config.photo_base_url);
	config_load_u64(lv, CONFIG_TAGCLOUD_MIN_SIZE, &config.tagcloud_min_size,
			DEFAULT_TAGCLOUD_MIN_SIZE);
	config_load_u64(lv, CONFIG_TAGCLOUD_MAX_SIZE, &config.tagcloud_max_size,
			DEFAULT_TAGCLOUD_MAX_SIZE);
	config_load_str(lv, CONFIG_TWITTER_USERNAME, &config.twitter_username,
			NULL);
	config_load_str(lv, CONFIG_TWITTER_DESCRIPTION, &config.twitter_description,
			NULL);
	config_load_list(lv, CONFIG_WORDPRESS_CATEGORIES,
			 &config.wordpress_categories);

	val_putref(lv);

	DBG("config.scgi_port = %u", config.scgi_port);
	DBG("config.scgi_threads = %d", config.scgi_threads);
	DBG("config.html_index_stories = %"PRIu64, config.html_index_stories);
	DBG("config.feed_index_stories = %"PRIu64, config.feed_index_stories);
	DBG("config.comment_max_think = %"PRIu64, config.comment_max_think);
	DBG("config.comment_min_think = %"PRIu64, config.comment_min_think);
	DBG("config.comment_captcha_a = %"PRIu64, config.comment_captcha_a);
	DBG("config.comment_captcha_b = %"PRIu64, config.comment_captcha_b);
	DBG("config.data_dir = %s", str_cstr(config.data_dir));
	DBG("config.web_dir = %s", str_cstr(config.web_dir));
	DBG("config.base_url = %s", str_cstr(config.base_url));
	DBG("config.wiki_base_url = %s", str_cstr(config.wiki_base_url));
	DBG("config.bug_base_url = %s", str_cstr(config.bug_base_url));
	DBG("config.photo_base_url = %s", str_cstr(config.photo_base_url));
	DBG("config.tagcloud_min_size = %"PRIu64, config.tagcloud_min_size);
	DBG("config.tagcloud_max_size = %"PRIu64, config.tagcloud_max_size);
	DBG("config.twitter_username = %s", str_cstr(config.twitter_username));
	DBG("config.twitter_description = %s", str_cstr(config.twitter_description));

	return 0;
}
