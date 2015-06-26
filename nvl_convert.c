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

#include "nvl.h"
#include "utils.h"
#include "error.h"

void nvl_convert(nvlist_t *nvl, const struct convert_info *table)
{
	uint64_t intval;
	char *str;
	int ret;
	int i;

	/*
	 * If we weren't given a table, we have nothing to do other than the
	 * generic conversion (see above)
	 */
	if (!table)
		return;

	for (i = 0; table[i].name; i++) {
		nvpair_t *pair;

		ret = nvlist_lookup_nvpair(nvl, table[i].name, &pair);
		if (ret)
			continue;

		if (nvpair_type(pair) != DATA_TYPE_STRING)
			continue;

		ret = nvpair_value_string(pair, &str);
		if (ret)
			continue;

		switch (table[i].type) {
			case DATA_TYPE_UINT64:
				ret = str2u64(str, &intval);
				if (!ret)
					nvl_set_int(nvl, table[i].name, intval);
				break;
			default:
				ASSERT(0);
		}
	}
}
