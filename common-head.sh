# usage: get_xattr <attrname> <filename>
get_xattr()
{
	getfattr --only-values -n "user.$1" "$2" 2> /dev/null
}

# usage: get_date <filename>
get_date()
{
	date "+%B %e, %Y" -d "`get_xattr post_time "$1"`"
}

# usage: get_time <filename>
get_time()
{
	date "+%H:%M" -d "`get_xattr post_time "$1"`"
}

# usage: str_month <MM>
str_month()
{
	case "$1" in
		01) echo "january" ;;
		02) echo "february" ;;
		03) echo "march" ;;
		04) echo "april" ;;
		05) echo "may" ;;
		06) echo "june" ;;
		07) echo "july" ;;
		08) echo "august" ;;
		09) echo "september" ;;
		10) echo "october" ;;
		11) echo "november" ;;
		12) echo "december" ;;
		*)  echo "unknown" ;;
	esac
}

# usage: arch_date <YYYYMM>
arch_date()
{
	m=`expr substr "$1" 5 2`
	echo "`str_month $m` `expr substr "$1" 1 4`"
}

# usage: arch_title_date <YYYYMM>
arch_title_date()
{
	m=`expr substr "$1" 5 2`
	echo "`expr substr "$1" 1 4` \&raquo; `str_month $m`"
}

# usage: cat_post <filename>
cat_post()
{
#/^<!--more-->$/ { exit; }
#{ print "<br/>" }
	cat "$1" | awk '
BEGIN { par=1 }
(par) { print "<p>"; par=0 }
{ print $0 }
/^$/ { print "</p>"; par=1; next }
END { if (!par) print "</p>" }
'
}

echo "Content-type: text/html"
echo ""
