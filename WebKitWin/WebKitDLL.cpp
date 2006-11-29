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

#include "IWebURLResponse.h"
#include "WebKit.h"
#include "WebKitClassFactory.h"
#include "resource.h"
#pragma warning( push, 0 )
#include <WebCore/Page.h>
#include <WebCore/Widget.h>
#include <wtf/Vector.h>
#pragma warning(pop)
#include <tchar.h>
#include <olectl.h>

ULONG gLockCount;
ULONG gClassCount;
HINSTANCE gInstance;

STDAPI_(BOOL) DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID /*lpReserved*/)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            gLockCount = gClassCount = 0;
            gInstance = hModule;
            WebCore::Page::setInstanceHandle(hModule);
            return TRUE;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    if (!IsEqualGUID(rclsid, CLSID_WebView) && !IsEqualGUID(rclsid, CLSID_WebIconDatabase) && !IsEqualGUID(rclsid, CLSID_WebMutableURLRequest) && 
        !IsEqualGUID(rclsid, CLSID_WebNotificationCenter) && !IsEqualGUID(rclsid, CLSID_WebIconDatabase) && !IsEqualGUID(rclsid, CLSID_WebHistory) && 
        !IsEqualGUID(rclsid, CLSID_CFDictionaryPropertyBag) && !IsEqualGUID(rclsid, CLSID_WebHistoryItem))
    return E_FAIL;

    if (!IsEqualGUID(riid, IID_IUnknown) && !IsEqualGUID(riid, IID_IClassFactory))
        return E_NOINTERFACE;

    WebKitClassFactory* factory = new WebKitClassFactory(rclsid);
    *ppv = reinterpret_cast<LPVOID>(factory);
    if (!factory)
        return E_OUTOFMEMORY;

    factory->AddRef();
    return S_OK;
}

STDAPI DllCanUnloadNow(void)
{
    if (!gClassCount && !gLockCount)
        return S_OK;
    
    return S_FALSE;
}

static CLSID gRegCLSIDs[] = {
    CLSID_WebView,
    CLSID_WebIconDatabase,
    CLSID_WebMutableURLRequest,
    CLSID_WebNotificationCenter,
    CLSID_WebHistory,
    CLSID_CFDictionaryPropertyBag,
    CLSID_WebHistoryItem
};

static const int gSlotsPerEntry = 7;
static LPCTSTR gRegTable[][3] = {
//key                                                                                       value name              value }

{ TEXT("CLSID\\{########-####-####-####-############}"),                                    0,                      TEXT("WebView") },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    0,                      (LPCTSTR)-1 },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    TEXT("ThreadingModel"), TEXT("Apartment") },
{ TEXT("CLSID\\{########-####-####-####-############}\\ProgID"),                            0,                      TEXT("WebKit.WebView.3") },
{ TEXT("CLSID\\{########-####-####-####-############}\\VersionIndependentProgID"),          0,                      TEXT("WebKit.WebView") },
{ TEXT("WebKit.WebView.3"),                                                                 0,                      TEXT("WebView") },
{ TEXT("WebKit.WebView.3\\CLSID"),                                                          0,                      TEXT("{########-####-####-####-############}") },

{ TEXT("CLSID\\{########-####-####-####-############}"),                                    0,                      TEXT("WebIconDatabase") },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    0,                      (LPCTSTR)-1 },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    TEXT("ThreadingModel"), TEXT("Apartment") },
{ TEXT("CLSID\\{########-####-####-####-############}\\ProgID"),                            0,                      TEXT("WebKit.WebIconDatabase.3") },
{ TEXT("CLSID\\{########-####-####-####-############}\\VersionIndependentProgID"),          0,                      TEXT("WebKit.WebIconDatabase") },
{ TEXT("WebKit.WebIconDatabase.3"),                                                         0,                      TEXT("WebIconDatabase") },
{ TEXT("WebKit.WebIconDatabase.3\\CLSID"),                                                  0,                      TEXT("{########-####-####-####-############}") },

{ TEXT("CLSID\\{########-####-####-####-############}"),                                    0,                      TEXT("WebMutableURLRequest") },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    0,                      (LPCTSTR)-1 },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    TEXT("ThreadingModel"), TEXT("Apartment") },
{ TEXT("CLSID\\{########-####-####-####-############}\\ProgID"),                            0,                      TEXT("WebKit.WebMutableURLRequest.3") },
{ TEXT("CLSID\\{########-####-####-####-############}\\VersionIndependentProgID"),          0,                      TEXT("WebKit.WebMutableURLRequest") },
{ TEXT("WebKit.WebMutableURLRequest.3"),                                                    0,                      TEXT("WebMutableURLRequest") },
{ TEXT("WebKit.WebMutableURLRequest.3\\CLSID"),                                             0,                      TEXT("{########-####-####-####-############}") },

{ TEXT("CLSID\\{########-####-####-####-############}"),                                    0,                      TEXT("WebNotificationCenter") },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    0,                      (LPCTSTR)-1 },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    TEXT("ThreadingModel"), TEXT("Apartment") },
{ TEXT("CLSID\\{########-####-####-####-############}\\ProgID"),                            0,                      TEXT("WebKit.WebNotificationCenter.3") },
{ TEXT("CLSID\\{########-####-####-####-############}\\VersionIndependentProgID"),          0,                      TEXT("WebKit.WebNotificationCenter") },
{ TEXT("WebKit.WebNotificationCenter.3"),                                                   0,                      TEXT("WebNotificationCenter") },
{ TEXT("WebKit.WebNotificationCenter.3\\CLSID"),                                            0,                      TEXT("{########-####-####-####-############}") },

{ TEXT("CLSID\\{########-####-####-####-############}"),                                    0,                      TEXT("WebHistory") },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    0,                      (LPCTSTR)-1 },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    TEXT("ThreadingModel"), TEXT("Apartment") },
{ TEXT("CLSID\\{########-####-####-####-############}\\ProgID"),                            0,                      TEXT("WebKit.WebHistory.3") },
{ TEXT("CLSID\\{########-####-####-####-############}\\VersionIndependentProgID"),          0,                      TEXT("WebKit.WebHistory") },
{ TEXT("WebKit.WebHistory.3"),                                                              0,                      TEXT("WebHistory") },
{ TEXT("WebKit.WebHistory.3\\CLSID"),                                                       0,                      TEXT("{########-####-####-####-############}") },

{ TEXT("CLSID\\{########-####-####-####-############}"),                                    0,                      TEXT("CFDictionaryPropertyBag") },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    0,                      (LPCTSTR)-1 },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    TEXT("ThreadingModel"), TEXT("Apartment") },
{ TEXT("CLSID\\{########-####-####-####-############}\\ProgID"),                            0,                      TEXT("WebKit.CFDictionaryPropertyBag.3") },
{ TEXT("CLSID\\{########-####-####-####-############}\\VersionIndependentProgID"),          0,                      TEXT("WebKit.CFDictionaryPropertyBag") },
{ TEXT("WebKit.CFDictionaryPropertyBag.3"),                                                 0,                      TEXT("CFDictionaryPropertyBag") },
{ TEXT("WebKit.CFDictionaryPropertyBag.3\\CLSID"),                                          0,                      TEXT("{########-####-####-####-############}") },

{ TEXT("CLSID\\{########-####-####-####-############}"),                                    0,                      TEXT("WebHistoryItem") },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    0,                      (LPCTSTR)-1 },
{ TEXT("CLSID\\{########-####-####-####-############}\\InprocServer32"),                    TEXT("ThreadingModel"), TEXT("Apartment") },
{ TEXT("CLSID\\{########-####-####-####-############}\\ProgID"),                            0,                      TEXT("WebKit.WebHistoryItem.3") },
{ TEXT("CLSID\\{########-####-####-####-############}\\VersionIndependentProgID"),          0,                      TEXT("WebKit.WebHistoryItem") },
{ TEXT("WebKit.WebHistoryItem.3"),                                                          0,                      TEXT("WebHistoryItem") },
{ TEXT("WebKit.WebHistoryItem.3\\CLSID"),                                                   0,                      TEXT("{########-####-####-####-############}") }
};

static void substituteGUID(LPTSTR str, const UUID* guid)
{
    if (!guid || !str)
        return;

    TCHAR uuidString[40];
    _stprintf_s(uuidString, ARRAYSIZE(uuidString), TEXT("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"), guid->Data1, guid->Data2, guid->Data3,
        guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);

    LPCTSTR guidPattern = TEXT("########-####-####-####-############");
    size_t patternLength = _tcslen(guidPattern);
    size_t strLength = _tcslen(str);
    LPTSTR guidSubStr = str;
    while (strLength) {
        guidSubStr = _tcsstr(guidSubStr, guidPattern);
        if (!guidSubStr)
            break;
        _tcsncpy(guidSubStr, uuidString, patternLength);
        guidSubStr += patternLength;
        strLength -= (guidSubStr - str);
    }
}

STDAPI DllUnregisterServer(void)
{
    HRESULT hr = S_OK;
    HKEY userClasses;

    if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\CLASSES"), 0, KEY_WRITE, &userClasses) != ERROR_SUCCESS)
        userClasses = 0;

    int nEntries = ARRAYSIZE(gRegTable);
    for (int i = nEntries - 1; i >= 0; i--) {
        LPTSTR pszKeyName = _tcsdup(gRegTable[i][0]);
        if (pszKeyName) {
            substituteGUID(pszKeyName, &gRegCLSIDs[i/gSlotsPerEntry]);
            RegDeleteKey(HKEY_CLASSES_ROOT, pszKeyName);
            if (userClasses)
                RegDeleteKey(userClasses, pszKeyName);
            free(pszKeyName);
        } else
            hr = E_OUTOFMEMORY;
    }

    if (userClasses)
        RegCloseKey(userClasses);
    return hr;
}

STDAPI DllRegisterServer(void)
{
    HRESULT hr = S_OK;

    // look up server's file name
    TCHAR szFileName[MAX_PATH];
    GetModuleFileName(gInstance, szFileName, MAX_PATH);

    HKEY userClasses;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\CLASSES"), 0, KEY_WRITE, &userClasses) != ERROR_SUCCESS)
        userClasses = 0;

    // register entries from table
    int nEntries = ARRAYSIZE(gRegTable);
    for (int i = 0; SUCCEEDED(hr) && i < nEntries; i++) {
        LPTSTR pszKeyName   = _tcsdup(gRegTable[i][0]);
        LPTSTR pszValueName = gRegTable[i][1] ? _tcsdup(gRegTable[i][1]) : 0;
        LPTSTR allocatedValue   = (gRegTable[i][2] != (LPTSTR)-1) ? _tcsdup(gRegTable[i][2]) : (LPTSTR)-1;
        LPTSTR pszValue     = allocatedValue;

        if (pszKeyName && pszValue) {

            int clsidIndex = i/gSlotsPerEntry;
            substituteGUID(pszKeyName, &gRegCLSIDs[clsidIndex]);
            substituteGUID(pszValueName, &gRegCLSIDs[clsidIndex]);

            // map rogue value to module file name
            if (pszValue == (LPTSTR)-1)
                pszValue = szFileName;
            else
                substituteGUID(pszValue, &gRegCLSIDs[clsidIndex]);

            // create the key
            HKEY hkey;
            LONG err = RegCreateKey(HKEY_CLASSES_ROOT, pszKeyName, &hkey);
            if (err != ERROR_SUCCESS && userClasses)
                err = RegCreateKey(userClasses, pszKeyName, &hkey);
            if (err == ERROR_SUCCESS) {
                // set the value
                err = RegSetValueEx(hkey, pszValueName, 0, REG_SZ, (const BYTE*)pszValue, (DWORD) sizeof(pszValue[0])*(_tcslen(pszValue) + 1));
                RegCloseKey(hkey);
            }
            if (err != ERROR_SUCCESS || FAILED(hr)) {
                // if cannot add key or value, back out and fail
                DllUnregisterServer(); 
                hr = SELFREG_E_CLASS;
            }
        }
        if (pszKeyName)
            free(pszKeyName);
        if (pszValueName)
            free(pszValueName);
        if (allocatedValue && allocatedValue != (LPTSTR)-1)
            free(allocatedValue);
    }

    if (userClasses)
        RegCloseKey(userClasses);
    return hr;
}

//FIXME: We should consider moving this to a new file for cross-project functionality
Vector<char> loadResourceIntoArray(const char* name)
{
    int idr;
    // temporary hack to get resource id
    if (!strcmp(name, "textAreaResizeCorner"))
        idr = IDR_RESIZE_CORNER;
    else if (!strcmp(name, "missingImage"))
        idr = IDR_MISSING_IMAGE;
    else if (!strcmp(name, "urlIcon"))
        idr = IDR_URL_ICON;
    else if (!strcmp(name, "nullPlugin"))
        idr = IDR_NULL_PLUGIN;
    else
        return Vector<char>();

    HRSRC resInfo = FindResource(gInstance, MAKEINTRESOURCE(idr), L"PNG");
    if (!resInfo)
        return Vector<char>();
    HANDLE res = LoadResource(gInstance, resInfo);
    if (!res)
        return Vector<char>();
    void* resource = LockResource(res);
    if (!resource)
        return Vector<char>();
    int size = SizeofResource(gInstance, resInfo);

    Vector<char> arr(size);
    memcpy(arr.data(), resource, size);
    return arr;
}
