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
#include <sys/list.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <port.h>

#include "file_cache.h"
#include "utils.h"
#include "str.h"
#include "mx.h"
#include "iter.h"
#include "refcnt.h"

#define FILE_EVENTS	(FILE_MODIFIED | FILE_ATTRIB)

static avl_tree_t file_cache;
static pthread_mutex_t file_lock;

static pthread_t filemon_thread;
static int filemon_port;

struct file_callback {
	list_node_t list;

	void (*cb)(void *);
	void *arg;
};

struct file_node {
	char *name;			/* the filename */
	avl_node_t node;
	refcnt_t refcnt;
	pthread_mutex_t lock;

	/* everything else is protected by the lock */
	struct str *contents;		/* cache file contents if allowed */
	list_t callbacks;		/* list of callbacks to invoke */
	struct stat stat;		/* the stat info of the cached file */
	bool needs_reload;		/* caching stale data */
	struct file_obj fobj;		/* FEN port object */
};

static void fn_free(struct file_node *node);
static int __reload(struct file_node *node);

REFCNT_INLINE_FXNS(struct file_node, fn, refcnt, fn_free)

static void print_event(const char *fname, int event)
{
	printf("%s: %s: ", __func__, fname);
	if (event & FILE_ACCESS)
		printf("FILE_ACCESS ");
	if (event & FILE_MODIFIED)
		printf("FILE_MODIFIED ");
	if (event & FILE_ATTRIB)
		printf("FILE_ATTRIB ");
	if (event & FILE_DELETE)
		printf("FILE_DELETE ");
	if (event & FILE_RENAME_TO)
		printf("FILE_RENAME_TO ");
	if (event & FILE_RENAME_FROM)
		printf("FILE_RENAME_FROM ");
	if (event & UNMOUNTED)
		printf("UNMOUNTED ");
	if (event & MOUNTEDOVER)
		printf("MOUNTEDOVER ");
	printf("\n");
}

static void process_file(struct file_node *node, int events)
{
	struct file_obj *fobj = &node->fobj;
	struct file_callback *fcb;
	struct stat statbuf;

	MXLOCK(&node->lock);

	if (!(events & FILE_EXCEPTION) && stat(fobj->fo_name, &statbuf) == -1) {
		fprintf(stderr, "failed to stat '%s' errno %d\n",
			fobj->fo_name, errno);
		goto free;
	}

	if (events) {
		/* something changed, we need to reload */
		node->needs_reload = true;

		print_event(fobj->fo_name, events);

		list_for_each(&node->callbacks, fcb)
			fcb->cb(fcb->arg);

		/*
		 * If the file went away, we could rely on reloading to deal
		 * with it.  Or we can just remove the node and have the
		 * next caller read it from disk.  We take the later
		 * approach.
		 */
		if (events & FILE_EXCEPTION)
			goto free;
	}

	/* re-register */
	fobj->fo_atime = node->stat.st_atim;
	fobj->fo_mtime = node->stat.st_mtim;
	fobj->fo_ctime = node->stat.st_ctim;

	if (port_associate(filemon_port, PORT_SOURCE_FILE, (uintptr_t) fobj,
			   FILE_EVENTS, node) == -1) {
		fprintf(stderr, "failed to register file '%s' errno %d\n",
			fobj->fo_name, errno);
		goto free;
	}

	MXUNLOCK(&node->lock);

	return;

free:
	/*
	 * If there was an error, remove the node from the cache.  The next
	 * time someone tries to get this file, they'll pull it in from
	 * disk.
	 */
	MXLOCK(&file_lock);
	avl_remove(&file_cache, node);
	MXUNLOCK(&file_lock);

	MXUNLOCK(&node->lock);
	fn_putref(node); /* put the cache's reference */
}

static int add_cb(struct file_node *node, void (*cb)(void *), void *arg)
{
	struct file_callback *fcb;

	if (!cb)
		return 0;

	list_for_each(&node->callbacks, fcb)
		if ((fcb->cb == cb) && (fcb->arg == arg))
			return 0;

	fcb = malloc(sizeof(struct file_callback));
	if (!fcb)
		return ENOMEM;

	fcb->cb = cb;
	fcb->arg = arg;

	list_insert_tail(&node->callbacks, fcb);

	return 0;
}

static void *filemon(void *arg)
{
	port_event_t pe;

	while (!port_get(filemon_port, &pe, NULL)) {
		switch (pe.portev_source) {
			case PORT_SOURCE_FILE:
				process_file(pe.portev_user,
					     pe.portev_events);
				break;
			default:
				ASSERT(0);
		}
	}

	return NULL;
}

static int filename_cmp(const void *va, const void *vb)
{
	const struct file_node *a = va;
	const struct file_node *b = vb;
	int ret;

	ret = strcmp(a->name, b->name);
	if (ret < 0)
		return -1;
	if (ret > 0)
		return 1;
	return 0;
}

void init_file_cache(void)
{
	int ret;

	MXINIT(&file_lock);

	avl_create(&file_cache, filename_cmp, sizeof(struct file_node),
		   offsetof(struct file_node, node));

	/* start the file event monitor */
	filemon_port = port_create();
	ASSERT(filemon_port != -1);

	ret = pthread_create(&filemon_thread, NULL, filemon, NULL);
	ASSERT0(ret);
}

static struct file_node *fn_alloc(const char *name)
{
	struct file_node *node;

	node = malloc(sizeof(struct file_node));
	if (!node)
		return NULL;

	node->name = strdup(name);
	if (!node->name)
		goto err;

	node->fobj.fo_name = node->name;
	node->contents = NULL;
	node->needs_reload = true;

	MXINIT(&node->lock);
	refcnt_init(&node->refcnt, 1);
	list_create(&node->callbacks, sizeof(struct file_callback),
		    offsetof(struct file_callback, list));

	return node;

err:
	free(node);
	return NULL;
}

static void fn_free(struct file_node *node)
{
	if (!node)
		return;

	str_putref(node->contents);
	free(node->name);
	free(node);
}

static int __reload(struct file_node *node)
{
	char *tmp;

	if (!node->needs_reload)
		return 0;

	/* free the previous */
	str_putref(node->contents);
	node->contents = NULL;

	/* read the current */
	tmp = read_file_common(node->name, &node->stat);
	if (IS_ERR(tmp)) {
		LOG("file (%s) read error: %s", node->name,
		    strerror(PTR_ERR(tmp)));
		return PTR_ERR(tmp);
	}

	node->contents = str_alloc(tmp);
	if (!node->contents) {
		LOG("file (%s) str_alloc error", node->name);
		free(tmp);
		return ENOMEM;
	}

	node->needs_reload = false;

	return 0;
}

static struct file_node *load_file(const char *name)
{
	struct file_node *node;
	int ret;

	node = fn_alloc(name);
	if (!node)
		return ERR_PTR(ENOMEM);

	if ((ret = __reload(node))) {
		fn_free(node);
		return ERR_PTR(ret);
	}

	return node;
}

struct str *file_cache_get_cb(const char *name, void (*cb)(void *), void *arg)
{
	struct file_node *out, *tmp;
	struct file_node key;
	struct str *str;
	avl_index_t where;
	int ret;

	key.name = (char *) name;

	/* do we have it? */
	MXLOCK(&file_lock);
	out = avl_find(&file_cache, &key, NULL);
	fn_getref(out);
	MXUNLOCK(&file_lock);

	/* already had it, so return that */
	if (out)
		goto output;

	/* have to load it from disk...*/
	out = load_file(name);
	if (IS_ERR(out))
		return ERR_CAST(out);

	MXLOCK(&out->lock);

	/* register the callback */
	if ((ret = add_cb(out, cb, arg))) {
		/* Dang! An error... time to undo everything */
		MXUNLOCK(&out->lock);
		fn_putref(out);
		return ERR_PTR(ret);
	}

	/* ...and insert it into the cache */
	MXLOCK(&file_lock);
	tmp = avl_find(&file_cache, &key, &where);
	if (tmp) {
		/*
		 * uh oh, someone beat us to it; free our copy & return
		 * existing
		 */
		fn_getref(tmp);
		MXUNLOCK(&file_lock);

		/* release the node we allocated */
		MXUNLOCK(&out->lock);
		fn_putref(out);

		/* work with the node found in the tree */
		out = tmp;
		goto output;
	}

	/* get a ref for the cache */
	fn_getref(out);

	avl_insert(&file_cache, out, where);

	MXUNLOCK(&file_lock);
	MXUNLOCK(&out->lock);

	/*
	 * Ok!  The node is in the cache.
	 */

	/* start watching */
	process_file(out, 0);

output:
	/* get a reference for the string */
	MXLOCK(&out->lock);
	ret = __reload(out);
	if (ret) {
		/* there was an error reloading - bail */
		MXUNLOCK(&out->lock);
		fn_putref(out);
		return ERR_PTR(ret);
	}

	str = str_getref(out->contents);
	MXUNLOCK(&out->lock);

	/* put the reference for the file node */
	fn_putref(out);

	return str;
}
