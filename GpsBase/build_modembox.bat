@echo Building GpsBase
call make_buildinfo.bat +logToServer application=ModemBox

rem Run java compiler
ant -buildfile build_modembox.xml package

