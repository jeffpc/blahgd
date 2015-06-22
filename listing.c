/*
 * Copyright (c) 2011-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include "listing.h"
#include "utils.h"
#include "mangle.h"
#include "file_cache.h"
#include "post.h"

#define PAGE 4096

struct str *listing(struct post *post, char *fname)
{
	char path[FILENAME_MAX];
	struct str *in;

	snprintf(path, FILENAME_MAX, DATA_DIR "/posts/%d/%s", post->id, fname);

	in = file_cache_get_cb(path, post->preview ? NULL : revalidate_post,
			       post);
	if (!in)
		goto err;

	return listing_str(in);

err:
	snprintf(path, FILENAME_MAX, "Failed to read in listing '%d/%s'",
		 post->id, fname);
	return STR_DUP(path);
}
