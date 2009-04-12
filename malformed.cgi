#!/bin/sh

echo "Content-Type: text/plain"
echo ""

cat <<DONE
There has been an error processing your request.  The site administrator has
been informed.

Sorry for the inconvenience.

Josef 'Jeff' Sipek.
DONE

(
echo "----"
echo "PATH_INFO:    $PATH_INFO"
echo "QUERY_STRING: $QUERY_STRING"
echo "----"
env
echo "----"
cat
) | mail -s "Faulty blahg request" jeffpc@josefsipek.net
