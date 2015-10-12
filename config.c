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

static void config_load_scgi_port(struct val *lv)
{
	uint64_t tmp;
	bool found;

	tmp = lisp_alist_lookup_int(lv, CONFIG_SCGI_PORT, &found);

	if (found || (tmp < 65536))
		config.scgi_port = tmp;
	else
		config.scgi_port = DEFAULT_SCGI_PORT;
}

int config_load(const char *fname)
{
	struct val *lv;
	struct str *raw;

	raw = file_cache_get(fname);
	if (IS_ERR(raw))
		return PTR_ERR(raw);

	lv = parse_lisp_str(raw);
	if (!lv)
		goto err;

	config_load_scgi_port(lv);

	val_putref(lv);

	printf("config.scgi_port = %u\n", config.scgi_port);

	return 0;

err:
	return EINVAL;
}
