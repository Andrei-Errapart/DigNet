#/bin/sh

# expects parameters -s subject address1 address2 -m message_body
# subject and body can be in doublequotes

CMD="mail"
IS_BODY=0
IS_SUBJECT=0
for i in "$@" ; do
	# Subject
	if [ "$i"  == "-s" ]; then
		CMD="$CMD -s "
		IS_SUBJECT=1
		continue
	fi

	#message body
	if [ "$i" == "-m" ]; then 
		IS_BODY=1
		cat /dev/null > tmp.txt
		continue
	fi
	
	#write body to file
	if [ "$IS_BODY" == "1" ]; then
		echo $i >> tmp.txt
		continue
	fi

	#properly append subject
	if [ "$IS_SUBJECT" == "1" ]; then 
		CMD="$CMD \"$i\""
		IS_SUBJECT=0
		continue
	fi
	

	#everything else is simply appended
	CMD="$CMD $i"
done
#echo $CMD  > cmd.txt
eval "$CMD" < tmp.txt
rm tmp.txt
