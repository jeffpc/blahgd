#!/bin/sh

echo "Content-Type: text/plain"
echo ""

cat <<DONE
There has been an error processing your request.  Please send the following
information to jeffpc@josefsipek.net to facilitate fixing of this issue.

Thank you.

Josef 'Jeff' Sipek.
DONE

echo "----"
echo "$PATH_INFO"
echo "$QUERY_STRING"
echo "----"
env | sed -n -e '
/^PWD/ { n }
/^SCRIPT_FILENAME/ { n }
/^DOCUMENT_ROOT/ { n }
p
'
echo "----"
cat
