set FILES=images/workmark_dredged.bmp ^
	images/workmark_ok.bmp		^
	images/workmark_problem.bmp	^
	images/zoom_in.gif		^
	images/zoom_out.gif		^
	images/workmark_icon_dredged.gif	^
	images/workmark_icon_none.gif		^
	images/workmark_icon_ok.gif		^
	images/workmark_icon_problem.gif	^
	images/icon_Hand.gif			^
	images/icon_Pointer.gif

..\..\base\bin\reswrap -i -z -n resources -o src/resources.h %FILES%
echo #include "resources.h" > src/resources.cxx
..\..\base\bin\reswrap -n resources %FILES% >> src/resources.cxx

