//
//  WebNSWindowExtras.m
//  WebKit
//
//  Created by Darin Adler on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebNSWindowExtras.h"

#import "WebKitLogging.h"
#import <objc/objc-runtime.h>

#define DISPLAY_REFRESH_INTERVAL (1.0 / 60.0)

static NSString *WebKitDisplayThrottleRunLoopMode = @"WebKitDisplayThrottleRunLoopMode";

static BOOL throttlingWindowDisplay;
static CFMutableDictionaryRef windowDisplayInfoDictionary;
static IMP oldNSWindowPostWindowNeedsDisplayIMP;
static IMP oldNSWindowDeallocIMP;
static IMP oldNSWindowFinalizeIMP;

typedef struct {
    NSWindow *window;
    CFTimeInterval lastDisplayTime;
    NSTimer *displayTimer;
} WindowDisplayInfo;

@interface NSWindow (WebExtrasInternal)
static IMP swizzleInstanceMethod(Class class, SEL selector, IMP newImplementation);
static void replacementPostWindowNeedsDisplay(id self, SEL cmd);
static void replacementDealloc(id self, SEL cmd);
static void replacementFinalize(id self, SEL cmd);
static WindowDisplayInfo *getWindowDisplayInfo(NSWindow *window);
static BOOL requestWindowDisplay(NSWindow *window);
static void cancelPendingWindowDisplay(WindowDisplayInfo *displayInfo);
- (void)_webkit_doPendingPostWindowNeedsDisplay:(NSTimer *)timer;
@end

@interface NSWindow (AppKitSecretsIKnow)
- (void)_postWindowNeedsDisplay;
@end

@implementation NSWindow (WebExtras)

+ (void)_webkit_enableWindowDisplayThrottle
{
    if (throttlingWindowDisplay)
        return;
    
    static BOOL installedWindowThrottle = NO;
    if (!installedWindowThrottle) {        
        // Override -[NSWindow _postWindowNeedsDisplay]
        ASSERT(!oldNSWindowPostWindowNeedsDisplayIMP);
        oldNSWindowPostWindowNeedsDisplayIMP = swizzleInstanceMethod(self, @selector(_postWindowNeedsDisplay), (IMP)replacementPostWindowNeedsDisplay);
        ASSERT(oldNSWindowPostWindowNeedsDisplayIMP);
        
        // Override -[NSWindow dealloc]
        ASSERT(!oldNSWindowDeallocIMP);
        oldNSWindowDeallocIMP = swizzleInstanceMethod(self, @selector(dealloc), (IMP)replacementDealloc);
        ASSERT(oldNSWindowDeallocIMP);

        // Override -[NSWindow finalize]
        ASSERT(!oldNSWindowFinalizeIMP);
        oldNSWindowFinalizeIMP = swizzleInstanceMethod(self, @selector(finalize), (IMP)replacementFinalize);
        ASSERT(oldNSWindowFinalizeIMP);

        installedWindowThrottle = YES;
    }

    windowDisplayInfoDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
    ASSERT(windowDisplayInfoDictionary);

    throttlingWindowDisplay = YES;
}

static void disableWindowDisplayThrottleApplierFunction(const void *key, const void *value, void *context)
{
    WindowDisplayInfo *displayInfo = (WindowDisplayInfo *)value;
    
    // Display immediately
    [displayInfo->window _postWindowNeedsDisplay];
    cancelPendingWindowDisplay(displayInfo);
    free(displayInfo);
}

+ (void)_webkit_displayThrottledWindows
{
    if (!throttlingWindowDisplay)
        return;
        
    // Force all throttle timers to fire by running the runloop in WebKitDisplayThrottleRunLoopMode until there are
    // no more runloop timers/sources for that mode.
    while (CFRunLoopRunInMode((CFStringRef)WebKitDisplayThrottleRunLoopMode, 1.0 / DISPLAY_REFRESH_INTERVAL, true) == kCFRunLoopRunHandledSource) {}
}

+ (void)_webkit_disableWindowDisplayThrottle
{
    if (!throttlingWindowDisplay)
        return;

    // Clear throttlingWindowDisplay first so that the displays triggered by disableWindowDisplayThrottleApplierFunction()
    // go through the normal NSWindow display path
    throttlingWindowDisplay = NO;
    
    // Display any windows that have pending displays
    CFDictionaryApplyFunction(windowDisplayInfoDictionary, disableWindowDisplayThrottleApplierFunction, NULL);
    
    CFRelease(windowDisplayInfoDictionary);
    windowDisplayInfoDictionary = NULL;
}

- (void)centerOverMainWindow
{
    NSRect frameToCenterOver;
    if ([NSApp mainWindow]) {
        frameToCenterOver = [[NSApp mainWindow] frame];
    } else {
        frameToCenterOver = [[NSScreen mainScreen] visibleFrame];
    }
    
    NSSize size = [self frame].size;
    NSPoint origin;
    origin.y = NSMaxY(frameToCenterOver)
        - (frameToCenterOver.size.height - size.height) / 3
        - size.height;
    origin.x = frameToCenterOver.origin.x
        + (frameToCenterOver.size.width - size.width) / 2;
    [self setFrameOrigin:origin];
}

@end

@implementation NSWindow (WebExtrasInternal)

// Returns the old method implementation
static IMP swizzleInstanceMethod(Class class, SEL selector, IMP newImplementation)
{
    Method method = class_getInstanceMethod(class, selector);
    ASSERT(method);
    IMP oldIMP;
#if OBJC_API_VERSION > 0
    oldIMP = method_setImplementation(method, newImplementation);
#else
    oldIMP = method->method_imp;
    method->method_imp = newImplementation;
#endif
    return oldIMP;
}

static void replacementPostWindowNeedsDisplay(id self, SEL cmd)
{
    // Call the original NSWindow method implementation if window throttling is disabled.
    if (!throttlingWindowDisplay) {
        oldNSWindowPostWindowNeedsDisplayIMP(self, cmd);
        return;
    }
        
    // Do not call into -[NSWindow _postWindowNeedsDisplay] if requestWindowDisplay() returns NO.  In that case, requestWindowDisplay()
    // will schedule a timer to display at the appropriate time.
    if (requestWindowDisplay(self))
        oldNSWindowPostWindowNeedsDisplayIMP(self, cmd);
}

static void clearWindowDisplayInfo(id self)
{
    ASSERT(throttlingWindowDisplay);
    
    // Remove WindowDisplayInfo for this window
    WindowDisplayInfo *displayInfo = (WindowDisplayInfo *)CFDictionaryGetValue(windowDisplayInfoDictionary, self);
    if (displayInfo) {
        cancelPendingWindowDisplay(displayInfo);
        free(displayInfo);
        CFDictionaryRemoveValue(windowDisplayInfoDictionary, self);
    }
}

static void replacementDealloc(id self, SEL cmd)
{
    oldNSWindowDeallocIMP(self, cmd);
    
    if (throttlingWindowDisplay)
        clearWindowDisplayInfo(self);
}

static void replacementFinalize(id self, SEL cmd)
{
    oldNSWindowFinalizeIMP(self, cmd);
    
    if (throttlingWindowDisplay)
        clearWindowDisplayInfo(self);
}

static WindowDisplayInfo *getWindowDisplayInfo(NSWindow *window)
{
    ASSERT(throttlingWindowDisplay);
    ASSERT(windowDisplayInfoDictionary);
    
    // Get the WindowDisplayInfo for this window, or create it if it does not exist
    WindowDisplayInfo *displayInfo = (WindowDisplayInfo *)CFDictionaryGetValue(windowDisplayInfoDictionary, window);
    if (!displayInfo) {
        displayInfo = (WindowDisplayInfo *)malloc(sizeof(WindowDisplayInfo));
        displayInfo->window = window;
        displayInfo->lastDisplayTime = 0;
        displayInfo->displayTimer = nil;
        CFDictionarySetValue(windowDisplayInfoDictionary, window, displayInfo);
    }
    
    return displayInfo;
}

static BOOL requestWindowDisplay(NSWindow *window)
{
    ASSERT(throttlingWindowDisplay);

    // Defer display if there is already a pending display
    WindowDisplayInfo *displayInfo = getWindowDisplayInfo(window);
    if (displayInfo->displayTimer)
        return NO;
        
    // Defer display if it hasn't been at least DISPLAY_REFRESH_INTERVAL seconds since the last display
    CFTimeInterval now = CFAbsoluteTimeGetCurrent();
    CFTimeInterval timeSinceLastDisplay = now - displayInfo->lastDisplayTime;
    if (timeSinceLastDisplay < DISPLAY_REFRESH_INTERVAL) {
        // Redisplay soon -- if we redisplay too quickly, we'll block due to pending CG coalesced updates
        displayInfo->displayTimer = [[NSTimer timerWithTimeInterval:(DISPLAY_REFRESH_INTERVAL - timeSinceLastDisplay)
                                                             target:window
                                                           selector:@selector(_webkit_doPendingPostWindowNeedsDisplay:)
                                                           userInfo:nil
                                                            repeats:NO] retain];
        
        // The NSWindow autodisplay mechanism is documented to only work for NSDefaultRunLoopMode, NSEventTrackingRunLoopMode, and NSModalPanelRunLoopMode
        NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
        [runLoop addTimer:displayInfo->displayTimer forMode:NSDefaultRunLoopMode];
        [runLoop addTimer:displayInfo->displayTimer forMode:NSEventTrackingRunLoopMode];
        [runLoop addTimer:displayInfo->displayTimer forMode:NSModalPanelRunLoopMode];

        // Schedule the timer in WebKitDisplayThrottleRunLoopMode so that +_webkit_displayThrottledWindows can
        // force all window throttle timers to fire by running in just that mode
        [runLoop addTimer:displayInfo->displayTimer forMode:WebKitDisplayThrottleRunLoopMode];
        
        return NO;
    }

    // Allow the display: there is no pending display, and it's been long enough to display again.
    displayInfo->lastDisplayTime = now;
    return YES;
}

static void cancelPendingWindowDisplay(WindowDisplayInfo *displayInfo)
{
    if (!displayInfo->displayTimer)
        return;

    [displayInfo->displayTimer invalidate];
    [displayInfo->displayTimer release];
    displayInfo->displayTimer = nil;
}

- (void)_webkit_doPendingPostWindowNeedsDisplay:(NSTimer *)timer
{
    WindowDisplayInfo *displayInfo = getWindowDisplayInfo(self);
    ASSERT(timer == displayInfo->displayTimer);
    ASSERT(throttlingWindowDisplay);
    
    // Clear displayInfo->displayTimer first because requestWindowDisplay() will not allow a display if the timer is non-nil.
    // However, we must wait to invalidate timer because it may have the last reference to this window.
    NSTimer *oldDisplayTimer = displayInfo->displayTimer;
    displayInfo->displayTimer = nil;
    
    // Don't call directly into oldNSWindowPostWindowNeedsDisplayIMP because we don't want to bypass subclass implementations
    // of -_postWindowNeedsDisplay
    [self _postWindowNeedsDisplay];
    
    // Finally, invalidate the firing display timer.
    [oldDisplayTimer invalidate];
    [oldDisplayTimer release];
}

@end
