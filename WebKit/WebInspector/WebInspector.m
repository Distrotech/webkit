/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebInspector.h"
#import "WebInspectorInternal.h"

#import "WebFrame.h"
#import "WebFrameInternal.h"
#import "WebInspectorPanel.h"
#import "WebNodeHighlight.h"
#import "WebPreferences.h"
#import "WebScriptDebugDelegate.h"
#import "WebView.h"
#import "WebViewPrivate.h"

#import <WebKit/DOMCore.h>
#import <WebKit/DOMExtensions.h>

@interface NSWindow (NSWindowPrivate)
- (void)_setContentHasShadow:(BOOL)hasShadow;
@end

#pragma mark -

@implementation WebInspector

+ (WebInspector *)sharedWebInspector
{
    static WebInspector *_sharedWebInspector = nil;
    if (!_sharedWebInspector) {
        _sharedWebInspector = [[[self alloc] init] autorelease];
        CFRetain(_sharedWebInspector);
        _sharedWebInspector->_private->isSharedInspector = YES;
    }
    return _sharedWebInspector;
}

#pragma mark -

- (id)init
{
    if (![super initWithWindow:nil])
        return nil;

    [self setWindowFrameAutosaveName:@"Web Inspector"];

    _private = [[WebInspectorPrivate alloc] init];

    return self;
}

- (id)initWithWebFrame:(WebFrame *)webFrame
{
    if (![self init])
        return nil;
    [self setWebFrame:webFrame];
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

#pragma mark -

- (NSWindow *)window
{
    NSWindow *window = [super window];
    if (!window) {
        NSPanel *window = [[WebInspectorPanel alloc] initWithContentRect:NSMakeRect(60.0f, 200.0f, 350.0f, 550.0f)\
            styleMask:(NSBorderlessWindowMask | NSUtilityWindowMask) backing:NSBackingStoreBuffered defer:YES];
        [window setBackgroundColor:[NSColor clearColor]];
        [window setOpaque:NO];
        [window setHasShadow:YES];
        [window _setContentHasShadow:NO];
        [window setWorksWhenModal:YES];
        [window setAcceptsMouseMovedEvents:YES];
        [window setIgnoresMouseEvents:NO];
        [window setFloatingPanel:YES];
        [window setReleasedWhenClosed:YES];
        [window setMovableByWindowBackground:YES];
        [window setHidesOnDeactivate:NO];
        [window setDelegate:self];
        [window setMinSize:NSMakeSize(280.0f, 450.0f)];

        // Keep preferences separate from the rest of the client, making sure we are using expected preference values.
        // One reason this is good is that it keeps the inspector out of history via "private browsing".
        WebPreferences *preferences = [[WebPreferences alloc] init];
        [preferences setPrivateBrowsingEnabled:YES];
        [preferences setLoadsImagesAutomatically:YES];
        [preferences setPlugInsEnabled:YES];
        [preferences setJavaScriptEnabled:YES];
        [preferences setUserStyleSheetEnabled:NO];
        [preferences setTabsToLinks:NO];
        [preferences setMinimumFontSize:0];
        [preferences setMinimumLogicalFontSize:9];

        _private->webView = [[WebView alloc] initWithFrame:[[window contentView] frame] frameName:nil groupName:nil];
        [_private->webView setPreferences:preferences];
        [_private->webView setFrameLoadDelegate:self];
        [_private->webView setUIDelegate:self];
#ifndef NDEBUG
        [_private->webView setScriptDebugDelegate:self];
#endif
        [_private->webView setDrawsBackground:NO];
        [_private->webView setProhibitsMainFrameScrolling:YES];
        [_private->webView _setDashboardBehavior:WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows to:YES];
        [_private->webView _setDashboardBehavior:WebDashboardBehaviorAlwaysAcceptsFirstMouse to:YES];

        [preferences release];

        [window setContentView:_private->webView];

        NSString *path = [[NSBundle bundleForClass:[self class]] pathForResource:@"inspector" ofType:@"html" inDirectory:@"webInspector"];
        [[_private->webView mainFrame] loadRequest:[[[NSURLRequest alloc] initWithURL:[NSURL fileURLWithPath:path]] autorelease]];

        [self setWindow:window];
        return window;
    }

    return window;
}

- (void)windowWillClose:(NSNotification *)notification
{
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationWillResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationDidBecomeActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:WebNodeHighlightExpiredNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSViewFrameDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSSystemColorsDidChangeNotification object:nil];
    [self setWebFrame:nil];

    if (!_private->isSharedInspector)
        [self release];
}

- (IBAction)showWindow:(id)sender
{
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_nodeHighlightExpired:) name:WebNodeHighlightExpiredNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_updateSystemColors) name:NSSystemColorsDidChangeNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationWillResignActive) name:NSApplicationWillResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationDidBecomeActive) name:NSApplicationDidBecomeActiveNotification object:nil];

    [self _update];
    [self _updateSystemColors];

    [super showWindow:sender];
}

#pragma mark -

- (void)setWebFrame:(WebFrame *)webFrame
{
    if ([webFrame isEqual:_private->webFrame])
        return;

    if (_private->webFrame) {
        [[NSNotificationCenter defaultCenter] removeObserver:self name:WebViewProgressFinishedNotification object:[_private->webFrame webView]];
        [_private->webFrame _removeInspector:self];
    }

    [webFrame retain];
    [_private->webFrame release];
    _private->webFrame = webFrame;

    if (_private->webFrame) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(inspectedWebViewProgressFinished:) name:WebViewProgressFinishedNotification object:[_private->webFrame webView]];
        [_private->webFrame _addInspector:self];
    }

    _private->preventHighlight = YES;
    [self setFocusedDOMNode:[webFrame DOMDocument]];
    _private->preventHighlight = NO;
}

- (WebFrame *)webFrame
{
    return _private->webFrame;
}

#pragma mark -

- (void)setRootDOMNode:(DOMNode *)node
{
    if ([node isSameNode:_private->rootNode])
        return;

    [node retain];
    [_private->rootNode release];
    _private->rootNode = node;

    [self _updateRoot];
}

- (DOMNode *)rootDOMNode
{
    return _private->rootNode;
}

#pragma mark -

- (void)setFocusedDOMNode:(DOMNode *)node
{
    if ([node isSameNode:_private->focusedNode])
        return;

    [[NSRunLoop currentRunLoop] cancelPerformSelector:@selector(_highlightNode:) target:self argument:_private->focusedNode];

    [node retain];
    [_private->focusedNode release];
    _private->focusedNode = node;

    DOMNode *root = _private->rootNode;
    if (!root || (![root isSameNode:node] && ![root _isAncestorOfNode:node]))
        [self setRootDOMNode:node];

    if (!_private->webViewLoaded)
        return;

    [self _update];

    if (!_private->preventHighlight) {
        NSRect bounds = [node boundingBox];
        if (!NSIsEmptyRect(bounds)) {
            NSRect visible = [[[_private->webFrame frameView] documentView] visibleRect];
            BOOL needsScroll = !NSContainsRect(visible, bounds) && !NSContainsRect(bounds, visible);

            // only scroll if the bounds isn't in the visible rect and dosen't contain the visible rect
            if (needsScroll) {
                // scroll to the parent element if we aren't focused on an element
                DOMElement *element = (DOMElement *)_private->focusedNode;
                if (![element isKindOfClass:[DOMElement class]])
                    element = (DOMElement *)[element parentNode];

                if ([element isKindOfClass:[DOMElement class]])
                    [element scrollIntoViewIfNeeded:YES];

                // give time for the scroll to happen
                [self performSelector:@selector(_highlightNode:) withObject:node afterDelay:0.25];
            } else
                [self _highlightNode:node];
        } else
            [_private->currentHighlight expire];
    }
}

- (DOMNode *)focusedDOMNode
{
    return _private->focusedNode;
}

#pragma mark -

- (void)setSearchQuery:(NSString *)query
{
    if (_private->webViewLoaded) {
        if (!query)
            query = @"";
        [[_private->webView windowScriptObject] callWebScriptMethod:@"performSearch" withArguments:[NSArray arrayWithObject:query]];
        DOMHTMLInputElement *search = (DOMHTMLInputElement *)[_private->domDocument getElementById:@"search"];
        [search setValue:query];
    } else {
        [query retain];
        [_private->searchQuery release];
        _private->searchQuery = query;
    }
}

- (NSString *)searchQuery
{
    return _private->searchQuery;
}
@end

#pragma mark -

@implementation WebInspector (WebInspectorScripting)
- (void)showOptionsMenu
{
    NSMenu *menu = [[NSMenu alloc] init];
    [menu setAutoenablesItems:NO];

    NSMenuItem *item = [[NSMenuItem alloc] init];
    [item setTitle:@"Ignore Whitespace"];
    [item setTarget:self];
    [item setAction:@selector(_toggleIgnoreWhitespace:)];

    id value = [[_private->webView windowScriptObject] valueForKey:@"ignoreWhitespace"];
    [item setState:( value && [value isKindOfClass:[NSNumber class]] && [value boolValue] ? NSOnState : NSOffState )];

    [menu addItem:item];
    [item release];

    item = [[NSMenuItem alloc] init];
    [item setTitle:@"Show User Agent Styles"];
    [item setTarget:self];
    [item setAction:@selector(_toggleShowUserAgentStyles:)];

    value = [[_private->webView windowScriptObject] valueForKey:@"showUserAgentStyles"];
    [item setState:( value && [value isKindOfClass:[NSNumber class]] && [value boolValue] ? NSOnState : NSOffState )];

    [menu addItem:item];
    [item release];

    [NSMenu popUpContextMenu:menu withEvent:[[self window] currentEvent] forView:_private->webView];
    [menu release];

    // hack to force a layout and balance mouse events
    NSEvent *currentEvent = [[self window] currentEvent];
    NSEvent *event = [NSEvent mouseEventWithType:NSLeftMouseUp location:[currentEvent locationInWindow] modifierFlags:[currentEvent modifierFlags] timestamp:[currentEvent timestamp] windowNumber:[[currentEvent window] windowNumber] context:[currentEvent context] eventNumber:[currentEvent eventNumber] clickCount:[currentEvent clickCount] pressure:[currentEvent pressure]];
    [[[[_private->webView mainFrame] frameView] documentView] mouseUp:event];
}
@end

#pragma mark -

@implementation WebInspector (WebInspectorPrivate)
- (IBAction)_toggleIgnoreWhitespace:(id)sender
{
    if (_private->webViewLoaded)
        [[_private->webView windowScriptObject] callWebScriptMethod:@"toggleIgnoreWhitespace" withArguments:nil];
}

- (IBAction)_toggleShowUserAgentStyles:(id)sender
{
    if (_private->webViewLoaded)
        [[_private->webView windowScriptObject] callWebScriptMethod:@"toggleShowUserAgentStyles" withArguments:nil];
}

- (void)_highlightNode:(DOMNode *)node
{
    if (_private->currentHighlight) {
        [_private->currentHighlight expire];
        [_private->currentHighlight release];
        _private->currentHighlight = nil;
    }

    NSView *view = [[_private->webFrame frameView] documentView];
    NSRect bounds = NSIntersectionRect([node boundingBox], [view visibleRect]);
    if (!NSIsEmptyRect(bounds)) {
        NSArray *rects = nil;
        if ([node isKindOfClass:[DOMElement class]]) {
            DOMCSSStyleDeclaration *style = [_private->domDocument getComputedStyle:(DOMElement *)node pseudoElement:@""];
            if ([[style getPropertyValue:@"display"] isEqualToString:@"inline"])
                rects = [node lineBoxRects];
        } else if ([node isKindOfClass:[DOMText class]]
#if ENABLE(SVG)
                   && ![[node parentNode] isKindOfClass:NSClassFromString(@"DOMSVGElement")]
#endif
                  )
            rects = [node lineBoxRects];

        if (![rects count])
            rects = [NSArray arrayWithObject:[NSValue valueWithRect:bounds]];

        if ([view window]) // skip the highlight if we have no window (e.g. hidden tab)
            _private->currentHighlight = [[WebNodeHighlight alloc] initWithBounds:bounds andRects:rects forView:view];
    }
}

- (void)_nodeHighlightExpired:(NSNotification *)notification
{
    if (_private->currentHighlight == [notification object]) {
        [_private->currentHighlight release];
        _private->currentHighlight = nil;
    }
}

- (void)_update
{
    if (_private->webViewLoaded)
        [[_private->webView windowScriptObject] callWebScriptMethod:@"updatePanes" withArguments:nil];
}

- (void)_updateRoot
{
    if (_private->webViewLoaded)
        [[_private->webView windowScriptObject] callWebScriptMethod:@"updateTreeOutline" withArguments:nil];
}

- (void)_updateSystemColors
{
    if (!_private->webViewLoaded)
        return;

    CGFloat red = 0.0f, green = 0.0f, blue = 0.0f;
    NSColor *color = [[NSColor alternateSelectedControlColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    [color getRed:&red green:&green blue:&blue alpha:NULL];

    NSString *colorText = [NSString stringWithFormat:@"rgba(%d,%d,%d,0.4) !important", (int)(red * 255), (int)(green * 255), (int)(blue * 255)];
    NSString *styleText = [NSString stringWithFormat:@".focused .selected { background-color: %1$@ } .blured .selected { border-color: %1$@ }", colorText];
    DOMElement *style = [_private->domDocument getElementById:@"systemColors"];
    if (!style) {
        style = [_private->domDocument createElement:@"style"];
        [[[_private->domDocument getElementsByTagName:@"head"] item:0] appendChild:style];
    }

    [style setAttribute:@"id" value:@"systemColors"];
    [style setTextContent:styleText];
}

- (void)_applicationWillResignActive
{
    [(NSPanel *)[self window] setFloatingPanel:NO];
}

- (void)_applicationDidBecomeActive
{
    [(NSPanel *)[self window] setFloatingPanel:YES];
}

- (void)_webFrameDetached:(WebFrame *)frame
{
    [self setFocusedDOMNode:nil];
    [self setWebFrame:nil];
    [self _update];
    [self _updateRoot];
}

#pragma mark -

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    return NO;
}

+ (NSString *)webScriptNameForSelector:(SEL)aSelector {
    NSMutableString *name = [[NSStringFromSelector(aSelector) mutableCopy] autorelease];
    [name replaceOccurrencesOfString:@":" withString:@"_" options:NSLiteralSearch range:NSMakeRange(0, [name length])];
    if ([name hasSuffix:@"_"])
        return [name substringToIndex:[name length] - 1];
    return name;
}

+ (BOOL)isKeyExcludedFromWebScript:(const char *)name
{
    return NO;
}

#pragma mark -

- (void)inspectedWebViewProgressFinished:(NSNotification *)notification
{
    if ([notification object] == [[self webFrame] webView]) {
        [self setFocusedDOMNode:[[self webFrame] DOMDocument]];
        [self _update];
        [self _updateRoot];
        [self _highlightNode:[self focusedDOMNode]];
    }
}

#pragma mark -

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    // note: this is the Inspector's own WebView, not the one being inspected
    _private->webViewLoaded = YES;
    [[sender windowScriptObject] setValue:self forKey:@"Inspector"];

    [_private->domDocument release];
    _private->domDocument = (DOMHTMLDocument *)[[[sender mainFrame] DOMDocument] retain];

    [[self window] invalidateShadow];

    [self _update];
    [self _updateRoot];
    [self _updateSystemColors];

    if ([[self searchQuery] length])
        [self setSearchQuery:[self searchQuery]];

    [self _highlightNode:[self focusedDOMNode]];
}

- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    NSLog(@"%@", message);
}

#ifndef NDEBUG
- (void)webView:(WebView *)view didParseSource:(NSString *)source baseLineNumber:(unsigned)baseLine fromURL:(NSURL *)url sourceId:(int)sid forWebFrame:(WebFrame *)webFrame
{
    if (!_private->debugFileMap)
        _private->debugFileMap = [[NSMutableDictionary alloc] init];
    if (url)
        [_private->debugFileMap setObject:url forKey:[NSNumber numberWithInt:sid]];
}

- (void)webView:(WebView *)view exceptionWasRaised:(WebScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno forWebFrame:(WebFrame *)webFrame
{
    NSURL *url = [_private->debugFileMap objectForKey:[NSNumber numberWithInt:sid]];
    NSLog(@"JS exception: %@ on %@ line %d", [[frame exception] valueForKey:@"message"], url, lineno);

    NSMutableArray *stack = [[NSMutableArray alloc] init];
    WebScriptCallFrame *currentFrame = frame;
    while (currentFrame) {
        if ([currentFrame functionName])
            [stack addObject:[currentFrame functionName]];
        else if ([frame caller])
            [stack addObject:@"(anonymous function)"];
        else
            [stack addObject:@"(global scope)"];
        currentFrame = [currentFrame caller];
    }

    NSLog(@"Stack trace:\n%@", [stack componentsJoinedByString:@"\n"]);
    [stack release];
}
#endif
@end

#pragma mark -

@implementation WebInspectorPrivate
- (void)dealloc
{
    [webView release];
    [domDocument release];
    [webFrame release];
    [rootNode release];
    [focusedNode release];
    [searchQuery release];
    [currentHighlight release];
#ifndef NDEBUG
    [debugFileMap release];
#endif
    [super dealloc];
}
@end

#pragma mark -

@implementation DOMNode (DOMNodeInspectorAdditions)
- (BOOL)_isAncestorOfNode:(DOMNode *)node
{
    DOMNode *currentNode = [node parentNode];
    while (currentNode) {
        if ([self isSameNode:currentNode])
            return YES;
        currentNode = [currentNode parentNode];
    }
    return NO;
}
@end
