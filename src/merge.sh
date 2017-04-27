#!/bin/bash

RAWFILE=$1
OTAFILE=$2
NEWFILE=$3
#echo $1 $2 $3

let RAWFILESIZE=256*1024
let NEWFILESIZE="$RAWFILESIZE"*2

raw_size=$(stat -c %s $RAWFILE)
if [ "$raw_size" != "$RAWFILESIZE" ];then
	echo "The file $RAWFILE size is not equal 256k."
	exit 1
fi

cp $RAWFILE $NEWFILE

#write NVRM 
echo -e -n "\x`printf %x 0x4E`" >>$NEWFILE
echo -e -n "\x`printf %x 0x56`" >>$NEWFILE
echo -e -n "\x`printf %x 0x52`" >>$NEWFILE
echo -e -n "\x`printf %x 0x4D`" >>$NEWFILE

#write OTAFILE
cat $OTAFILE >> $NEWFILE
new_size=$(stat -c %s $NEWFILE)
if [ "$new_size" -gt "$NEWFILESIZE" ];then
	echo "The file $NEWFILE size is too big."
	rm -f $NEWFILE
	exit 2
fi

#fill 0xFF
#let left_size="$NEWFILESIZE"-"$new_size"
#echo $left_size
#for ((i = 0;i < "$left_size";i++))
#do
#echo -e -n "\x`printf %x 0xFF`" >> $NEWFILE
#done
echo "merge success."
exit 0

