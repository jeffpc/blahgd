#!/bin/sh

echo "Content-Type: text/plain"
echo ""

cat <<DONE
There has been an error processing your request.  The site administrator has
been informed.

Sorry for the inconvenience.

Josef 'Jeff' Sipek.
DONE

case "$SCRIPT_URL" in
	/blahg/wp-*.php)
		exit 0
		;;
esac

(
echo "----"
echo "SCRIPT_URL:   $SCRIPT_URL"
echo "PATH_INFO:    $PATH_INFO"
echo "QUERY_STRING: $QUERY_STRING"
echo "----"
env
echo "----"
cat
) | mail -s "Faulty blahg request" jeffpc@josefsipek.net
