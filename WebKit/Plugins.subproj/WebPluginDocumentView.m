/*	
    WebPluginDocumentView.m
    Copyright 2004, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebPluginDocumentView.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPlugin.h>
#import <WebKit/WebPluginPrivate.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPluginPackage.h>
#import <WebKit/WebPluginViewFactoryPrivate.h>
#import <WebKit/WebView.h>

@implementation WebPluginDocumentView

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    needsLayout = YES;
    pluginController = [[WebPluginController alloc] initWithDocumentView:self];
    return self;
}

- (void)dealloc
{
    [pluginView release];
    [plugin release];
    [pluginController destroyAllPlugins];
    [pluginController release];
    [super dealloc];
}

- (void)drawRect:(NSRect)rect
{
    if (needsLayout) {
        [self layout];
    }
    [super drawRect:rect];
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    // Since this class is a WebDocumentView and WebDocumentRepresentation, setDataSource: will be called twice. Do work only once.
    if (dataSourceHasBeenSet)
        return;

    // As noted above, -setDataSource: will be called twice -- once for the WebDocumentRepresentation, once for the WebDocumentView.
    // We don't want to make the plugin until we know we're being committed as the WebDocumentView for the WebFrame.  This check is
    // to ensure that we've been added to the view hierarchy before attempting to create the plugin, as some plugins currently work
    // under this assumption.
    if (![self superview])
        return;

    dataSourceHasBeenSet = YES;
    
    NSURLResponse *response = [dataSource response];
    NSString *MIMEType = [response MIMEType];    
    plugin = (WebPluginPackage *)[[[WebPluginDatabase installedPlugins] pluginForMIMEType:MIMEType] retain];
    ASSERT([plugin isKindOfClass:[WebPluginPackage class]]);
    
    NSURL *URL = [response URL];
    NSString *URLString = [URL _web_originalDataAsString];
    NSDictionary *attributes = [[NSDictionary alloc] initWithObjectsAndKeys:MIMEType, @"type", URLString, @"src", nil];
    NSDictionary *arguments = [[NSDictionary alloc] initWithObjectsAndKeys:
        URL,                WebPlugInBaseURLKey,
        attributes,         WebPlugInAttributesKey,
        pluginController,   WebPlugInContainerKey,
        [NSNumber numberWithInt:WebPlugInModeFull], WebPlugInModeKey,
        [NSNumber numberWithBool:NO], WebPlugInShouldLoadMainResourceKey, // NO because we're already loading the data!
        nil];
    [attributes release];
    pluginView = [[WebPluginController plugInViewWithArguments:arguments fromPluginPackage:plugin] retain];
    [arguments release];

    ASSERT(pluginView != nil);

    [self addSubview:pluginView];
    [pluginController addPlugin:pluginView];
    [pluginView setFrame:[self bounds]];
    [pluginView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    
    if ([pluginView respondsToSelector:@selector(webPlugInMainResourceDidReceiveResponse:)])
        [pluginView webPlugInMainResourceDidReceiveResponse:[dataSource response]];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
    if (![pluginView respondsToSelector:@selector(webPlugInMainResourceDidReceiveData:)]) {
        // Cancel the load since this plug-in does its own loading.
        NSURLResponse *response = [dataSource response];
        // FIXME: See <rdar://problem/4258008>
        NSError *error = [[NSError alloc] _initWithPluginErrorCode:WebKitErrorPlugInWillHandleLoad
                                                        contentURL:[response URL]
                                                     pluginPageURL:nil
                                                        pluginName:[plugin name]
                                                          MIMEType:[response MIMEType]];
        [dataSource _stopLoadingWithError:error];
        [error release];
    }
}

- (void)setNeedsLayout:(BOOL)flag
{
    needsLayout = flag;
}

- (void)layout
{
    NSRect superFrame = [[self _web_superviewOfClass:[WebFrameView class]] frame];
    [self setFrame:NSMakeRect(0, 0, NSWidth(superFrame), NSHeight(superFrame))];
    needsLayout = NO;    
}

- (NSWindow *)currentWindow
{
    NSWindow *window = [self window];
    return window != nil ? window : [[self _webView] hostWindow];
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    if ([[self _webView] hostWindow] == nil) {
        [pluginController stopAllPlugins];
    }
}

- (void)viewDidMoveToWindow
{
    if ([self currentWindow] != nil) {
        [pluginController startAllPlugins];
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    if ([self window] == nil) {
        [pluginController stopAllPlugins];
    }
}

- (void)viewDidMoveToHostWindow
{
    if ([self currentWindow] != nil) {
        [pluginController startAllPlugins];
    }    
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
    if ([pluginView respondsToSelector:@selector(webPlugInMainResourceDidReceiveData:)])
        [pluginView webPlugInMainResourceDidReceiveData:data];
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource
{
    if ([pluginView respondsToSelector:@selector(webPlugInMainResourceDidFailWithError:)])
        [pluginView webPlugInMainResourceDidFailWithError:error];
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    if ([pluginView respondsToSelector:@selector(webPlugInMainResourceDidFinishLoading)])
        [pluginView webPlugInMainResourceDidFinishLoading];
}

- (BOOL)canProvideDocumentSource
{
    return NO;
}

- (NSString *)documentSource
{
    return nil;
}

- (NSString *)title
{
    return nil;
}

@end
