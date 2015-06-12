/*
 * Copyright (c) 2014-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <sys/avl.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>

#include "template_cache.h"
#include "utils.h"
#include "str.h"
#include "mx.h"

static avl_tree_t tmpl_cache;
static pthread_mutex_t cache_lock;

struct tmpl_node {
	char *name;
	struct str *contents;
	avl_node_t node;
};

static int filename_cmp(const void *va, const void *vb)
{
	const struct tmpl_node *a = va;
	const struct tmpl_node *b = vb;
	int ret;

	ret = strcmp(a->name, b->name);
	if (ret < 0)
		return -1;
	if (ret > 0)
		return 1;
	return 0;
}

void init_template_cache(void)
{
	MXINIT(&cache_lock);

	avl_create(&tmpl_cache, filename_cmp, sizeof(struct tmpl_node),
		   offsetof(struct tmpl_node, node));
}

struct tmpl_node *load_tmpl(const char *name)
{
	struct tmpl_node *node;
	char *tmp;

	tmp = NULL;

	node = malloc(sizeof(struct tmpl_node));
	if (!node) {
		LOG("template (%s) malloc error", name);
		return NULL;
	}

	node->name = strdup(name);
	if (!node->name) {
		LOG("template (%s) malloc error", name);
		goto err;
	}

	tmp = read_file(name);
	if (!tmp) {
		LOG("template (%s) read error", name);
		goto err;
	}

	node->contents = str_alloc(tmp);
	if (!node->contents) {
		LOG("template (%s) str_alloc error", name);
		goto err;
	}

	return node;

err:
	free(tmp);
	free(node);

	return NULL;
}

struct str *template_cache_get(const char *name)
{
	struct tmpl_node *out, *tmp;
	struct tmpl_node key;
	avl_index_t where;

	key.name = (char *) name;

	/* do we have it? */
	MXLOCK(&cache_lock);
	out = avl_find(&tmpl_cache, &key, NULL);
	if (out)
		str_getref(out->contents);
	MXUNLOCK(&cache_lock);

	/* already had it, so return that */
	if (out)
		return out->contents;

	/* have to load it from disk...*/
	out = load_tmpl(name);
	if (!out)
		return NULL;

	/* ...and insert it into the cache */
	MXLOCK(&cache_lock);
	tmp = avl_find(&tmpl_cache, &key, &where);
	if (tmp) {
		/*
		 * uh oh, someone beat us to it; free our copy & return
		 * existing
		 */
		str_getref(tmp->contents);
		MXUNLOCK(&cache_lock);

		str_putref(out->contents);
		free(out->name);
		free(out);

		return tmp->contents;
	}

	str_getref(out->contents);

	avl_insert(&tmpl_cache, out, where);

	MXUNLOCK(&cache_lock);

	return out->contents;
}
