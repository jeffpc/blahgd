/*
 * Copyright (c) 2010-2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fsm.h"
#include "decode.h"

#define UD_NORMAL	0
#define UD_DECODE1	1
#define UD_DECODE2	2

struct ud_state {
	char *in;
	int len;
	int ioff;
	char *out;
	int ooff;

	int tmp;
};

static int copy_char(struct fsm *fsm, int inp)
{
	struct ud_state *uds = fsm->private;

	uds->out[uds->ooff++] = uds->in[uds->ioff++];

	return 0;
}

static int space_char(struct fsm *fsm, int inp)
{
	struct ud_state *uds = fsm->private;

	uds->out[uds->ooff++] = ' ';
	uds->ioff++;

	return 0;
}

static int escape_char(struct fsm *fsm, int inp)
{
	struct ud_state *uds = fsm->private;

	uds->tmp = 0;
	uds->ioff++;

	fsm->state = UD_DECODE1;

	return 0;
}

static int err_char(struct fsm *fsm, int inp)
{
	struct ud_state *uds = fsm->private;

	uds->out[uds->ooff++] = '%';
	if (fsm->state == UD_DECODE2) {
		if (uds->tmp >= 10)
			uds->out[uds->ooff++] = uds->tmp + 'A' - 10;
		else
			uds->out[uds->ooff++] = uds->tmp + '0';
	}
	uds->ioff++;

	fsm->state = UD_NORMAL;

	return 0;
}

#define ESCAPE_CHAR(name, base, addition)			\
	static int name(struct fsm *fsm, int inp)			\
	{							\
		struct ud_state *uds = fsm->private;		\
		char tmp = uds->in[uds->ioff++];		\
		uds->tmp = (uds->tmp << 4) | (tmp - base + addition);	\
		if (fsm->state == UD_DECODE1)			\
			fsm->state = UD_DECODE2;		\
		else {						\
			fsm->state = UD_NORMAL;			\
			uds->out[uds->ooff++] = uds->tmp;	\
		}						\
		return 0;					\
	}

ESCAPE_CHAR(hex_char_digit, '0', 0);
ESCAPE_CHAR(hex_char_lower, 'a', 10);
ESCAPE_CHAR(hex_char_upper, 'A', 10);

static fsm_table_entry_t normal_table[256] = {
	['%'] = escape_char,
	['+'] = space_char,
};

static fsm_table_entry_t decode_table[256] = {
	['0'] = hex_char_digit,
	['1'] = hex_char_digit,
	['2'] = hex_char_digit,
	['3'] = hex_char_digit,
	['4'] = hex_char_digit,
	['5'] = hex_char_digit,
	['6'] = hex_char_digit,
	['7'] = hex_char_digit,
	['8'] = hex_char_digit,
	['9'] = hex_char_digit,
	['A'] = hex_char_upper,
	['B'] = hex_char_upper,
	['C'] = hex_char_upper,
	['D'] = hex_char_upper,
	['E'] = hex_char_upper,
	['F'] = hex_char_upper,
	['a'] = hex_char_lower,
	['b'] = hex_char_lower,
	['c'] = hex_char_lower,
	['d'] = hex_char_lower,
	['e'] = hex_char_lower,
	['f'] = hex_char_lower,
};

static struct fsm_state ud_state_table[4] = {
	[UD_NORMAL] = {
		.action_table = normal_table,
		.default_action = copy_char,
	},
	[UD_DECODE1] = {
		.action_table = decode_table,
		.default_action = err_char,
	},
	[UD_DECODE2] = {
		.action_table = decode_table,
		.default_action = err_char,
	},
};

static int ud_inp(struct fsm *fsm)
{
	struct ud_state *uds = fsm->private;
	int ret;

	if (uds->ioff >= uds->len)
		return -1;

	ret = uds->in[uds->ioff];

	return ret ? ret : -1;
}

void urldecode(char *in, int len, char *out)
{
	struct ud_state ud_state = {
		.in = in,
		.len = len,
		.ioff = 0,
		.out = out,
		.ooff = 0,
	};

	struct fsm fsm = {
		.state		= UD_NORMAL,
		.state_table	= ud_state_table,
		.inp_fxn	= ud_inp,
		.private	= &ud_state,
	};

	fsm_process(&fsm);

	out[ud_state.ooff] = '\0';
}
