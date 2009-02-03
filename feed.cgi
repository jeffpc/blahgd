#!/bin/sh

case "$PATH_INFO" in
	/rss)
		fmt="rss"
		view="index"
		;;
	/rss2)
		fmt="rss2"
		view="index"
		;;
	/atom)
		fmt="atom"
		view="index"
		;;
	/rss/[0-9]*)
		fmt="rss"
		view="comments"
		postid="`echo $PATH_INFO | cut -d/ -f3`"
		;;
	/rss2/[0-9]*)
		fmt="rss2"
		view="comments"
		postid="`echo $PATH_INFO | cut -d/ -f3`"
		;;
	/atom/[0-9]*)
		fmt="atom"
		view="comments"
		postid="`echo $PATH_INFO | cut -d/ -f3`"
		;;
	*)
		echo "Location: /blahg-test/"
		echo ""
		exit 0
		;;
esac

case "$fmt" in
	rss)
		CONTENT_TYPE="text/xml; charset=UTF-8"
		;;
	rss2)
		CONTENT_TYPE="text/xml; charset=UTF-8"
		;;
	atom)
		CONTENT_TYPE="application/atom+xml; charset=UTF-8"
		;;
esac

. common-head.sh

##########################

last=`ls data/posts/ | sort -nr | head -1`
cat templates/header.$fmt | sed -e "s|@@LASTPUBDATE@@|`get_rss_date $fmt data/posts/$last/`|g"

cnt=0

for postid in `ls data/posts/ | sort -rn` ; do
	cnt=$(($cnt + 1))
	[ $cnt -gt 15 ] && break

	fn="data/posts/$postid/"

	cat templates/story-top.$fmt | sed -e "
s|@@POSTID@@|`basename $postid`|g
s|@@POSTDATE@@|`get_rss_date $fmt "$fn"`|g
s|@@TITLE@@|`get_xattr post_title "$fn"`|g
"

	for catname in `get_xattr post_cats "$fn" | sed -e 's|,| |g'` ; do
		cat templates/story-cat-item.rss | sed -e "
s|@@CATNAME@@|$catname|g
	"
	done

	cat templates/story-middle-desc.$fmt | sed -e "
s|@@POSTTIME@@|`get_rss_date $fmt "$fn"`|g
	"
	cat_post_preview "$fn/post.txt"
	cat templates/story-bottom-desc.$fmt

	cat templates/story-middle.$fmt | sed -e "
s|@@POSTTIME@@|`get_rss_date $fmt "$fn"`|g
s|@@POSTID@@|`basename $postid`|g
	"
	cat_post "$fn/post.txt"
	cat templates/story-bottom.$fmt
done

cat templates/footer.$fmt
