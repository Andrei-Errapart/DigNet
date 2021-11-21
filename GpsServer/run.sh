#! /bin/sh
function sendMail {
	sh mail.sh -s "Server crashis" kalle.last@gmail.com -m "Server crashis `date '+%m.%d.%y %H:%M:%S'`."
}

while true
do
	echo "Starting GpsServer..."
	./GpsServer
	CRASHDIR=`date '+%m.%d.%y_%H-%M-%S'`
	mkdir $CRASHDIR
	mv core $CRASHDIR
	mv lastpacket.raw $CRASHDIR
	mv packets.raw $CRASHDIR
	sendMail
	echo "Pause 5 seconds..."
	sleep 5
done

