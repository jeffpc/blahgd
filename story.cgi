#!/bin/sh

. common-head.sh

##########################

postid=`echo "$PATH_INFO" | sed -e 's,^/,,'`

fn="data/posts/$postid/post.txt"

title=`get_xattr post_title "$fn"`
postdate=`get_date "$fn"`
posttime=`get_time "$fn"`

(
cat templates/header.html
cat templates/story-top.html
) | sed -e "
s|@@POSTID@@|$postid|g
s|@@POSTDATE@@|$postdate|g
s|@@TITLE@@|$title|g
"

for catname in `get_xattr post_cats "$fn" | sed -e 's|,| |g'` ; do
	cat templates/story-cat-item.html | sed -e "
s|@@CATNAME@@|$catname|g
"
done

cat templates/story-middle.html | sed -e "
s|@@POSTTIME@@|$posttime|g
"

cat_post "$fn"

cat templates/story-bottom.html

##########################

. common-foot.sh
