!if !defined(BUILDSTYLE)
BUILDSTYLE=Release
!ENDIF

all:
	set WebKitSDKDir="$(SRCROOT)\AppleInternal"
	set WebKitOutputDir=$(SYMROOT)
	devenv "JavaScriptCore.sln" /rebuild $(BUILDSTYLE)
	xcopy "$(SYMROOT)\bin\*" "$(DSTROOT)\AppleInternal\bin\" /e/v/i/h/y
	xcopy "$(SYMROOT)\include\*" "$(DSTROOT)\AppleInternal\include\" /e/v/i/h/y	
	xcopy "$(SYMROOT)\lib\*" "$(DSTROOT)\AppleInternal\lib\" /e/v/i/h/y	