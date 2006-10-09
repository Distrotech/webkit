/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#if USE(WININET)
#include "ResourceLoaderWin.h"
#include "ResourceLoader.h"
#include "ResourceLoaderInternal.h"

#include "CString.h"
#include "DocLoader.h"
#include "Frame.h"
#include "Document.h"
#include "Page.h"
#include <tchar.h>
#include <wininet.h>
#include <shlwapi.h>

namespace WebCore {

static unsigned transferJobId = 0;
static HashMap<int, ResourceLoader*>* jobIdMap = 0;

static HWND transferJobWindowHandle = 0;
const LPCWSTR kResourceLoaderWindowClassName = L"ResourceLoaderWindowClass";

// Message types for internal use (keep in sync with messageHandlers)
enum {
  handleCreatedMessage = WM_USER,
  requestRedirectedMessage,
  requestCompleteMessage
};

typedef void (ResourceLoader:: *ResourceLoaderEventHandler)(LPARAM);
static const ResourceLoaderEventHandler messageHandlers[] = {
    &ResourceLoader::onHandleCreated,
    &ResourceLoader::onRequestRedirected,
    &ResourceLoader::onRequestComplete
};

static int addToOutstandingJobs(ResourceLoader* job)
{
    if (!jobIdMap)
        jobIdMap = new HashMap<int, ResourceLoader*>;
    transferJobId++;
    if (!transferJobId)
        transferJobId++; // 0 has a special meaning for WinInet and needs to be skipped
    jobIdMap->set(transferJobId, job);
    return transferJobId;
}

static void removeFromOutstandingJobs(int jobId)
{
    if (!jobIdMap)
        return;
    jobIdMap->remove(jobId);
}

static ResourceLoader* lookupResourceLoader(int jobId)
{
    if (!jobIdMap)
        return 0;
    return jobIdMap->get(jobId);
}

static void logLastError(DWORD error, LPCTSTR lpszFunction, int line) 
{ 
#ifndef NDEBUG
    TCHAR szBuf[300];
    LPVOID lpMsgBuf;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        error,
        0,
        (LPTSTR)&lpMsgBuf,
        0, NULL );

    _snwprintf(szBuf, 300,
        _T("%s failed with error %d: %s%s(%d)\n"), 
        lpszFunction, error, lpMsgBuf, _T(__FILE__), line);

    OutputDebugString(szBuf);

    LocalFree(lpMsgBuf);
#endif
}

LRESULT CALLBACK ResourceLoaderWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message < handleCreatedMessage)
        return DefWindowProc(hWnd, message, wParam, lParam);

    UINT index = message - handleCreatedMessage;
    if (index >= _countof(messageHandlers))
        return 0;

    unsigned jobId = (unsigned) wParam;
    RefPtr<ResourceLoader> job = lookupResourceLoader(jobId);
    if (!job || job->d->m_cancelled)
        return 0;

    ASSERT(job->d->m_jobId == jobId);
    ASSERT(job->d->m_threadId == GetCurrentThreadId());

    (job.get()->*(messageHandlers[index]))(lParam);

    if (message == requestRedirectedMessage)
        free((char*)lParam);

    return 0;
}

void ResourceLoader::onHandleCreated(LPARAM lParam)
{
    if (!d->m_resourceHandle) {
        d->m_resourceHandle = HINTERNET(lParam);
        if (d->status != 0) {
            // We were canceled before Windows actually created a handle for us, close and delete now.
            InternetCloseHandle(d->m_resourceHandle);
            kill();
            return;
        }

        bool isPost = method() == "POST";
        if (isPost || d->m_resend) {
            // FIXME: Too late to set referrer properly.
            DeprecatedString urlStr = d->URL.path();
            int fragmentIndex = urlStr.find('#');
            if (fragmentIndex != -1)
                urlStr = urlStr.left(fragmentIndex);
            static LPCSTR accept[2]={"*/*", NULL};
            DWORD flags = INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP;
            if (url().protocol() == "https")
                flags |= INTERNET_FLAG_SECURE;
            if (isPost)
                flags |= INTERNET_FLAG_FORMS_SUBMIT | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
            HINTERNET urlHandle = HttpOpenRequestA(d->m_resourceHandle, 
                                                   isPost ? "POST" : "GET", urlStr.latin1(), 0, 0, accept,
                                                   flags,
                                                   (DWORD_PTR)d->m_jobId);
            if (urlHandle == INVALID_HANDLE_VALUE) {
                InternetCloseHandle(d->m_resourceHandle);
                kill();
                return;
            }
        }
    } else if (!d->m_secondaryHandle) {
        bool isPost = (method() == "POST"); 
        ASSERT(isPost || d->m_resend);
        d->m_secondaryHandle = HINTERNET(lParam);
        
        // Need to actually send the request now.
        INTERNET_BUFFERSA buffers = {0};
        Vector<char> formData;
        String headers;
        if (isPost) {
            headers = "Content-Type: application/x-www-form-urlencoded\r\n";
            postData().flatten(formData);
            buffers.dwStructSize = sizeof(INTERNET_BUFFERSA);
            size_t dataSize = formData.size();
            char* data = new char[dataSize];
            strncpy(data, formData.data(), dataSize);
            delete[] d->m_formDataString;
            d->m_formDataString = data;
            d->m_formDataLength = dataSize;
            d->m_writing = true;
        }
        headers += "Referer: ";
        headers += d->m_postReferrer;
        headers += "\r\n";
        CString headersAscii = headers.latin1();
        buffers.lpcszHeader = headersAscii;
        buffers.dwHeadersLength = headersAscii.length();
        buffers.dwHeadersTotal = headersAscii.length();
        buffers.dwBufferTotal = formData.size();
        d->m_bytesRemainingToWrite = formData.size();

        HttpSendRequestExA(d->m_secondaryHandle, &buffers, 0, HSR_INITIATE, (DWORD_PTR)d->m_jobId);
    }
}

void ResourceLoader::onRequestRedirected(LPARAM lParam)
{
    KURL redirectKURL((char*)lParam); 
    client()->receivedRedirect(this, redirectKURL);
    d->URL = redirectKURL;
    if (d->method == "POST") {
        bool redirectAsGet = true;
        DWORD statusCode;
        DWORD statusCodeLength = sizeof(statusCode);
        if (d->m_secondaryHandle && HttpQueryInfoA(d->m_secondaryHandle, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeLength, 0)) {
            // see <http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html>
            if (statusCode == HTTP_STATUS_REDIRECT) {
                // FIXME - spec says we need to prompt user
                redirectAsGet = true;
            } else if (statusCode == HTTP_STATUS_REDIRECT_METHOD)
                redirectAsGet = true;
            else if (statusCode == HTTP_STATUS_MOVED)
                redirectAsGet = false;
            else if (statusCode == HTTP_STATUS_REDIRECT_KEEP_VERB) {
                // FIXME - spec says we need to prompt user
                redirectAsGet = false;
            }
        }
        if (d->m_secondaryHandle) {
            InternetCloseHandle(d->m_secondaryHandle);
            d->m_secondaryHandle = 0;
            d->m_writing = false;
        }
        if (redirectAsGet)
            d->method = "GET";
    }
}

void ResourceLoader::onRequestComplete(LPARAM lParam)
{
    DWORD dwError = (DWORD)lParam;

    bool reissueRequest = false;
    HINTERNET handle = (method() == "GET" && !d->m_resend) ? d->m_resourceHandle : d->m_secondaryHandle;

    // handle confirmation-required redirects by reissuing the request to the redirected URL
    if (dwError == ERROR_HTTP_REDIRECT_NEEDS_CONFIRMATION)
        reissueRequest = true;

    // handle HTTP authentication (harder since we're using InternetOpenURL)
    DWORD handleType;
    DWORD handleTypeSize = sizeof(handleType);
    if (InternetQueryOption(handle, INTERNET_OPTION_HANDLE_TYPE, &handleType, &handleTypeSize) &&
            (handleType == INTERNET_HANDLE_TYPE_HTTP_REQUEST || handleType == INTERNET_HANDLE_TYPE_CONNECT_HTTP)) {
        DWORD optionCode;
        DWORD optionSize = sizeof(optionCode);
        if (HttpQueryInfo(handle, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &optionCode, &optionSize, 0) && optionCode == HTTP_STATUS_DENIED) {
            if (handleType == INTERNET_HANDLE_TYPE_CONNECT_HTTP)
                reissueRequest = true;
            else if (handleType == INTERNET_HANDLE_TYPE_HTTP_REQUEST) {
                // then query for the user's credentials
                if (InternetErrorDlg(GetDesktopWindow(), handle, ERROR_INTERNET_INCORRECT_PASSWORD, FLAGS_ERROR_UI_FILTER_FOR_ERRORS | FLAGS_ERROR_UI_FLAGS_GENERATE_DATA | FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS, 0) == ERROR_INTERNET_FORCE_RETRY) {
                    // then resend the request (handle has had username/password added via InternetErrorDlg call above)
                    HttpSendRequest(handle, 0, 0, 0, 0);
                    return;
                }
            }
        }
    }

    if (reissueRequest) {
        if (d->m_secondaryHandle) {
            InternetCloseHandle(d->m_secondaryHandle);
            d->m_secondaryHandle = 0;
            d->m_writing = false;
            delete[] d->m_formDataString;
            d->m_formDataString = 0;
            d->m_bytesRemainingToWrite = 0;
          }
        InternetCloseHandle(d->m_resourceHandle);
        d->m_resourceHandle = 0;
        removeFromOutstandingJobs(d->m_jobId);
        startHTTPRequest(d->m_postReferrer);
          return;
      }

    if (dwError == ERROR_SUCCESS) {
        if (d->m_writing) {
            DWORD bytesWritten;
            InternetWriteFile(d->m_secondaryHandle,
                              d->m_formDataString + (d->m_formDataLength - d->m_bytesRemainingToWrite),
                              d->m_bytesRemainingToWrite,
                              &bytesWritten);
            d->m_bytesRemainingToWrite -= bytesWritten;
            if (!d->m_bytesRemainingToWrite) {
                // End the request.
                d->m_writing = false;
                HttpEndRequest(d->m_secondaryHandle, 0, 0, (DWORD_PTR)d->m_jobId);
                delete[] d->m_formDataString;
                d->m_formDataString = 0;
            }
            return;
        } else if (d->m_resend) {
            HttpEndRequest(d->m_secondaryHandle, 0, 0, (DWORD_PTR)d->m_jobId);
            return;
          }

        BOOL ok = FALSE;

        char buffer[32768];
        INTERNET_BUFFERSA buffers = {0};
        buffers.dwStructSize = sizeof(INTERNET_BUFFERSA);
        buffers.lpvBuffer = buffer;
          buffers.dwBufferLength = ARRAYSIZE(buffer);
        buffers.dwBufferTotal = sizeof(buffer);

        while ((ok = InternetReadFileExA(handle, &buffers, IRF_NO_WAIT, (DWORD_PTR)d->m_jobId)) && buffers.dwBufferLength) {
            if (!d->m_hasReceivedResponse) {
                d->m_hasReceivedResponse = true;
                PlatformResponseStruct response = {0};
                WCHAR contentTypeStr[256];
                DWORD headerLength = (sizeof(contentTypeStr)/sizeof(contentTypeStr[0]))-1;
                if (HttpQueryInfo(handle, HTTP_QUERY_CONTENT_TYPE, contentTypeStr, &headerLength, 0))
                    response.contentType = contentTypeStr;

                if (!response.contentType) {
                    // Try to figure out the content type from the URL extension

                    DeprecatedString urlStr = d->URL.url();
                    int fragmentIndex = urlStr.find('#');
                    if (fragmentIndex != -1)
                        urlStr = urlStr.left(fragmentIndex);
                    int extensionIndex = urlStr.findRev('.');
                    if (extensionIndex != -1) {
                        String extension = urlStr.mid(extensionIndex);
                        DWORD keyType;
                        DWORD contentTypeStrLen = sizeof(contentTypeStr);
                        HRESULT result = SHGetValue(HKEY_CLASSES_ROOT, extension.charactersWithNullTermination(), TEXT("Content Type"), &keyType, (LPVOID)contentTypeStr, &contentTypeStrLen);
                        if (result == ERROR_SUCCESS && keyType == REG_SZ)
                            response.contentType = contentTypeStr;
                    }
                }

                // Bail out and use application/octet-stream
                if (!response.contentType)
                    response.contentType = TEXT("application/octet-stream");

                DWORD contentLength;
                DWORD contentLengthSize = sizeof(contentLength);
                if (HttpQueryInfo(handle, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &contentLengthSize, 0))
                    response.contentLength = contentLength;
                client()->receivedResponse(this, &response);
            }
            client()->receivedData(this, buffer, buffers.dwBufferLength);
            buffers.dwBufferLength = ARRAYSIZE(buffer);
          }
        if (!ok) {
            dwError = GetLastError();
            if (dwError == ERROR_IO_PENDING)
                return;
        }
    }

    if (dwError != ERROR_SUCCESS) {
        setError(dwError);
        logLastError(dwError, _T(__FUNCTION__), __LINE__);
    }

    if (d->m_secondaryHandle)
        InternetCloseHandle(d->m_secondaryHandle);
    InternetCloseHandle(d->m_resourceHandle);
    
    client()->receivedAllData(this, 0);
    client()->receivedAllData(this);
    kill();
}

static void initializeOffScreenResourceLoaderWindow()
{
    if (transferJobWindowHandle)
        return;

    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc    = ResourceLoaderWndProc;
    wcex.hInstance      = Page::instanceHandle();
    wcex.lpszClassName  = kResourceLoaderWindowClassName;
    RegisterClassEx(&wcex);

    transferJobWindowHandle = CreateWindow(kResourceLoaderWindowClassName, 0, 0, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
        HWND_MESSAGE, 0, Page::instanceHandle(), 0);
}

ResourceLoaderInternal::~ResourceLoaderInternal()
{
    delete[] m_formDataString;

    if (m_fileHandle)
        CloseHandle(m_fileHandle);
}

ResourceLoader::~ResourceLoader()
{
    if (d->m_jobId)
        removeFromOutstandingJobs(d->m_jobId);
}

static void __stdcall transferJobStatusCallback(HINTERNET internetHandle, DWORD_PTR jobId, DWORD internetStatus, LPVOID statusInformation, DWORD statusInformationLength)
{
    bool shouldPost = false;
    UINT message;
    LPARAM lParam;

    LPINTERNET_ASYNC_RESULT result = (LPINTERNET_ASYNC_RESULT)statusInformation;

    switch (internetStatus) {
    case INTERNET_STATUS_HANDLE_CREATED:
        shouldPost = true;
        message = handleCreatedMessage;
        lParam = (LPARAM)result->dwResult;
        break;
    case INTERNET_STATUS_REQUEST_COMPLETE:
        shouldPost = true;
        message = requestCompleteMessage;
        lParam = (LPARAM)result->dwError;
        break;
    case INTERNET_STATUS_REDIRECT:
        shouldPost = true;
        message = requestRedirectedMessage;
        lParam = (LPARAM)strdup((char*)statusInformation);
        break;
    }

    if (shouldPost)
        PostMessage(transferJobWindowHandle, message, (WPARAM)jobId, lParam);
}

static HINTERNET internetOpenHandle;

bool ResourceLoader::startHTTPRequest(const String& referrer)
{
    d->m_jobId = addToOutstandingJobs(this);
    d->m_postReferrer = referrer;

    // For form posting, we can't use InternetOpenURL.  We have to use InternetConnect followed by
    // HttpSendRequest.
    HINTERNET urlHandle;
    if (method() == "POST" || d->m_resend) {
        DeprecatedString host = d->URL.host();
        host += "\0";
        INTERNET_PORT port = d->URL.port();
        if (!port) {
            if (d->URL.protocol() == "https")
                port = INTERNET_DEFAULT_HTTPS_PORT;
            else
                port = INTERNET_DEFAULT_HTTP_PORT;
        }
        urlHandle = InternetConnectA(internetOpenHandle, host.ascii(), port, 0, 0, 
                                     INTERNET_SERVICE_HTTP,  0, (DWORD_PTR)d->m_jobId);
    } else {
        DeprecatedString urlStr = d->URL.url();
        int fragmentIndex = urlStr.find('#');
        if (fragmentIndex != -1)
            urlStr = urlStr.left(fragmentIndex);
        String headers;
        if (!referrer.isEmpty())
            headers += String("Referer: ") + referrer + "\r\n";
        DWORD flags = INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS;
        if (url().protocol() == "https")
            flags |= INTERNET_FLAG_SECURE;

        urlHandle = InternetOpenUrlA(internetOpenHandle, urlStr.ascii(), headers.latin1(), headers.length(),
                                     flags, (DWORD_PTR)d->m_jobId);
    }

    if (urlHandle == INVALID_HANDLE_VALUE) {
        delete this;
        return false;
    }
    d->m_threadId = GetCurrentThreadId();
    return true;
}

bool ResourceLoader::start(DocLoader* docLoader)
{
    ref();
    d->m_loading = true;

    if (d->URL.isLocalFile()) {
        DeprecatedString path = d->URL.path();
        // windows does not enjoy a leading slash on paths
        if (path[0] == '/')
            path = path.mid(1);
        d->m_fileHandle = CreateFileA(path.ascii(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        // FIXME: perhaps this error should be reported asynchronously for
        // consistency.
        if (d->m_fileHandle == INVALID_HANDLE_VALUE) {
            kill();
            return false;
        }

        d->m_fileLoadTimer.startOneShot(0.0);
        return true;
    } else {
        if (!internetOpenHandle) {
            String userAgentStr = docLoader->frame()->userAgent() + String("", 1);
            LPCWSTR userAgent = reinterpret_cast<const WCHAR*>(userAgentStr.characters());
            // leak the Internet for now
            internetOpenHandle = InternetOpen(userAgent, INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, INTERNET_FLAG_ASYNC);
        }
        if (!internetOpenHandle) {
            delete this;
            return false;
        }
        static INTERNET_STATUS_CALLBACK callbackHandle = InternetSetStatusCallbackA(internetOpenHandle, transferJobStatusCallback);

        initializeOffScreenResourceLoaderWindow();
        return startHTTPRequest(docLoader->frame()->referrer());
    }
}

void ResourceLoader::fileLoadTimer(Timer<ResourceLoader>* timer)
{
    d->client->receivedResponse(this, 0);

    bool result = false;
    DWORD bytesRead = 0;

    do {
        const int bufferSize = 8192;
        char buffer[bufferSize];
        result = ReadFile(d->m_fileHandle, &buffer, bufferSize, &bytesRead, NULL); 
        d->client->receivedData(this, buffer, bytesRead);
        // Check for end of file. 
    } while (result && bytesRead);

    // FIXME: handle errors better

    CloseHandle(d->m_fileHandle);
    d->m_fileHandle = INVALID_HANDLE_VALUE;

    PlatformDataStruct platformData;
    platformData.errorString = 0;
    platformData.error = 0;
    platformData.loaded = TRUE;

    d->client->receivedAllData(this, &platformData);
    d->client->receivedAllData(this);
}

void ResourceLoader::cancel()
{
    if (d->m_resourceHandle) {
        InternetCloseHandle(d->m_resourceHandle);
        d->m_resourceHandle = 0;
    }
    else
        d->m_fileLoadTimer.stop();

    setError(1);
    PlatformDataStruct platformData;
    platformData.errorString = 0;
    platformData.error = 0;
    platformData.loaded = FALSE;

    d->client->receivedAllData(this, &platformData);
    d->client->receivedAllData(this);
}

} // namespace WebCore

#endif // USE(WININET)
