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
#include "WebHistory.h"

#include "MarshallingHelpers.h"
#include "WebHistoryItem.h"
#include "WebPreferences.h"
#include "WebNotificationCenter.h"
#include "CFDictionaryPropertyBag.h"

#include <CoreFoundation/CoreFoundation.h>

#pragma warning( push, 0 )
#include <wtf/Vector.h>
#pragma warning( pop )

CFStringRef DatesArrayKey = CFSTR("WebHistoryDates");
CFStringRef FileVersionKey = CFSTR("WebHistoryFileVersion");

const IID IID_IWebHistoryPrivate = { 0x3de04e59, 0x93f9, 0x4369, { 0x8b, 0x43, 0x97, 0x64, 0x58, 0xd7, 0xe3, 0x19 } };

#define currentFileVersion 1

static bool areEqualOrClose(double d1, double d2)
{
    double diff = d1-d2;
    return (diff < .000001 && diff > -.000001);
}

static CFDictionaryPropertyBag* createUserInfoFromArray(BSTR notificationStr, CFArrayRef arrayItem)
{
    CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    // CFDictionary will release reference to this CFString
    CFStringRef key = MarshallingHelpers::BSTRToCFStringRef(notificationStr);
    CFDictionaryAddValue(dictionary, (const void*) key, (const void*) arrayItem);
    CFDictionaryPropertyBag* result = CFDictionaryPropertyBag::createInstance();
    result->setDictionary(dictionary);
    CFRelease(dictionary);
    return result;
}

static CFDictionaryPropertyBag* createUserInfoFromHistoryItem(BSTR notificationStr, IWebHistoryItem* item)
{
    // reference counting of item added to the array is managed by the CFArray value callbacks
    CFArrayRef itemList = CFArrayCreate(0, (const void**) &item, 1, &MarshallingHelpers::kIUnknownArrayCallBacks);
    CFDictionaryPropertyBag* info = createUserInfoFromArray(notificationStr, itemList);
    CFRelease(itemList);
    return info;
}

static void releaseUserInfo(CFDictionaryPropertyBag* userInfo)
{
    // free the dictionary
    userInfo->setDictionary(0);
    ASSERT(userInfo->Release() == 0);   // make sure no one else holds a reference to the userInfo.
}

// WebHistory -----------------------------------------------------------------

IWebHistory* WebHistory::m_optionalSharedHistory = 0;

WebHistory::WebHistory()
: m_refCount(0)
, m_entriesByURL(0)
, m_datesWithEntries(0)
, m_entriesByDate(0)
, m_preferences(0)
{
    gClassCount++;

    m_entriesByURL = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, 0);
    m_datesWithEntries = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    m_entriesByDate = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);

    WebPreferences* tempPrefs = WebPreferences::createInstance();
    IWebPreferences* standardPrefs;
    tempPrefs->standardPreferences(&standardPrefs);
    m_preferences = static_cast<WebPreferences*>(standardPrefs); // should be safe, since there's no setter for standardPrefs
    tempPrefs->Release();
}

WebHistory::~WebHistory()
{
    gClassCount--;

    if (m_entriesByURL)
        CFRelease(m_entriesByURL);
    if (m_datesWithEntries)
        CFRelease(m_datesWithEntries);
    if (m_entriesByDate)
        CFRelease(m_entriesByDate);
    if (m_preferences)
        m_preferences->Release();
}

WebHistory* WebHistory::createInstance()
{
    WebHistory* instance = new WebHistory();
    instance->AddRef();
    return instance;
}

HRESULT WebHistory::postNotification(NotificationType notifyType, IPropertyBag* userInfo /*=0*/)
{
    IWebNotificationCenter* nc = WebNotificationCenter::defaultCenterInternal();
    HRESULT hr = nc->postNotificationName(getNotificationString(notifyType), static_cast<IWebHistory*>(this), userInfo);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

BSTR WebHistory::getNotificationString(NotificationType notifyType)
{
    static BSTR keys[6] = {0};
    if (!keys[0]) {
        keys[0] = SysAllocString(WebHistoryItemsAddedNotification);
        keys[1] = SysAllocString(WebHistoryItemsRemovedNotification);
        keys[2] = SysAllocString(WebHistoryAllItemsRemovedNotification);
        keys[3] = SysAllocString(WebHistoryLoadedNotification);
        keys[4] = SysAllocString(WebHistoryItemsDiscardedWhileLoadingNotification);
        keys[5] = SysAllocString(WebHistorySavedNotification);
    }
    return keys[notifyType];
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebHistory::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebHistory*>(this);
    else if (IsEqualGUID(riid, IID_IWebHistory))
        *ppvObject = static_cast<IWebHistory*>(this);
    else if (IsEqualGUID(riid, IID_IWebHistoryPrivate))
        *ppvObject = static_cast<IWebHistoryPrivate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebHistory::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebHistory::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebHistory ----------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebHistory::optionalSharedHistory( 
    /* [retval][out] */ IWebHistory** history)
{
    *history = m_optionalSharedHistory;
    if (m_optionalSharedHistory)
        m_optionalSharedHistory->AddRef();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::setOptionalSharedHistory( 
    /* [in] */ IWebHistory* history)
{
    if (m_optionalSharedHistory) {
        m_optionalSharedHistory->Release();
        m_optionalSharedHistory = 0;        
    }

    m_optionalSharedHistory = history;
    if (history)
        history->AddRef();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::loadFromURL( 
    /* [in] */ BSTR url,
    /* [out] */ IWebError** error,
    /* [retval][out] */ BOOL* succeeded)
{
    HRESULT hr = S_OK;
    CFMutableArrayRef discardedItems = CFArrayCreateMutable(0, 0, 0);
    CFURLRef urlRef = MarshallingHelpers::BSTRToCFURLRef(url);

    hr = loadHistoryGutsFromURL(urlRef, discardedItems, error);
    if (FAILED(hr))
        goto exit;

    hr = postNotification(kWebHistoryLoadedNotification);
    if (FAILED(hr))
        goto exit;

    if (CFArrayGetCount(discardedItems) > 0) {
        CFDictionaryPropertyBag* userInfo = createUserInfoFromArray(getNotificationString(kWebHistoryItemsDiscardedWhileLoadingNotification), discardedItems);
        hr = postNotification(kWebHistoryItemsDiscardedWhileLoadingNotification, userInfo);
        releaseUserInfo(userInfo);
        if (FAILED(hr))
            goto exit;
    }

exit:
    if (urlRef)
        CFRelease(urlRef);
    if (discardedItems) {
        CFIndex count = CFArrayGetCount(discardedItems);
        for (CFIndex i=0; i<count; i++) {
            IWebHistoryItem* item = (IWebHistoryItem*) CFArrayGetValueAtIndex(discardedItems, i);
            item->Release(); // release ref held by discarded items list (added in loadHistoryGutsFromURL)
        }
        CFRelease(discardedItems);
    }

    if (succeeded)
        *succeeded = SUCCEEDED(hr);
    return hr;
}

HRESULT WebHistory::loadHistoryGutsFromURL(CFURLRef url, CFMutableArrayRef discardedItems, IWebError** /*error*/) //FIXME
{
    CFPropertyListFormat format = kCFPropertyListBinaryFormat_v1_0 | kCFPropertyListXMLFormat_v1_0;
    CFReadStreamRef stream = 0;
    CFDictionaryRef historyList = 0;
    CFArrayRef datesArray = 0;
    CFNumberRef fileVersionObject = 0;
    HRESULT hr = S_OK;
    int numberOfItemsLoaded = 0;

    stream = CFReadStreamCreateWithFile(0, url);
    if (!stream) {
        hr = E_FAIL;
        goto exit;
    }

    if (!CFReadStreamOpen(stream)) {
        hr = E_FAIL;
        goto exit;
    }

    historyList = (CFDictionaryRef) CFPropertyListCreateFromStream(0, stream, 0, kCFPropertyListImmutable, &format, 0);
    if (!historyList) {
        hr = E_FAIL;
        goto exit;
    }

    fileVersionObject = (CFNumberRef) CFDictionaryGetValue(historyList, FileVersionKey);
    int fileVersion;
    if (!CFNumberGetValue(fileVersionObject, kCFNumberIntType, &fileVersion)) {
        hr = E_FAIL;
        goto exit;
    }
    if (fileVersion > currentFileVersion) {
        hr = E_FAIL;
        goto exit;
    }
    
    datesArray = (CFArrayRef) CFDictionaryGetValue(historyList, DatesArrayKey);

    int itemCountLimit;
    hr = historyItemLimit(&itemCountLimit);
    if (FAILED(hr))
        goto exit;

    CFAbsoluteTime limitDate;
    hr = ageLimitDate(&limitDate);
    if (FAILED(hr))
        goto exit;

    bool ageLimitPassed = false;
    bool itemLimitPassed = false;

    CFIndex itemCount = CFArrayGetCount(datesArray);
    for (CFIndex i=0; i<itemCount; i++) {
        CFDictionaryRef itemAsDictionary = (CFDictionaryRef) CFArrayGetValueAtIndex(datesArray, i);
        WebHistoryItem* item = WebHistoryItem::createInstance();
        hr = item->initFromDictionaryRepresentation((void*)itemAsDictionary);
        if (FAILED(hr))
            goto exit;
        // item without URL is useless; data on disk must have been bad; ignore
        BOOL hasURL;
        hr = item->hasURLString(&hasURL);
        if (FAILED(hr))
            goto exit;
        if (hasURL) {
            // Test against date limit. Since the items are ordered newest to oldest, we can stop comparing
            // once we've found the first item that's too old.
            if (!ageLimitPassed) {
                DATE lastVisitedTime;
                hr = item->lastVisitedTimeInterval(&lastVisitedTime);
                if (FAILED(hr))
                    goto exit;
                if (timeToDate(MarshallingHelpers::DATEToCFAbsoluteTime(lastVisitedTime)) <= limitDate)
                    ageLimitPassed = true;
            }
            if (ageLimitPassed || itemLimitPassed) {
                CFArrayAppendValue(discardedItems, item);
                item->AddRef(); // discardedItems array holds a ref to each item
            }
            else {
                addItem(item); // ref is added inside addItem
                ++numberOfItemsLoaded;
                if (numberOfItemsLoaded == itemCountLimit)
                    itemLimitPassed = true;
            }
        }
        item->Release(); // release ref from createInstance (if needed, ref will have been added by either discardedItems or addItem above)
    }

exit:
    if (stream) {
        CFReadStreamClose(stream);
        CFRelease(stream);
    }
    if (fileVersionObject)
        CFRelease(fileVersionObject);
    if (historyList)
        CFRelease(historyList);

    return hr;
}

HRESULT STDMETHODCALLTYPE WebHistory::saveToURL( 
    /* [in] */ BSTR url,
    /* [out] */ IWebError** error,
    /* [retval][out] */ BOOL* succeeded)
{
    HRESULT hr = S_OK;
    CFURLRef urlRef = MarshallingHelpers::BSTRToCFURLRef(url);

    hr = saveHistoryGuts(urlRef, error);

    if (succeeded)
        *succeeded = SUCCEEDED(hr);
    if (SUCCEEDED(hr)) {
        hr = postNotification(kWebHistorySavedNotification);
    }
    if (urlRef)
        CFRelease(urlRef);

    return hr;
}

HRESULT WebHistory::saveHistoryGuts(CFURLRef url, IWebError** error)
{
    HRESULT hr = S_OK;

    CFWriteStreamRef stream = 0;
    CFDictionaryRef dictionary = 0;
    CFMutableArrayRef entries = 0;

    // FIXME:  Correctly report error when new API is ready.
    if (error)
        *error = 0;

    stream = CFWriteStreamCreateWithFile(kCFAllocatorDefault, url);
    if (!stream) {
        hr = E_FAIL;
        goto exit;
    }

    if (!CFWriteStreamOpen(stream)) {
        hr = E_FAIL;
        goto exit;
    }

    hr = datesArray(&entries);
    if (FAILED(hr))
        goto exit;

    // create the outer dictionary
    int version = currentFileVersion;
    CFStringRef keys[2];
    CFTypeRef values[2];
    keys[0]   = DatesArrayKey;
    values[0] = entries;
    keys[1]   = FileVersionKey;
    values[1] = CFNumberCreate(0, kCFNumberIntType, &version);
    dictionary = CFDictionaryCreate(0, (const void**)keys, (const void**)values, sizeof(keys)/sizeof(keys[0]), 0, 0);

    if (!CFPropertyListWriteToStream(dictionary, stream, kCFPropertyListXMLFormat_v1_0, 0)) {
        hr = E_FAIL;
        goto exit;
    }

exit:
    if (dictionary) {
        CFRelease(dictionary);
        entries = 0;
    }
    if (entries)
        CFRelease(entries);
    if (stream) {
        CFWriteStreamClose(stream);
        CFRelease(stream);
    }
    return hr;
}

HRESULT WebHistory::datesArray(CFMutableArrayRef* datesArray)
{
    HRESULT hr = S_OK;

    *datesArray = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    
    // for each date with entries
    int dateCount = CFArrayGetCount(m_entriesByDate);
    for (int i=0; i<dateCount; i++) {

        // get the entries for that date
        CFArrayRef entries = (CFArrayRef) CFArrayGetValueAtIndex(m_entriesByDate, i);
        int entriesCount = CFArrayGetCount(entries);
        for (int j=entriesCount-1; j>=0; j--) {
            IWebHistoryItem* item = (IWebHistoryItem*) CFArrayGetValueAtIndex(entries, j);
            IWebHistoryItemPrivate* webHistoryItem;
            hr = item->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&webHistoryItem);
            if (FAILED(hr))
                goto exit;
            item->Release();
            CFDictionaryRef itemDict;
            hr = webHistoryItem->dictionaryRepresentation((void**)&itemDict);
            if (FAILED(hr)) {
                hr = E_FAIL;
                goto exit;
            }
            CFArrayAppendValue(*datesArray, itemDict);
        }
    }

exit:
    if (FAILED(hr)) {
        CFRelease(*datesArray);
        *datesArray = 0;
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE WebHistory::addItems( 
    /* [in] */ int itemCount,
    /* [in] */ IWebHistoryItem** items)
{
    // There is no guarantee that the incoming entries are in any particular
    // order, but if this is called with a set of entries that were created by
    // iterating through the results of orderedLastVisitedDays and orderedItemsLastVisitedOnDay
    // then they will be ordered chronologically from newest to oldest. We can make adding them
    // faster (fewer compares) by inserting them from oldest to newest.

    HRESULT hr;
    for (int i=itemCount-1; i>=0; i--) {
        hr = addItem(items[i]);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::removeItems( 
    /* [in] */ int itemCount,
    /* [in] */ IWebHistoryItem** items)
{
    HRESULT hr;
    for (int i=0; i<itemCount; i++) {
        hr = removeItem(items[i]);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

static void dictReleaseApplier(const void* /*key*/, const void* value, void* /*context*/)
{
    IWebHistoryItem* item = (IWebHistoryItem*) value;
    item->Release();
}

HRESULT STDMETHODCALLTYPE WebHistory::removeAllItems( void)
{
    CFArrayRemoveAllValues(m_entriesByDate);
    CFArrayRemoveAllValues(m_datesWithEntries);

    CFDictionaryRemoveAllValues(m_entriesByURL);
    CFDictionaryApplyFunction(m_entriesByURL, dictReleaseApplier, 0);

    return postNotification(kWebHistoryAllItemsRemovedNotification);
}

HRESULT STDMETHODCALLTYPE WebHistory::orderedLastVisitedDays( 
    /* [out][in] */ int* count,
    /* [in] */ DATE* calendarDates)
{
    int dateCount = CFArrayGetCount(m_datesWithEntries);
    if (!calendarDates) {
        *count = dateCount;
        return S_OK;
    }

    if (*count < dateCount) {
        *count = dateCount;
        return E_FAIL;
    }

    *count = dateCount;
    for (int i=0; i<dateCount; i++) {
        CFNumberRef absoluteTimeNumberRef = (CFNumberRef) CFArrayGetValueAtIndex(m_datesWithEntries, i);
        CFAbsoluteTime absoluteTime;
        if (!CFNumberGetValue(absoluteTimeNumberRef, kCFNumberDoubleType, &absoluteTime))
            return E_FAIL;
        calendarDates[i] = MarshallingHelpers::CFAbsoluteTimeToDATE(absoluteTime);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::orderedItemsLastVisitedOnDay( 
    /* [out][in] */ int* count,
    /* [in] */ IWebHistoryItem** items,
    /* [in] */ DATE calendarDate)
{
    int index;
    if (!findIndex(&index, MarshallingHelpers::DATEToCFAbsoluteTime(calendarDate))) {
        *count = 0;
        return 0;
    }

    CFArrayRef entries = (CFArrayRef) CFArrayGetValueAtIndex(m_entriesByDate, index);
    if (!entries) {
        *count = 0;
        return 0;
    }

    int newCount = CFArrayGetCount(entries);

    if (!items) {
        *count = newCount;
        return S_OK;
    }

    if (*count < newCount) {
        *count = newCount;
        return E_FAIL;
    }

    *count = newCount;
    for (int i=0; i<newCount; i++) {
        IWebHistoryItem* item = (IWebHistoryItem*) CFArrayGetValueAtIndex(entries, i);
        if (!item)
            return E_FAIL;
        item->AddRef();
        items[newCount-i-1] = item; // reverse when inserting to get the list sorted oldest to newest
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::itemForURL( 
    /* [in] */ BSTR url,
    /* [retval][out] */ IWebHistoryItem** item)
{
    if (!item)
        return E_FAIL;
    *item = 0;

    CFStringRef urlString = MarshallingHelpers::BSTRToCFStringRef(url);
    IWebHistoryItem* foundItem = (IWebHistoryItem*) CFDictionaryGetValue(m_entriesByURL, urlString);
    CFRelease(urlString);
    if (!foundItem)
        return E_FAIL;

    foundItem->AddRef();
    *item = foundItem;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::setHistoryItemLimit( 
    /* [in] */ int limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->setHistoryItemLimit(limit);
}

HRESULT STDMETHODCALLTYPE WebHistory::historyItemLimit( 
    /* [retval][out] */ int* limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->historyItemLimit(limit);
}

HRESULT STDMETHODCALLTYPE WebHistory::setHistoryAgeInDaysLimit( 
    /* [in] */ int limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->setHistoryAgeInDaysLimit(limit);
}

HRESULT STDMETHODCALLTYPE WebHistory::historyAgeInDaysLimit( 
    /* [retval][out] */ int* limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->historyAgeInDaysLimit(limit);
}

HRESULT WebHistory::removeItem(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;
    BSTR urlBStr = 0;
    CFStringRef urlString = 0;

    hr = entry->URLString(&urlBStr);
    if (FAILED(hr))
        goto exit;

    urlString = MarshallingHelpers::BSTRToCFStringRef(urlBStr);

    // If this exact object isn't stored, then make no change.
    // FIXME: Is this the right behavior if this entry isn't present, but another entry for the same URL is?
    // Maybe need to change the API to make something like removeEntryForURLString public instead.
    IWebHistoryItem *matchingEntry = (IWebHistoryItem*) CFDictionaryGetValue(m_entriesByURL, urlString);
    if (matchingEntry != entry) {
        hr = E_FAIL;
        goto exit;
    }
    hr = removeItemForURLString(urlString);
    if (FAILED(hr))
        goto exit;

    if (SUCCEEDED(hr)) {
        CFDictionaryPropertyBag* userInfo = createUserInfoFromHistoryItem(getNotificationString(kWebHistoryItemsRemovedNotification), entry);
        hr = postNotification(kWebHistoryItemsRemovedNotification, userInfo);
        releaseUserInfo(userInfo);
    }

exit:
    SysFreeString(urlBStr);
    if (urlString)
        CFRelease(urlString);

    return hr;
}

HRESULT WebHistory::addItem(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;
    BSTR urlBStr = 0;
    CFStringRef urlString = 0;

    if (!entry)
        return E_FAIL;

    hr = entry->URLString(&urlBStr);
    if (FAILED(hr))
        goto exit;

    urlString = MarshallingHelpers::BSTRToCFStringRef(urlBStr);

    IWebHistoryItem *oldEntry = (IWebHistoryItem*) CFDictionaryGetValue(m_entriesByURL, urlString);
    if (oldEntry) {
        // The last reference to oldEntry might be this dictionary, so we hold onto a reference
        // until we're done with oldEntry.
        oldEntry->AddRef();

        removeItemForURLString(urlString);

        // If we already have an item with this URL, we need to merge info that drives the
        // URL autocomplete heuristics from that item into the new one.
        IWebHistoryItemPrivate* entryPriv;
        hr = entry->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&entryPriv);
        if (SUCCEEDED(hr)) {
            entryPriv->mergeAutoCompleteHints(oldEntry);
            entryPriv->Release();
        }

        oldEntry->Release();
    }

    hr = addItemToDateCaches(entry);
    if (FAILED(hr))
        goto exit;

    CFDictionarySetValue(m_entriesByURL, urlString, entry);
    entry->AddRef(); // hold a ref to the item while it is in the history (released in removeItemForURLString)

    if (SUCCEEDED(hr)) {
        CFDictionaryPropertyBag* userInfo = createUserInfoFromHistoryItem(getNotificationString(kWebHistoryItemsAddedNotification), entry);
        hr = postNotification(kWebHistoryItemsAddedNotification, userInfo);
        releaseUserInfo(userInfo);
    }

exit:
    SysFreeString(urlBStr);
    if (urlString)
        CFRelease(urlString);
    return hr;
}

HRESULT WebHistory::addItemForURL(BSTR url, BSTR title)
{
    HRESULT hr = S_OK;

    WebHistoryItem* item = WebHistoryItem::createInstance();
    if (!item) {
        hr = E_FAIL;
        goto exit;
    }

    SYSTEMTIME currentTime;
    GetSystemTime(&currentTime);
    DATE lastVisited;
    if (!SystemTimeToVariantTime(&currentTime, &lastVisited))
        goto exit;

    hr = item->initWithURLString(url, title, lastVisited);
    if (FAILED(hr))
        goto exit;

    hr = addItem(item);
    if (FAILED(hr))
        goto exit;

    hr = item->setLastVisitedTimeInterval(lastVisited); // also increments visitedCount
    if (FAILED(hr))
        goto exit;

exit:
    if (item)
        item->Release();
    return hr;
}

HRESULT WebHistory::removeItemForURLString(CFStringRef urlString)
{
    HRESULT hr = S_OK;

    IWebHistoryItem* entry = (IWebHistoryItem*) CFDictionaryGetValue(m_entriesByURL, urlString);
    if (!entry) {
        hr = E_FAIL;
        goto exit;
    }

    CFDictionaryRemoveValue(m_entriesByURL, urlString);
    hr = removeItemFromDateCaches(entry);
    entry->Release(); // release the ref held by the history (added in addItem)

exit:
    return hr;
}

HRESULT WebHistory::addItemToDateCaches(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;

    DATE lastVisitedCOMTime;
    entry->lastVisitedTimeInterval(&lastVisitedCOMTime);
    CFAbsoluteTime lastVisitedDate = timeToDate(MarshallingHelpers::DATEToCFAbsoluteTime(lastVisitedCOMTime));
    
    int dateIndex;
    if (findIndex(&dateIndex, lastVisitedDate)) {
        // other entries already exist for this date
        hr = insertItem(entry, dateIndex);
    } else {
        // no other entries exist for this date
        CFNumberRef lastVisitedDateRef = CFNumberCreate(0, kCFNumberDoubleType, &lastVisitedDate);
        CFArrayInsertValueAtIndex(m_datesWithEntries, dateIndex, lastVisitedDateRef);
        CFMutableArrayRef entryArray = CFArrayCreateMutable(0, 0, 0);
        CFArrayAppendValue(entryArray, entry);
        CFArrayInsertValueAtIndex(m_entriesByDate, dateIndex, entryArray);
    }

    return hr;
}

HRESULT WebHistory::removeItemFromDateCaches(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;

    DATE lastVisitedCOMTime;
    entry->lastVisitedTimeInterval(&lastVisitedCOMTime);
    CFAbsoluteTime lastVisitedDate = timeToDate(MarshallingHelpers::DATEToCFAbsoluteTime(lastVisitedCOMTime));

    int dateIndex;
    if (!findIndex(&dateIndex, lastVisitedDate))
        return E_FAIL;

    CFMutableArrayRef entriesForDate = (CFMutableArrayRef) CFArrayGetValueAtIndex(m_entriesByDate, dateIndex);
    CFIndex count = CFArrayGetCount(entriesForDate);
    for (int i=count-1; i>=0; i--) {
        if ((IWebHistoryItem*)CFArrayGetValueAtIndex(entriesForDate, i) == entry)
            CFArrayRemoveValueAtIndex(entriesForDate, i);
    }

    // remove this date entirely if there are no other entries on it
    if (CFArrayGetCount(entriesForDate) == 0) {
        CFArrayRemoveValueAtIndex(m_entriesByDate, dateIndex);
        CFArrayRemoveValueAtIndex(m_datesWithEntries, dateIndex);
    }

    return hr;
}

// Returns whether the day is already in the list of days,
// and fills in *index with the found or proposed index.
bool WebHistory::findIndex(int* index, CFAbsoluteTime forDay)
{
    CFAbsoluteTime forDayInDays = timeToDate(forDay);

    //FIXME: just does linear search through days; inefficient if many days
    int count = CFArrayGetCount(m_datesWithEntries);
    for (*index = 0; *index < count; ++*index) {
        CFNumberRef entryTimeNumberRef = (CFNumberRef) CFArrayGetValueAtIndex(m_datesWithEntries, *index);
        CFAbsoluteTime entryTime;
        CFNumberGetValue(entryTimeNumberRef, kCFNumberDoubleType, &entryTime);
        CFAbsoluteTime entryInDays = timeToDate(entryTime);
        if (areEqualOrClose(forDayInDays, entryInDays))
            return true;
        else if (forDayInDays > entryInDays)
            return false;
    }
    return false;
}

HRESULT WebHistory::insertItem(IWebHistoryItem* entry, int dateIndex)
{
    HRESULT hr = S_OK;

    if (!entry)
        return E_FAIL;
    if (dateIndex < 0 || dateIndex >= CFArrayGetCount(m_entriesByDate))
        return E_FAIL;

    //FIXME: just does linear search through entries; inefficient if many entries for this date
    DATE entryTime;
    entry->lastVisitedTimeInterval(&entryTime);
    CFMutableArrayRef entriesForDate = (CFMutableArrayRef) CFArrayGetValueAtIndex(m_entriesByDate, dateIndex);
    int count = CFArrayGetCount(entriesForDate);
    // optimized for inserting oldest to youngest
    int index;
    for (index = 0; index < count; ++index) {
        IWebHistoryItem* indEntry = (IWebHistoryItem*) CFArrayGetValueAtIndex(entriesForDate, index);
        DATE indTime;
        hr = indEntry->lastVisitedTimeInterval(&indTime);
        if (FAILED(hr))
            return hr;
        if (entryTime < indTime)
            break;
    }    
    CFArrayInsertValueAtIndex(entriesForDate, index, entry);
    return S_OK;
}

CFAbsoluteTime WebHistory::timeToDate(CFAbsoluteTime time)
{
    // can't just divide/round since the day boundaries depend on our current time zone
    const double secondsPerDay = 60 * 60 * 24;
    CFTimeZoneRef timeZone = CFTimeZoneCopySystem();
    CFGregorianDate date = CFAbsoluteTimeGetGregorianDate(time, timeZone);
    date.hour = date.minute = 0;
    date.second = 0.0;
    CFAbsoluteTime timeInDays = CFGregorianDateGetAbsoluteTime(date, timeZone);
    if (areEqualOrClose(time - timeInDays, secondsPerDay))
        timeInDays += secondsPerDay;
    return timeInDays;
}

// Return a date that marks the age limit for history entries saved to or
// loaded from disk. Any entry older than this item should be rejected.
HRESULT WebHistory::ageLimitDate(CFAbsoluteTime* time)
{
    // get the current date as a CFAbsoluteTime
    CFAbsoluteTime currentDate = timeToDate(CFAbsoluteTimeGetCurrent());

    CFGregorianUnits ageLimit = {0};
    int historyLimitDays;
    HRESULT hr = historyAgeInDaysLimit(&historyLimitDays);
    if (FAILED(hr))
        return hr;
    ageLimit.days = -historyLimitDays;
    *time = CFAbsoluteTimeAddGregorianUnits(currentDate, CFTimeZoneCopySystem(), ageLimit);
    return S_OK;
}

IWebHistoryPrivate* WebHistory::optionalSharedHistoryInternal()
{
    if (!m_optionalSharedHistory)
        return 0;

    IWebHistoryPrivate* historyPrivate;
    if (FAILED(m_optionalSharedHistory->QueryInterface(IID_IWebHistoryPrivate, (void**)&historyPrivate)))
        return 0;

    historyPrivate->Release(); // don't add an additional ref for this internal call
    return historyPrivate;
}
