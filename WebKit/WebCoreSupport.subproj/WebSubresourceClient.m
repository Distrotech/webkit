/*	
    WebSubresourceClient.m
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebSubresourceClient.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFormDataStream.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebViewPrivate.h>

#import <Foundation/NSError_NSURLExtras.h>
#import <Foundation/NSURLRequestPrivate.h>
#import <Foundation/NSURLResponse.h>

#import <WebCore/WebCoreResourceLoader.h>

// We could include NSURLRequestPrivate.h if we knew it was a version new enough
// to have this (AppleInternal from 10.4.3 or newer), but for now we'd like to
// be able to compile even if we only have older headers, so declare this here.
@interface NSMutableURLRequest (WebKitSystemInterfaceSecrets) 
- (void)setHTTPShouldHandleMixedReplace:(BOOL)yorn;
@end

@implementation WebSubresourceClient

- initWithLoader:(id <WebCoreResourceLoader>)l dataSource:(WebDataSource *)s
{
    [super init];
    
    loader = [l retain];

    [self setDataSource: s];
    
    return self;
}

- (void)dealloc
{
    [loader release];
    [super dealloc];
}

+ (WebSubresourceClient *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
				   withRequest:(NSMutableURLRequest *)newRequest
                                 customHeaders:(NSDictionary *)customHeaders
				      referrer:(NSString *)referrer 
				 forDataSource:(WebDataSource *)source
{
    WebSubresourceClient *client = [[[self alloc] initWithLoader:rLoader dataSource:source] autorelease];
    
    if ([newRequest respondsToSelector:@selector(setHTTPShouldHandleMixedReplace:)]) {
        [newRequest setHTTPShouldHandleMixedReplace:YES];
        [client setSupportsMultipartContent:YES]; 
    } else
        [client setSupportsMultipartContent:NO];
        
    [source _addSubresourceClient:client];

    NSEnumerator *e = [customHeaders keyEnumerator];
    NSString *key;
    while ((key = (NSString *)[e nextObject]) != nil) {
	[newRequest addValue:[customHeaders objectForKey:key] forHTTPHeaderField:key];
    }

    // Use the original request's cache policy for two reasons:
    // 1. For POST requests, we mutate the cache policy for the main resource,
    //    but we do not want this to apply to subresources
    // 2. Delegates that modify the cache policy using willSendRequest: should
    //    not affect any other resources. Such changes need to be done
    //    per request.
    [newRequest setCachePolicy:[[source _originalRequest] cachePolicy]];
    [newRequest setHTTPReferrer:referrer];
    
    WebView *_webView = [source _webView];
    [newRequest setMainDocumentURL:[[[[_webView mainFrame] dataSource] request] URL]];
    [newRequest setHTTPUserAgent:[_webView userAgentForURL:[newRequest URL]]];
            
    if (![client loadWithRequest:newRequest]) {
        client = nil;
    }
    
    return client;
}

+ (WebSubresourceClient *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
				       withURL:(NSURL *)URL
				 customHeaders:(NSDictionary *)customHeaders
				      referrer:(NSString *)referrer
				 forDataSource:(WebDataSource *)source
{
    NSMutableURLRequest *newRequest = [[NSMutableURLRequest alloc] initWithURL:URL];
    WebSubresourceClient *client = [self startLoadingResource:rLoader withRequest:newRequest customHeaders:customHeaders referrer:referrer forDataSource:source];
    [newRequest release];

    return client;
}

+ (WebSubresourceClient *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
				       withURL:(NSURL *)URL
				 customHeaders:(NSDictionary *)customHeaders
				      postData:(NSArray *)postData
				      referrer:(NSString *)referrer
				 forDataSource:(WebDataSource *)source
{
    NSMutableURLRequest *newRequest = [[NSMutableURLRequest alloc] initWithURL:URL];

    [newRequest setHTTPMethod:@"POST"];
    webSetHTTPBody(newRequest, postData);

    WebSubresourceClient *client = [self startLoadingResource:rLoader withRequest:newRequest customHeaders:customHeaders referrer:referrer forDataSource:source];
    [newRequest release];

    return client;

}

- (void)receivedError:(NSError *)error
{
    [[dataSource _webView] _receivedError:error fromDataSource:dataSource];
}

- (NSURLRequest *)willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse;
{
    NSURL *oldURL = [request URL];
    NSURLRequest *clientRequest = [super willSendRequest:newRequest redirectResponse:redirectResponse];
    
    if (clientRequest != nil && oldURL != [clientRequest URL] && ![oldURL isEqual:[clientRequest URL]]) {
	[loader redirectedToURL:[clientRequest URL]];
    }

    return clientRequest;
}

- (void)didReceiveResponse:(NSURLResponse *)r
{
    ASSERT(r);

    if ([[r MIMEType] isEqualToString:@"multipart/x-mixed-replace"]) {
        if (!supportsMultipartContent) {
            [dataSource _removeSubresourceClient:self];
            [[[dataSource _webView] mainFrame] _checkLoadComplete];
            [self cancelWithError:[NSError _webKitErrorWithDomain:NSURLErrorDomain
                                                             code:NSURLErrorUnsupportedURL
                                                              URL:[r URL]]];
            return;
        }   
        loadingMultipartContent = YES;
    }

    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    [loader receivedResponse:r];
    // The coreLoader can cancel a load if it receives a multipart response for a non-image
    if (reachedTerminalState) {
        [self release];
        return;
    }
    [super didReceiveResponse:r];
    [self release];
    
    if (loadingMultipartContent && [[self resourceData] length]) {
        // A subresource loader does not load multipart sections progressively, deliver the previously received data to the coreLoader all at once
        [loader addData:[self resourceData]];
        // Tells the dataSource to save the just completed section, necessary for saving/dragging multipart images
        [self saveResource];
        // Clears the data to make way for the next multipart section
        [self clearResourceData];
        
        // After the first multipart section is complete, signal to delegates that this load is "finished" 
        if (!signalledFinish)
            [self signalFinish];
    }
}

- (void)didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    // A subresource loader does not load multipart sections progressively, don't deliver any data to the coreLoader yet
    if (!loadingMultipartContent)
        [loader addData:data];
    [super didReceiveData:data lengthReceived:lengthReceived];
    [self release];
}

- (void)signalFinish
{
    [dataSource _removeSubresourceClient:self];
    [[dataSource _webView] _finishedLoadingResourceFromDataSource:dataSource];
    [super signalFinish];
}

- (void)didFinishLoading
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader finishWithData:[self resourceData]];
    
    if (!signalledFinish)
        [self signalFinish];
        
    [super didFinishLoading];

    [self release];    
}

- (void)didFailWithError:(NSError *)error
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
    
    [loader reportError];
    [dataSource _removeSubresourceClient:self];
    [self receivedError:error];
    [super didFailWithError:error];

    [self release];
}

- (void)cancel
{
    // Calling _removeSubresourceClient will likely result in a call to release, so we must retain.
    [self retain];
        
    [loader cancel];
    [dataSource _removeSubresourceClient:self];
    [self receivedError:[self cancelledError]];
    [super cancel];

    [self release];
}

@end
