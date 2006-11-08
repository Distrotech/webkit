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
#include "Screen.h"

#include "IntRect.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "Page.h"
#include <windows.h>

namespace WebCore {

static HWND getWindow(Page* page)
{
    Frame* frame = page->mainFrame();
    if (!frame)
        return 0;

    FrameView* frameView = frame->view();
    if (!frameView)
        return 0;

    return frameView->containingWindow();
}

static MONITORINFOEX getMonitorInfo(HWND window)
{
    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(monitor, &monitorInfo);
    return monitorInfo;
}

static DEVMODE getDeviceInfo(HWND window)
{
    MONITORINFOEX monitorInfo = getMonitorInfo(window);

    DEVMODE deviceInfo;
    deviceInfo.dmSize = sizeof(DEVMODE);
    deviceInfo.dmDriverExtra = 0;
    EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &deviceInfo);

    return deviceInfo;
}

int Screen::depth() const
{
    DEVMODE deviceInfo = getDeviceInfo(getWindow(m_page));
    return deviceInfo.dmBitsPerPel;
}

int Screen::depthPerComponent() const
{
    // FIXME: Assumes RGB -- not sure if this is right.
    DEVMODE deviceInfo = getDeviceInfo(getWindow(m_page));
    return deviceInfo.dmBitsPerPel / 3;
}

bool Screen::isMonochrome() const
{
    DEVMODE deviceInfo = getDeviceInfo(getWindow(m_page));
    return deviceInfo.dmColor == DMCOLOR_MONOCHROME;
}

FloatRect Screen::rect() const
{
    MONITORINFOEX monitorInfo = getMonitorInfo(getWindow(m_page));
    return monitorInfo.rcMonitor;
}

FloatRect Screen::usableRect() const
{
    MONITORINFOEX monitorInfo = getMonitorInfo(getWindow(m_page));
    return monitorInfo.rcWork;
}

} // namespace WebCore
