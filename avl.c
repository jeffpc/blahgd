/*
 * Copyright (c) 2011-2012 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>

#include "avl.h"

static inline int avl_min(int a, int b)
{
	return a < b ? a : b;
}

static inline int avl_max(int a, int b)
{
	return a > b ? a : b;
}

static int avl_rotate_single_right(struct avl_node **_cur)
{
	struct avl_node *oldroot = *_cur;
	struct avl_node *root;
	int change;

	assert(oldroot->left);
	change = !oldroot->left->bal ? 0 : 1;

	/* new root */
	*_cur = oldroot->left;
	root = *_cur;

	oldroot->left = root->right;
	root->right = oldroot;

	oldroot->bal = -(++root->bal);

	return change;
}

static int avl_rotate_single_left(struct avl_node **_cur)
{
	struct avl_node *oldroot = *_cur;
	struct avl_node *root;
	int change;

	assert(oldroot->right);
	change = !oldroot->right->bal ? 0 : 1;

	/* new root */
	*_cur = oldroot->right;
	root = *_cur;

	oldroot->right = root->left;
	root->left = oldroot;

	oldroot->bal = -(--root->bal);

	return change;
}

static int avl_rotate_double_right(struct avl_node **_cur)
{
	struct avl_node *oldroot = *_cur;
	struct avl_node *root;
	struct avl_node *L = oldroot->left;

	/* new root */
	*_cur = oldroot->left->right;
	root = *_cur;

	oldroot->left = root->right;
	root->right = oldroot;

	L->right = root->left;
	root->left = L;

	root->left->bal = -avl_max(root->bal, 0);
	root->right->bal = -avl_min(root->bal, 0);
	root->bal = 0;

	return 1;
}

static int avl_rotate_double_left(struct avl_node **_cur)
{
	struct avl_node *oldroot = *_cur;
	struct avl_node *root;
	struct avl_node *R = oldroot->right;

	/* new root */
	*_cur = oldroot->right->left;
	root = *_cur;

	oldroot->right = root->left;
	root->left = oldroot;

	R->left = root->right;
	root->right = R;

	root->left->bal = -avl_max(root->bal, 0);
	root->right->bal = -avl_min(root->bal, 0);
	root->bal = 0;

	return 1;
}

struct avl_node *avl_find_node(struct avl_root *root,
			       struct avl_node *key)
{
	struct avl_node *cur;
	struct avl_node *next;
	int ret;

	if (!root->root)
		return NULL;

	for(cur = root->root; cur; cur = next) {
		ret = root->cmp(key, cur);

		if (ret == 0)
			/* found it */
			return cur;
		else if (ret < 0)
			/* go left */
			next = cur->left;
		else
			next = cur->right;
	}

	/* not found */
	return NULL;
}

static int __avl_rebalance(struct avl_node **_cur)
{
	struct avl_node *cur = *_cur;
	int change = 0;

	if (cur->bal < -1) {
		assert(cur->left);
		if (cur->left->bal == 1)
			change = avl_rotate_double_right(_cur); /* RL */
		else
			change = avl_rotate_single_right(_cur); /* RR */
	} else if (cur->bal > 1) {
		assert(cur->right);
		if (cur->right->bal == -1)
			change = avl_rotate_double_left(_cur); /* LR */
		else
			change = avl_rotate_single_left(_cur); /* LL */
	}

	return change;
}

int __avl_insert_node(struct avl_root *root,
		      struct avl_node **_cur,
		      struct avl_node *new,
		      int *change)
{
	struct avl_node *cur = *_cur;
	int ret;

	if (!cur) {
		*_cur = new;
		*change = 1;
		return 0;
	}

	ret = root->cmp(new, cur);

	if (ret < 0) {
		/* put it left... */
		ret = __avl_insert_node(root, &(cur->left), new, change);
		if (ret)
			return ret;
		ret = -*change;
	} else if ((ret > 0) || (root->dups && ret == 0)) {
		/* put it right... */
		ret = __avl_insert_node(root, &(cur->right), new, change);
		if (ret)
			return ret;
		ret = *change;
	} else
		/* This key is already present, and we don't allow dups */
		return 1;

	cur->bal += ret;

	if (ret && cur->bal)
		*change = 1 - __avl_rebalance(_cur);
	else
		*change = 0;

	return 0;
}

struct avl_node **__avl_remove_helper(struct avl_node **_cur,
				      int *change)
{
	struct avl_node *cur = *_cur;
	struct avl_node **found;

	if (!cur) {
		*change = 0;
		return NULL;
	}

	if (cur->left) {
		found = __avl_remove_helper(&(cur->left), change);
		if (!found)
			return found;
	} else {
		found = _cur;
		*change = 1;
		return found;
	}

	cur->bal += *change;

	if (*change) {
		if (cur->bal)
			*change = __avl_rebalance(_cur);
		else
			*change = 1;
	}

	return found;
}

struct avl_node **__avl_remove_node(struct avl_root *root,
				    struct avl_node **_cur,
				    struct avl_node *node,
				    int *change)
{
	struct avl_node *cur = *_cur;
	struct avl_node **found;
	int delta;
	int ret;

	if (!cur) {
		*change = 0;
		return NULL;
	}

	ret = root->cmp(node, cur);

	if (root->dups && !ret)
		ret = (node != cur);

	if (ret < 0) {
		/* delete from left */
		found = __avl_remove_node(root, &(cur->left), node, change);
		if (!found)
			return found;
		delta = -*change;
	} else if (ret > 0) {
		/* delete from right */
		found = __avl_remove_node(root, &(cur->right), node, change);
		if (!found)
			return found;
		delta = *change;
	} else {
		found = _cur;
		delta = 0;

		if (!cur->left && !cur->right) {
			/* leaf */
			*_cur = NULL;
			*change = 1;
			AVL_NODE_INIT(cur);
			return found;
		} else if (!cur->left || !cur->right) {
			/* one child */
			*_cur = cur->left ? cur->left : cur->right;
			*change = 1;
			AVL_NODE_INIT(cur);
			return found;
		} else {
			/* two childern */
			struct avl_node *tmp;
			struct avl_node **leaf = __avl_remove_helper(&cur->right, &delta);

			tmp = *leaf;
			*leaf = (*leaf)->right;

			tmp->left = cur->left;
			tmp->right = cur->right;
			tmp->bal  = cur->bal;
			*_cur = tmp;
			AVL_NODE_INIT(cur);
			cur = *_cur;
		}
	}

	cur->bal -= delta;

	if (delta) {
		if (cur->bal)
			*change = __avl_rebalance(_cur);
		else
			*change = 1;
	} else
		*change = 0;

	return found;
}

static int __avl_for_each(struct avl_root *root, struct avl_node *node,
			  int (*fn)(struct avl_node*, void*), void *data,
			  struct avl_node *start, struct avl_node *end)
{
	int ret;
	int s;
	int e;
	int L, N, R;

	if (!node)
		return 0;

	/*
	 *  L = traverse left
	 *  N = call fn
	 *  R = traverse right
	 *  - = do nothing/illegal/impossible state
	 *
	 * +-----------+-------------------------------+
	 * |           |              end              |
	 * |           +-------+-------+-------+-------+
	 * |           | NULL  | under | equal | over  |
	 * +---+-------+-------+-------+-------+-------+
	 * | s | NULL  | LNR   | LNR   | LNR   | L     |
	 * | t | under | R     | R     | -     | -     |
	 * | r | equal | NR    | NR    | NR    | -     |
	 * | t | over  | LNR   | LNR   | LNR   | L     |
	 * +---+-------+-------+-------+-------+-------+
	 *
	 */

	s = start ? root->cmp(node, start) : 0;
	e = end ? root->cmp(node, end) : 0;

	L = !start || s>0;
	N = (!start || s>=0) && (!end || e<=0);
	R = !end || e<=0;

	if (L)
		if (__avl_for_each(root, node->left, fn, data, start, end))
			return 1;

	/* visit the current node, and traverse the right subtree */
	if (N) {
		ret = fn(node, data);
		if (ret)
			return 1;
	}

	if (R)
		if (__avl_for_each(root, node->right, fn, data, start, end))
			return 1;

	return 0;
}

void avl_for_each(struct avl_root *root, int (*fn)(struct avl_node*, void*),
		  void *data, struct avl_node *start, struct avl_node *end)
{
	if (start && end)
		assert(root->cmp(start, end) <= 0);

	__avl_for_each(root, root->root, fn, data, start, end);
}
