cd /d "%~dp0"

del /s BuildTmp\*.res
del gen-versioninfo.h

setlocal
for /f "usebackq tokens=*" %%i in (`"%programfiles(x86)%\microsoft visual studio\installer\vswhere.exe" -version [16.0^,17.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set InstallDir=%%i
)
if exist "%InstallDir%\Common7\Tools\vsdevcmd.bat" (
  call "%InstallDir%\Common7\Tools\vsdevcmd.bat
)

if "%1" == "" (
  call :BuildBin Win32 || goto :eof
  call :BuildBin ARM64 || goto :eof
  call :BuildBin x64 || goto :eof
) else (
  call :BuildBin %1 || goto :eof
)

endlocal

goto :eof

:BuildBin

if "%1" == "Win32" (
  del /s Build\Release\*.exe
) else (
  del /s Build\%1\Release\*.exe
)

for %%i in ( ^
  ..\freeimage\Source\FreeImageLib\FreeImageLib.vcxproj ^
  ..\freeimage\Wrapper\FreeImagePlus\FreeImagePlus.vcxproj ^
  src\WinIMergeLib.vcxproj ^
  src\WinIMerge.vcxproj ^
  src\cidiff.vcxproj ^
  ) do (
  MSBuild %%i /t:build /p:Configuration=Release /p:Platform="%1" /p:PlatformToolset=v142 || goto :eof
)

if exist "%SIGNBAT_PATH%" (
  if "%1" == "Win32" (
    call "%SIGNBAT_PATH%" Build\Release\WinIMerge.exe
    call "%SIGNBAT_PATH%" Build\Release\WinIMergeLib.dll
    call "%SIGNBAT_PATH%" Build\Release\cidiff.exe
  ) else (
    call "%SIGNBAT_PATH%" Build\%1\Release\WinIMerge.exe
    call "%SIGNBAT_PATH%" Build\%1\Release\WinIMergeLib.dll
    call "%SIGNBAT_PATH%" Build\%1\Release\cidiff.exe
  )
)
goto :eof
