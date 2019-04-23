cd /d "%~dp0"

del /s Build\*.exe
del /s BuildTmp\*.res

call SetVersion.cmd
cscript /nologo ExpandEnvironmenStrings.vbs Version.in > Version.h

setlocal
for /f "usebackq tokens=*" %%i in (`"%programfiles(x86)%\microsoft visual studio\installer\vswhere.exe" -version [16.0^,17.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set InstallDir=%%i
)
if exist "%InstallDir%\Common7\Tools\vsdevcmd.bat" (
  call "%InstallDir%\Common7\Tools\vsdevcmd.bat
)

for %%i in ( ^
  ..\freeimage\Source\FreeImageLib\FreeImageLib.vs2019.vcxproj ^
  ..\freeimage\Wrapper\FreeImagePlus\FreeImagePlus.vs2019.vcxproj ^
  src\WinIMergeLib.vs2019.vcxproj ^
  src\WinIMerge.vs2019.vcxproj ^
  src\cidiff.vs2019.vcxproj ^
  ) do (
  MSBuild %%i /t:build /p:Configuration=Release /p:Platform="Win32" /p:PlatformToolset=v142 || pause
  MSBuild %%i /t:build /p:Configuration=Release /p:Platform="x64" /p:PlatformToolset=v142 || pause
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

