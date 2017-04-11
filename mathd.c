/*
 * Copyright (c) 2014-2016 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <door.h>
#include <stropts.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <alloca.h>

#include <jeffpc/jeffpc.h>
#include <jeffpc/version.h>
#include <jeffpc/error.h>
#include <jeffpc/val.h>
#include <jeffpc/str.h>
#include <jeffpc/synch.h>

#include "math.h"
#include "config.h"
#include "debug.h"
#include "version.h"

static struct lock lock;

static void process(void *cookie, char *argp, size_t argsz, door_desc_t *dp,
		    uint_t ndesc)
{
	struct str *in;
	struct str *out;
	char *str;

	MXLOCK(&lock);

	DBG("arg: '%s'", argp);

	in = STR_DUP(argp);

	out = render_math(in);

	str = alloca(str_len(out) + 1);

	strcpy(str, str_cstr(out));

	str_putref(out);

	DBG("out: '%s'", str);

	MXUNLOCK(&lock);

	door_return(str, strlen(str) + 1, NULL, 0);
}

static int setup_door(void)
{
	int ret;
	int tmp;
	int fd;

	fd = door_create(process, NULL, DOOR_NO_CANCEL);
	if (fd < 0) {
		ret = -errno;
		DBG("door_create failed: %s", xstrerror(ret));
		return ret;
	}

	fdetach(MATHD_DOOR_PATH);

	tmp = creat(MATHD_DOOR_PATH, S_IRUSR | S_IWUSR);
	if (tmp < 0) {
		ret = -errno;
		DBG("creat failed: %s", xstrerror(ret));
		return ret;
	}

	close(tmp);

	if (fattach(fd, MATHD_DOOR_PATH) < 0) {
		ret = -errno;
		DBG("fattach failed: %s", xstrerror(ret));
		return ret;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret;

	openlog("mathd", LOG_NDELAY | LOG_PID, LOG_LOCAL0);

	cmn_err(CE_INFO, "mathd version %s", version_string);
	cmn_err(CE_INFO, "libjeffpc version %s", jeffpc_version);

	jeffpc_init(&init_ops);
	init_math(false);

	ret = config_load((argc >= 2) ? argv[1] : NULL);
	if (ret)
		goto out;

	MXINIT(&lock);

	ret = setup_door();
	if (ret)
		goto out;

	for (;;)
		sleep(3600);

out:
	return ret;
}
