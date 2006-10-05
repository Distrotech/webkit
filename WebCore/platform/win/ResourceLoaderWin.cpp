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

#include <wtf/platform.h>

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

namespace WebCore {

static unsigned transferJobId = 0;
static HashMap<int, ResourceLoader*>* jobIdMap = 0;

static HINTERNET internetHandle = 0;
static INTERNET_STATUS_CALLBACK callbackHandle = 0;

static HWND transferJobWindowHandle = 0;
static UINT loadStatusMessage = 0;
const LPCWSTR kResourceLoaderWindowClassName = L"ResourceLoaderWindowClass";

static HINTERNET globalInternetHandle(const String& userAgentStr)
{
    if (!internetHandle) {
        LPCWSTR userAgent = reinterpret_cast<const WCHAR*>(userAgentStr.characters());
        // leak the Internet for now
        internetHandle = InternetOpen(userAgent, INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, INTERNET_FLAG_ASYNC);
        if (!internetHandle) {
            return false;
        }

        callbackHandle = InternetSetStatusCallbackA(internetHandle, transferJobStatusCallback);
    }

    return internetHandle;
}

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

struct JobLoadStatus {
    DWORD internetStatus;
    DWORD_PTR dwResult;
    char* redirectURL;
};

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
    if (message != loadStatusMessage)
        return DefWindowProc(hWnd, message, wParam, lParam);

    JobLoadStatus* jobLoadStatus = (JobLoadStatus*)lParam;
    DWORD internetStatus = jobLoadStatus->internetStatus;
    DWORD_PTR dwResult = jobLoadStatus->dwResult;
    char* redirectURL = jobLoadStatus->redirectURL;
    delete jobLoadStatus;
    jobLoadStatus = 0;

    // If we get a message about a job we no longer know about (already deleted), ignore it.
    unsigned jobId = (unsigned)wParam;
    RefPtr<ResourceLoader> job = lookupResourceLoader(jobId);
    if (!job)
        return 0;

    if (job->d->m_cancelled)
        return 0;

    ASSERT(job->d->m_jobId == jobId);
    ASSERT(job->d->m_threadId == GetCurrentThreadId());

    if (internetStatus == INTERNET_STATUS_HANDLE_CREATED) {
        if (!job->d->m_resourceHandle) {
            job->d->m_resourceHandle = HINTERNET(dwResult);
            if (job->d->status != 0) {
                // We were canceled before Windows actually created a handle for us, close and delete now.
                InternetCloseHandle(job->d->m_resourceHandle);
                job->kill();
                return 0;
            }

            if (job->method() == "POST") {
                // FIXME: Too late to set referrer properly.
                DeprecatedString urlStr = job->d->URL.path();
                int fragmentIndex = urlStr.find('#');
                if (fragmentIndex != -1)
                    urlStr = urlStr.left(fragmentIndex);
                static LPCSTR accept[2]={"*/*", NULL};
                DWORD flags = INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP
                    | INTERNET_FLAG_FORMS_SUBMIT | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
                if (job->url().protocol() == "https")
                    flags |= INTERNET_FLAG_SECURE;
                HINTERNET urlHandle = HttpOpenRequestA(job->d->m_resourceHandle, 
                                                       "POST", urlStr.latin1(), 0, 0, accept,
                                                       flags,
                                                       (DWORD_PTR)job->d->m_jobId);
                if (urlHandle == INVALID_HANDLE_VALUE) {
                    InternetCloseHandle(job->d->m_resourceHandle);
                    job->kill();
                    return 0;
                }
            }
        } else if (!job->d->m_secondaryHandle) {
            ASSERT(job->method() == "POST");
            job->d->m_secondaryHandle = HINTERNET(dwResult);
            
            // Need to actually send the request now.
            String headers = "Content-Type: application/x-www-form-urlencoded\n";
            headers += "Referer: ";
            headers += job->d->m_postReferrer;
            headers += "\n";
            Vector<char> formData;
            job->postData().flatten(formData);
            INTERNET_BUFFERSA buffers = {0};
            buffers.dwStructSize = sizeof(INTERNET_BUFFERSA);
            CString headersAscii = headers.latin1();
            buffers.lpcszHeader = headersAscii;
            buffers.dwHeadersLength = headersAscii.length();
            buffers.dwHeadersTotal = headersAscii.length();
            buffers.dwBufferTotal = formData.size();
            
            job->d->m_bytesRemainingToWrite = formData.size();
            size_t dataSize = formData.size();
            char* data = new char[dataSize];
            strncpy(data, formData.data(), dataSize);
            job->d->m_formDataString = data;
            job->d->m_formDataLength = dataSize;
            job->d->m_writing = true;
            HttpSendRequestExA(job->d->m_secondaryHandle, &buffers, 0, 0, (DWORD_PTR)job->d->m_jobId);
        }
    } else if (internetStatus == INTERNET_STATUS_REDIRECT) {
        KURL redirectKURL(redirectURL); 
        free(redirectURL);
        job->client()->receivedRedirect(job.get(), redirectKURL);
        job->d->URL = redirectKURL;
        if (job->d->method == "POST") {
            bool redirectAsGet = true;
            char statusCode[32];
            DWORD statusCodeLength = ARRAYSIZE(statusCode)-1;
            if (job->d->m_secondaryHandle && HttpQueryInfoA(job->d->m_secondaryHandle, HTTP_QUERY_STATUS_CODE, statusCode, &statusCodeLength, 0)) {
                // see <http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html>
                if (!strcmp(statusCode, "302")) {
                    // FIXME - spec says we need to prompt user
                    redirectAsGet = true;
                } else if (!strcmp(statusCode, "303"))
                    redirectAsGet = true;
                else if (!strcmp(statusCode, "301"))
                    redirectAsGet = false;
                else if (!strcmp(statusCode, "307")) {
                    // FIXME - spec says we need to prompt user
                    redirectAsGet = false;
                }
            }
            if (job->d->m_secondaryHandle) {
                if (redirectAsGet)
                    // We need to skip our next call to InternetReadFileEx because our handles will have closed by the time we get there.
                    job->d->m_skipNextRead = true;
                InternetCloseHandle(job->d->m_secondaryHandle);
                job->d->m_secondaryHandle = 0;
                job->d->m_writing = false;
                delete[] job->d->m_formDataString;
                job->d->m_formDataString = 0;
                job->d->m_bytesRemainingToWrite = 0;
            }
            if (redirectAsGet) {
                if (job->d->m_resourceHandle) {
                    InternetCloseHandle(job->d->m_resourceHandle);
                    job->d->m_resourceHandle = 0;
                }

                // We need to create a new HINTERNET for the GET request
                HINTERNET urlHandle;
                {
                    // FIXME: This code is copied exactly out of ResourceLoader::start (except for the lack of setting Referrer).
                    // Need a way to avoid this copy-and-paste.
                    DeprecatedString urlStr = job->d->URL.url();
                    int fragmentIndex = urlStr.find('#');
                    if (fragmentIndex != -1)
                        urlStr = urlStr.left(fragmentIndex);
                    String headers;

                    DWORD flags = INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP;
                    if (job->d->URL.protocol() == "https")
                        flags |= INTERNET_FLAG_SECURE;

                    urlHandle = InternetOpenUrlA(internetHandle, urlStr.ascii(), headers.latin1(), headers.length(),
                        flags, (DWORD_PTR)job->d->m_jobId);
                }

                if (urlHandle == INVALID_HANDLE_VALUE) {
                    job->kill();
                    return 0;
                }

                job->d->method = "GET";
            }
        }
    } else if (internetStatus == INTERNET_STATUS_REQUEST_COMPLETE) {
        if (job->d->m_writing) {
            DWORD bytesWritten;
            InternetWriteFile(job->d->m_secondaryHandle,
                              job->d->m_formDataString + (job->d->m_formDataLength - job->d->m_bytesRemainingToWrite),
                              job->d->m_bytesRemainingToWrite,
                              &bytesWritten);
            job->d->m_bytesRemainingToWrite -= bytesWritten;
            if (!job->d->m_bytesRemainingToWrite) {
                // End the request.
                job->d->m_writing = false;
                HttpEndRequest(job->d->m_secondaryHandle, 0, 0, (DWORD_PTR)job->d->m_jobId);
                delete[] job->d->m_formDataString;
                job->d->m_formDataString = 0;
            }
            return 0;
        }

        if (job->d->m_skipNextRead) {
            job->d->m_skipNextRead = false;
            return 0;
        }

        HINTERNET handle = job->method() == "GET" ? job->d->m_resourceHandle : job->d->m_secondaryHandle;
        BOOL ok = FALSE;

        char buffer[32768];
        INTERNET_BUFFERSA buffers = {0};
        buffers.dwStructSize = sizeof(INTERNET_BUFFERSA);
        buffers.lpvBuffer = buffer;
        buffers.dwBufferLength = ARRAYSIZE(buffer);
        buffers.dwBufferTotal = sizeof(buffer);

        while ((ok = InternetReadFileExA(handle, &buffers, IRF_NO_WAIT, (DWORD_PTR)job->d->m_jobId)) && buffers.dwBufferLength) {
            if (!job->hasReceivedResponse()) {
                job->setHasReceivedResponse();
                PlatformResponseStruct response = {0};
                WCHAR contentTypeStr[256];
                DWORD headerLength = (sizeof(contentTypeStr)/sizeof(contentTypeStr[0]))-1;
                if (HttpQueryInfo(handle, HTTP_QUERY_CONTENT_TYPE, contentTypeStr, &headerLength, 0))
                    response.contentType = contentTypeStr;
                WCHAR contentLengthStr[32];
                headerLength = (sizeof(contentLengthStr)/sizeof(contentLengthStr[0]))-1;
                if (HttpQueryInfo(handle, HTTP_QUERY_CONTENT_LENGTH, contentLengthStr, &headerLength, 0))
                    response.contentLength = _wtol(contentLengthStr);
                job->client()->receivedResponse(job.get(), &response);
            }
            job->client()->receivedData(job.get(), buffer, buffers.dwBufferLength);
            buffers.dwBufferLength = ARRAYSIZE(buffer);
        }

        if (!ok) {
            int error = GetLastError();
            if (error == ERROR_IO_PENDING)
                return 0;
            else {
                job->setError(error);
                logLastError(error, _T(__FUNCTION__), __LINE__);
            }
        }

        if (job->d->m_secondaryHandle)
            InternetCloseHandle(job->d->m_secondaryHandle);
        InternetCloseHandle(job->d->m_resourceHandle);
        
        job->client()->receivedAllData(job.get(), 0);
        job->client()->receivedAllData(job.get());
        job->kill();
    }

    return 0;
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
    loadStatusMessage = RegisterWindowMessage(L"com.apple.WebKit.ResourceLoaderLoadStatus");
}

ResourceLoaderInternal::~ResourceLoaderInternal()
{
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
    switch (internetStatus) {
    case INTERNET_STATUS_HANDLE_CREATED:
    case INTERNET_STATUS_REQUEST_COMPLETE:
    case INTERNET_STATUS_REDIRECT:
        JobLoadStatus* jobLoadStatus = new JobLoadStatus;
        jobLoadStatus->internetStatus = internetStatus;
        if (internetStatus == INTERNET_STATUS_REDIRECT)
            jobLoadStatus->redirectURL = strdup((char*)statusInformation);
        else if (internetStatus == INTERNET_STATUS_HANDLE_CREATED || internetStatus == INTERNET_STATUS_REQUEST_COMPLETE)
            jobLoadStatus->dwResult = LPINTERNET_ASYNC_RESULT(statusInformation)->dwResult;
        PostMessage(transferJobWindowHandle, loadStatusMessage, (WPARAM)jobId, (LPARAM)jobLoadStatus);
    }
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
        HINTERNET internetHandle = globalInternetHandle(docLoader->frame()->userAgent() + String("", 1));
        if (!internetHandle) {
            kill();
            return false;
        }

        initializeOffScreenResourceLoaderWindow();
        d->m_jobId = addToOutstandingJobs(this);

        // For form posting, we can't use InternetOpenURL.  We have to use InternetConnect followed by
        // HttpSendRequest.
        HINTERNET urlHandle;
        String referrer = docLoader->frame()->referrer();
        if (method() == "POST") {
            d->m_postReferrer = referrer;
            DeprecatedString host = d->URL.host();
            host += "\0";
            INTERNET_PORT port = d->URL.port();
            if (!port) {
                if (d->URL.protocol().compare("https"))
                    port = INTERNET_DEFAULT_HTTP_PORT;
                else
                    port = INTERNET_DEFAULT_HTTPS_PORT;
            }
            urlHandle = InternetConnectA(internetHandle, host.ascii(), port, 0, 0, 
                                         INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)d->m_jobId);
        } else {
            DeprecatedString urlStr = d->URL.url();
            int fragmentIndex = urlStr.find('#');
            if (fragmentIndex != -1)
                urlStr = urlStr.left(fragmentIndex);
            String headers;
            if (!referrer.isEmpty())
                headers += String("Referer: ") + referrer + "\r\n";

            DWORD flags = INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP;
            if (d->URL.protocol() == "https")
                flags |= INTERNET_FLAG_SECURE;

            urlHandle = InternetOpenUrlA(internetHandle, urlStr.ascii(), headers.latin1(), headers.length(),
                                         flags, (DWORD_PTR)d->m_jobId);
        }

        if (urlHandle == INVALID_HANDLE_VALUE) {
            kill();
            return false;
        }
        d->m_threadId = GetCurrentThreadId();

        return true;
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

void ResourceLoader::setHasReceivedResponse(bool b)
{
    d->m_hasReceivedResponse = b;
}

bool ResourceLoader::hasReceivedResponse() const
{
    return d->m_hasReceivedResponse;
}

} // namespace WebCore

#endif // USE(WININET)
