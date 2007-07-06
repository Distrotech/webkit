%module(package="wx") webview

%{
#include "wx/wxPython/wxPython.h"
#include "wx/wxPython/pyclasses.h"
#include "WebView.h"
#include "WebFrame.h"
%}

//---------------------------------------------------------------------------

%import core.i
%import windows.i

MustHaveApp(wxWebView);
MustHaveApp(wxWebFrame);

%include WebView.h
%include WebFrame.h

%pythoncode { wx = _core }
%pythoncode { __docfilter__ = wx.__DocFilter(globals()) }
