; GpsViewer installation script.
;
;--------------------------------

!ifndef VERSION
!define VERSION "DigNet"
!endif

Icon "hydro32.ico"
UninstallIcon "hydro32.ico"

!define NAME 		"$%PROJECT_NAME%"
!define FULLNAME	"${NAME} ${VERSION}"
!define EXEFILE		"GpsViewer.exe"
!define OUTEXE		"$%PROJECT_NAME%.exe"

!define	SMDIR		"$SMPROGRAMS\$%PROJECT_NAME%"


; The name of the installer
Name "${FULLNAME}"

; The file to write
OutFile "$%OUTFILE%"

; The default installation directory
InstallDir "$PROGRAMFILES\${FULLNAME}"

BrandingText "© 2007 Errapart Engineering Ltd"


; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\${FULLNAME}" "Install_Dir"

;--------------------------------
;Function .onInit
;FunctionEnd

; Pages

Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------

; The stuff to install
Section "" ;No components page, name is not important
  ; Put files in installation directory.
  SetOutPath $INSTDIR
  
  File /oname=${OUTEXE} "Release\${EXEFILE}"
  File "..\GarminUSB\GarminUSB.exe"
  File "..\GarminUSB\GarminUSB.pdf"
  File /oname=points.ipt "$%POINTSFILE%"
  File /oname=Project.dxf "$%PROJECTFILE%"
  File "axis.txt"
  File "axis2.txt"
  File "..\bin\reschangecon.exe"
  File "navigator.dxf"

  ; Ask for .ini overwrites
  IfFileExists "$%PROJECT_NAME%.ini" 0 WriteGpsViewerIni
  MessageBox MB_YESNOCANCEL|MB_DEFBUTTON2 "Found initialization file $%PROJECT_NAME%.ini.$\r$\n$\r$\nOverwrite $%PROJECT_NAME%.ini?" IDYES  WriteGpsViewerIni IDNO SkipGpsViewerIni
  Goto SkipGpsViewerIni
WriteGpsViewerIni:
  File "$%CONFIGFILE%"
SkipGpsViewerIni:

  WriteUninstaller $INSTDIR\uninstall.exe

  ; Write the installation path into the registry
  WriteRegStr HKLM "SOFTWARE\${FULLNAME}" "Install_Dir" "$INSTDIR"

  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${FULLNAME}" "DisplayName" "${FULLNAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${FULLNAME}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${FULLNAME}" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${FULLNAME}" "NoRepair" 1

  ; Create shortcuts with working dir in installation dir.
  CreateDirectory "${SMDIR}"
  SetOutPath $INSTDIR
  CreateShortCut "${SMDIR}\Uninstall ${NAME}.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "${SMDIR}\${NAME}.lnk" "$INSTDIR\${OUTEXE}" "" "$INSTDIR\${OUTEXE}" 0
  CreateShortCut "${SMDIR}\GarminUSB - Install.lnk" "$INSTDIR\GarminUSB.exe" "" "$INSTDIR\GarminUSB.exe" 0
  CreateShortCut "${SMDIR}\GarminUSB - Manual.lnk" "$INSTDIR\GarminUSB.pdf" "" "$INSTDIR\GarminUSB.pdf" 0
  ; CreateShortCut "${SMDIR}\Manual.lnk" "$INSTDIR\GpsViewer.pdf" "" "$INSTDIR\GpsViewer.pdf" 0
SectionEnd

Section "Uninstall"
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${FULLNAME}"
  DeleteRegKey HKLM "SOFTWARE\${FULLNAME}"

  Delete $INSTDIR\uninstall.exe ; delete self, this works...
  Delete "$INSTDIR\${OUTEXE}"
  Delete "$INSTDIR\..\GarminUSB\GarminUSB.exe"
  Delete "$INSTDIR\..\GarminUSB\GarminUSB.pdf"
  Delete "$INSTDIR\$%PROJECT_NAME%.ini"
  Delete "$INSTDIR\points.ipt"
  Delete "$INSTDIR\Project.dxf"
  Delete "$INSTDIR\reschangecon.exe"
  Delete "$INSTDIR\Navigator.dxf"

  ; Remove shortcuts, if any
  Delete "${SMDIR}\*.lnk"
  RMDir "${SMDIR}"

  ; Remove installation directory.
  RMDir $INSTDIR
SectionEnd

