cat templates/sidebar-top.html
for catname in `find data/by-category/ -mindepth 1 -type d | sed -e 's,data/by-category/,,' | sort` ; do
	cat templates/sidebar-cat-item.html | sed -e "
s|@@CATNAME@@|$catname|g
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
cat templates/footer.html

