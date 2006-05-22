/*
    WebFrameView.h
    Copyright (C) 2005 Apple Computer, Inc. All rights reserved.
    Private header file.
*/

#import <WebKit/WebFrameView.h>

@interface WebFrameView (WebPrivate)

/*!
    @method canPrintHeadersAndFooters
    @abstract Tells whether this frame can print headers and footers
    @result YES if the frame can, no otherwise
*/
- (BOOL)canPrintHeadersAndFooters;

/*!
    @method printOperationWithPrintInfo
    @abstract Creates a print operation set up to print this frame
    @result A newly created print operation object
*/
- (NSPrintOperation *)printOperationWithPrintInfo:(NSPrintInfo *)printInfo;

/*!
    @method documentViewShouldHandlePrint
    @abstract Called by the host application before it initializes and runs a print operation.
    @result If NO is returned, the host application will abort its print operation and call -printDocumentView on the
    WebFrameView.  The document view is then expected to run its own print operation.  If YES is returned, the host 
    application's print operation will continue as normal.
*/
- (BOOL)documentViewShouldHandlePrint;

/*!
    @method printDocumentView
    @abstract Called by the host application when the WebFrameView returns YES from -documentViewShouldHandlePrint.
*/
- (void)printDocumentView;

@end
