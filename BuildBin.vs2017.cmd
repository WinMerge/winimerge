cd /d "%~dp0"

del /s Build\*.exe
del /s BuildTmp\*.res

call SetVersion.cmd
cscript /nologo ExpandEnvironmenStrings.vbs Version.in > Version.h

setlocal
set VisualStudioVersion=15.0
call "%VS141COMNTOOLS%vsvars32.bat"
for %%i in ( ^
  ..\freeimage\Source\FreeImageLib\FreeImageLib.vs2017.vcxproj ^
  ..\freeimage\Wrapper\FreeImagePlus\FreeImagePlus.vs2017.vcxproj ^
  src\WinIMergeLib.vs2017.vcxproj ^
  src\WinIMerge.vs2017.vcxproj ^
  src\cidiff.vs2017.vcxproj ^
  ) do (
  MSBuild %%i /t:build /p:Configuration=Release /p:Platform="Win32" /p:PlatformToolset=v141_xp || pause
  MSBuild %%i /t:build /p:Configuration=Release /p:Platform="x64" /p:PlatformToolset=v141_xp || pause
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

