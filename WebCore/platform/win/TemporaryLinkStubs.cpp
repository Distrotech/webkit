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
#include "BrowserExtensionWin.h"
#include "CookieJar.h"
#include "Cursor.h"
#include "EditCommand.h"
#include "FileChooser.h"
#include "Font.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "FrameWin.h"
#include "GraphicsContext.h"
#include "GraphicsContextPlatformPrivate.h"
#include "History.h"
#include "IconLoader.h"
#include "IntPoint.h"
#include "KURL.h"
#include "Language.h"
#include "ListBox.h"
#include "LocalizedStrings.h"
#include "Node.h"
#include "NotImplemented.h"
#include "Path.h"
#include "PlatformMouseEvent.h"
#include "PlugInInfoStore.h"
#include "PopupMenu.h"
#include "RenderTheme.h"
#include "RenderThemeWin.h"
#include "ResourceLoader.h"
#include "Screen.h"
#include "ScrollBar.h"
#include "Slider.h"
#include "TextBoundaries.h"
#include "TextField.h"
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

void TextField::selectAll() { LOG_NOIMPL(); }
void TextField::addSearchResult() { LOG_NOIMPL(); }
int TextField::selectionStart() const { LOG_NOIMPL(); return 0; }
bool TextField::hasSelectedText() const { LOG_NOIMPL(); return false; }
String TextField::selectedText() const { LOG_NOIMPL(); return String(); }
void TextField::setAutoSaveName(String const&) { LOG_NOIMPL(); }
bool TextField::checksDescendantsForFocus() const { LOG_NOIMPL(); return false; }
void TextField::setSelection(int,int) { LOG_NOIMPL(); }
void TextField::setMaxResults(int) { LOG_NOIMPL(); }
bool TextField::edited() const { LOG_NOIMPL(); return false; }

Slider::Slider() { STOP_NOIMPL(); }
IntSize Slider::sizeHint() const { STOP_NOIMPL(); return IntSize(); }
void Slider::setValue(double) { STOP_NOIMPL(); }
void Slider::setMaxValue(double) { STOP_NOIMPL(); }
void Slider::setMinValue(double) { STOP_NOIMPL(); }
Slider::~Slider() { STOP_NOIMPL(); }
void Slider::setFont(WebCore::Font const&) { STOP_NOIMPL(); }
double Slider::value() const { STOP_NOIMPL(); return 0; }

IntSize ListBox::sizeForNumberOfLines(int) const { LOG_NOIMPL(); return IntSize(); }
bool ListBox::isSelected(int) const { LOG_NOIMPL(); return false; }
bool ListBox::checksDescendantsForFocus() const { LOG_NOIMPL(); return false; }

Widget::FocusPolicy Slider::focusPolicy() const { LOG_NOIMPL(); return NoFocus; }
Widget::FocusPolicy ListBox::focusPolicy() const { LOG_NOIMPL(); return NoFocus; }
Widget::FocusPolicy TextField::focusPolicy() const { LOG_NOIMPL(); return NoFocus; }

Cursor::Cursor(Image*, const IntPoint&) { STOP_NOIMPL(); }

PlatformMouseEvent::PlatformMouseEvent(const CurrentEventTag&) { LOG_NOIMPL(); }
String WebCore::searchableIndexIntroduction() { LOG_NOIMPL(); return String(); }

int WebCore::findNextSentenceFromIndex(UChar const*,int,int,bool) { LOG_NOIMPL(); return 0; }
void WebCore::findSentenceBoundary(UChar const*,int,int,int*,int*) { LOG_NOIMPL(); }
int WebCore::findNextWordFromIndex(UChar const*,int,int,bool) { LOG_NOIMPL(); return 0; }

namespace WebCore {

Vector<char> ServeSynchronousRequest(Loader*,DocLoader*,ResourceLoader*,KURL&,DeprecatedString&) { STOP_NOIMPL(); return Vector<char>(); }

}

void FrameWin::focusWindow() { LOG_NOIMPL(); }
void FrameWin::unfocusWindow() { LOG_NOIMPL(); }
bool FrameWin::locationbarVisible() { LOG_NOIMPL(); return 0; }
void FrameWin::registerCommandForRedo(PassRefPtr<WebCore::EditCommand>) { STOP_NOIMPL(); }
KJS::Bindings::Instance* FrameWin::getAppletInstanceForWidget(Widget*) { STOP_NOIMPL(); return 0; }
bool FrameWin::canRedo() const { LOG_NOIMPL(); return false; }
bool FrameWin::canUndo() const { LOG_NOIMPL(); return false; }
bool FrameWin::canPaste() const { LOG_NOIMPL(); return false; }
bool FrameWin::canGoBackOrForward(int) const { LOG_NOIMPL(); return false; }
bool FrameWin::shouldInterruptJavaScript() { LOG_NOIMPL(); return false; }
bool FrameWin::openURL(KURL const&) { LOG_NOIMPL(); return false; }
void FrameWin::print() { LOG_NOIMPL(); }
void FrameWin::issueCutCommand() { LOG_NOIMPL(); }
void FrameWin::issueCopyCommand() { LOG_NOIMPL(); }
void FrameWin::issuePasteCommand() { LOG_NOIMPL(); }
void FrameWin::issueUndoCommand() { LOG_NOIMPL(); }
void FrameWin::issueRedoCommand(void) { LOG_NOIMPL(); }
void FrameWin::issuePasteAndMatchStyleCommand() { LOG_NOIMPL(); }
void FrameWin::issueTransposeCommand() { LOG_NOIMPL(); }
bool FrameWin::lastEventIsMouseUp() const { return false; }
void FrameWin::openURLRequest(const struct WebCore::FrameLoadRequest&) { LOG_NOIMPL(); }
String FrameWin::mimeTypeForFileName(String const&) const { LOG_NOIMPL(); return String(); }
void FrameWin::scheduleClose() { STOP_NOIMPL(); }
void FrameWin::markMisspellings(WebCore::Selection const&) { LOG_NOIMPL(); }
bool FrameWin::menubarVisible() { LOG_NOIMPL(); return false; }
bool FrameWin::personalbarVisible() { LOG_NOIMPL(); return false; }
bool FrameWin::statusbarVisible() { LOG_NOIMPL(); return false; }
bool FrameWin::toolbarVisible() { LOG_NOIMPL(); return false; }
KURL FrameWin::originalRequestURL() const { LOG_NOIMPL(); return KURL(); }
bool FrameWin::isLoadTypeReload(void) { LOG_NOIMPL(); return false; }
Widget* FrameWin::createJavaAppletWidget(const IntSize&, Element* element, const HashMap<String, String>& args) { STOP_NOIMPL(); return 0; }

bool BrowserExtensionWin::canRunModal() { LOG_NOIMPL(); return false; }
void BrowserExtensionWin::createNewWindow(struct WebCore::FrameLoadRequest const&,struct WebCore::WindowArgs const&,Frame*&) { STOP_NOIMPL(); }
bool BrowserExtensionWin::canRunModalNow() { LOG_NOIMPL(); return false; }
void BrowserExtensionWin::runModal() { STOP_NOIMPL(); }
void BrowserExtensionWin::goBackOrForward(int) { LOG_NOIMPL(); }
KURL BrowserExtensionWin::historyURL(int distance) { LOG_NOIMPL(); return KURL(); }
void BrowserExtensionWin::createNewWindow(struct WebCore::FrameLoadRequest const&) { STOP_NOIMPL(); }

void GraphicsContextPlatformPrivate::clip(const Path&) { LOG_NOIMPL(); }

int WebCore::screenDepthPerComponent(const Page*) { LOG_NOIMPL(); return 0; }
bool WebCore::screenIsMonochrome(const Page*) { LOG_NOIMPL(); return false; }

/********************************************************/
/* Completely empty stubs (mostly to allow DRT to run): */
/********************************************************/
static Cursor localCursor;
const Cursor& WebCore::moveCursor() { LOG_NOIMPL(); return localCursor; }

bool AXObjectCache::gAccessibilityEnabled = false;

String WebCore::submitButtonDefaultLabel() { return "Submit"; }
String WebCore::inputElementAltText() { LOG_NOIMPL(); return DeprecatedString(); }
String WebCore::resetButtonDefaultLabel() { return "Reset"; }
String WebCore::fileButtonChooseFileLabel() { return "Browse..."; }
String WebCore::fileButtonNoFileSelectedLabel() { return "no file selected"; }
String WebCore::defaultLanguage() { return "en"; }

void WebCore::ResourceLoader::assembleResponseHeaders() const { LOG_NOIMPL(); }
void WebCore::ResourceLoader::retrieveCharset() const { LOG_NOIMPL(); }

void FrameView::updateBorder() { LOG_NOIMPL(); }

void FrameWin::restoreDocumentState() { LOG_NOIMPL(); }
void FrameWin::partClearedInBegin() { LOG_NOIMPL(); }
void FrameWin::createEmptyDocument() { LOG_NOIMPL(); }
String FrameWin::overrideMediaType() const { LOG_NOIMPL(); return String(); }
Range* FrameWin::markedTextRange() const { LOG_NOIMPL(); return 0; }
void FrameWin::addMessageToConsole(String const&,unsigned int,String const&) { LOG_NOIMPL(); }
bool FrameWin::shouldChangeSelection(Selection const&,Selection const&,WebCore::EAffinity,bool) const { LOG_NOIMPL(); return true; }
void FrameWin::respondToChangedSelection(WebCore::Selection const&,bool) { LOG_NOIMPL(); }
static int frameNumber = 0;
void FrameWin::saveDocumentState() { LOG_NOIMPL(); }
void FrameWin::registerCommandForUndo(PassRefPtr<WebCore::EditCommand>) { LOG_NOIMPL(); }
void FrameWin::clearUndoRedoOperations(void) { LOG_NOIMPL(); }
String FrameWin::incomingReferrer() const { LOG_NOIMPL(); return String(); }
void FrameWin::markMisspellingsInAdjacentWords(WebCore::VisiblePosition const&) { LOG_NOIMPL(); }
void FrameWin::respondToChangedContents(const Selection &endingSelection) { LOG_NOIMPL(); }

BrowserExtensionWin::BrowserExtensionWin(WebCore::FrameWin*) { LOG_NOIMPL(); }
void BrowserExtensionWin::setTypedIconURL(KURL const&, const String&) { LOG_NOIMPL(); }
void BrowserExtensionWin::setIconURL(KURL const&) { LOG_NOIMPL(); }
int BrowserExtensionWin::getHistoryLength() { LOG_NOIMPL(); return 0; }

namespace WebCore {

bool CheckIfReloading(WebCore::DocLoader*) { LOG_NOIMPL(); return false; }
void CheckCacheObjectStatus(DocLoader*, CachedResource*) { LOG_NOIMPL(); }

}

void Widget::setIsSelected(bool) { LOG_NOIMPL(); }

bool ScrollView::inWindow() const { LOG_NOIMPL(); return true; }

void GraphicsContext::drawFocusRing(const Color& color) { LOG_NOIMPL(); }
void GraphicsContext::drawLineForMisspelling(const IntPoint& point, int width) { LOG_NOIMPL(); }
void GraphicsContext::setCompositeOperation(CompositeOperator) { LOG_NOIMPL(); }

TextField::TextField(TextField::Type) { LOG_NOIMPL(); }
TextField::~TextField() { LOG_NOIMPL(); }
void TextField::setFont(WebCore::Font const&) { LOG_NOIMPL(); }
void TextField::setAlignment(HorizontalAlignment) { LOG_NOIMPL(); }
void TextField::setWritingDirection(TextDirection) { LOG_NOIMPL(); }
int TextField::maxLength() const { LOG_NOIMPL(); return 0; }
void TextField::setMaxLength(int) { LOG_NOIMPL(); }
String TextField::text() const { LOG_NOIMPL(); return String(); }
void TextField::setText(String const&) { LOG_NOIMPL(); }
int TextField::cursorPosition() const { LOG_NOIMPL(); return 0; }
void TextField::setCursorPosition(int) { LOG_NOIMPL(); }
void TextField::setEdited(bool) { LOG_NOIMPL(); }
void TextField::setReadOnly(bool) { LOG_NOIMPL(); }
void TextField::setPlaceholderString(String const&) { LOG_NOIMPL(); }
void TextField::setColors(Color const&,Color const&) { LOG_NOIMPL(); }
IntSize TextField::sizeForCharacterWidth(int) const { LOG_NOIMPL(); return IntSize(); }
int TextField::baselinePosition(int) const { LOG_NOIMPL(); return 0; }
void TextField::setLiveSearch(bool) { LOG_NOIMPL(); }

ListBox::ListBox() { LOG_NOIMPL(); }
ListBox::~ListBox() { LOG_NOIMPL(); }
void ListBox::setSelectionMode(ListBox::SelectionMode) { LOG_NOIMPL(); }
void ListBox::setFont(WebCore::Font const&) { LOG_NOIMPL(); }
void ListBox::setWritingDirection(TextDirection) { LOG_NOIMPL(); }
void ListBox::clear() { LOG_NOIMPL(); }
void ListBox::doneAppendingItems() { LOG_NOIMPL(); }
void ListBox::setEnabled(bool) { LOG_NOIMPL(); }
void ListBox::appendItem(DeprecatedString const&,ListBoxItemType,bool) { LOG_NOIMPL(); }
void ListBox::setSelected(int,bool) { LOG_NOIMPL(); }

Color WebCore::focusRingColor() { LOG_NOIMPL(); return 0xFF0000FF; }
void WebCore::setFocusRingColorChangeFunction(void (*)()) { LOG_NOIMPL(); }

void Frame::setNeedsReapplyStyles() { LOG_NOIMPL(); }

void PopupMenu::addSeparator() { LOG_NOIMPL(); }
void PopupMenu::addGroupLabel(HTMLOptGroupElement*) { LOG_NOIMPL(); }

void IconLoader::receivedResponse(ResourceLoader*, PlatformResponse) { LOG_NOIMPL(); }
void IconLoader::notifyIconChanged(const KURL& iconURL) { LOG_NOIMPL(); }
