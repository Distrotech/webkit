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

#include "ClipboardWin.h"
#include "Cursor.h"
#include "FloatPoint.h"
#include "FocusController.h"
#include "FrameView.h"
#include "FrameWin.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "MouseEventWithHitTestResults.h"
#include "Page.h"
#include "PlatformScrollbar.h"
#include "PlatformWheelEvent.h"
#include "SelectionController.h"
#include "NotImplemented.h"

namespace WebCore {

bool EventHandler::passMousePressEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    if (m_frame->view()->capturingMouse())
        subframe->view()->setCapturingMouse(true);
    subframe->eventHandler()->handleMousePressEvent(mev.event());
    return true;
}

bool EventHandler::passMouseMoveEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    subframe->view()->handleMouseMoveEvent(mev.event());
    return true;
}

bool EventHandler::passMouseReleaseEventToSubframe(MouseEventWithHitTestResults& mev, Frame* subframe)
{
    subframe->view()->handleMouseReleaseEvent(mev.event());
    return true;
}

bool EventHandler::passWheelEventToSubframe(PlatformWheelEvent& wheelEvent, Frame* subframe)
{
    subframe->eventHandler()->handleWheelEvent(wheelEvent);
    return true;
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

bool EventHandler::handleDrag(const MouseEventWithHitTestResults& event)
{
    if (event.event().button() == LeftButton && event.event().eventType() == MouseEventMoved) {
        // Careful that the drag starting logic stays in sync with eventMayStartDrag()  
    
        if (m_mouseDownMayStartDrag && !dragState().m_dragSrc) {
            bool tempFlag1, tempFlag2;
            allowDHTMLDrag(tempFlag1, tempFlag2);
            dragState().m_dragSrcMayBeDHTML = tempFlag1;
            dragState().m_dragSrcMayBeUA = tempFlag2;
            if (!dragState().m_dragSrcMayBeDHTML && !dragState().m_dragSrcMayBeUA)
                m_mouseDownMayStartDrag = false;     // no element is draggable
        }
            
        if (m_mouseDownMayStartDrag && !dragState().m_dragSrc) {
            // try to find an element that wants to be dragged
            HitTestRequest request(true, false);
            HitTestResult result(m_mouseDownPos);
            m_frame->renderer()->layer()->hitTest(request, result);
            Node* node = result.innerNode();
            dragState().m_dragSrc = (node && node->renderer())
                ? node->renderer()->draggableNode(dragState().m_dragSrcMayBeDHTML, dragState().m_dragSrcMayBeUA,
                    m_mouseDownPos.x(), m_mouseDownPos.y(), dragState().m_dragSrcIsDHTML)
                : 0;
            if (!dragState().m_dragSrc) {
                m_mouseDownMayStartDrag = false;     // no element is draggable
            } else {
                // remember some facts about this source, while we have a HitTestResult handy
                node = result.URLElement();
                dragState().m_dragSrcIsLink = node && node->isLink();

                node = result.innerNonSharedNode();
                dragState().m_dragSrcIsImage = node && node->renderer() && node->renderer()->isImage();
                
                dragState().m_dragSrcInSelection = m_frame->selectionController()->contains(m_mouseDownPos);
            }                
        }
        
        // For drags starting in the selection, the user must wait between the mousedown and mousedrag,
        // or else we bail on the dragging stuff and allow selection to occur
        if (m_mouseDownMayStartDrag && dragState().m_dragSrcInSelection && event.event().timestamp() - m_mouseDownTimestamp < TextDragDelay) {
            m_mouseDownMayStartDrag = false;
            // ...but if this was the first click in the window, we don't even want to start selection
            if (event.event().activatedWebView())
                m_mouseDownMayStartSelect = false;
        }

        if (m_mouseDownMayStartDrag) {
            // We are starting a text/image/url drag, so the cursor should be an arrow
            m_frame->view()->setCursor(pointerCursor());
            
            if (dragHysteresisExceeded(event.event().pos())) {
                
                // Once we're past the hysteresis point, we don't want to treat this gesture as a click
                invalidateClick();
                LOG_NOIMPL();
            }

            // FIXME: currently don't handle the drag so we don't correctly clear the drag state :(
            dragState().m_dragSrc = false;

            // No more default handling (like selection), whether we're past the hysteresis bounds or not
            return true;
        }

        if (!mouseDownMayStartSelect() && !m_mouseDownMayStartAutoscroll)
            return true;
            
    } else {
        // If we allowed the other side of the bridge to handle a drag
        // last time, then m_mousePressed might still be set. So we
        // clear it now to make sure the next move after a drag
        // doesn't look like a drag.
        m_mousePressed = false;
    }
    return false;
}

bool EventHandler::handleMouseUp(const MouseEventWithHitTestResults&)
{
    LOG_NOIMPL();
    return false;
}

void EventHandler::focusDocumentView()
{
    Page* page = m_frame->page();
    if (!page)
        return;
    page->focusController()->setFocusedFrame(m_frame);
}

bool EventHandler::passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&)
{
    LOG_NOIMPL();
    return false;
}

}
