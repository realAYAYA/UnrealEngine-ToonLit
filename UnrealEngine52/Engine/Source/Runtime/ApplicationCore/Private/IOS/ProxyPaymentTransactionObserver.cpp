// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOS/ProxyPaymentTransactionObserver.h"

@implementation FProxyPaymentTransactionObserver

+ (FProxyPaymentTransactionObserver*) sharedInstance
{
    static dispatch_once_t onceToken;
    static FProxyPaymentTransactionObserver* transactionObserverSharedInstance;

    dispatch_once(&onceToken, ^{
        transactionObserverSharedInstance = [[FProxyPaymentTransactionObserver alloc] init];
    });
    return transactionObserverSharedInstance;
}

- (id) init
{
	self = [super init];
	return self;
}

-(void) dealloc
{
    if(_ossObserver)
    {
        [_ossObserver release];
    }
    [super dealloc];
}

-(void)paymentQueue:(SKPaymentQueue*)queue updatedTransactions : (NSArray*)transactions
{
    if(self.ossObserver)
    {
        [self.ossObserver paymentQueue:queue updatedTransactions:transactions];
    }
}

/// Logs all transactions that have been removed from the payment queue.
-(void)paymentQueue:(SKPaymentQueue*)queue removedTransactions : (NSArray*)transactions
{
    if(self.ossObserver)
    {
        [self.ossObserver paymentQueue:queue removedTransactions:transactions];
    }
}

/// Called when an error occur while restoring purchases. Notifies the user about the error.
-(void)paymentQueue:(SKPaymentQueue*)queue restoreCompletedTransactionsFailedWithError : (NSError*)error
{
    if(self.ossObserver)
    {
        [self.ossObserver paymentQueue:queue restoreCompletedTransactionsFailedWithError:error];
    }
}

/// Called when all restorable transactions have been processed by the payment queue.
-(void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue*)queue
{
    if(self.ossObserver)
    {
        [self.ossObserver paymentQueueRestoreCompletedTransactionsFinished:queue];
    }
}

@end
