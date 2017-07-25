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

#include <jeffpc/error.h>

#include "static.h"
#include "utils.h"

struct uri_info {
	const char *uri;
	const char *content_type;	/* required for URI_STATIC */
	enum uri_type type;
	bool recurse;
};

static const struct uri_info safe_uris[] = {
	{ "/",			NULL,		URI_DYNAMIC,	false },
	{ "/bug.png",		"image/png",	URI_STATIC,	false },
	{ "/favicon.ico",	"image/png",	URI_STATIC,	false },
	{ "/style.css",		"text/css",	URI_STATIC,	false },
	{ "/wiki.png",		"image/png",	URI_STATIC,	false },
	{ "/math/",		"image/png",	URI_STATIC,	true  },
	{ NULL,			NULL,		URI_BAD,	false },
};

static const struct uri_info *get_uri_info(const char *path)
{
	int i;

	if (!path)
		return NULL;

	if (hasdotdot(path))
		return NULL;

	for (i = 0; safe_uris[i].uri; i++) {
		const char *uri = safe_uris[i].uri;
		bool rec = safe_uris[i].recurse;

		/* recursive paths must begin exactly the same */
		if (rec && !strncmp(uri, path, strlen(uri)))
			return &safe_uris[i];

		/* non-recursive paths must match exactly */
		if (!rec && !strcmp(uri, path))
			return &safe_uris[i];
	}

	return NULL;
}

enum uri_type get_uri_type(struct str *path)
{
	const struct uri_info *info;

	info = get_uri_info(str_cstr(path));

	str_putref(path);

	return info ? info->type : URI_BAD;
}

int blahg_static(struct req *req)
{
	char path[FILENAME_MAX];
	const struct uri_info *info;
	struct str *uri_str;
	const char *uri;

	uri_str = nvl_lookup_str(req->scgi->request.headers, DOCUMENT_URI);
	ASSERT(!IS_ERR(uri_str));

	uri = str_cstr(uri_str);

	info = get_uri_info(uri);
	ASSERT(info);

	/* DOCUMENT_URI comes with a leading /, remove it. */
	uri++;

	snprintf(path, sizeof(path), "%s/%s", str_cstr(config.web_dir), uri);

	str_putref(uri_str);

	/*
	 * We assume that the URI is relative to the web dir.  Since we
	 * have a whitelist of allowed URIs, whe should be safe here.
	 */
	req->scgi->response.body = read_file_len(path,
						 &req->scgi->response.bodylen);

	if (IS_ERR(req->scgi->response.body))
		return R404(req, NULL);

	req_head(req, "Content-Type", info->content_type);

	req->dump_latency = false;

	return 0;
}

