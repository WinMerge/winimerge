cd /d "%~dp0"

del /s Build\*.exe
del /s BuildTmp\*.res

setlocal
for /f "usebackq tokens=*" %%i in (`"%programfiles(x86)%\microsoft visual studio\installer\vswhere.exe" -version [15.0^,16.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set InstallDir=%%i
)
if exist "%InstallDir%\Common7\Tools\vsdevcmd.bat" (
  call "%InstallDir%\Common7\Tools\vsdevcmd.bat
)

for %%i in ( ^
  ..\freeimage\Source\FreeImageLib\FreeImageLib.vs2017.vcxproj ^
  ..\freeimage\Wrapper\FreeImagePlus\FreeImagePlus.vs2017.vcxproj ^
  src\WinIMergeLib.vcxproj ^
  src\WinIMerge.vcxproj ^
  src\cidiff.vcxproj ^
  ) do (
  MSBuild %%i /t:build /p:Configuration=Release /p:Platform="Win32" /p:PlatformToolset=v141_xp || goto :eof
  MSBuild %%i /t:build /p:Configuration=Release /p:Platform="x64" /p:PlatformToolset=v141 || goto :eof
)

if exist "%SIGNBAT_PATH%" (
  "%SIGNBAT_PATH%" Build\Release\WinIMerge.exe
  "%SIGNBAT_PATH%" Build\Release\WinIMergeLib.dll
  "%SIGNBAT_PATH%" Build\Release\cidiff.exe
  "%SIGNBAT_PATH%" Build\x64\Release\WinIMerge.exe
  "%SIGNBAT_PATH%" Build\x64\Release\WinIMergeLib.dll
  "%SIGNBAT_PATH%" Build\x64\Release\cidiff.exe
)

endlocal

