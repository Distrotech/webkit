@echo off

IF "%WebKitSDKDir%"=="" goto end;
IF "%WebKitOutputDir%"=="" goto end;
IF "%1"=="" goto end;

mkdir 2>NUL "%WebKitSDKDir%\include\%1\DerivedSources"

xcopy /y /d /i "%WebKitOutputDir%\lib\WebCore.lib" "%WebKitSDKDir%\lib"
xcopy /y /d /i "%WebKitOutputDir%\lib\WebCore_debug.lib" "%WebKitSDKDir%\lib"
xcopy /y /d /s /i "%WebKitOutputDir%\include\%1" "%WebKitSDKDir%\include\%1"
xcopy /y /d "%WebKitOutputDir%\obj\%1\DerivedSources\*.h" "%WebKitSDKDir%\include\%1\DerivedSources"
:end
EXIT /B