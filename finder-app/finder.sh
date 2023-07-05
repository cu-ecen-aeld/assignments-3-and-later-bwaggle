#!/bin/sh

filesdir=$1
searchstr=$2

# Check for correct count of arguments

if test $# -ne 2
then
	echo "failed: must enter 2 arguments"
	exit 1
fi	


if test ! -d "$filesdir" 
then
	echo "failed: expected a directory as the first argument"
	exit 1
fi


file_count=$(find "$filesdir" -type f | wc -l)
line_count=$(grep -r "$searchstr" $filesdir | wc -l)

echo "The number of files are $file_count and the number of matching lines are $line_count"
