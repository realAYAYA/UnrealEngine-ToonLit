// Copyright Epic Games, Inc. All Rights Reserved.
//
//  UnrealWrapper.h
//  FrameworkWrapper
//
//  Created by Andrew Grant on 9/4/18.
//

#if defined(__aarch64__)
    #define CAN_USE_UE 1
#else
    #define CAN_USE_UE 0
#endif

#define NW_LOG NSLog

#if SHIPPING
    #define NW_LOG_DEBUG(...)
#else
    #define NW_LOG_DEBUG NW_LOG
#endif


#import <UIKit/UIKit.h>

typedef void(^EmbeddedParamsCompletionType)(NSDictionary<NSString*, NSString*>* _Nullable Results , NSString* _Nullable Error);

NS_ASSUME_NONNULL_BEGIN

IB_DESIGNABLE
@interface UnrealContainerView : UIView

+(void)DelayedCreateView;


+(void)EmbeddedCallForSubsystem:(NSString* _Nullable)Subsystem WithCommand:(NSString* _Nullable)Command Params:(NSDictionary<NSString*, NSString*>* _Nullable)Params Priority:(int)Priority CompletionHandler:(EmbeddedParamsCompletionType)InHandler;
// Allows native code to more easily generate the embedded call required to invoke UObject bridge methods by name. NOTE: responses not hooked up yet.
+(void)JSBridgeCallByName:(NSString*)UObjectName MethodName:(NSString*)MethodName MethodParamsJSON:(NSString*)MethodParamsJSON Priority:(int)Priority CompletionHandler:(EmbeddedParamsCompletionType)CompletionHandler;
+(void)SetEmbeddedObject:(void* _Nullable)Object ForName:(NSString* _Nonnull)Name;
+(void)HandleTouchMessage:(NSDictionary*)Message;
+(void)WakeUpUnreal;
+(void)AllowUnrealToSleep;


+(void)ResizeUnrealView:(CGSize)Size;
+(void)PositionUnrealView:(CGPoint)Location;
+(void)ResetUnrealViewToFullScreen;
+(void)EnsureVisible;
-(void)TakeOwnershipOfUnrealView;


+(void)GoFullResolution;
+(void)RestoreResolution;

@property (weak, nonatomic, readonly) UIView* unrealView;
@property (nonatomic, assign) IBInspectable NSString* CommandLine;

@end

NS_ASSUME_NONNULL_END
