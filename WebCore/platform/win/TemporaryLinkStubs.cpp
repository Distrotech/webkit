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
#include "LocalizedStrings.h"
#include "MainResourceLoader.h"
#include "NavigationAction.h"
#include "Node.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PageCache.h"
#include "Path.h"
#include "PlatformMouseEvent.h"
#include "PlugInInfoStore.h"
#include "RenderTheme.h"
#include "RenderThemeWin.h"
#include "ResourceHandle.h"
#include "ResourceLoader.h"
#include "ResourceResponse.h"
#include "ResourceRequest.h"
#include "Screen.h"
#include "ScrollBar.h"
#include "SSLKeyGenerator.h"
#include "SubresourceLoader.h"
#include "SystemTime.h"
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

void Frame::print() { LOG_NOIMPL(); }
void Frame::issueTransposeCommand() { LOG_NOIMPL(); }

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
String WebCore::contextMenuItemTagOpenLinkInNewWindow() { return "Open Link in New Window"; }
String WebCore::contextMenuItemTagDownloadLinkToDisk() { return "Download Linked File"; }
String WebCore::contextMenuItemTagCopyLinkToClipboard() { return "Copy Link"; }
String WebCore::contextMenuItemTagOpenImageInNewWindow() { return "Open Image in New Window"; }
String WebCore::contextMenuItemTagDownloadImageToDisk() { return "Download Image"; }
String WebCore::contextMenuItemTagCopyImageToClipboard() { return "Copy Image"; }
String WebCore::contextMenuItemTagOpenFrameInNewWindow() { return "Open Frame in New Window"; }
String WebCore::contextMenuItemTagCopy() { return "Copy"; }
String WebCore::contextMenuItemTagGoBack() { return "Back"; }
String WebCore::contextMenuItemTagGoForward() { return "Forward"; }
String WebCore::contextMenuItemTagStop() { return "Stop"; }
String WebCore::contextMenuItemTagReload() { return "Reload"; }
String WebCore::contextMenuItemTagCut() { return "Cut"; }
String WebCore::contextMenuItemTagPaste() { return "Paste"; }
String WebCore::contextMenuItemTagNoGuessesFound() { return "No Guesses Found"; }
String WebCore::contextMenuItemTagIgnoreSpelling() { return "Ignore Spelling"; }
String WebCore::contextMenuItemTagLearnSpelling() { return "Learn Spelling"; }
String WebCore::contextMenuItemTagSearchWeb() { return "Search in Google"; }
String WebCore::contextMenuItemTagLookUpInDictionary() { return "Look Up in Dictionary"; }
String WebCore::contextMenuItemTagOpenLink() { return "Open Link"; }
String WebCore::contextMenuItemTagIgnoreGrammar() { return "Ignore Grammar"; }
String WebCore::contextMenuItemTagSpellingMenu() { return "Spelling and Grammar"; }
String WebCore::contextMenuItemTagCheckSpelling() { return "Check Document Now"; }
String WebCore::contextMenuItemTagCheckSpellingWhileTyping() { return "Check Spelling While Typing"; }
String WebCore::contextMenuItemTagCheckGrammarWithSpelling() { return "Check Grammar With Spelling"; }
String WebCore::contextMenuItemTagFontMenu() { return "Font"; }
String WebCore::contextMenuItemTagBold() { return "Bold"; }
String WebCore::contextMenuItemTagItalic() { return "Italic"; }
String WebCore::contextMenuItemTagUnderline() { return "Underline"; }
String WebCore::contextMenuItemTagOutline() { return "Outline"; }
String WebCore::contextMenuItemTagWritingDirectionMenu() { return "Writing Direction"; }
String WebCore::contextMenuItemTagDefaultDirection() { return "Default"; }
String WebCore::contextMenuItemTagLeftToRight() { return "Left to Right"; }
String WebCore::contextMenuItemTagRightToLeft() { return "Right to Left"; }
String WebCore::contextMenuItemTagShowSpellingPanel(bool show)
{
    return show ? "Show Spelling and Grammar" : "Hide Spelling and Grammar";
}
// Search menu strings
String WebCore::searchMenuNoRecentSearchesText() { return "No recent searches"; }
String WebCore::searchMenuRecentSearchesText() { return "Recent Searches"; }
String WebCore::searchMenuClearRecentSearchesText() { return "Clear Recent Searches"; }

// Accessibility strings
String WebCore::AXWebAreaText() { return "web area"; }
String WebCore::AXLinkText() { return "link"; }
String WebCore::AXListMarkerText() { return "list marker"; }
String WebCore::AXImageMapText() { return "image map"; }
String WebCore::AXHeadingText() { return "heading"; }

namespace WebCore {

void FrameView::updateBorder() { LOG_NOIMPL(); }

void Frame::respondToChangedSelection(WebCore::Selection const&,bool) { LOG_NOIMPL(); }

void Editor::ignoreSpelling() { LOG_NOIMPL(); }
void Editor::learnSpelling() { LOG_NOIMPL(); }
bool Editor::isSelectionMisspelled() { LOG_NOIMPL(); return false; }
bool Editor::isSelectionUngrammatical() { LOG_NOIMPL(); return false; }
Vector<String> Editor::guessesForMisspelledSelection() { LOG_NOIMPL(); return Vector<String>(); }
Vector<String> Editor::guessesForUngrammaticalSelection() { LOG_NOIMPL(); return Vector<String>(); }
void Editor::markMisspellingsAfterTypingToPosition(WebCore::VisiblePosition const&) { LOG_NOIMPL(); }
void Editor::advanceToNextMisspelling(bool) { LOG_NOIMPL(); }

}

void Widget::setIsSelected(bool) { LOG_NOIMPL(); }

bool ScrollView::inWindow() const { LOG_NOIMPL(); return true; }

void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint& point, int width, bool grammar) { LOG_NOIMPL(); }

Color WebCore::focusRingColor() { LOG_NOIMPL(); return 0xFF7DADD9; }
void WebCore::setFocusRingColorChangeFunction(void (*)()) { LOG_NOIMPL(); }

void Frame::setNeedsReapplyStyles() { LOG_NOIMPL(); }


String FrameLoader::overrideMediaType()const { LOG_NOIMPL(); return String(); }

bool ResourceHandle::loadsBlocked() { return false; }
void ResourceHandle::setDefersLoading(bool) { }

// CRITFIXME: See if any of the following are actually implemented in OpenSource and copy the impls to internal
bool DocumentLoader::getResponseModifiedHeader(String&) const { LOG_NOIMPL(); return false; }
bool ResourceHandle::willLoadFromCache(ResourceRequest&) { LOG_NOIMPL(); return false; }
float WebCore::userIdleTime() { LOG_NOIMPL(); return 0.0; }
void PageCache::close() { LOG_NOIMPL(); }

Vector<String> supportedKeySizes() { LOG_NOIMPL(); return Vector<String>(); }
String signedPublicKeyAndChallengeString(unsigned, const String&, const KURL&) { LOG_NOIMPL(); return String();
