/*
 * Copyright (c) 2014-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <sha1.h>

#include <jeffpc/str.h>
#include <jeffpc/synch.h>
#include <jeffpc/error.h>
#include <jeffpc/atomic.h>
#include <jeffpc/hexdump.h>
#include <jeffpc/io.h>

#include "utils.h"
#include "math.h"
#include "config.h"
#include "debug.h"

/*
 * There are three different ways we get called to render a math expression.
 *
 * (1) render_math() without an init_math() call - this is done by the test,
 *     the math code can chdir as necessary
 * (2) render_math() after an init_math() call - this is done by the
 *     daemon's main process, the math input needs to be sent over the
 *     pipefd to the math worker process, no math is actually rendered by
 *     the main process
 * (3) render_math_processor() reads in a string - this is the math worker
 *     process doing its job; it can chdir as needed
 */

static struct lock lock;

static int pipefd = -1;

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

static struct str *render_math_real(struct str *val)
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

		return str_cat(3, STATIC_STR("<span>Math Error ("),
			       STR_ALLOC_STATIC(xstrerror(ret)),
			       STATIC_STR(")</span>"));
	}

out:
	return str_cat(5, STATIC_STR("<img src=\""), STR_DUP(finalpath),
		       STATIC_STR("\" alt=\"$"), val, STATIC_STR("$\" />"));
}

static int consume(int fd, size_t len)
{
	char buf[1024];

	while (len) {
		int ret;

		ret = xread(fd, buf, MIN(len, sizeof(buf)));
		if (ret)
			return ret;

		len -= MIN(len, sizeof(buf));
	}

	return 0;
}

static struct str *render_math_send(struct str *val)
{
	size_t len;
	char *buf;
	int ret;

	VERIFY3S(pipefd, !=, -1);

	len = str_len(val);

	MXLOCK(&lock);

	/* write the input length */
	ret = xwrite(pipefd, &len, sizeof(len));
	if (ret)
		goto failed;

	/* write the input */
	ret = xwrite(pipefd, str_cstr(val), len);
	if (ret)
		goto failed;

	str_putref(val);

	/* read the output length */
	ret = xread(pipefd, &len, sizeof(len));
	if (ret)
		goto failed;

	/* allocate output buffer */
	buf = malloc(len + 1);

	/* either read or just consume the output */
	if (!buf)
		ret = consume(pipefd, len);
	else
		ret = xread(pipefd, buf, len);

	if (!buf) {
		ret = -ENOMEM;
		goto failed;
	}

	if (ret)
		goto failed;

	MXUNLOCK(&lock);

	/* nul-terminate the output */
	buf[len] = '\0';

	return STR_ALLOC(buf);

failed:
	/*
	 * We failed because of a partial read or write.  This is *very*
	 * bad.  We could either invent a complicated re-synchronization
	 * protocol, or use a much bigger hammer and assert.  In theory, we
	 * could try to close the pipe, kill the worker, and start up a new
	 * one.  But in practice, we already dropped privs and there is no
	 * way for us to fork again.  So, we just panic.
	 */
	panic("Failed to send/receive via math worker pipe: %s",
	      xstrerror(ret));
}

struct str *render_math(struct str *val)
{
	if (pipefd != -1)
		return render_math_send(val);

	return render_math_real(val);
}

/* run "server" */
int render_math_processor(int fd)
{
	int ret;

	/*
	 * Process requests until we get the first error reading or writing
	 * the pipe.  At that point, just break out of the loop and return.
	 * This will cause the worker to terminate, and the parent will
	 * panic (see comment in render_math).
	 */

	for (;;) {
		struct str *out;
		size_t len;
		char *in;

		/* read the input length */
		ret = xread(fd, &len, sizeof(len));
		if (ret)
			break;

		in = malloc(len + 1);
		if (!in) {
			ret = -ENOMEM;
			break;
		}

		/* read the input */
		ret = xread(fd, in, len);
		if (ret)
			break;

		in[len] = '\0';

		out = render_math_real(STR_ALLOC(in));

		len = str_len(out);

		/* write the output length */
		ret = xwrite(fd, &len, sizeof(len));
		if (ret)
			break;

		/* write the output */
		ret = xwrite(fd, str_cstr(out), len);
		if (ret)
			break;

		str_putref(out);
	}

	xclose(fd);

	return ret;
}

/* initialize "client" */
void init_math(int fd)
{
	ASSERT3S(pipefd, ==, -1);
	ASSERT3S(fd, !=, -1);

	pipefd = fd;

	MXINIT(&lock);
}
