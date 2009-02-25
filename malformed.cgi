#!/bin/sh

echo "Content-Type: text/plain"
echo ""

echo "$PATH_INFO"
echo "$QUERY_STRING"
echo "----"
env
echo "----"
cat
