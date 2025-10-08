cd /d "%~dp0"

set DISTDIR=.\Dist
set path="%ProgramFiles%\7-zip";"%ProgramFiles(x86)%\7-zip";%path%

for /f "usebackq tokens=*" %%i in (`"%programfiles(x86)%\microsoft visual studio\installer\vswhere.exe" -version [15.0^,16.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set InstallDir=%%i
)

if "%1" == "" (
  call :BuildArc x86 || goto :eof
  call :BuildArc x64 || goto :eof
  call :BuildArc ARM || goto :eof
  call :BuildArc ARM64 || goto :eof
) else (
  call :BuildArc %1 || goto :eof
)

goto :eof

:BuildArc

mkdir "%DISTDIR%\%1\WinIMerge\" 2> NUL

copy Build\%1\Release\WinIMerge\WinIMerge.exe "%DISTDIR%\%1\WinIMerge\"
copy Build\%1\Release\WinIMerge\WinIMergeLib.dll "%DISTDIR%\%1\WinIMerge\"
copy Build\%1\Release\WinIMerge\cidiff.exe "%DISTDIR%\%1\WinIMerge\"
call :GET_EXE_VERSION %~dp0Build\%1\Release\WinIMerge\WinIMerge.exe
copy GPL.txt "%DISTDIR%\%1\WinIMerge"
copy freeimage-license-gplv2.txt "%DISTDIR%\%1\WinIMerge"
copy "%InstallDir%\VC\Redist\MSVC\14.16.27012\%1\Microsoft.VC141.OpenMP\vcomp140.dll" "%DISTDIR%\%1\WinIMerge\"

7z.exe a -tzip "%DISTDIR%\winimerge-%EXE_VERSION%-%1.zip" "%DISTDIR%\%1\WinIMerge"

goto :eof

:GET_EXE_VERSION

SET EXE_PATH=%1
powershell -NoLogo -NoProfile -Command "(Get-Item '%EXE_PATH%').VersionInfo.FileVersion" > _tmp_.txt
set /P EXE_VERSIONTMP=<_tmp_.txt
set EXE_VERSION=%EXE_VERSIONTMP: =%
del _tmp_.txt
goto :eof

