#!/bin/sh
# Author: bwaggle

if test $# -ne 2
then
	echo "failed: must enter 2 arguments"
	exit 1
fi	

writefile="$1"
writestr="$2"

mkdir -p "$(dirname "$writefile")"
echo "$writestr" > "$writefile"

if [ $? -ne 0 ]
then
  echo "Error: Failed to create the file"
  exit 1
fi

exit 0

