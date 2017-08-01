#
# Copyright (c) 2016-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

include(CheckFunctionExists)

check_function_exists(reallocarray HAVE_REALLOCARRAY)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
find_package(jeffpc)

macro(set_default name default)
	if(NOT DEFINED ${name})
		message("-- Defaulting ${name} to ${default}")
		set(${name} ${default})
	else()
		message("-- User set   ${name} to ${${name}}")
	endif()
endmacro()

set_default(DEFAULT_SCGI_PORT		2014)
set_default(DEFAULT_SCGI_THREADS	32)

set_default(DEFAULT_HTML_INDEX_STORIES	10)
set_default(DEFAULT_FEED_INDEX_STORIES	15)

set_default(DEFAULT_COMMENT_MAX_THINK	86400)	# 1 day max think time
set_default(DEFAULT_COMMENT_MIN_THINK	10)	# 10 secs min think time

set_default(DEFAULT_DATA_DIR		"./data")
set_default(DEFAULT_WEB_DIR		"./web")

set_default(BASE_URL			"http://blahg.josefsipek.net")
set_default(BUG_BASE_URL		"http://bugs.31bits.net")
set_default(WIKI_BASE_URL		"http://en.wikipedia.org/wiki")
set_default(PHOTO_BASE_URL		"http://www.josefsipek.net/photos")

set_default(VAR_MAX_SCOPES		10)
set_default(VAR_NAME_MAXLEN		32)

set_default(COND_STACK_DEPTH		10)

set_default(DEFAULT_TAGCLOUD_MIN_SIZE	6)
set_default(DEFAULT_TAGCLOUD_MAX_SIZE	18)

set_default(DEFAULT_LATEX_BIN		"/usr/bin/latex")
set_default(DEFAULT_DVIPS_BIN		"/usr/bin/dvips")
set_default(DEFAULT_CONVERT_BIN		"/usr/bin/convert")

set_default(PREVIEW_SECRET		0x1985)

configure_file(config.h.in config.h)
