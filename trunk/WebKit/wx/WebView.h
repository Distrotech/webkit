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
 
#ifndef WXWEBVIEW_H
#define WXWEBVIEW_H

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

class WebViewPrivate;
class WebViewFrameData;

class WXEXPORT wxWebView : public wxScrolledWindow
{
public:
    // ctor(s)
    wxWebView(wxWindow* parent, int id = wxID_ANY, 
                const wxPoint& point = wxDefaultPosition, 
                const wxSize& size = wxDefaultSize, 
                WebViewFrameData* data = NULL); // For wxWebView internal data passing
    ~wxWebView();
    
    void LoadURL(wxString url);
    bool GoBack();
    bool GoForward();
    void Stop();
    void Reload();
    
    bool CanCut();
    bool CanCopy();
    bool CanPaste();
    
    void Cut();
    void Copy();
    void Paste();
    
    //bool CanGetPageSource();
    wxString GetPageSource();
    void SetPageSource(const wxString& source, const wxString& baseUrl = wxEmptyString);
    
    wxString RunScript(const wxString& javascript);
    
    bool CanIncreaseTextSize();
    void IncreaseTextSize();
    bool CanDecreaseTextSize();
    void DecreaseTextSize();
    void MakeEditable(bool enable);
    bool IsEditable() { return m_isEditable; }

    WebViewPrivate* m_impl;

protected:

    // event handlers (these functions should _not_ be virtual)
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnMouseEvents(wxMouseEvent& event);
    void OnKeyEvents(wxKeyEvent& event);
    void OnSetFocus(wxFocusEvent& event);
    void OnKillFocus(wxFocusEvent& event);
    void OnActivate(wxActivateEvent& event);

private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
    float m_textMagnifier;
    bool m_isEditable;
    bool m_isInitialized;
    bool m_beingDestroyed;
};

// ----------------------------------------------------------------------------
// Web Kit Events
// ----------------------------------------------------------------------------

enum {
    wxWEBVIEW_STATE_START = 1,
    wxWEBVIEW_STATE_NEGOTIATING = 2,
    wxWEBVIEW_STATE_REDIRECTING = 4,
    wxWEBVIEW_STATE_TRANSFERRING = 8,
    wxWEBVIEW_STATE_STOP = 16,
        wxWEBVIEW_STATE_FAILED = 32
};

enum {
    wxWEBVIEW_NAV_LINK_CLICKED = 1,
    wxWEBVIEW_NAV_BACK_NEXT = 2,
    wxWEBVIEW_NAV_FORM_SUBMITTED = 4,
    wxWEBVIEW_NAV_RELOAD = 8,
    wxWEBVIEW_NAV_FORM_RESUBMITTED = 16,
    wxWEBVIEW_NAV_OTHER = 32

};

class wxWebViewDOMElementInfo
{
public: 
    wxWebViewDOMElementInfo();
    ~wxWebViewDOMElementInfo() { }
    
    wxString GetTagName() { return m_tagName; }
    void SetTagName(const wxString& name) { m_tagName = name; }

    bool IsSelected() { return m_isSelected; }
    void SetSelected(bool sel) { m_isSelected = sel; }
 
    wxString GetText() { return m_text; }
    void SetText(const wxString& text) { m_text = text; }
 
    wxString GetImageSrc() { return m_imageSrc; }
    void SetImageSrc(const wxString& src) { m_imageSrc = src; }
 
    wxString GetLink() { return m_link; }
    void SetLink(const wxString& link) { m_link = link; }

protected:
    void* m_domElement;
    bool m_isSelected;
    wxString m_tagName;
    wxString m_text;
    wxString m_imageSrc;
    wxString m_link;
};

class wxWebViewBeforeLoadEvent : public wxCommandEvent
{
    DECLARE_DYNAMIC_CLASS( wxWebViewBeforeLoadEvent )
    
public:
    bool IsCancelled() { return m_cancelled; }
    void Cancel(bool cancel = true) { m_cancelled = cancel; }
    wxString GetURL() { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }
    void SetNavigationType(int navType) { m_navType = navType; }
    int GetNavigationType() { return m_navType; }

    wxWebViewBeforeLoadEvent( wxWindow* win = (wxWindow*) NULL );
    wxEvent *Clone(void) const { return new wxWebViewBeforeLoadEvent(*this); }

protected:
    bool m_cancelled;
    wxString m_url;
    int m_navType;
};

class wxWebViewStateChangedEvent : public wxCommandEvent
{
    DECLARE_DYNAMIC_CLASS( wxWebViewStateChangedEvent )

public:
    int GetState() { return m_state; }
    void SetState(const int state) { m_state = state; }
    wxString GetURL() { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }

    wxWebViewStateChangedEvent( wxWindow* win = (wxWindow*) NULL );
    wxEvent *Clone(void) const { return new wxWebViewStateChangedEvent(*this); }

protected:
    int m_state;
    wxString m_url;
};

class wxWebViewNewWindowEvent : public wxCommandEvent
{
    DECLARE_DYNAMIC_CLASS( wxWebViewNewWindowEvent )

public:
    wxString GetURL() { return m_url; }
    void SetURL(const wxString& url) { m_url = url; }

    wxWebViewNewWindowEvent( wxWindow* win = (wxWindow*) NULL );
    wxEvent *Clone(void) const { return new wxWebViewNewWindowEvent(*this); }

protected:
    wxString m_url;
};

class wxWebViewRightClickEvent : public wxCommandEvent
{
    DECLARE_DYNAMIC_CLASS( wxWebViewRightClickEvent )

public:
    wxWebViewRightClickEvent( wxWindow* win = (wxWindow*) NULL );
    wxEvent *Clone(void) const { return new wxWebViewRightClickEvent(*this); }
    
    wxWebViewDOMElementInfo GetInfo() { return m_info; }
    void SetInfo(wxWebViewDOMElementInfo info) { m_info = info; }
    
    wxPoint GetPosition() { return m_position; }
    void SetPosition(wxPoint pos) { m_position = pos; }

protected:
    wxWebViewDOMElementInfo m_info;
    wxPoint m_position;
};

typedef void (wxEvtHandler::*wxWebViewStateChangedEventFunction)(wxWebViewStateChangedEvent&);
typedef void (wxEvtHandler::*wxWebViewBeforeLoadEventFunction)(wxWebViewBeforeLoadEvent&);
typedef void (wxEvtHandler::*wxWebViewNewWindowEventFunction)(wxWebViewNewWindowEvent&);
typedef void (wxEvtHandler::*wxWebViewRightClickEventFunction)(wxWebViewRightClickEvent&);

BEGIN_DECLARE_EVENT_TYPES()
    DECLARE_LOCAL_EVENT_TYPE(wxEVT_WEBVIEW_BEFORE_LOAD, wxID_ANY)
    DECLARE_LOCAL_EVENT_TYPE(wxEVT_WEBVIEW_STATE_CHANGED, wxID_ANY)
    DECLARE_LOCAL_EVENT_TYPE(wxEVT_WEBVIEW_NEW_WINDOW, wxID_ANY)
    DECLARE_LOCAL_EVENT_TYPE(wxEVT_WEBVIEW_RIGHT_CLICK, wxID_ANY)
END_DECLARE_EVENT_TYPES()

#define EVT_WEBVIEW_STATE_CHANGED(func) \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_STATE_CHANGED, \
                            wxID_ANY, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewStateChangedEventFunction) & func, \
                            (wxObject *) NULL ),
                            
#define EVT_WEBVIEW_BEFORE_LOAD(func) \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_BEFORE_LOAD, \
                            wxID_ANY, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewBeforeLoadEventFunction) & func, \
                            (wxObject *) NULL ),
                            
#define EVT_WEBVIEW_NEW_WINDOW(func) \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_BEFORE_LOAD, \
                            wxID_ANY, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewBeforeLoadEventFunction) & func, \
                            (wxObject *) NULL ),

#define EVT_WEBVIEW_RIGHT_CLICK(func) \
            DECLARE_EVENT_TABLE_ENTRY( wxEVT_WEBVIEW_RIGHT_CLICK, \
                            wxID_ANY, \
                            wxID_ANY, \
                            (wxObjectEventFunction)   \
                            (wxWebViewRightClickEventFunction) & func, \
                            (wxObject *) NULL ),

#endif // ifndef WXWEBVIEW_H
