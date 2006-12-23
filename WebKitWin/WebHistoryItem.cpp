/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "WebKitDLL.h"
#include "WebHistoryItem.h"

#include "MarshallingHelpers.h"
#include "WebIconDatabase.h"
#include "logging.h"

// WebHistoryItem ----------------------------------------------------------------

IWebIconDatabase* WebHistoryItem::m_sharedIconDatabase = 0;

WebHistoryItem::WebHistoryItem()
: m_refCount(0)
, m_url(0)
, m_title(0)
, m_visitedCount(0)
, m_RSSFeedReferrer(0)
{
    if (!m_sharedIconDatabase) {
        m_sharedIconDatabase = WebIconDatabase::sharedWebIconDatabase();
        m_sharedIconDatabase->Release(); // don't hold our own ref to this - none of our objects outlive the other objects that hold refs
    }

    gClassCount++;
}

WebHistoryItem::~WebHistoryItem()
{
    SysFreeString(m_url);
    SysFreeString(m_title);
    SysFreeString(m_RSSFeedReferrer);
    gClassCount--;
}

WebHistoryItem* WebHistoryItem::createInstance()
{
    WebHistoryItem* instance = new WebHistoryItem();
    instance->AddRef();
    return instance;
}

void WebHistoryItem::releaseAllPendingPageCaches()
{
    // FIXME - TODO
}

// IWebHistoryItemPrivate -----------------------------------------------------

static CFStringRef urlKey = CFSTR("");
static CFStringRef lastVisitedDateKey = CFSTR("lastVisitedDate");
static CFStringRef titleKey = CFSTR("title");
static CFStringRef visitCountKey = CFSTR("visitCount");

HRESULT STDMETHODCALLTYPE WebHistoryItem::initFromDictionaryRepresentation(void* dictionary)
{
    CFDictionaryRef dictionaryRef = (CFDictionaryRef) dictionary;
    HRESULT hr = S_OK;
    BSTR url = 0;
    BSTR title = 0;
    int visitedCount = 0;
    CFAbsoluteTime lastVisitedTime = 0.0;

    CFStringRef urlStringRef = (CFStringRef) CFDictionaryGetValue(dictionaryRef, urlKey);
    if (urlStringRef && CFGetTypeID(urlStringRef) != CFStringGetTypeID()) {
        hr = E_FAIL;
        goto exit;
    }

    CFStringRef lastVisitedRef = (CFStringRef) CFDictionaryGetValue(dictionaryRef, lastVisitedDateKey);
    if (!lastVisitedRef || CFGetTypeID(lastVisitedRef) != CFStringGetTypeID()) {
        hr = E_FAIL;
        goto exit;
    }
    lastVisitedTime = CFStringGetDoubleValue(lastVisitedRef);

    CFStringRef titleRef = (CFStringRef) CFDictionaryGetValue(dictionaryRef, titleKey);
    if (titleRef && CFGetTypeID(titleRef) != CFStringGetTypeID()) {
        hr = E_FAIL;
        goto exit;
    }

    CFNumberRef visitCountRef = (CFNumberRef) CFDictionaryGetValue(dictionaryRef, visitCountKey);
    if (!visitCountRef || CFGetTypeID(visitCountRef) != CFNumberGetTypeID()) {
        hr = E_FAIL;
        goto exit;
    }

    url = urlStringRef ? MarshallingHelpers::CFStringRefToBSTR(urlStringRef) : 0;
    title = titleRef ? MarshallingHelpers::CFStringRefToBSTR(titleRef) : 0;
    if (!CFNumberGetValue(visitCountRef, kCFNumberIntType, &visitedCount)) {
        hr = E_FAIL;
        goto exit;
    }

    if (m_url) {
        hr = m_sharedIconDatabase->releaseIconForURL(m_url);
        if (FAILED(hr))
            goto exit;
        SysFreeString(m_url);
    }

    m_url = url;
    hr = m_sharedIconDatabase->retainIconForURL(m_url);
    if (FAILED(hr))
        goto exit;
    
    SysFreeString(m_title);
    m_title = title;
    m_visitedCount = visitedCount;
    m_lastVisited = MarshallingHelpers::CFAbsoluteTimeToDATE(lastVisitedTime);

exit:
    if (FAILED(hr)) {
        SysFreeString(url);
        SysFreeString(title);
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::dictionaryRepresentation(void** dictionary)
{
    CFDictionaryRef* dictionaryRef = (CFDictionaryRef*) dictionary;
    static CFStringRef lastVisitedFormat = CFSTR("%.1lf");
    CFStringRef lastVisitedStringRef = CFStringCreateWithFormat(0, 0, lastVisitedFormat, MarshallingHelpers::DATEToCFAbsoluteTime(m_lastVisited));
    if (!lastVisitedStringRef)
        return E_FAIL;

    int keyCount = 0;
    CFStringRef keys[4];
    CFTypeRef values[4];
    if (m_url) {
        keys[keyCount]     = urlKey;
        values[keyCount++] = MarshallingHelpers::BSTRToCFStringRef(m_url);
    }
    keys[keyCount]         = lastVisitedDateKey;
    values[keyCount++]     = lastVisitedStringRef;
    if (m_title) {
        keys[keyCount]     = titleKey;
        values[keyCount++] = MarshallingHelpers::BSTRToCFStringRef(m_title);
    }
    keys[keyCount]   = visitCountKey;
    values[keyCount++] = CFNumberCreate(0, kCFNumberIntType, &m_visitedCount);
    *dictionaryRef = CFDictionaryCreate(0, (const void**)keys, (const void**)values, keyCount, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    CFRelease(lastVisitedStringRef);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::hasURLString(BOOL *hasURL)
{
    *hasURL = (m_url && m_url[0]) ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::visitCount(int *count)
{
    *count = m_visitedCount;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::setVisitCount(int count)
{
    m_visitedCount = count;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::mergeAutoCompleteHints(IWebHistoryItem* otherItem)
{
    if (!otherItem)
        return E_FAIL;

    if (otherItem == this)
        return S_OK;

    IWebHistoryItemPrivate* otherItemPriv;
    HRESULT hr = otherItem->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&otherItemPriv);
    if (FAILED(hr))
        return hr;

    int otherVisitCount;
    hr = otherItemPriv->visitCount(&otherVisitCount);
    if (FAILED(hr))
        return hr;

    otherItemPriv->Release();

    m_visitedCount = otherVisitCount;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::setLastVisitedTimeInterval(DATE time)
{
    m_lastVisited = time;
    m_visitedCount++;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::setTitle(BSTR title)
{
    if (m_title)
        SysFreeString(m_title);
    m_title = SysAllocString(title);
    if (title && !m_title)
        return E_OUTOFMEMORY;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::RSSFeedReferrer(BSTR* url)
{
    *url = SysAllocString(m_RSSFeedReferrer);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::setRSSFeedReferrer(BSTR url)
{
    if (m_RSSFeedReferrer)
        SysFreeString(m_RSSFeedReferrer);
    m_RSSFeedReferrer = SysAllocString(url);
    if (url && !m_RSSFeedReferrer)
        return E_OUTOFMEMORY;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::hasPageCache(BOOL* /*hasCache*/)
{
    // FIXME - TODO
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::setHasPageCache(BOOL /*hasCache*/)
{
    // FIXME - TODO
    return E_NOTIMPL;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebHistoryItem::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebHistoryItem*>(this);
    else if (IsEqualGUID(riid, IID_IWebHistoryItem))
        *ppvObject = static_cast<IWebHistoryItem*>(this);
    else if (IsEqualGUID(riid, IID_IWebHistoryItemPrivate))
        *ppvObject = static_cast<IWebHistoryItemPrivate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebHistoryItem::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebHistoryItem::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebHistoryItem -------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebHistoryItem::initWithURLString(
    /* [in] */ BSTR urlString,
    /* [in] */ BSTR title,
    /* [in] */ DATE lastVisited)
{
    if (m_url) {
        if (FAILED(m_sharedIconDatabase->releaseIconForURL(m_url)))
            LOG_ERROR("Failed to release icon");
        SysFreeString(m_url);
    }

    if (FAILED(m_sharedIconDatabase->retainIconForURL(urlString)))
        LOG_ERROR("Failed to retain icon");
    
    m_url = SysAllocString(urlString);
    m_title = SysAllocString(title);
    m_lastVisited = lastVisited;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::originalURLString( 
    /* [retval][out] */ BSTR* /*url*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::URLString( 
    /* [retval][out] */ BSTR* url)
{
    *url = SysAllocString(m_url);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::title( 
    /* [retval][out] */ BSTR* pageTitle)
{
    *pageTitle = SysAllocString(m_title);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::lastVisitedTimeInterval( 
    /* [retval][out] */ DATE* lastVisited)
{
    *lastVisited = m_lastVisited;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::setAlternateTitle( 
    /* [retval][out] */ BSTR* /*title*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::alternateTitle( 
    /* [in] */ BSTR /*title*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::icon( 
    /* [out, retval] */ HBITMAP* /*image*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
