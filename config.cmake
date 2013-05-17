include(CheckFunctionExists)

check_function_exists(flock HAVE_FLOCK)

macro(set_default name default)
	if(NOT DEFINED ${name})
		message("-- Defaulting ${name} to ${default}")
		set(${name} ${default})
	else()
		message("-- User set   ${name} to ${${name}}")
	endif()
endmacro()

set_default(HTML_INDEX_STORIES		10)
set_default(HTML_ARCHIVE_STORIES	10)
set_default(HTML_CATEGORY_STORIES	10)
set_default(HTML_TAG_STORIES		10)
set_default(FEED_INDEX_STORIES		15)

set_default(COMMENT_MAX_DELAY		3600)	# 60 mins max think time
set_default(COMMENT_MIN_DELAY		10)	# 10 secs min think time

set_default(DB_FILE			"data/db2")

set_default(BASE_URL			"http://blahg.josefsipek.net")
set_default(BUG_BASE_URL		"http://bugs.31bits.net")
set_default(WIKI_BASE_URL		"http://en.wikipedia.org/wiki")
set_default(PHOTO_BASE_URL		"http://www.josefsipek.net/photos")

set_default(VAR_MAX_SCOPES		10)
set_default(VAR_MAX_VAR_NAME		32)
set_default(VAR_MAX_ARRAY_SIZE		200)
set_default(VAR_MAX_VARS_SIZE		10)

set_default(TAGCLOUD_MIN_SIZE		6)
set_default(TAGCLOUD_MAX_SIZE		18)

set_default(LATEX_BIN			"/opt/local/bin/latex")
set_default(DVIPNG_BIN			"/opt/local/bin/dvipng")

set_default(PREVIEW_SECRET		0x1985)

configure_file(config.h.in config.h)
