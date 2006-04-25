/*
    WebPluginViewFactoryPrivate.h
    Copyright 2004, Apple, Inc. All rights reserved.
 
*/

#import <WebKit/WebPluginViewFactory.h>

typedef enum {
    WebPlugInModeEmbed = 0,
    WebPlugInModeFull  = 1
} WebPlugInMode;

/*!
    @constant WebPlugInModeKey REQUIRED. Number with one of the values from the WebPlugInMode enum.
*/
extern NSString *WebPlugInModeKey;

/*!
    @constant WebPlugInShouldLoadMainResourceKey REQUIRED. NSNumber (BOOL) indicating whether the plug-in should load its
    own main resource (the "src" URL, in most cases). If YES, the plug-in should load its own main resource. If NO, the
    plug-in should use the data provided by WebKit. See -webPlugInMainResourceReceivedData: in WebPluginPrivate.h.
    For compatibility with older versions of WebKit, the plug-in should assume that the value for
    WebPlugInShouldLoadMainResourceKey is NO if it is absent from the arguments dictionary.
*/
extern NSString *WebPlugInShouldLoadMainResourceKey;
