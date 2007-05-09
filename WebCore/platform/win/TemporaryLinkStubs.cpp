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
#include "CachedPage.h"
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

void Frame::print() { LOG_NOIMPL(); }
void Frame::issueTransposeCommand() { LOG_NOIMPL(); }

void GraphicsContextPlatformPrivate::clip(const Path&) { LOG_NOIMPL(); }

bool AXObjectCache::gAccessibilityEnabled = false;

String WebCore::defaultLanguage() { return "en"; }

namespace WebCore {

void FrameView::updateBorder() { LOG_NOIMPL(); }

}

void Widget::setIsSelected(bool) { LOG_NOIMPL(); }

bool ScrollView::inWindow() const { LOG_NOIMPL(); return true; }

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
void CachedPage::close() { LOG_NOIMPL(); }

Vector<String> WebCore::supportedKeySizes() { LOG_NOIMPL(); return Vector<String>(); }
String WebCore::signedPublicKeyAndChallengeString(unsigned, const String&, const KURL&) { LOG_NOIMPL(); return String(); }
