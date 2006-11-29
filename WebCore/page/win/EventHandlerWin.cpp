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
#include "EventHandler.h"

#include "FrameView.h"
#include "FrameWin.h"
#include "MouseEventWithHitTestResults.h"
#include "PlatformScrollbar.h"
#include "PlatformWheelEvent.h"

namespace WebCore {

bool EventHandler::passMousePressEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    if (m_frame->view()->capturingMouse())
        subframe->view()->setCapturingMouse(true);
    return subframe->eventHandler()->handleMousePressEvent(mev.event());
}

bool EventHandler::passMouseMoveEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    return subframe->view()->handleMouseMoveEvent(mev.event());
}

bool EventHandler::passMouseReleaseEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    return subframe->view()->handleMouseReleaseEvent(mev.event());
}

bool EventHandler::passWheelEventToSubframe(PlatformWheelEvent& wheelEvent, Frame* subframe)
{
    return subframe->eventHandler()->handleWheelEvent(wheelEvent);
}

bool EventHandler::passMousePressEventToScrollbar(MouseEventWithHitTestResults& mev, PlatformScrollbar* scrollbar)
{
    if (!scrollbar || !scrollbar->isEnabled())
        return false;
    return scrollbar->handleMousePressEvent(mev.event());
}

bool EventHandler::tabsToAllControls(KeyboardEvent*) const
{
    return true;
}

bool EventHandler::tabsToLinks(KeyboardEvent*) const
{
    if (FrameWinClient* client = Win(m_frame)->client())
        return client->tabsToLinks();
    return true;
}

bool EventHandler::handleDrag(const MouseEventWithHitTestResults&)
{
    return false;
}

bool EventHandler::handleMouseUp(const MouseEventWithHitTestResults&)
{
    return false;
}

bool EventHandler::shouldDragAutoNode(Node*, const IntPoint&) const
{
    return false;
}

void EventHandler::focusDocumentView()
{
}

bool EventHandler::passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&)
{
    return false;
}

}
