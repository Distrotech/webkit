//
//  WebNSWindowExtras.h
//  WebKit
//
//  Created by Darin Adler on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface NSWindow (WebExtras)
// Throttles the NSWindow autodisplay mechanism to a maximum of 60 frames per second.
// This ensures that dynamically updated web content does not cause the window to display
// too often, which can cause performance problems with CoreGraphics' coalesced updates
// feature.  See <http://developer.apple.com/technotes/tn2005/tn2133.html>.
+ (void)_webkit_enableWindowDisplayThrottle;

// Disables NSWindow display throttling.  Any windows with pending displays will be displayed
// immediately when window throttling is disabled.
+ (void)_webkit_disableWindowDisplayThrottle;

// Force all throttle timers to fire by running the runloop in WebKitDisplayThrottleRunLoopMode until there are
// no more runloop timers/sources for that mode.
+ (void)_webkit_displayThrottledWindows;

// centers "visually", putting 1/3 of the remaining space above, and 2/3 below
- (void)centerOverMainWindow; 
@end
