/*	
    WebPDFView.m
    Copyright 2004, Apple, Inc. All rights reserved.
*/

#ifndef OMIT_TIGER_FEATURES

#import <Foundation/NSString_NSURLExtras.h>

#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameInternal.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebAssertions.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPDFView.h>

#import <Quartz/Quartz.h>

// QuartzPrivate.h doesn't include the PDFKit private headers, so we can't get at PDFViewPriv.h. (3957971)
// Even if that was fixed, we'd have to tweak compile options to include QuartzPrivate.h. (3957839)

@interface PDFDocument (PDFKitSecretsIKnow)
- (NSPrintOperation *)getPrintOperationForPrintInfo:(NSPrintInfo *)printInfo autoRotate:(BOOL)doRotate;
@end

NSString *_NSPathForSystemFramework(NSString *framework);

@implementation WebPDFView

+ (NSBundle *)PDFKitBundle
{
    static NSBundle *PDFKitBundle = nil;
    if (PDFKitBundle == nil) {
        NSString *PDFKitPath = [_NSPathForSystemFramework(@"Quartz.framework") stringByAppendingString:@"/Frameworks/PDFKit.framework"];
        if (PDFKitPath == nil) {
            ERROR("Couldn't find PDFKit.framework");
            return nil;
        }
        PDFKitBundle = [NSBundle bundleWithPath:PDFKitPath];
        if (![PDFKitBundle load]) {
            ERROR("Couldn't load PDFKit.framework");
        }
    }
    return PDFKitBundle;
}

+ (Class)PDFViewClass
{
    static Class PDFViewClass = nil;
    if (PDFViewClass == nil) {
        PDFViewClass = [[WebPDFView PDFKitBundle] classNamed:@"PDFView"];
        if (PDFViewClass == nil) {
            ERROR("Couldn't find PDFView class in PDFKit.framework");
        }
    }
    return PDFViewClass;
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        [self setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
        PDFSubview = [[[[self class] PDFViewClass] alloc] initWithFrame:frame];
        [PDFSubview setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
        [self addSubview:PDFSubview];
        [PDFSubview setDelegate:self];
        written = NO;
    }
    return self;
}

- (void)dealloc
{
    [PDFSubview release];
    [path release];
    [super dealloc];
}

- (PDFView *)PDFSubview
{
    return PDFSubview;
}

#define TEMP_PREFIX "/tmp/XXXXXX-"
#define OBJC_TEMP_PREFIX @"/tmp/XXXXXX-"

static void applicationInfoForMIMEType(NSString *type, NSString **name, NSImage **image)
{
    NSURL *appURL = nil;
    
    OSStatus error = LSCopyApplicationForMIMEType((CFStringRef)type, kLSRolesAll, (CFURLRef *)&appURL);
    if(error != noErr){
        return;
    }

    NSString *appPath = [appURL path];
    CFRelease (appURL);
    
    *image = [[NSWorkspace sharedWorkspace] iconForFile:appPath];  
    [*image setSize:NSMakeSize(16.f,16.f)];  
    
    NSString *appName = [[NSFileManager defaultManager] displayNameAtPath:appPath];
    *name = appName;
}


- (NSString *)path
{
    // Generate path once.
    if (path)
        return path;
        
    NSString *filename = [[dataSource response] suggestedFilename];
    NSFileManager *manager = [NSFileManager defaultManager];    
    
    path = [@"/tmp/" stringByAppendingPathComponent: filename];
    if ([manager fileExistsAtPath:path]) {
        path = [OBJC_TEMP_PREFIX stringByAppendingString:filename];
        char *cpath = (char *)[path fileSystemRepresentation];
        
        int fd = mkstemps (cpath, strlen(cpath) - strlen(TEMP_PREFIX) + 1);
        if (fd < 0) {
            // Couldn't create a temporary file!  Should never happen.  Do
            // we need an alert here?
            path = nil;
        }
        else {
            close (fd);
            path = [manager stringWithFileSystemRepresentation:cpath length:strlen(cpath)];
        }
    }
    
    [path retain];
    
    return path;
}

- (NSView *)hitTest:(NSPoint)point
{
    // Override hitTest so we can override menuForEvent.
    NSEvent *event = [NSApp currentEvent];
    NSEventType type = [event type];
    if (type == NSRightMouseDown || (type == NSLeftMouseDown && ([event modifierFlags] & NSControlKeyMask))) {
        return self;
    }
    return [super hitTest:point];
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
    NSMenu *menu = [PDFSubview menuForEvent:theEvent];
    
    NSString *appName = nil;
    NSImage *appIcon = nil;
    
    applicationInfoForMIMEType([[dataSource response] MIMEType], &appName, &appIcon);
    if (!appName)
        appName = UI_STRING("Finder", "Default application name for Open With context menu");
    
    NSString *title = [NSString stringWithFormat:UI_STRING("Open with %@", "Open document using the Finder"), appName];
    
    NSMenuItem *item = [menu insertItemWithTitle:title action:@selector(openWithFinder:) keyEquivalent:@"" atIndex:0];
    if (appIcon)
        [item setImage:appIcon];
        
    [menu insertItem:[NSMenuItem separatorItem] atIndex:1];
    
    return menu;
}

- (void)setDataSource:(WebDataSource *)ds
{
    dataSource = ds;
    [self setFrame:[[self superview] frame]];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

- (void)setNeedsLayout:(BOOL)flag
{
}

- (void)layout
{
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
}

- (void)viewDidMoveToHostWindow
{
}

- (void)openWithFinder:(id)sender
{
    NSString *opath = [self path];
    
    if (opath) {
        if (!written) {
            // Create a PDF file with the minimal permissions (only accessible to the current user, see 4145714)
            NSNumber *permissions = [[NSNumber alloc] initWithInt:S_IRUSR];
            NSDictionary *fileAttributes = [[NSDictionary alloc] initWithObjectsAndKeys:permissions, NSFilePosixPermissions, nil];
            [permissions release];
            
            [[NSFileManager defaultManager] createFileAtPath:opath contents:[dataSource data] attributes:fileAttributes];
            
            [fileAttributes release];
            written = YES;
        }
    
        if (![[NSWorkspace sharedWorkspace] openFile:opath]) {
            // NSWorkspace couldn't open file.  Do we need an alert
            // here?  We ignore the error elsewhere.
        }
    }
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag;
{
    int options = 0;
    if (!forward) {
        options |= NSBackwardsSearch;
    }
    if (!caseFlag) {
        options |= NSCaseInsensitiveSearch;
    }
    PDFDocument *document = [PDFSubview document];
    PDFSelection *selection = [document findString:string fromSelection:[PDFSubview currentSelection] withOptions:options];
    if (selection == nil && wrapFlag) {
        selection = [document findString:string fromSelection:nil withOptions:options];
    }
    if (selection != nil) {
        [PDFSubview setCurrentSelection:selection];
        [PDFSubview scrollSelectionToVisible:nil];
        return YES;
    }
    return NO;
}

- (void)takeFindStringFromSelection:(id)sender
{
    [NSPasteboard _web_setFindPasteboardString:[[PDFSubview currentSelection] string] withOwner:self];
}

- (void)jumpToSelection:(id)sender
{
    [PDFSubview scrollSelectionToVisible:nil];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item 
{
    SEL action = [item action];    
    if (action == @selector(takeFindStringFromSelection:) || action == @selector(jumpToSelection:)) {
        return [PDFSubview currentSelection] != nil;
    }
    return YES;
}

- (BOOL)canPrintHeadersAndFooters
{
    return NO;
}

- (NSPrintOperation *)printOperationWithPrintInfo:(NSPrintInfo *)printInfo
{
    return [[PDFSubview document] getPrintOperationForPrintInfo:printInfo autoRotate:YES];
}

// Delegates implementing the following method will be called to handle clicks on URL
// links within the PDFView.  
- (void)PDFViewWillClickOnLink:(PDFView *)sender withURL:(NSURL *)URL
{
    if (URL != nil) {    
        WebFrame *frame = [[self _web_parentWebFrameView] webFrame];
        [frame _safeLoadURL:URL];
    }
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)becomeFirstResponder
{
    // This works together with setNextKeyView to splice our PDFSubview into
    // the key loop similar to the way NSScrollView does this.
    NSWindow *window = [self window];
    id newFirstResponder = nil;
    
    if ([window keyViewSelectionDirection] == NSSelectingPrevious) {
        NSView *previousValidKeyView = [self previousValidKeyView];
        if ((previousValidKeyView != self) && (previousValidKeyView != PDFSubview))
            newFirstResponder = previousValidKeyView;
    } else {
        NSView *PDFDocumentView = [PDFSubview documentView];
        if ([PDFDocumentView acceptsFirstResponder]) {
            newFirstResponder = PDFDocumentView;
        }
    }
    
    if (!newFirstResponder)
        return NO;
    
    if (![window makeFirstResponder:newFirstResponder])
        return NO;
    
    return YES;
}

- (void)setNextKeyView:(NSView *)aView
{
    // This works together with becomeFirstResponder to splice PDFSubview into
    // the key loop similar to the way NSScrollView and NSClipView do this.
    NSView *documentView = [PDFSubview documentView];
    if (documentView) {
        [documentView setNextKeyView:aView];
        
        // We need to make the documentView be the next view in the keyview loop.
        // It would seem more sensible to do this in our init method, but it turns out
        // that [NSClipView setDocumentView] won't call this method if our next key view
        // is already set, so we wait until we're called before adding this connection.
        // We'll also clear it when we're called with nil, so this could go through the
        // same code path more than once successfully.
        [super setNextKeyView: aView ? documentView : nil];
    } else
        [super setNextKeyView:aView];
}

@end

#endif // OMIT_TIGER_FEATURES
