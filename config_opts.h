#ifndef __CONFIG_OPTS_H
#define __CONFIG_OPTS_H

#define HTML_INDEX_STORIES		10
#define HTML_ARCHIVE_STORIES		10
#define HTML_CATEGORY_STORIES		10
#define HTML_TAG_STORIES		10
#define FEED_INDEX_STORIES		15

#define COMMENT_MAX_DELAY		(3600)	/* 60 mins max think time */
#define COMMENT_MIN_DELAY		(10)	/* 10 secs min think time */

#define DB_FILE				"data/db"

#define ERROR_LOG_FILE			"data/logs/comment_error.log"

#define BASE_URL			"http://blahg.josefsipek.net"
#define BUG_BASE_URL			"http://bugs.31bits.net"
#define WIKI_BASE_URL			"http://en.wikipedia.org/wiki"
#define PHOTO_BASE_URL			"http://www.josefsipek.net/photos"

#define VAR_MAX_SCOPES			10
#define VAR_MAX_ARRAY_SIZE		100
#define VAR_MAX_VARS_SIZE		10

#define TAGCLOUD_MIN_SIZE		6
#define TAGCLOUD_MAX_SIZE		20

#endif

