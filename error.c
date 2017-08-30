/*
 * Copyright (c) 2013-2016 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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
#include <jeffpc/jeffpc.h>

#include <syslog.h>
#include <stdarg.h>

/* session info */
static __thread char session_str[64];
static __thread uint32_t session_id;

static void mylog(int level, const char *fmt, va_list ap)
{
	char msg[512];

	if (getenv("BLAHG_DISABLE_SYSLOG"))
		return;

	vsnprintf(msg, sizeof(msg), fmt, ap);

	syslog(LOG_LOCAL0 | level, "%s", msg);
}

void set_session(uint32_t id)
{
	session_id = id;
}

static const char *get_session(void)
{
	if (!session_id)
		return "";

	snprintf(session_str, sizeof(session_str), " {%u}", session_id);

	return session_str;
}

struct jeffpc_ops init_ops = {
	.log = mylog,
	.get_session = get_session,
};
