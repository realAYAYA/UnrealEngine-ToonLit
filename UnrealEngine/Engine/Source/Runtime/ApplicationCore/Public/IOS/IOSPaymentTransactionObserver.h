// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#import "StoreKit/StoreKit.h"

#include "Containers/Queue.h"

/** The type of event received via the SKPaymentTransactionObserver protocol */
enum class EPaymentTransactionObserverEventType
{
	Unknown,
	UpdatedTransaction,
	RemovedTransaction,
	RestoreCompletedTransactionsFailed,
	RestoreCompletedTransactionsFinished
};

/** An event received via the SKPaymentTransactionObserver protocol */
struct FPaymentTransactionObserverEvent
{
	FPaymentTransactionObserverEvent() = default;
	FPaymentTransactionObserverEvent(EPaymentTransactionObserverEventType InType) : Type(InType) {}
	FPaymentTransactionObserverEvent(EPaymentTransactionObserverEventType InType, SKPaymentTransaction* InTransaction);
	FPaymentTransactionObserverEvent(EPaymentTransactionObserverEventType InType, long InErrorCode) : Type(InType), ErrorCode(InErrorCode) {}
	FPaymentTransactionObserverEvent(const FPaymentTransactionObserverEvent&);
	~FPaymentTransactionObserverEvent();

	/** The event type */
	EPaymentTransactionObserverEventType Type = EPaymentTransactionObserverEventType::Unknown;
	/** The transaction object for applicable events */
	SKPaymentTransaction* Transaction = nullptr;
	/** The error code for error events */
	long ErrorCode = 0;
};

/** Delegate protocol to allow bindees to be notified of new events */
@protocol FPaymentTransactionObserverEventReceivedDelegate
- (void) onPaymentTransactionObserverEventReceived;
@end

/**
 * The SKPaymentTransactionObserver implementation that receives transaction events raised by StoreKit. This class
 * enables us to bind an observer immediately in application:didFinishLaunchingWithOptions and start collecting events.
 * The consuming OSS starts up later, binds itself to eventReceivedDelegate to receive notifications from that
 * point on, and performs an initial pump of any events received between launch and then.
 */
/** */
@interface FPaymentTransactionObserver : NSObject <SKPaymentTransactionObserver>
{
	TQueue<FPaymentTransactionObserverEvent, EQueueMode::Spsc> _EventQueue;
};

@property (assign) id <FPaymentTransactionObserverEventReceivedDelegate> eventReceivedDelegate;

+ (FPaymentTransactionObserver*) sharedInstance;

- (TQueue<FPaymentTransactionObserverEvent, EQueueMode::Spsc>&) getEventQueue;

@end