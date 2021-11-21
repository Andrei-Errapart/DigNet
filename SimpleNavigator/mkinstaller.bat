call cleanbuild.bat
cd src
..\..\bin\BuildNumber.exe http://194.204.26.104/~kalle/getBuild.php %PROJECT_NAME%
if %ERRORLEVEL% == 0 goto good 
cd ..
echo Problem generating build number
goto problem

:good
cd ..
vcbuild.exe /logfile:BuildLog.txt %PROJECT_NAME%.sln "Release|Win32" %BUILD_FLAGS%
type BuildLog.txt
makensis Installer_GpsViewer.nsi

:problem
