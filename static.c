#include "static.h"
#include "error.h"
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

enum uri_type get_uri_type(const char *path)
{
	const struct uri_info *info;

	info = get_uri_info(path);

	return info ? info->type : URI_BAD;
}

int blahg_static(struct req *req)
{
	const struct uri_info *info;
	const char *uri;

	uri = nvl_lookup_str(req->request_headers, DOCUMENT_URI);
	ASSERT(uri);

	info = get_uri_info(uri);
	ASSERT(info);

	/* DOCUMENT_URI comes with a leading /, remove it. */
	uri++;

	/*
	 * We assume that the URI is relative to the web dir.  Since we
	 * have a whitelist of allowed URIs, whe should be safe here.
	 */
	req->body = read_file_len(uri, &req->bodylen);

	if (!req->body)
		return R404(req, NULL);

	req_head(req, "Content-Type", info->content_type);

	req->dump_latency = false;

	return 0;
}
