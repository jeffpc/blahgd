#!/bin/sh

. common-head.sh

##########################

catid=`echo "$PATH_INFO" | sed -e 's,^/,,'`

cat templates/header.html | sed -e 's|@@TITLE@@|$catid|'

for postid in `ls data/by-category/$catid/ | sort -r` ; do
	[ -d "data/by-category/$catid/$postid" ] && continue

	fn="data/posts/$postid/post.txt"

	cat templates/story-top.html | sed -e "
s|@@POSTID@@|`basename $postid`|g
s|@@POSTDATE@@|`get_date "$fn"`|g
s|@@TITLE@@|`get_xattr post_title "$fn"`|g
"

	for catname in `get_xattr post_cats "$fn" | sed -e 's|,| |g'` ; do
		cat templates/story-cat-item.html | sed -e "
s|@@CATNAME@@|$catname|g
	"
	done

	cat templates/story-middle.html | sed -e "
s|@@POSTTIME@@|`get_time "$fn"`|g
	"

	cat_post "$fn"

	cat templates/story-bottom.html
done

##########################

. common-foot.sh
