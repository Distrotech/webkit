import wx
import wx.webview

class wkFrame(wx.Frame):
    def __init__(self):
        wx.Frame.__init__(self, None, -1, "WebKit in wxPython!")
        
        self.webkit = wx.webview.WebView(self, -1)
        self.webkit.LoadURL("http://www.wxpython.org")
        

class wkApp(wx.App):
    def OnInit(self):
        self.webFrame = wkFrame()
        self.SetTopWindow(self.webFrame)
        self.webFrame.Show()
    
        return True
        
app = wkApp(redirect=False)
app.MainLoop()
