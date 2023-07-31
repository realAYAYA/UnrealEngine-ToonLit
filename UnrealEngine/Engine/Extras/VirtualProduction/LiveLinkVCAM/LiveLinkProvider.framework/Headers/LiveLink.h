// Copyright Epic Games, Inc. All Rights Reserved.

#import <Foundation/Foundation.h>

#import "LiveLinkProvider.h"

__attribute__((visibility("default")))
@protocol LiveLinkLogDelegate

- (void) LogMessage:(NSString *)message;

@end

 
__attribute__((visibility("default")))
@interface LiveLink : NSObject

+ (void) initialize:(id<LiveLinkLogDelegate>)logDelegate;
+ (void) restart;
+ (void) shutdown;
+ (id<LiveLinkProvider> ) createProvider:(NSString *)name;

@end
