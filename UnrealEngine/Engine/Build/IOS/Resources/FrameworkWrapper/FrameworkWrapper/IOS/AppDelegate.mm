// Copyright Epic Games, Inc. All Rights Reserved.
//
//  AppDelegate.m
//  FrameworkWrapper
//
//  Created by Ryan West on 6/13/19.
//

#import "AppDelegate.h"
#import "UnrealView.h"

#if !CAN_USE_UE || !__cplusplus
    // Stub class to inherit from in a non-UE context
    @implementation IOSAppDelegate
        - (void)applicationWillEnterForeground:(UIApplication *)application {}
        - (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions { return YES; }
        - (void)applicationDidEnterBackground:(UIApplication *)application {}
    @end
#endif // !CAN_USE_UE

@implementation AppDelegate

@dynamic window;


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    [super application:application didFinishLaunchingWithOptions:launchOptions];

    return YES;
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    [super applicationDidEnterBackground:application];
    [UnrealContainerView AllowUnrealToSleep];
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    [super applicationWillEnterForeground:application];
    [UnrealContainerView WakeUpUnreal];
}

@end
