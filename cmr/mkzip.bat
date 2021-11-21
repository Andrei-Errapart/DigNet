vcbuild.exe cmr.sln
copy /y ..\doc\ModemBox.doc .
copy /y ..\doc\CMR_Modified.doc .
copy /y Release\cmr.exe .
del /q ModemBox_spec.zip
zip ModemBox_spec.zip		^
	ModemBox.doc			^
	CMR_Modified.doc		^
	cmr.h				^
	cmr.c				^
	main.cxx			^
	cmr.sln				^
	cmr.vcproj			^
	cmr.exe				^
	test_to_modem_and_gps.txt	^
	test_to_modem.txt		^
	test_to_gps.txt			^
	test_ping.txt			^
	test_music.mp3			^
	test_modem_bt_big.txt		^
	test_gpsline1.txt		^
	test_gps2_bt.txt		^
	test_gps2_a_bt.txt		^
	test_gps1_bt.txt		^
	test_gps1_a_bt.txt		^
	test_gps_abuse2.txt		^
	test_gps_abuse.txt

