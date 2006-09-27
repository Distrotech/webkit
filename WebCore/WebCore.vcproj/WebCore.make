all:
	set WebKitSDKDir="%SRCROOT%\AppleInternal"
	set WebKitOutputDir="%DSTROOT%"
	devenv "WebCore.sln" /rebuild Release
