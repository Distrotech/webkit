@echo off

IF "%WebKitSDKDir%"=="" goto end;
IF "%WebKitOutputDir%"=="" goto end;
IF "%1"=="" goto end;

xcopy /y /d /i "%WebKitOutputDir%\lib\WebKitGUID.lib" "%WebKitSDKDir%\lib"
xcopy /y /d /i "%WebKitOutputDir%\lib\WebKitGUID_debug.lib" "%WebKitSDKDir%\lib"
xcopy /y /d /i "%WebKitOutputDir%\bin\WebKit.dll" "%WebKitSDKDir%\bin"
xcopy /y /d /i "%WebKitOutputDir%\bin\WebKit_debug.dll" "%WebKitSDKDir%\bin"
xcopy /y /d /i "%WebKitOutputDir%\bin\WebKit.pdb" "%WebKitSDKDir%\bin"
xcopy /y /d /i "%WebKitOutputDir%\bin\WebKit_debug.pdb" "%WebKitSDKDir%\bin"
xcopy /y /d /s /i "%WebKitOutputDir%\include\%1" "%WebKitSDKDir%\include\%1"

:end
EXIT /B