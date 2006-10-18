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

#include "WebPreferences.h"

#include "WebNotificationCenter.h"
#include "WebPreferenceKeysPrivate.h"

#pragma warning( push, 0 )
#include <wtf/Vector.h>
#include <WebCore/platform/Font.h>
#pragma warning( pop )

#include <CoreFoundation/CoreFoundation.h>
#include <shlobj.h>
#include <shfolder.h>
#include <tchar.h>

// WebPreferences ----------------------------------------------------------------

CFMutableDictionaryRef WebPreferences::m_standardUserDefaults = 0;
WebPreferences* WebPreferences::m_standardPreferences = 0;

WebPreferences::WebPreferences()
: m_refCount(0)
, m_privatePrefs(0)
, m_autoSaves(0)
{
    gClassCount++;
}

WebPreferences::~WebPreferences()
{
    if (m_privatePrefs)
        CFRelease(m_privatePrefs);
    gClassCount--;
}

WebPreferences* WebPreferences::createInstance()
{
    WebPreferences* instance = new WebPreferences();
    instance->AddRef();
    return instance;
}

HRESULT WebPreferences::postPreferencesChangesNotification()
{
    IWebNotificationCenter* nc = WebNotificationCenter::defaultCenterInternal();
    HRESULT hr = nc->postNotificationName(webPreferencesChangedNotification(), this, 0);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

WebPreferences* WebPreferences::getInstanceForIdentifier(BSTR identifier)
{
    if (!identifier) {
        if (m_standardPreferences)
            return m_standardPreferences;
        return 0;
    }    
    
    //WebPreferences *instance = [webPreferencesInstances objectForKey:[self _concatenateKeyWithIBCreatorID:ident]];
    DebugBreak(); //FIXME
    return 0;
}

void WebPreferences::initialize()
{
    WebCore::Font::setCodePath(WebCore::Font::Simple);

    if (!m_standardUserDefaults)
        m_standardUserDefaults = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFStringRef key = CFSTR(WebKitStandardFontPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("Times New Roman"));

    key = CFSTR(WebKitFixedFontPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("Courier New"));
    
    key = CFSTR(WebKitSerifFontPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("Times New Roman"));
    
    key = CFSTR(WebKitSansSerifFontPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("Arial"));

    key = CFSTR(WebKitCursiveFontPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("Comic Sans MS"));

    key = CFSTR(WebKitFantasyFontPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("Comic Sans MS"));

    key = CFSTR(WebKitMinimumFontSizePreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("1"));

    key = CFSTR(WebKitMinimumLogicalFontSizePreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("9"));

    key = CFSTR(WebKitDefaultFontSizePreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("16"));

    key = CFSTR(WebKitDefaultFixedFontSizePreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("13"));

    key = CFSTR(WebKitDefaultTextEncodingNamePreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("ISO-8859-1"));

    key = CFSTR(WebKitPageCacheSizePreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("3"));

    key = CFSTR(WebKitObjectCacheSizePreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("16777216"));

    key = CFSTR(WebKitUserStyleSheetEnabledPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanFalse);

    key = CFSTR(WebKitUserStyleSheetLocationPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR(""));

    key = CFSTR(WebKitShouldPrintBackgroundsPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanFalse);

    key = CFSTR(WebKitTextAreasAreResizablePreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanFalse);

    key = CFSTR(WebKitJavaEnabledPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanTrue);

    key = CFSTR(WebKitJavaScriptEnabledPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanTrue);

    key = CFSTR(WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanTrue);

    key = CFSTR(WebKitPluginsEnabledPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanTrue);

    key = CFSTR(WebKitAllowAnimatedImagesPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanTrue);

    key = CFSTR(WebKitAllowAnimatedImageLoopingPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanTrue);

    key = CFSTR(WebKitDisplayImagesKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanTrue);

    key = CFSTR(WebKitBackForwardCacheExpirationIntervalKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("1800"));

    key = CFSTR(WebKitTabToLinksPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanFalse);

    key = CFSTR(WebKitPrivateBrowsingEnabledPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanFalse);

    key = CFSTR(WebKitRespectStandardStyleKeyEquivalentsPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanFalse);

    key = CFSTR(WebKitShowsURLsInToolTipsPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanFalse);

    key = CFSTR(WebKitPDFDisplayModePreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("1"));

    key = CFSTR(WebKitPDFScaleFactorPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("0"));

    key = CFSTR(WebKitHistoryItemLimitKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("1000"));

    key = CFSTR(WebKitHistoryAgeInDaysLimitKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("7"));

    key = CFSTR(WebKitIconDatabaseLocationKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR(""));

    key = CFSTR(WebKitIconDatabaseEnabledPreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               kCFBooleanTrue);

    key = CFSTR(WebKitFontSmothingTypePreferenceKey);
    if (!CFDictionaryContainsKey(m_standardUserDefaults, key))
        CFDictionaryAddValue(m_standardUserDefaults, key,                               CFSTR("2"));
}

const void* WebPreferences::valueForKey(CFStringRef key)
{
    const void* value = CFDictionaryGetValue(m_privatePrefs, key);
    if (!value)
        value = CFDictionaryGetValue(m_standardUserDefaults, key);

    return value;
}

BSTR WebPreferences::stringValueForKey(CFStringRef key)
{
    CFStringRef str = (CFStringRef) valueForKey(key);
    
    if (!str || (CFGetTypeID(str) != CFStringGetTypeID()))
        return 0;

    CFIndex length = CFStringGetLength(str);
    const UniChar* uniChars = CFStringGetCharactersPtr(str);
    if (uniChars)
        return SysAllocStringLen((LPCTSTR)uniChars, length);

    BSTR bstr = SysAllocStringLen(0, length);
    if (!bstr)
        return 0;

    if (!CFStringGetCString(str, (char*)bstr, (length+1)*sizeof(WCHAR), kCFStringEncodingUTF16)) {
        SysFreeString(bstr);
        return 0;
    }
        
    bstr[length] = 0;
    return bstr;
}

int WebPreferences::integerValueForKey(CFStringRef key)
{
    CFTypeRef cfVal = (CFStringRef) valueForKey(key);
    if (!cfVal)
        return 0;

    CFTypeID cfType = CFGetTypeID(cfVal);
    if (cfType == CFStringGetTypeID())
        return CFStringGetIntValue((CFStringRef)cfVal);
    else if (cfType == CFBooleanGetTypeID()) {
        Boolean boolVal = CFBooleanGetValue((CFBooleanRef)cfVal);
        return (boolVal) ? 1 : 0;
    }
    else if (cfType == CFNumberGetTypeID()) {
        int val = 0;
        CFNumberGetValue((CFNumberRef)cfVal, kCFNumberSInt32Type, &val);
        return val;
    }

    return 0;
}

BOOL WebPreferences::boolValueForKey(CFStringRef key)
{
    return (integerValueForKey(key) != 0);
}

float WebPreferences::floatValueForKey(CFStringRef key)
{
    CFTypeRef cfVal = (CFStringRef) valueForKey(key);
    if (!cfVal)
        return 0.0;

    CFTypeID cfType = CFGetTypeID(cfVal);
    if (cfType == CFStringGetTypeID())
        return (float)CFStringGetDoubleValue((CFStringRef)cfVal);
    else if (cfType == CFBooleanGetTypeID()) {
        Boolean boolVal = CFBooleanGetValue((CFBooleanRef)cfVal);
        return (boolVal) ? 1.0f : 0.0f;
    }
    else if (cfType == CFNumberGetTypeID()) {
        float val = 0.0;
        CFNumberGetValue((CFNumberRef)cfVal, kCFNumberFloatType, &val);
        return val;
    }

    return 0.0;
}

void WebPreferences::setStringValue(CFStringRef key, LPCTSTR value)
{
    BSTR val = stringValueForKey(key);
    if (val && !_tcscmp(val, value))
        return;
    SysFreeString(val);
    
    CFStringRef valueRef = CFStringCreateWithCharactersNoCopy(0, (UniChar*)_wcsdup(value), (CFIndex)_tcslen(value), kCFAllocatorMalloc);
    CFDictionarySetValue(m_privatePrefs, key, valueRef);
    if (m_autoSaves) {
        CFDictionarySetValue(m_standardUserDefaults, key, valueRef);
        save();
    }
    CFRelease(valueRef);

    postPreferencesChangesNotification();
}

BSTR WebPreferences::webPreferencesChangedNotification()
{
    static BSTR webPreferencesChangedNotification = SysAllocString(WebPreferencesChangedNotification);
    return webPreferencesChangedNotification;
}

void WebPreferences::setIntegerValue(CFStringRef key, int value)
{
    if (integerValueForKey(key) == value)
        return;

    CFNumberRef valueRef = CFNumberCreate(0, kCFNumberSInt32Type, &value);
    CFDictionarySetValue(m_privatePrefs, key, valueRef);
    if (m_autoSaves) {
        CFDictionarySetValue(m_standardUserDefaults, key, valueRef);
        save();
    }
    CFRelease(valueRef);

    postPreferencesChangesNotification();
}

void WebPreferences::setBoolValue(CFStringRef key, BOOL value)
{
    if (boolValueForKey(key) == value)
        return;

    CFDictionarySetValue(m_privatePrefs, key, value ? kCFBooleanTrue : kCFBooleanFalse);
    if (m_autoSaves) {
        CFDictionarySetValue(m_standardUserDefaults, key, value ? kCFBooleanTrue : kCFBooleanFalse);
        save();
    }

    postPreferencesChangesNotification();
}

void WebPreferences::save()
{
    CFWriteStreamRef stream = 0;
    CFDataRef dataRef = 0;
    TCHAR appDataPath[MAX_PATH];

    HRESULT hr = preferencesPath(appDataPath, sizeof(appDataPath)/sizeof(appDataPath[0]));
    if (FAILED(hr))
        goto exit;

    stream = CFWriteStreamCreateWithAllocatedBuffers(kCFAllocatorDefault, kCFAllocatorDefault);
    if (!stream)
        goto exit;

    if (!CFWriteStreamOpen(stream))
        goto exit;

    if (!CFPropertyListWriteToStream(m_standardUserDefaults, stream, kCFPropertyListXMLFormat_v1_0, 0))
        goto exit;

    CFWriteStreamClose(stream);
    dataRef = (CFDataRef) CFWriteStreamCopyProperty(stream, kCFStreamPropertyDataWritten);
    if (!dataRef)
        goto exit;

    void* bytes = (void*) CFDataGetBytePtr(dataRef);
    size_t length = CFDataGetLength(dataRef);
    safeCreateFileWithData(appDataPath, bytes, length);

exit:
    if (dataRef)
        CFRelease(dataRef);
    if (stream) {
        CFWriteStreamClose(stream);
        CFRelease(stream);
    }
}

void WebPreferences::load()
{
    TCHAR appDataPath[MAX_PATH];
    CFURLRef urlRef = 0;
    CFPropertyListFormat format = kCFPropertyListBinaryFormat_v1_0 | kCFPropertyListXMLFormat_v1_0;
    CFReadStreamRef stream = 0;
    Vector<UInt8>* utf8Path = 0;

    HRESULT hr = preferencesPath(appDataPath, sizeof(appDataPath)/sizeof(appDataPath[0]));
    if (FAILED(hr))
        goto exit;

    DWORD appDataPathLength = (DWORD) _tcslen(appDataPath)+1;
    int result = WideCharToMultiByte(CP_UTF8, 0, appDataPath, appDataPathLength, 0, 0, 0, 0);
    utf8Path = new Vector<UInt8>(result);
    result = WideCharToMultiByte(CP_UTF8, 0, appDataPath, appDataPathLength, (LPSTR)utf8Path->data(), result, 0, 0);
    if (!result) {
        hr = E_FAIL;
        goto exit;
    }

    urlRef = CFURLCreateFromFileSystemRepresentation(0, *utf8Path, result-1, false);
    if (!urlRef) {
        hr = E_FAIL;
        goto exit;
    }

    stream = CFReadStreamCreateWithFile(0, urlRef);
    if (!stream) {
        hr = E_FAIL;
        goto exit;
    }

    if (!CFReadStreamOpen(stream)) {
        hr = E_FAIL;
        goto exit;
    }

    m_standardUserDefaults = (CFMutableDictionaryRef) CFPropertyListCreateFromStream(0, stream, 0, kCFPropertyListMutableContainersAndLeaves, &format, 0);

exit:
    delete utf8Path;

    if (stream) {
        CFReadStreamClose(stream);
        CFRelease(stream);
    }
    if (urlRef)
        CFRelease(urlRef);

    if (FAILED(hr)) {
        if (m_standardUserDefaults)
            CFRelease(m_standardUserDefaults);
        m_standardUserDefaults = 0;
    }

    initialize();
}

HRESULT WebPreferences::preferencesPath(LPTSTR path, size_t cchPath)
{
    // get the path to the user's Application Data folder (Example: C:\Documents and Settings\{username}\Application Data\Apple Computer\WebKit)
    HRESULT hr = FAILED(SHGetFolderPath(0, CSIDL_APPDATA | CSIDL_FLAG_CREATE, 0, 0, path));
    if (FAILED(hr))
        return hr;
    if (_tcscat_s(path, cchPath, TEXT("\\Apple Computer\\WebKit")))
        return E_FAIL;
    int err = SHCreateDirectoryEx(0, path, 0);
    if (err != ERROR_SUCCESS && err != ERROR_ALREADY_EXISTS)
        return E_FAIL;

    if (_tcscat_s(path, cchPath, TEXT("\\Preferences.plist")))
        return E_FAIL;

    return S_OK;
}

HRESULT WebPreferences::safeCreateFileWithData(LPCTSTR path, void* data, size_t length)
{
    TCHAR tempDirPath[MAX_PATH];
    TCHAR tempPath[MAX_PATH];
    HANDLE tempFileHandle = INVALID_HANDLE_VALUE;
    HRESULT hr = S_OK;
    tempPath[0] = 0;

    // create a temporary file
    if (!GetTempPath(sizeof(tempDirPath)/sizeof(tempDirPath[0]), tempDirPath)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto exit;
    }
    if (!GetTempFileName(tempDirPath, TEXT("WEBKIT"), 0, tempPath))
        return HRESULT_FROM_WIN32(GetLastError());
    tempFileHandle = CreateFile(tempPath, GENERIC_READ|GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (tempFileHandle == INVALID_HANDLE_VALUE) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto exit;
    }

    // write the data to this temp file
    DWORD written;
    if (!WriteFile(tempFileHandle, data, (DWORD)length, &written, 0)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto exit;
    }

    // copy the temp file to the destination file
    CloseHandle(tempFileHandle);
    tempFileHandle = INVALID_HANDLE_VALUE;
    if (!MoveFileEx(tempPath, path, MOVEFILE_REPLACE_EXISTING)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto exit;
    }

exit:
    if (tempFileHandle != INVALID_HANDLE_VALUE)
        CloseHandle(tempFileHandle);
    if (tempPath[0])
        DeleteFile(tempPath);

    return hr;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebPreferences::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebPreferences*>(this);
    else if (IsEqualGUID(riid, IID_IWebPreferences))
        *ppvObject = static_cast<IWebPreferences*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebPreferences::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebPreferences::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebPreferences ------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebPreferences::standardPreferences( 
    /* [retval][out] */ IWebPreferences** standardPreferences)
{
    HRESULT hr = S_OK;

    if (!m_standardPreferences) {
        IWebPreferences* standardPrefs;
        hr = initWithIdentifier(0, &standardPrefs);
        if (FAILED(hr))
            return hr;
        m_standardPreferences = static_cast<WebPreferences*>(standardPrefs);
        hr = m_standardPreferences->setAutosaves(TRUE);
        if (FAILED(hr))
            return hr;
        hr = m_standardPreferences->postPreferencesChangesNotification();
        if (FAILED(hr))
            return hr;
    }

    m_standardPreferences->AddRef();
    *standardPreferences = m_standardPreferences;
    return hr;
}

HRESULT STDMETHODCALLTYPE WebPreferences::initWithIdentifier( 
        /* [in] */ BSTR anIdentifier,
        /* [retval][out] */ IWebPreferences** preferences)
{
    WebPreferences *instance = getInstanceForIdentifier(anIdentifier);
    if (instance) {
        *preferences = instance;
        instance->AddRef();
        return S_OK;
    }

    load();

    if (m_privatePrefs)
        CFRelease(m_privatePrefs);
    m_privatePrefs = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    *preferences = this;
    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::identifier( 
    /* [retval][out] */ BSTR* /*ident*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::standardFontFamily( 
    /* [retval][out] */ BSTR* family)
{
    *family = stringValueForKey(CFSTR(WebKitStandardFontPreferenceKey));
    return (*family) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setStandardFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(CFSTR(WebKitStandardFontPreferenceKey), family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::fixedFontFamily( 
    /* [retval][out] */ BSTR* family)
{
    *family = stringValueForKey(CFSTR(WebKitFixedFontPreferenceKey));
    return (*family) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setFixedFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(CFSTR(WebKitFixedFontPreferenceKey), family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::serifFontFamily( 
    /* [retval][out] */ BSTR* fontFamily)
{
    *fontFamily = stringValueForKey(CFSTR(WebKitSerifFontPreferenceKey));
    return (*fontFamily) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setSerifFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(CFSTR(WebKitSerifFontPreferenceKey), family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::sansSerifFontFamily( 
    /* [retval][out] */ BSTR* family)
{
    *family = stringValueForKey(CFSTR(WebKitSansSerifFontPreferenceKey));
    return (*family) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setSansSerifFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(CFSTR(WebKitSansSerifFontPreferenceKey), family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::cursiveFontFamily( 
    /* [retval][out] */ BSTR* family)
{
    *family = stringValueForKey(CFSTR(WebKitCursiveFontPreferenceKey));
    return (*family) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setCursiveFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(CFSTR(WebKitCursiveFontPreferenceKey), family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::fantasyFontFamily( 
    /* [retval][out] */ BSTR* family)
{
    *family = stringValueForKey(CFSTR(WebKitFantasyFontPreferenceKey));
    return (*family) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setFantasyFontFamily( 
    /* [in] */ BSTR family)
{
    setStringValue(CFSTR(WebKitFantasyFontPreferenceKey), family);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::defaultFontSize( 
    /* [retval][out] */ int* fontSize)
{
    *fontSize = integerValueForKey(CFSTR(WebKitDefaultFontSizePreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setDefaultFontSize( 
    /* [in] */ int fontSize)
{
    setIntegerValue(CFSTR(WebKitDefaultFontSizePreferenceKey), fontSize);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::defaultFixedFontSize( 
    /* [retval][out] */ int* fontSize)
{
    *fontSize = integerValueForKey(CFSTR(WebKitDefaultFixedFontSizePreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setDefaultFixedFontSize( 
    /* [in] */ int fontSize)
{
    setIntegerValue(CFSTR(WebKitDefaultFixedFontSizePreferenceKey), fontSize);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::minimumFontSize( 
    /* [retval][out] */ int* fontSize)
{
    *fontSize = integerValueForKey(CFSTR(WebKitMinimumFontSizePreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setMinimumFontSize( 
    /* [in] */ int fontSize)
{
    setIntegerValue(CFSTR(WebKitMinimumFontSizePreferenceKey), fontSize);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::minimumLogicalFontSize( 
    /* [retval][out] */ int* fontSize)
{
    *fontSize = integerValueForKey(CFSTR(WebKitMinimumLogicalFontSizePreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setMinimumLogicalFontSize( 
    /* [in] */ int fontSize)
{
    setIntegerValue(CFSTR(WebKitMinimumLogicalFontSizePreferenceKey), fontSize);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::defaultTextEncodingName( 
    /* [retval][out] */ BSTR* name)
{
    *name = stringValueForKey(CFSTR(WebKitDefaultTextEncodingNamePreferenceKey));
    return (*name) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setDefaultTextEncodingName( 
    /* [in] */ BSTR name)
{
    setStringValue(CFSTR(WebKitDefaultTextEncodingNamePreferenceKey), name);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::userStyleSheetEnabled( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(CFSTR(WebKitUserStyleSheetEnabledPreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setUserStyleSheetEnabled( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(CFSTR(WebKitUserStyleSheetEnabledPreferenceKey), enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::userStyleSheetLocation( 
    /* [retval][out] */ BSTR* location)
{
    *location = stringValueForKey(CFSTR(WebKitUserStyleSheetLocationPreferenceKey));
    return (*location) ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setUserStyleSheetLocation( 
    /* [in] */ BSTR location)
{
    setStringValue(CFSTR(WebKitUserStyleSheetLocationPreferenceKey), location);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::isJavaEnabled( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(CFSTR(WebKitJavaEnabledPreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setJavaEnabled( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(CFSTR(WebKitJavaEnabledPreferenceKey), enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::isJavaScriptEnabled( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(CFSTR(WebKitJavaScriptEnabledPreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setJavaScriptEnabled( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(CFSTR(WebKitJavaScriptEnabledPreferenceKey), enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::javaScriptCanOpenWindowsAutomatically( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(CFSTR(WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setJavaScriptCanOpenWindowsAutomatically( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(CFSTR(WebKitJavaScriptCanOpenWindowsAutomaticallyPreferenceKey), enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::arePlugInsEnabled( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(CFSTR(WebKitPluginsEnabledPreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setPlugInsEnabled( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(CFSTR(WebKitPluginsEnabledPreferenceKey), enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::allowsAnimatedImages( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(CFSTR(WebKitAllowAnimatedImagesPreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setAllowsAnimatedImages( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(CFSTR(WebKitAllowAnimatedImagesPreferenceKey), enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::allowAnimatedImageLooping( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(CFSTR(WebKitAllowAnimatedImageLoopingPreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setAllowAnimatedImageLooping( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(CFSTR(WebKitAllowAnimatedImageLoopingPreferenceKey), enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setLoadsImagesAutomatically( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(CFSTR(WebKitDisplayImagesKey), enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::loadsImagesAutomatically( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(CFSTR(WebKitDisplayImagesKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setAutosaves( 
    /* [in] */ BOOL enabled)
{
    m_autoSaves = !!enabled;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::autosaves( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = m_autoSaves ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setShouldPrintBackgrounds( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(CFSTR(WebKitShouldPrintBackgroundsPreferenceKey), enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::shouldPrintBackgrounds( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(CFSTR(WebKitShouldPrintBackgroundsPreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setPrivateBrowsingEnabled( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(CFSTR(WebKitPrivateBrowsingEnabledPreferenceKey), enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::privateBrowsingEnabled( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(CFSTR(WebKitPrivateBrowsingEnabledPreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setTabsToLinks( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(CFSTR(WebKitTabToLinksPreferenceKey), enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::tabsToLinks( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(CFSTR(WebKitTabToLinksPreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::textAreasAreResizable( 
    /* [retval][out] */ BOOL* enabled)
{
    *enabled = boolValueForKey(CFSTR(WebKitTextAreasAreResizablePreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setTextAreasAreResizable( 
    /* [in] */ BOOL enabled)
{
    setBoolValue(CFSTR(WebKitTextAreasAreResizablePreferenceKey), enabled);
    return S_OK;
}

HRESULT WebPreferences::historyItemLimit(int* limit)
{
    *limit = integerValueForKey(CFSTR(WebKitHistoryItemLimitKey));
    return S_OK;
}

HRESULT WebPreferences::setHistoryItemLimit(int limit)
{
    setIntegerValue(CFSTR(WebKitHistoryItemLimitKey), limit);
    return S_OK;
}

HRESULT WebPreferences::historyAgeInDaysLimit(int* limit)
{
    *limit = integerValueForKey(CFSTR(WebKitHistoryAgeInDaysLimitKey));
    return S_OK;
}

HRESULT WebPreferences::setHistoryAgeInDaysLimit(int limit)
{
    setIntegerValue(CFSTR(WebKitHistoryAgeInDaysLimitKey), limit);
    return S_OK;
}

HRESULT WebPreferences::pageCacheSize(unsigned int* limit)
{
    *limit = integerValueForKey(CFSTR(WebKitPageCacheSizePreferenceKey));
    return S_OK;
}

HRESULT WebPreferences::objectCacheSize(unsigned int* limit)
{
    *limit = integerValueForKey(CFSTR(WebKitObjectCacheSizePreferenceKey));
    return S_OK;
}

HRESULT WebPreferences::iconDatabaseLocation(
    /* [out] */ BSTR* location)
{
    *location = stringValueForKey(CFSTR(WebKitIconDatabaseLocationKey));
    return (*location) ? S_OK : E_FAIL;
}

HRESULT WebPreferences::setIconDatabaseLocation(
    /* [in] */ BSTR location)
{
    setStringValue(CFSTR(WebKitIconDatabaseLocationKey), location);
    return S_OK;
}

HRESULT WebPreferences::iconDatabaseEnabled(BOOL* enabled)//location)
{
    *enabled = boolValueForKey(CFSTR(WebKitIconDatabaseEnabledPreferenceKey));
    return S_OK;
}

HRESULT WebPreferences::setIconDatabaseEnabled(BOOL enabled )//location)
{
    setBoolValue(CFSTR(WebKitIconDatabaseEnabledPreferenceKey), enabled);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::fontSmoothing( 
    /* [retval][out] */ FontSmoothingType* smoothingType)
{
    *smoothingType = (FontSmoothingType) integerValueForKey(CFSTR(WebKitFontSmothingTypePreferenceKey));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebPreferences::setFontSmoothing( 
    /* [in] */ FontSmoothingType smoothingType)
{
    setIntegerValue(CFSTR(WebKitFontSmothingTypePreferenceKey), smoothingType);
    return S_OK;
}