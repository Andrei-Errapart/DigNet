set CONFIGFILE=lib\SimpleNavigator\SimpleNavigator.ini
set POINTSFILE=lib\points_big.ipt
set OUTFILE=SimpleNavigator.exe
set PROJECTFILE=Project.dxf
set PROJECT_NAME=SimpleNavigator
set BUILD_FLAGS=/msbuild:"IS_SIMPLE_NAVIGATOR"
call mkinstaller.bat

