// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#import "StoreKit/StoreKit.h"

/**
 * The proxy SKPaymentTransactionObserver implementation that receives transaction events raised by StoreKit. This
 * class enables us to bind an observer on initialization in application:didFinishLaunchingWithOptions so 
 * SKPaymentsQueue is properly synchronized with the App Store
 * The consuming OSS starts up later and binds itself as the nested observer to receive notifications
 */
/** */
@interface FProxyPaymentTransactionObserver : NSObject <SKPaymentTransactionObserver>
{
};

@property (atomic, retain) id <SKPaymentTransactionObserver> ossObserver;

+ (FProxyPaymentTransactionObserver*) sharedInstance;

@end
