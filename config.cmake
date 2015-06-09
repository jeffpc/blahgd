include(CheckFunctionExists)

macro(set_default name default)
	if(NOT DEFINED ${name})
		message("-- Defaulting ${name} to ${default}")
		set(${name} ${default})
	else()
		message("-- User set   ${name} to ${${name}}")
	endif()
endmacro()

set_default(SCGI_NHELPERS		32)

set_default(HTML_INDEX_STORIES		10)
set_default(FEED_INDEX_STORIES		15)

set_default(COMMENT_MAX_DELAY		86400)	# 1 day max think time
set_default(COMMENT_MIN_DELAY		10)	# 10 secs min think time
set_default(COMMENT_CAPTCHA_A		5)	# first number to add
set_default(COMMENT_CAPTCHA_B		7)	# second number to add

set_default(DATA_DIR			"data")
set_default(DB_FILE			"${DATA_DIR}/db2")

set_default(MATHD_DOOR_PATH		"/tmp/mathd_door")

set_default(BASE_URL			"http://blahg.josefsipek.net")
set_default(BUG_BASE_URL		"http://bugs.31bits.net")
set_default(WIKI_BASE_URL		"http://en.wikipedia.org/wiki")
set_default(PHOTO_BASE_URL		"http://www.josefsipek.net/photos")

set_default(VAR_MAX_SCOPES		10)
set_default(VAR_MAX_VARS_SIZE		10)
set_default(VAR_VAL_KEY_MAXLEN		32)
set_default(VAR_NAME_MAXLEN		32)

set_default(COND_STACK_DEPTH		10)

set_default(TAGCLOUD_MIN_SIZE		6)
set_default(TAGCLOUD_MAX_SIZE		18)

set_default(LATEX_BIN			"/opt/local/bin/latex")
set_default(DVIPNG_BIN			"/opt/local/bin/dvipng")

set_default(PREVIEW_SECRET		0x1985)

configure_file(config.h.in config.h)
