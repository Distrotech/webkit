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
#include "FileChooser.h"

#include "FrameView.h"
#include "Icon.h"
#include "LocalizedStrings.h"
#include "RenderFileUploadControl.h"
#include "RenderObject.h"
#include "RenderThemeWin.h"
#include "RenderView.h"
#include "Shlwapi.h"
#include <tchar.h>
#include <windows.h>

namespace WebCore {

PassRefPtr<FileChooser> FileChooser::create(Document* document, RenderFileUploadControl* uploadControl)
{
    PassRefPtr<FileChooser> fileChooser(new FileChooser(document, uploadControl));
    return fileChooser;
}

FileChooser::FileChooser(Document* document, RenderFileUploadControl* uploadControl)
    : m_document(document)
    , m_uploadControl(uploadControl)
    , m_icon(0)
{
}

FileChooser::~FileChooser()
{
}

void FileChooser::openFileChooser()
{
    TCHAR fileBuf[_MAX_PATH];
    OPENFILENAME ofn;

    memset(&ofn, 0, sizeof(ofn));
    
    // Need to zero out the first char of fileBuf so GetOpenFileName doesn't think it's an initialization string
    fileBuf[0] = '\0';

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = uploadControl()->view()->frameView()->containingWindow();
    ofn.lpstrFilter = _T("All Files\0*.*\0\0");
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = sizeof(fileBuf);
    ofn.lpstrTitle = _T("Upload file");
    ofn.Flags = OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    // We need this protector because otherwise we can be deleted if the file upload control is detached while
    // we're within the GetOpenFileName call.
    RefPtr<FileChooser> protector(this);

    if (GetOpenFileName(&ofn))
        chooseFile(String(fileBuf));
}

String FileChooser::basenameForWidth(int width) const
{
    if (width <= 0)
        return String();

    // FIXME: Need to actually truncate the string to width here.
    if (m_filename.isEmpty())
        return fileButtonNoFileSelectedLabel();

    String tmpFilename = m_filename;
    LPTSTR basename = PathFindFileName(tmpFilename.charactersWithNullTermination());
    return String(basename);
}

void FileChooser::uploadControlDetaching()
{
    m_uploadControl = 0;
}

void FileChooser::chooseFile(const String& filename)
{
    if (filename == m_filename)
        return;

    m_filename = filename;

    if (!m_filename.length())
        m_icon = 0;
    else
        m_icon = Icon::newIconForFile(m_filename);

    if (uploadControl())
        uploadControl()->valueChanged();
}

}