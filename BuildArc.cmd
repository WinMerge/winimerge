cd /d "%~dp0"

set DISTDIR=.\Dist
set path="%ProgramFiles%\7-zip";"%ProgramFiles(x86)%\7-zip";%path%

for /f "usebackq tokens=*" %%i in (`"%programfiles(x86)%\microsoft visual studio\installer\vswhere.exe" -version [15.0^,16.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set InstallDir=%%i
)

mkdir "%DISTDIR%\WinIMerge" 2> NUL
mkdir "%DISTDIR%\WinIMerge\bin" 2> NUL
mkdir "%DISTDIR%\WinIMerge\bin64" 2> NUL

copy Build\Release\WinIMerge.exe "%DISTDIR%\WinIMerge\bin"
copy Build\Release\WinIMergeLib.dll "%DISTDIR%\WinIMerge\bin"
copy Build\Release\cidiff.exe "%DISTDIR%\WinIMerge\bin"
copy Build\x64\Release\WinIMerge.exe "%DISTDIR%\WinIMerge\bin64"
copy Build\x64\Release\WinIMergeLib.dll "%DISTDIR%\WinIMerge\bin64"
copy Build\x64\Release\cidiff.exe "%DISTDIR%\WinIMerge\bin64"
copy "%InstallDir%\VC\Redist\MSVC\14.16.27012\x86\Microsoft.VC141.OpenMP\vcomp140.dll" "%DISTDIR%\WinIMerge\bin"
copy "%InstallDir%\VC\Redist\MSVC\14.16.27012\x64\Microsoft.VC141.OpenMP\vcomp140.dll" "%DISTDIR%\WinIMerge\bin64"

copy GPL.txt "%DISTDIR%\WinIMerge"
copy freeimage-license-gplv2.txt "%DISTDIR%\WinIMerge"

call :GET_EXE_VERSION %~dp0Build\X64\Release\WinIMerge.exe

7z.exe a -tzip "%DISTDIR%\winimerge-%EXE_VERSION%-exe.zip" "%DISTDIR%\WinIMerge\"

goto :eof

:GET_EXE_VERSION

SET EXE_PATH=%1
WMIC Path CIM_DataFile WHERE Name='%EXE_PATH:\=\\%' Get Version | findstr /v Version > _tmp_.txt
set /P EXE_VERSIONTMP=<_tmp_.txt
set EXE_VERSION=%EXE_VERSIONTMP: =%
del _tmp_.txt
goto :eof

