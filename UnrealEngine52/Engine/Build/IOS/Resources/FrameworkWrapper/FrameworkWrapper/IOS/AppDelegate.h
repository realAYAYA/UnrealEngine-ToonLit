// Copyright Epic Games, Inc. All Rights Reserved.
//
//  AppDelegate.h
//  FrameworkWrapper
//
//  Created by Ryan West on 6/13/19.
//

#import <UIKit/UIKit.h>

#include "UnrealView.h"

#if CAN_USE_UE && __cplusplus
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Weverything"

    #include "PreIOSEmbeddedView.h"
    #include "IOS/IOSAppDelegate.h"

    #pragma clang diagnostic pop
#else
    // Stub class to inherit from in a non-UE context
    @interface IOSAppDelegate : NSObject
        - (void)applicationWillEnterForeground:(UIApplication *)application;
        - (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions;
        - (void)applicationDidEnterBackground:(UIApplication *)application;
    @end
#endif // CAN_USE_UE


@interface AppDelegate : IOSAppDelegate <UIApplicationDelegate>

    @property (strong, nonatomic) UIWindow *window;

@end

