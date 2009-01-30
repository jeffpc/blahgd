cat templates/footer.html

cat templates/sidebar-top.html
for catname in `find data/by-category/ -mindepth 1 -type d | sort` ; do
	shortcatname=`echo $catname | sed -e 's,data/by-category/,,'`

	catdesc=`get_xattr category_desc "$catname"`
	[ -z "$catdesc" ] && catdesc="View all posts filed under $shortcatname"

	cat templates/sidebar-cat-item.html | sed -e "
s|@@CATNAME@@|$shortcatname|g
s|@@CATDESC@@|$catdesc|g
"
done
cat templates/sidebar-middle.html
for archname in `ls data/by-month/ | sort -r` ; do
	archname=`basename $archname`

	cat templates/sidebar-archive-item.html | sed -e "
s|@@ARCHNAME@@|$archname|g
s|@@ARCHDESC@@|`arch_date $archname`|g
"
done

cat templates/sidebar-bottom.html
