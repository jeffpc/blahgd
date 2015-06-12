/*
 * Copyright (c) 2014-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include "str.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <door.h>

#include <openssl/sha.h>

#include "utils.h"
#include "math.h"
#include "error.h"
#include "atomic.h"

static bool use_door_call;
static int doorfd;

static inline char asciify(unsigned char c)
{
	return (c < 10) ? (c + '0') : (c - 10 + 'A');
}

static void hexdump(char *a, unsigned char *b, int len)
{
	int i;

	for(i=0; i<len; i++) {
		a[i*2]   = asciify(b[i] >> 4);
		a[i*2+1] = asciify(b[i] & 15);
	}
	a[i*2] = '\0';
}

#define TEX_TMP_DIR "/tmp"
static int __render_math(char *tex, char *md, char *dstpath, char *texpath,
			 char *dvipath, char *pngpath)
{
	char cmd[3*FILENAME_MAX];
	char *pwd;
	FILE *f;

	pwd = getcwd(NULL, FILENAME_MAX);
	if (!pwd)
		goto err;

	f = fopen(texpath, "w");
	if (!f)
		goto err;

	chdir(TEX_TMP_DIR);

	fprintf(f, "\\documentclass{article}\n");
	fprintf(f, "%% add additional packages here\n");
	fprintf(f, "\\usepackage{amsmath}\n");
	fprintf(f, "\\usepackage{bm}\n");
	fprintf(f, "\\pagestyle{empty}\n");
	fprintf(f, "\\begin{document}\n");
	fprintf(f, "\\begin{equation*}\n");
	fprintf(f, "\\large\n");
	fprintf(f, tex);
	fprintf(f, "\\end{equation*}\n");
	fprintf(f, "\\end{document}\n");
	fclose(f);

	snprintf(cmd, sizeof(cmd), LATEX_BIN " --interaction=nonstopmode %s > /dev/null", texpath);
	LOG("math cmd: '%s'", cmd);
	if (system(cmd))
		goto err_chdir;

	snprintf(cmd, sizeof(cmd), DVIPNG_BIN " -T tight -x 1200 -z 9 "
			"-bg Transparent -o %s %s > /dev/null", pngpath, dvipath);
	LOG("math cmd: '%s'", cmd);
	if (system(cmd))
		goto err_chdir;

	chdir(pwd);

	snprintf(cmd, sizeof(cmd), "cp %s %s", pngpath, dstpath);
	LOG("math cmd: '%s'", cmd);
	if (system(cmd))
		goto err;

	free(pwd);

	return 0;

err_chdir:
	chdir(pwd);

err:
	free(pwd);

	return 1;
}

static struct str *do_render_math(struct str *val)
{
	static atomic_t nonce;

	char finalpath[FILENAME_MAX];
	char texpath[FILENAME_MAX];
	char logpath[FILENAME_MAX];
	char auxpath[FILENAME_MAX];
	char dvipath[FILENAME_MAX];
	char pngpath[FILENAME_MAX];

	char *tex = val->str;
	struct stat statbuf;
	unsigned char md[20];
	char amd[41];
	uint32_t id;
	int ret;

	SHA1((const unsigned char*) tex, strlen(tex), md);
	hexdump(amd, md, 20);

	id = atomic_inc(&nonce);

	snprintf(finalpath, FILENAME_MAX, "math/%s.png", amd);
	snprintf(texpath, FILENAME_MAX, "%s/blahg_math_%s_%d.tex", TEX_TMP_DIR, amd, id);
	snprintf(logpath, FILENAME_MAX, "%s/blahg_math_%s_%d.log", TEX_TMP_DIR, amd, id);
	snprintf(auxpath, FILENAME_MAX, "%s/blahg_math_%s_%d.aux", TEX_TMP_DIR, amd, id);
	snprintf(dvipath, FILENAME_MAX, "%s/blahg_math_%s_%d.dvi", TEX_TMP_DIR, amd, id);
	snprintf(pngpath, FILENAME_MAX, "%s/blahg_math_%s_%d.png", TEX_TMP_DIR, amd, id);

	unlink(texpath);
	unlink(logpath);
	unlink(auxpath);
	unlink(dvipath);
	unlink(pngpath);

	ret = stat(finalpath, &statbuf);
	if (!ret)
		goto out;

	ret = __render_math(tex, amd, finalpath, texpath, dvipath, pngpath);

	unlink(texpath);
	unlink(logpath);
	unlink(auxpath);
	unlink(dvipath);
	unlink(pngpath);

	if (ret) {
		str_putref(val);

		return str_cat3(STR_DUP("<span>Math Error ("),
		                STR_DUP(strerror(errno)),
			        STR_DUP(")</span>"));
	}

out:
	return str_cat5(STR_DUP("<img src=\""), STR_DUP(finalpath),
	                STR_DUP("\" alt=\"$"), val, STR_DUP("$\" />"));
}

static struct str *door_call_render_math(struct str *val)
{
	door_arg_t params = {
		.data_ptr = val->str,
		.data_size = strlen(val->str) + 1,
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
