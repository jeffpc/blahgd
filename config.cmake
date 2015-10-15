include(CheckFunctionExists)

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

set_default(HTML_INDEX_STORIES		10)
set_default(FEED_INDEX_STORIES		15)

set_default(DEFAULT_COMMENT_MAX_THINK	86400)	# 1 day max think time
set_default(DEFAULT_COMMENT_MIN_THINK	10)	# 10 secs min think time

set_default(DATA_DIR			"data")

set_default(MATHD_DOOR_PATH		"/tmp/mathd_door")

set_default(BASE_URL			"http://blahg.josefsipek.net")
set_default(BUG_BASE_URL		"http://bugs.31bits.net")
set_default(WIKI_BASE_URL		"http://en.wikipedia.org/wiki")
set_default(PHOTO_BASE_URL		"http://www.josefsipek.net/photos")

set_default(VAR_MAX_SCOPES		10)
set_default(VAR_NAME_MAXLEN		32)

set_default(COND_STACK_DEPTH		10)

set_default(TAGCLOUD_MIN_SIZE		6)
set_default(TAGCLOUD_MAX_SIZE		18)

set_default(DEFAULT_LATEX_BIN		"/usr/bin/latex")
set_default(DEFAULT_DVIPNG_BIN		"/usr/bin/dvipng")

set_default(PREVIEW_SECRET		0x1985)

configure_file(config.h.in config.h)
