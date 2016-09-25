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

#include <jeffpc/str.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <door.h>

#include <sha1.h>

#include <jeffpc/error.h>
#include <jeffpc/atomic.h>
#include <jeffpc/hexdump.h>
#include <jeffpc/io.h>

#include "utils.h"
#include "math.h"
#include "config.h"
#include "debug.h"

static bool use_door_call;
static int doorfd;

#define TEX_TMP_DIR "/tmp"
static int __render_math(const char *tex, char *md, char *dstpath,
			 char *texpath, char *dvipath, char *pspath,
			 char *pngpath)
{
	char cmd[3*FILENAME_MAX];
	char *pwd;
	int ret;
	int fd;

	pwd = getcwd(NULL, FILENAME_MAX);
	if (!pwd)
		return -errno;

	chdir(TEX_TMP_DIR);

	fd = xopen(texpath, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		ret = fd;
		goto err_chdir;
	}

	if ((ret = xwrite_str(fd,
			      "\\documentclass{article}\n"
			      "% add additional packages here\n"
			      "\\usepackage{amsmath}\n"
			      "\\usepackage{bm}\n"
			      "\\pagestyle{empty}\n"
			      "\\begin{document}\n"
			      "\\begin{equation*}\n"
			      "\\large\n")))
		goto err_close;
	if ((ret = xwrite_str(fd, tex)))
		goto err_close;
	if ((ret = xwrite_str(fd,
			      "\\end{equation*}\n"
			      "\\end{document}\n")))
		goto err_close;

	xclose(fd);

	snprintf(cmd, sizeof(cmd), "%s --interaction=nonstopmode %s > /dev/null",
		 str_cstr(config.latex_bin), texpath);
	DBG("math cmd: '%s'", cmd);
	if (system(cmd) == -1) {
		ret = -errno;
		goto err_chdir;
	}

	snprintf(cmd, sizeof(cmd), "%s -q -o %s %s",
		 str_cstr(config.dvips_bin), pspath, dvipath);
	DBG("math cmd: '%s'", cmd);
	if (system(cmd) == -1) {
		ret = -errno;
		goto err_chdir;
	}

	snprintf(cmd, sizeof(cmd), "%s -trim -density 125 %s %s",
		 str_cstr(config.convert_bin), pspath, pngpath);
	DBG("math cmd: '%s'", cmd);
	if (system(cmd) == -1) {
		ret = -errno;
		goto err_chdir;
	}

	chdir(pwd);

	snprintf(cmd, sizeof(cmd), "cp %s %s", pngpath, dstpath);
	DBG("math cmd: '%s'", cmd);
	if (system(cmd) == -1) {
		ret = -errno;
		goto err;
	}

	free(pwd);

	return 0;

err_close:
	xclose(fd);

	xunlink(texpath);

err_chdir:
	chdir(pwd);

err:
	free(pwd);

	return ret;
}

static struct str *do_render_math(struct str *val)
{
	static atomic_t nonce;

	char finalpath[FILENAME_MAX];
	char texpath[FILENAME_MAX];
	char logpath[FILENAME_MAX];
	char auxpath[FILENAME_MAX];
	char dvipath[FILENAME_MAX];
	char pspath[FILENAME_MAX];
	char pngpath[FILENAME_MAX];

	const char *tex = str_cstr(val);
	SHA1_CTX digest;
	struct stat statbuf;
	unsigned char md[20];
	char amd[41];
	uint32_t id;
	int ret;

	SHA1Init(&digest);
	SHA1Update(&digest, tex, strlen(tex));
	SHA1Final(md, &digest);

	hexdumpz(amd, md, 20, true);

	id = atomic_inc(&nonce);

	snprintf(finalpath, FILENAME_MAX, "math/%s.png", amd);
	snprintf(texpath, FILENAME_MAX, "%s/blahg_math_%s_%d.tex", TEX_TMP_DIR, amd, id);
	snprintf(logpath, FILENAME_MAX, "%s/blahg_math_%s_%d.log", TEX_TMP_DIR, amd, id);
	snprintf(auxpath, FILENAME_MAX, "%s/blahg_math_%s_%d.aux", TEX_TMP_DIR, amd, id);
	snprintf(dvipath, FILENAME_MAX, "%s/blahg_math_%s_%d.dvi", TEX_TMP_DIR, amd, id);
	snprintf(pspath,  FILENAME_MAX, "%s/blahg_math_%s_%d.ps",  TEX_TMP_DIR, amd, id);
	snprintf(pngpath, FILENAME_MAX, "%s/blahg_math_%s_%d.png", TEX_TMP_DIR, amd, id);

	unlink(texpath);
	unlink(logpath);
	unlink(auxpath);
	unlink(dvipath);
	unlink(pspath);
	unlink(pngpath);

	ret = xstat(finalpath, &statbuf);
	if (!ret)
		goto out;

	ret = __render_math(tex, amd, finalpath, texpath, dvipath, pspath,
			    pngpath);

	xunlink(texpath);
	xunlink(logpath);
	xunlink(auxpath);
	xunlink(dvipath);
	xunlink(pspath);
	xunlink(pngpath);

	if (ret) {
		str_putref(val);

		return str_cat(3, STR_DUP("<span>Math Error ("),
			       STR_DUP(xstrerror(ret)),
			       STR_DUP(")</span>"));
	}

out:
	return str_cat(5, STR_DUP("<img src=\""), STR_DUP(finalpath),
		       STR_DUP("\" alt=\"$"), val, STR_DUP("$\" />"));
}

static struct str *door_call_render_math(struct str *val)
{
	door_arg_t params = {
		.data_ptr = (void *) str_cstr(val),
		.data_size = str_len(val) + 1,
		.desc_ptr = NULL,
		.desc_num = 0,
	};
	int ret;

	ret = door_call(doorfd, &params);
	ASSERT3S(ret, >=, 0);

	str_putref(val);

	val = STR_DUP(params.rbuf);

	return val;
}

struct str *render_math(struct str *val)
{
	if (!use_door_call)
		return do_render_math(val);

	return door_call_render_math(val);
}

void init_math(bool daemonized)
{
	if (use_door_call && !daemonized) {
		close(doorfd);
	} else if (!use_door_call && daemonized) {
		doorfd = open(MATHD_DOOR_PATH, O_RDWR);
		ASSERT3S(doorfd, >=, 0);
	}

	use_door_call = daemonized;
}
