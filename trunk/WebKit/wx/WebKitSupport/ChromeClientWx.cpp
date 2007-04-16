/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformString.h"
#include "ChromeClientWx.h"
#include <stdio.h>
#include "FloatRect.h"
#include "FrameLoadRequest.h"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <wx/textdlg.h>

#include "WebView.h"
#include "WebViewPrivate.h"
#include "WebFrame.h"


#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d (%s)\n", __FILE__, __LINE__, __FUNCTION__); } while(0)


ChromeClientWx::~ChromeClientWx()
{
}

void ChromeClientWx::chromeDestroyed()
{
}

void ChromeClientWx::setWindowRect(const WebCore::FloatRect&)
{
}

WebCore::FloatRect ChromeClientWx::windowRect()
{
    notImplemented();
    return WebCore::FloatRect();
}

WebCore::FloatRect ChromeClientWx::pageRect()
{
    notImplemented();
    return WebCore::FloatRect();
}

float ChromeClientWx::scaleFactor()
{
    notImplemented();
    return 0.0;
}

void ChromeClientWx::focus()
{
    notImplemented();
}

void ChromeClientWx::unfocus()
{
    notImplemented();
}

bool ChromeClientWx::canTakeFocus(WebCore::FocusDirection)
{
    notImplemented();
    return false;
}

void ChromeClientWx::takeFocus(WebCore::FocusDirection)
{
    notImplemented();
}


WebCore::Page* ChromeClientWx::createWindow(WebCore::Frame*, const WebCore::FrameLoadRequest& request)
{
    WebCore::Page* myPage = 0;
    wxWebFrame* newFrame = new wxWebFrame(wxTheApp->GetAppName());
    
    if (newFrame->webview)
    {
        newFrame->webview->LoadURL(request.resourceRequest().url().url());
        newFrame->Show(true);

        WebViewPrivate* impl = newFrame->webview->m_impl;
        if (impl)
            myPage = impl->frame->page();
    }
    
    return myPage;
}

WebCore::Page* ChromeClientWx::createModalDialog(WebCore::Frame*, const WebCore::FrameLoadRequest&)
{
    notImplemented();
    return 0;
}

void ChromeClientWx::show()
{
    notImplemented();
}

bool ChromeClientWx::canRunModal()
{
    notImplemented();
    return false;
}

void ChromeClientWx::runModal()
{
    notImplemented();
}

void ChromeClientWx::setToolbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientWx::toolbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClientWx::setStatusbarVisible(bool)
{
    notImplemented();
}

bool ChromeClientWx::statusbarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientWx::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClientWx::scrollbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClientWx::setMenubarVisible(bool)
{
    notImplemented();
}

bool ChromeClientWx::menubarVisible()
{
    notImplemented();
    return false;
}

void ChromeClientWx::setResizable(bool)
{
    notImplemented();
}

void ChromeClientWx::addMessageToConsole(const WebCore::String&,
                                          unsigned int,
                                          const WebCore::String&)
{
    notImplemented();
}

bool ChromeClientWx::canRunBeforeUnloadConfirmPanel()
{
    //notImplemented();
    return true;
}

bool ChromeClientWx::runBeforeUnloadConfirmPanel(const WebCore::String& string,
                                                  WebCore::Frame* frame)
{
    wxMessageDialog dialog(NULL, string, wxT("Confirm Action?"), wxYES_NO);
    return dialog.ShowModal() == wxYES;
}

void ChromeClientWx::closeWindowSoon()
{
    notImplemented();
}

/*
    Sites for testing prompts: 
    Alert - just type in a bad web address or http://www.htmlite.com/JS002.php
    Prompt - http://www.htmlite.com/JS007.php
    Confirm - http://www.htmlite.com/JS006.php
*/

void ChromeClientWx::runJavaScriptAlert(WebCore::Frame* frame, const WebCore::String& string)
{
    wxMessageBox(string, wxT("JavaScript Alert"), wxOK);
}

bool ChromeClientWx::runJavaScriptConfirm(WebCore::Frame* frame, const WebCore::String& string)
{
    wxMessageDialog dialog(NULL, string, wxT("JavaScript Confirm"), wxYES_NO);
    dialog.Centre();
    return (dialog.ShowModal() == wxID_YES);
}

bool ChromeClientWx::runJavaScriptPrompt(WebCore::Frame* frame, const WebCore::String& message, const WebCore::String& defaultValue, WebCore::String& result)
{
    wxTextEntryDialog dialog(NULL, message, wxT("JavaScript Prompt"), wxEmptyString, wxOK | wxCANCEL);
    dialog.Centre();
    if ( dialog.ShowModal() == wxID_OK )
    {
        result = dialog.GetValue();
        return true;
    }
    
    return false;
}

void ChromeClientWx::setStatusbarText(const WebCore::String&)
{
    notImplemented();
}

bool ChromeClientWx::shouldInterruptJavaScript()
{
    notImplemented();
    return false;
}

bool ChromeClientWx::tabsToLinks() const
{
    notImplemented();
    return false;
}

WebCore::IntRect ChromeClientWx::windowResizerRect() const
{
    notImplemented();
    return WebCore::IntRect();
}

void ChromeClientWx::addToDirtyRegion(const WebCore::IntRect&)
{
    notImplemented();
}

void ChromeClientWx::scrollBackingStore(int dx, int dy, 
                    const WebCore::IntRect& scrollViewRect, 
                    const WebCore::IntRect& clipRect)
{
    notImplemented();
}

void ChromeClientWx::updateBackingStore()
{
    notImplemented();
}
