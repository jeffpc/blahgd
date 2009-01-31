#!/bin/sh

. common-head.sh

##########################

catid=`echo "$PATH_INFO" | sed -e '
s,^/,,

s,^25$,astronomy,
s,^42$,documentation,
s,^8$,events,
s,^7$,events/ols-2005,
s,^20$,events/ols-2006,
s,^29$,events/ols-2007,
s,^40$,events/ols-2008,
s,^36$,events/sc-07,
s,^21$,fsl,
s,^22$,fsl/unionfs,
s,^11$,humor,
s,^39$,hvf,
s,^17$,legal,
s,^1$,miscellaneous,
s,^10$,movies,
s,^34$,music,
s,^24$,open-source,
s,^31$,photography,
s,^6$,programming,
s,^2$,programming/kernel,
s,^35$,programming/mainframes,
s,^26$,programming/vcs,
s,^27$,programming/vcs/git,
s,^30$,programming/vcs/guilt,
s,^28$,programming/vcs/mercurial,
s,^5$,random,
s,^9$,rants,
s,^3$,school,
s,^13$,star-trek,
s,^19$,star-trek/enterprise,
s,^15$,star-trek/the-next-generation,
s,^16$,star-trek/the-original-series,
s,^18$,star-trek/voyager,
s,^43$,stargate,
s,^23$,stargate/sg-1,
s,^41$,sysadmin,
s,^4$,work,
'`

cat templates/header.html | sed -e "s|@@TITLE@@|$catid|"

for postid in `ls data/by-category/$catid/ | sort -r` ; do
	[ -d "data/by-category/$catid/$postid" ] && continue

	fn="data/posts/$postid/"

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

	cat_post "$fn/post.txt"

	cat templates/story-bottom.html
done

##########################

. common-foot.sh
