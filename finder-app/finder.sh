#!/bin/sh

if [ $# -eq 2 ]
then
	echo "Pass"
	if [ -d "$1" ] 
	then
		X=$(find $1 -type f | wc -l)
		Y=$(grep -r $2 $1 | wc -l)
		echo "The number of files are ${X} and the number of matching lines are ${Y}"
	else
		echo "$1 isn't exist"
		exit 1
	fi
	exit 0
else
	echo "Statements if any of the parameters above were not specifed"
	exit 1
fi


