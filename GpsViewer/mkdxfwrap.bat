cd ..\..\base\DxfWrap
vcbuild.exe /logfile:BuildLog.txt DxfWrap.sln "Release|Win32"
cd ..\..\DigNet\GpsViewer
..\..\base\DxfWrap\Release\DxfWrap.exe src/dxfwrap.h src/dxfwrap.cxx doc/velsa1.dxf doc/tartu_barge.dxf doc/tartu.dxf doc/wille.dxf doc/omedu_barge.dxf
