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

#ifndef __NVL_H
#define __NVL_H

#include <stdbool.h>
#include <libnvpair.h>

/*
 * convert @name to type @type
 */
struct convert_info {
	const char *name;
	data_type_t type;
};

extern nvlist_t *nvl_alloc(void);
extern void nvl_set_str(nvlist_t *nvl, const char *name, const char *val);
extern void nvl_set_int(nvlist_t *nvl, const char *name, uint64_t val);
extern void nvl_set_bool(nvlist_t *nvl, const char *name, bool val);
extern void nvl_set_char(nvlist_t *nvl, const char *name, char val);
extern void nvl_set_nvl(nvlist_t *nvl, const char *name, nvlist_t *val);
extern void nvl_set_str_array(nvlist_t *nvl, const char *name, char **val,
			      uint_t nval);
extern void nvl_set_nvl_array(nvlist_t *nvl, const char *name,
			      nvlist_t **val, uint_t nval);
extern nvpair_t *nvl_lookup(nvlist_t *nvl, const char *name);
extern char *nvl_lookup_str(nvlist_t *nvl, const char *name);
extern uint64_t nvl_lookup_int(nvlist_t *nvl, const char *name);
extern void nvl_convert(nvlist_t *nvl, const struct convert_info *table);
extern void nvl_dump(nvlist_t *nvl);

extern char *pair2str(nvpair_t *pair);
extern uint64_t pair2int(nvpair_t *pair);
extern bool pair2bool(nvpair_t *pair);
extern char pair2char(nvpair_t *pair);
extern nvlist_t *pair2nvl(nvpair_t *pair);

#endif
