@echo Building GpsBase
call make_buildinfo.bat logToServer application=GpsBase

rem Run java compiler
ant -buildfile build_gpsbase.xml package

