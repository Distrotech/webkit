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

#include "config.h"
#include "WebKitDLL.h"
#include "WebBackForwardList.h"

#include "COMPtr.h"
#include "WebFrame.h"
#include "WebKit.h"
#include "WebPreferences.h"

#include <WebCore/BackForwardList.h>
#include <WebCore/HistoryItem.h>

using std::min;
using namespace WebCore;

const UINT computeDefaultPageCacheSize = UINT_MAX;

// WebBackForwardList ----------------------------------------------------------------

static HashMap<BackForwardList*, WebBackForwardList*>& backForwardListWrappers()
{
    static HashMap<BackForwardList*, WebBackForwardList*> staticBackForwardListWrappers;
    return staticBackForwardListWrappers;
}

WebBackForwardList::WebBackForwardList(PassRefPtr<BackForwardList> backForwardList)
    : m_refCount(0)
    , m_backForwardList(backForwardList)
{
    ASSERT(!backForwardListWrappers().contains(m_backForwardList.get()));
    backForwardListWrappers().set(m_backForwardList.get(), this);

    gClassCount++;
}

WebBackForwardList::~WebBackForwardList()
{
    ASSERT(m_backForwardList->closed());

    ASSERT(backForwardListWrappers().contains(m_backForwardList.get()));
    backForwardListWrappers().remove(m_backForwardList.get());

    gClassCount--;
}

WebBackForwardList* WebBackForwardList::createInstance()
{
    WebBackForwardList* instance = new WebBackForwardList(new BackForwardList);
    instance->AddRef();
    return instance;
}

WebBackForwardList* WebBackForwardList::createInstance(PassRefPtr<BackForwardList> backForwardList)
{
    WebBackForwardList* instance;

    instance = backForwardListWrappers().get(backForwardList.get());

    if (!instance)
        instance = new WebBackForwardList(backForwardList);

    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebBackForwardList::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebBackForwardList*>(this);
    else if (IsEqualGUID(riid, IID_IWebBackForwardList))
        *ppvObject = static_cast<IWebBackForwardList*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebBackForwardList::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebBackForwardList::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebBackForwardList ---------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebBackForwardList::addItem( 
    /* [in] */ IWebHistoryItem* item)
{
    COMPtr<WebHistoryItem> webHistoryItem;
 
    if (!item || FAILED(item->QueryInterface(CLSID_WebHistoryItem, (void**)&webHistoryItem)))
        return E_FAIL;
 
    m_backForwardList->addItem(webHistoryItem->historyItem());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::goBack( void)
{
    m_backForwardList->goBack();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::goForward( void)
{
    m_backForwardList->goForward();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::goToItem( 
    /* [in] */ IWebHistoryItem* item)
{
    COMPtr<WebHistoryItem> webHistoryItem;
 
    if (!item || FAILED(item->QueryInterface(&webHistoryItem)))
        return E_FAIL;

    m_backForwardList->goToItem(webHistoryItem->historyItem());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::backItem( 
    /* [retval][out] */ IWebHistoryItem** item)
{
    if (!item)
        return E_POINTER;

    HistoryItem* historyItem = m_backForwardList->backItem();

    if (!historyItem)
        return E_FAIL;

    *item = WebHistoryItem::createInstance(historyItem);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::currentItem( 
    /* [retval][out] */ IWebHistoryItem** item)
{
    if (!item)
        return E_POINTER;

    HistoryItem* historyItem = m_backForwardList->currentItem();

    if (!historyItem)
        return E_FAIL;

    *item = WebHistoryItem::createInstance(historyItem);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::forwardItem( 
    /* [retval][out] */ IWebHistoryItem** item)
{
    if (!item)
        return E_POINTER;

    HistoryItem* historyItem = m_backForwardList->forwardItem();

    if (!historyItem)
        return E_FAIL;

    *item = WebHistoryItem::createInstance(historyItem);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::backListWithLimit( 
    /* [in] */ int limit,
    /* [out] */ int* listCount,
    /* [retval][out] */ IWebHistoryItem** list)
{
    HistoryItemVector historyItemVector;
    m_backForwardList->backListWithLimit(limit, historyItemVector);

    *listCount = static_cast<int>(historyItemVector.size());

    if (list)
        for (unsigned i = 0; i < historyItemVector.size(); i++)
            list[i] = WebHistoryItem::createInstance(historyItemVector[i].get());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::forwardListWithLimit( 
    /* [in] */ int limit,
    /* [out] */ int* listCount,
    /* [retval][out] */ IWebHistoryItem** list)
{
    HistoryItemVector historyItemVector;
    m_backForwardList->forwardListWithLimit(limit, historyItemVector);

    *listCount = static_cast<int>(historyItemVector.size());

    if (list)
        for (unsigned i = 0; i < historyItemVector.size(); i++)
            list[i] = WebHistoryItem::createInstance(historyItemVector[i].get());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::capacity( 
    /* [retval][out] */ int* result)
{
    *result = (int)m_backForwardList->capacity();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::setCapacity( 
    /* [in] */ int size)
{
    if (size < 0)
        return E_FAIL;
    
    m_backForwardList->setCapacity(size);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::backListCount( 
    /* [retval][out] */ int* count)
{
    *count = m_backForwardList->backListCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::forwardListCount( 
    /* [retval][out] */ int* count)
{
    *count = m_backForwardList->forwardListCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::containsItem( 
    /* [in] */ IWebHistoryItem* item,
    /* [retval][out] */ BOOL* result)
{
    COMPtr<WebHistoryItem> webHistoryItem;

    if (!item || FAILED(item->QueryInterface(&webHistoryItem)))
        return E_FAIL;

    *result = m_backForwardList->containsItem(webHistoryItem->historyItem());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::itemAtIndex( 
    /* [in] */ int index,
    /* [retval][out] */ IWebHistoryItem** item)
{
    if (!item)
        return E_POINTER;

    HistoryItem* historyItem = m_backForwardList->itemAtIndex(index);

    if (!historyItem)
        return E_FAIL;
 
    *item = WebHistoryItem::createInstance(historyItem);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::setPageCacheSize( 
    /* [in] */ UINT size)
{
    m_backForwardList->setPageCacheSize(size);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebBackForwardList::pageCacheSize( 
    /* [retval][out] */ UINT* size)
{
    setDefaultPageCacheSizeIfNecessary();

    *size = m_backForwardList->pageCacheSize();
    return S_OK;
}

void WebBackForwardList::setDefaultPageCacheSizeIfNecessary()
{
    static bool initialized = false;
    if (initialized)
        return;

    UINT cacheSize;
    WebPreferences* prefs = WebPreferences::createInstance();
    COMPtr<IWebPreferences> standardPrefs;
    if (!prefs || FAILED(prefs->standardPreferences(&standardPrefs)) || FAILED(standardPrefs->pageCacheSize(&cacheSize)))
        cacheSize = 3;  // 3 is the default value for WebKitPageCacheSizePreferenceKey
    if (prefs)
        prefs->Release();
    unsigned long long memSize = WebSystemMainMemory();

    // On Windows the reported memory is a bit smaller, so we use 1000 instead of 1024 as a fudge factor.
    if (memSize >= 1024 * 1024 * 1000 /*1024*/)
        BackForwardList::setDefaultPageCacheSize(cacheSize);
    else if (memSize >= 512 * 1024 * 1000 /*1024*/)
        BackForwardList::setDefaultPageCacheSize(cacheSize - 1);
    else
        BackForwardList::setDefaultPageCacheSize(cacheSize - 2);
}
