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

%constant wxEventType wxEVT_WEBVIEW_BEFORE_LOAD;
%constant wxEventType wxEVT_WEBVIEW_STATE_CHANGED;
%constant wxEventType wxEVT_WEBVIEW_NEW_WINDOW;
%constant wxEventType wxEVT_WEBVIEW_RIGHT_CLICK;

%pythoncode {
EVT_WEBVIEW_BEFORE_LOAD = wx.PyEventBinder( wxEVT_WEBVIEW_BEFORE_LOAD )
EVT_WEBVIEW_STATE_CHANGED = wx.PyEventBinder( wxEVT_WEBVIEW_STATE_CHANGED )
EVT_WEBVIEW_NEW_WINDOW = wx.PyEventBinder( wxEVT_WEBVIEW_NEW_WINDOW )
EVT_WEBVIEW_RIGHT_CLICK = wx.PyEventBinder( wxEVT_WEBVIEW_RIGHT_CLICK )
}