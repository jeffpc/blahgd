# usage: get_xattr <attrname> <filename>
get_xattr()
{
	cat "$2_xattrs/$1" 2> /dev/null
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

# usage: arch_date <YYYYMM>
arch_date()
{
	y=`expr substr "$1" 1 4`

	case `expr substr "$1" 5 2` in
		01) m="january" ;;
		02) m="february" ;;
		03) m="march" ;;
		04) m="april" ;;
		05) m="may" ;;
		06) m="june" ;;
		07) m="july" ;;
		08) m="august" ;;
		09) m="september" ;;
		10) m="october" ;;
		11) m="november" ;;
		12) m="december" ;;
		*) m="unknown" ;;
	esac

	echo "$m $y"
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
