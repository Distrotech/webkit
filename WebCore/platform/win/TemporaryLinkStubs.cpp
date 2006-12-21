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

#define WIN32_COMPILE_HACK

#include "AXObjectCache.h"
#include "CachedResource.h"
#include "CookieJar.h"
#include "DocumentLoader.h"
#include "EditCommand.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FileChooser.h"
#include "Font.h"
#include "FormState.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "FrameWin.h"
#include "GraphicsContext.h"
#include "GraphicsContextPlatformPrivate.h"
#include "History.h"
#include "HTMLFormElement.h"
#include "IconLoader.h"
#include "IntPoint.h"
#include "KURL.h"
#include "Language.h"
#include "LoaderFunctions.h"
#include "LocalizedStrings.h"
#include "Node.h"
#include "NotImplemented.h"
#include "Page.h"
#include "Path.h"
#include "PlatformMouseEvent.h"
#include "PlugInInfoStore.h"
#include "RenderTheme.h"
#include "RenderThemeWin.h"
#include "ResourceHandle.h"
#include "ResourceLoader.h"
#include "ResourceResponse.h"
#include "Screen.h"
#include "ScrollBar.h"
#include "SubresourceLoader.h"
#include "TextBoundaries.h"
#include "Widget.h"
#include "loader.h"
#include <stdio.h>
#include <stdlib.h>

using namespace WebCore;

void Widget::enableFlushDrawing() { STOP_NOIMPL(); }
Widget::FocusPolicy Widget::focusPolicy() const { LOG_NOIMPL(); return NoFocus; }
void Widget::disableFlushDrawing() { STOP_NOIMPL(); }
GraphicsContext* Widget::lockDrawingFocus() { STOP_NOIMPL(); return 0; }
void Widget::unlockDrawingFocus(GraphicsContext*) { STOP_NOIMPL(); }

PlatformMouseEvent::PlatformMouseEvent(const CurrentEventTag&) { LOG_NOIMPL(); }
String WebCore::searchableIndexIntroduction() { LOG_NOIMPL(); return String(); }

int WebCore::findNextSentenceFromIndex(UChar const*,int,int,bool) { LOG_NOIMPL(); return 0; }
void WebCore::findSentenceBoundary(UChar const*,int,int,int*,int*) { LOG_NOIMPL(); }

void FrameWin::focusWindow() { LOG_NOIMPL(); }
void FrameWin::unfocusWindow() { LOG_NOIMPL(); }
KJS::Bindings::Instance* FrameWin::getAppletInstanceForWidget(Widget*) { STOP_NOIMPL(); return 0; }
bool FrameWin::canPaste() const { LOG_NOIMPL(); return false; }
bool FrameWin::shouldInterruptJavaScript() { LOG_NOIMPL(); return false; }
void FrameWin::print() { LOG_NOIMPL(); }
void FrameWin::issueCutCommand() { LOG_NOIMPL(); }
void FrameWin::issueCopyCommand() { LOG_NOIMPL(); }
void FrameWin::issuePasteCommand() { LOG_NOIMPL(); }
void FrameWin::issuePasteAndMatchStyleCommand() { LOG_NOIMPL(); }
void FrameWin::issueTransposeCommand() { LOG_NOIMPL(); }
String FrameWin::mimeTypeForFileName(String const&) const { LOG_NOIMPL(); return String(); }

void GraphicsContextPlatformPrivate::clip(const Path&) { LOG_NOIMPL(); }

/********************************************************/
/* Completely empty stubs (mostly to allow DRT to run): */
/********************************************************/
bool AXObjectCache::gAccessibilityEnabled = false;

String WebCore::submitButtonDefaultLabel() { return "Submit"; }
String WebCore::inputElementAltText() { LOG_NOIMPL(); return DeprecatedString(); }
String WebCore::resetButtonDefaultLabel() { return "Reset"; }
String WebCore::fileButtonChooseFileLabel() { return "Browse..."; }
String WebCore::fileButtonNoFileSelectedLabel() { return "no file selected"; }
String WebCore::defaultLanguage() { return "en"; }

namespace WebCore {

void FrameView::updateBorder() { LOG_NOIMPL(); }

Range* FrameWin::markedTextRange() const { LOG_NOIMPL(); return 0; }
bool FrameWin::shouldChangeSelection(Selection const&,Selection const&,WebCore::EAffinity,bool) const { LOG_NOIMPL(); return true; }
void FrameWin::respondToChangedSelection(WebCore::Selection const&,bool) { LOG_NOIMPL(); }
void FrameWin::respondToChangedContents(const Selection &endingSelection) { LOG_NOIMPL(); }

void Editor::ignoreSpelling() { LOG_NOIMPL(); }
void Editor::learnSpelling() { LOG_NOIMPL(); }
bool Editor::isSelectionMisspelled() { LOG_NOIMPL(); return false; }
bool Editor::isSelectionUngrammatical() { LOG_NOIMPL(); return false; }
Vector<String> Editor::guessesForMisspelledSelection() { LOG_NOIMPL(); return Vector<String>(); }
Vector<String> Editor::guessesForUngrammaticalSelection() { LOG_NOIMPL(); return Vector<String>(); }
void Editor::markMisspellingsInAdjacentWords(WebCore::VisiblePosition const&) { LOG_NOIMPL(); }
void Editor::advanceToNextMisspelling(bool) { LOG_NOIMPL(); }
    
bool CheckIfReloading(WebCore::DocLoader*) { LOG_NOIMPL(); return false; }
void CheckCacheObjectStatus(DocLoader*, CachedResource*) { LOG_NOIMPL(); }
void CachedResource::setAllData(PlatformData) { LOG_NOIMPL(); }

}

void Widget::setIsSelected(bool) { LOG_NOIMPL(); }

bool ScrollView::inWindow() const { LOG_NOIMPL(); return true; }

void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint& point, int width, bool grammar) { LOG_NOIMPL(); }

Color WebCore::focusRingColor() { LOG_NOIMPL(); return 0xFF7DADD9; }
void WebCore::setFocusRingColorChangeFunction(void (*)()) { LOG_NOIMPL(); }

void Frame::setNeedsReapplyStyles() { LOG_NOIMPL(); }

PolicyCheck::PolicyCheck() { LOG_NOIMPL(); }
void PolicyCheck::clear() { LOG_NOIMPL(); }
void PolicyCheck::call(bool) { LOG_NOIMPL(); }
void PolicyCheck::clearRequest() { LOG_NOIMPL(); }
void PolicyCheck::call(PolicyAction) { LOG_NOIMPL(); }

void FrameLoader::saveDocumentState() { LOG_NOIMPL(); }
void FrameLoader::restoreDocumentState() { LOG_NOIMPL(); }
void FrameLoader::goBackOrForward(int) { LOG_NOIMPL(); }
KURL FrameLoader::historyURL(int) { LOG_NOIMPL(); return KURL(); }
bool FrameLoader::canGoBackOrForward(int) const { LOG_NOIMPL(); return false; }
int FrameLoader::getHistoryLength() { LOG_NOIMPL(); return 0; }
void FrameLoader::reload() { LOG_NOIMPL(); }
void FrameLoader::load(const FrameLoadRequest&, bool userGesture, Event*, HTMLFormElement*,
                       const HashMap<String, String>& formValues) { LOG_NOIMPL(); }

void DocumentLoader::setupForReplaceByMIMEType(String const &) { STOP_NOIMPL(); }
void ResourceLoader::cancel() { STOP_NOIMPL(); }
void DocumentLoader::stopLoading() { STOP_NOIMPL(); }
void DocumentLoader::setLoading(bool) { STOP_NOIMPL(); }
void DocumentLoader::updateLoading() { STOP_NOIMPL(); }
void DocumentLoader::setFrame(Frame*) { STOP_NOIMPL(); }
void DocumentLoader::stopRecordingResponses() { STOP_NOIMPL(); }
void DocumentLoader::setPrimaryLoadComplete(bool) { STOP_NOIMPL(); }
void DocumentLoader::finishedLoading() { STOP_NOIMPL(); }
const KURL& DocumentLoader::URL() const { STOP_NOIMPL(); KURL* url = new KURL(); return *url; } // Eww, leaky. Implement this please.
bool DocumentLoader::isLoadingInAPISense()const { STOP_NOIMPL(); return false; }
FrameLoader* DocumentLoader::frameLoader()const { STOP_NOIMPL(); return 0; }

String FrameLoader::overrideMediaType()const { LOG_NOIMPL(); return String(); }
void FrameLoader::checkLoadCompleteForThisFrame() { LOG_NOIMPL(); }
void FrameLoader::loadEmptyDocumentSynchronously() { LOG_NOIMPL(); }
void FrameLoader::startLoading() { STOP_NOIMPL(); }
String FrameLoader::referrer() const { LOG_NOIMPL(); return String(); }
void FrameLoader::loadResourceSynchronously(const ResourceRequest&, ResourceResponse&, Vector<char>&) { }

bool EventHandler::lastEventIsMouseUp() const { return false; }

bool ResourceHandle::loadsBlocked() { return false; }
void ResourceHandle::loadResourceSynchronously(const ResourceRequest&, ResourceError&, ResourceResponse&, Vector<char>&) { }
