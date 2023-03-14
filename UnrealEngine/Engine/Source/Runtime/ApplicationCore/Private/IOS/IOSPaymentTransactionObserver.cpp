// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSPaymentTransactionObserver.h"
#include "CoreGlobals.h"

@implementation FPaymentTransactionObserver

+ (FPaymentTransactionObserver*) sharedInstance
{
	static dispatch_once_t onceToken;
	static FPaymentTransactionObserver* transactionObserverSharedInstance;

	dispatch_once(&onceToken, ^{
		transactionObserverSharedInstance = [[FPaymentTransactionObserver alloc] init];
	});
	return transactionObserverSharedInstance;
}

- (id) init
{
	self = [super init];
	return self;
}

- (TQueue<FPaymentTransactionObserverEvent>&) getEventQueue
{
	return _EventQueue;
}

-(void)paymentQueue:(SKPaymentQueue*)queue updatedTransactions : (NSArray*)transactions
{
	UE_LOG(LogIOS, Log, TEXT("FPaymentTransactionObserver::updatedTransaction"));

	for (SKPaymentTransaction* transaction in transactions)
	{
		_EventQueue.Enqueue({ EPaymentTransactionObserverEventType::UpdatedTransaction, transaction });
	}

	if (self.eventReceivedDelegate)
	{
		[self.eventReceivedDelegate onPaymentTransactionObserverEventReceived];
	}
}

/// Logs all transactions that have been removed from the payment queue.
-(void)paymentQueue:(SKPaymentQueue*)queue removedTransactions : (NSArray*)transactions
{
	UE_LOG(LogIOS, Log, TEXT("FPaymentTransactionObserver::removedTransaction"));

	for (SKPaymentTransaction* transaction in transactions)
	{
		_EventQueue.Enqueue({ EPaymentTransactionObserverEventType::RemovedTransaction, transaction });
	}

	if (self.eventReceivedDelegate)
	{
		[self.eventReceivedDelegate onPaymentTransactionObserverEventReceived];
	}
}

/// Called when an error occur while restoring purchases. Notifies the user about the error.
-(void)paymentQueue:(SKPaymentQueue*)queue restoreCompletedTransactionsFailedWithError : (NSError*)error
{
	UE_LOG(LogIOS, Log, TEXT("FPaymentTransactionObserver::failedRestore - %s"), *FString([error localizedDescription]));

	_EventQueue.Enqueue({ EPaymentTransactionObserverEventType::RestoreCompletedTransactionsFailed, error.code });

	if (self.eventReceivedDelegate)
	{
		[self.eventReceivedDelegate onPaymentTransactionObserverEventReceived];
	}
}

/// Called when all restorable transactions have been processed by the payment queue.
-(void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue*)queue
{
	UE_LOG(LogIOS, Log, TEXT("FPaymentTransactionObserver::finishedRestore"));

	_EventQueue.Enqueue({ EPaymentTransactionObserverEventType::RestoreCompletedTransactionsFinished });

	if (self.eventReceivedDelegate)
	{
		[self.eventReceivedDelegate onPaymentTransactionObserverEventReceived];
	}
}

@end

FPaymentTransactionObserverEvent::FPaymentTransactionObserverEvent(EPaymentTransactionObserverEventType InType, SKPaymentTransaction* InTransaction)
	: Type(InType)
	, Transaction(InTransaction)
{
	if (Transaction)
	{
		[Transaction retain];
	}
}

FPaymentTransactionObserverEvent::FPaymentTransactionObserverEvent(const FPaymentTransactionObserverEvent& Other)
	: Type(Other.Type)
	, Transaction(Other.Transaction)
	, ErrorCode(Other.ErrorCode)
{
	if (Transaction)
	{
		[Transaction retain];
	}
}

FPaymentTransactionObserverEvent::~FPaymentTransactionObserverEvent()
{
	if (Transaction)
	{
		[Transaction release];
	}
}