all:
	set WebKitSDKDir="%SRCROOT%\AppleInternal"
	set WebKitOutputDir="%DSTROOT%"
	devenv "WebKit.sln" /rebuild Release
