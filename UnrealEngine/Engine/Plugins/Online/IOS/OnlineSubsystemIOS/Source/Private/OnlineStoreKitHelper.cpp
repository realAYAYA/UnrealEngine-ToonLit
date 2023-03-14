// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineStoreKitHelper.h"
#include "Interfaces/OnlinePurchaseInterface.h"
#include "OnlineSubsystemIOS.h"
#include "IOS/IOSAppDelegate.h"
#include "Misc/CommandLine.h"

/**
 * Convert an Apple SKPaymentTransaction receipt into a string
 * 
 * @param transaction valid transaction to convert to hex encoded string
 *
 * @return hex encoded string with opaque data representing a completed transaction
 */
FString convertReceiptToString(const SKPaymentTransaction* transaction)
{
	FString ReceiptData;
	
	NSURL* nsReceiptUrl = [[NSBundle mainBundle] appStoreReceiptURL];
	NSData* nsReceiptData = [NSData dataWithContentsOfURL : nsReceiptUrl];
	if (nsReceiptData)
	{
		NSString* nsEncodedReceiptData = [nsReceiptData base64EncodedStringWithOptions : NSDataBase64EncodingEndLineWithLineFeed];
		ReceiptData = nsEncodedReceiptData;
	}
	else
	{
        UE_LOG_ONLINE_STOREV2(Log, TEXT("No receipt data found for transaction"));
	}
	
    UE_LOG_ONLINE_STOREV2(VeryVerbose, TEXT("FStoreKitHelper::convertReceiptToString %s"), *ReceiptData);
	return ReceiptData;
}

/**
 * Retrieve the original transaction id from an Apple transaction object
 * Successful attempts to repurchase already owned items (NOT restore purchase), will end in "purchased" state with an original transaction id
 *
 * @param Transaction the transaction to retrieve an original transaction id
 *
 * @return original transaction id for transactions in the "restored" state, otherwise the current transaction id
 */
FString GetOriginalTransactionId(const SKPaymentTransaction* Transaction)
{
	SKPaymentTransaction* OriginalTransaction = nullptr;
	if (Transaction.originalTransaction && Transaction.transactionState == SKPaymentTransactionStateRestored)
	{		
		int32 RecurseCount = 0;	
		if (Transaction != Transaction.originalTransaction)
		{
            UE_LOG_ONLINE_STOREV2(Log, TEXT("GetOriginalTransactionId TransactionId=%s"), *FString(Transaction.transactionIdentifier));

			OriginalTransaction = Transaction.originalTransaction;
			while (OriginalTransaction.originalTransaction && (RecurseCount < 100))
			{
				++RecurseCount;
				OriginalTransaction = OriginalTransaction.originalTransaction;
                UE_LOG_ONLINE_STOREV2(Log, TEXT("GetOriginalTransactionId RecurseCount=%d, OriginalTransactionId=%s"), RecurseCount, *FString(OriginalTransaction.transactionIdentifier));
			}
		}
	}
	
	return OriginalTransaction ? OriginalTransaction.transactionIdentifier : Transaction.transactionIdentifier;
}

////////////////////////////////////////////////////////////////////
/// FSKProductsRequestHelper implementation
/// Add a delegate to a product information request 

@implementation FSKProductsRequestHelper
@end

////////////////////////////////////////////////////////////////////
/// FStoreKitTransactionData implementation
FStoreKitTransactionData::FStoreKitTransactionData(const SKPaymentTransaction* Transaction)
	: ReceiptData(convertReceiptToString(Transaction))
	, ErrorRaw([Transaction.error localizedDescription])
	, ErrorCode([Transaction.error code])
	, ErrorDomain([Transaction.error domain])
	, TransactionIdentifier(Transaction.transactionIdentifier)
{
	if (!ErrorRaw.IsEmpty())
	{
		ErrorStr = FString::Printf(TEXT("%s [%s:%d]"), *ErrorRaw, *ErrorDomain, ErrorCode);
	}
	
	SKPayment* Payment = Transaction.payment;
	if (Payment)
	{
		OfferId = Payment.productIdentifier;
	}
	
	OriginalTransactionIdentifier = GetOriginalTransactionId(Transaction);
}

////////////////////////////////////////////////////////////////////
/// FStoreKitHelperV2 implementation

@implementation FStoreKitHelperV2
@synthesize Request;
@synthesize AvailableProducts;

- (id)init
{
	[FPaymentTransactionObserver sharedInstance].eventReceivedDelegate = self;
    self.PendingTransactions = [NSMutableSet setWithCapacity:5];
	return self;
}

-(void)dealloc
{
	[FPaymentTransactionObserver sharedInstance].eventReceivedDelegate = nil;
    [_PendingTransactions release];
    
	[Request release];
	[AvailableProducts release];
	[super dealloc];
}

-(void)onPaymentTransactionObserverEventReceived
{
    [FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
	{
		[self pumpObserverEventQueue];

		return true;
	}];
}

-(void)pumpObserverEventQueue
{
	FPaymentTransactionObserver* Observer = [FPaymentTransactionObserver sharedInstance];

	FPaymentTransactionObserverEvent ObserverEvent;
	while ([Observer getEventQueue].Dequeue(ObserverEvent))
	{
		switch (ObserverEvent.Type)
		{
		case EPaymentTransactionObserverEventType::UpdatedTransaction:
			[self updatedTransaction : ObserverEvent.Transaction];
			break;
		case EPaymentTransactionObserverEventType::RemovedTransaction:
			[self removedTransaction : ObserverEvent.Transaction];
			break;
		case EPaymentTransactionObserverEventType::RestoreCompletedTransactionsFailed:
			[self restoreCompletedTransactionsFailedWithError : ObserverEvent.ErrorCode];
			break;
		case EPaymentTransactionObserverEventType::RestoreCompletedTransactionsFinished:
			[self restoreCompletedTransactionsFinished];
			break;
		default:
			break;
		}
	}
}

-(void)updatedTransaction : (SKPaymentTransaction*)transaction
{
	UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelper::updatedTransaction"));

	// Parse the generic transaction update into appropriate execution paths
	switch ([transaction transactionState])
	{
	case SKPaymentTransactionStatePurchased:
		if (FParse::Param(FCommandLine::Get(), TEXT("disableiosredeem")))
		{
			UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelper::completeTransaction (disabled)"));
		}
		else
		{
			[self completeTransaction : transaction] ;
		}
		break;
	case SKPaymentTransactionStateFailed:
		[self failedTransaction : transaction] ;
		break;
	case SKPaymentTransactionStateRestored:
		[self restoreTransaction : transaction] ;
		break;
	case SKPaymentTransactionStatePurchasing:
		[self purchaseInProgress : transaction] ;
		break;
	case SKPaymentTransactionStateDeferred:
		[self purchaseDeferred : transaction] ;
		break;
	default:
		UE_LOG_ONLINE_STOREV2(Warning, TEXT("FStoreKitHelper unhandled state: %d"), [transaction transactionState]);
		break;
	}}

-(void)removedTransaction : (SKPaymentTransaction*)transaction
{
    UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelper::removedTransaction"));
}

-(void)requestProductData: (NSMutableSet*)productIDs
{
    UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelper::requestProductData"));

	Request = [[SKProductsRequest alloc] initWithProductIdentifiers:productIDs];
	Request.delegate = self;

	[Request start];
}

-(void)makePurchase: (NSMutableSet*)productIDs
{
    UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelper::makePurchase"));

	Request = [[SKProductsRequest alloc] initWithProductIdentifiers:productIDs];
	Request.delegate = self;
	
	[Request start];
}

-(void)requestDidFinish:(SKRequest*)request
{
	if ([Request isKindOfClass : [SKReceiptRefreshRequest class]])
	{
		[[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
	}
}

-(void)restorePurchases
{
	Request = [[SKReceiptRefreshRequest alloc] init];
	Request.delegate = self;
	[Request start];
}

-(void)restoreCompletedTransactionsFinished
{
	UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::restoreCompletedTransactionsFinished"));
	
	self.OnRestoreTransactionsComplete.Broadcast(EPurchaseTransactionState::Restored);
}

-(void)restoreCompletedTransactionsFailedWithError : (int)errorCode
{
	UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::failedRestore"));
	
	EPurchaseTransactionState CompletionState = EPurchaseTransactionState::Failed;
	switch (errorCode)
	{
		case SKErrorPaymentCancelled:
			CompletionState = EPurchaseTransactionState::Canceled;
			break;
		case SKErrorClientInvalid:
		case SKErrorStoreProductNotAvailable:
		case SKErrorPaymentInvalid:
			CompletionState = EPurchaseTransactionState::Invalid;
			break;
		case SKErrorPaymentNotAllowed:
			CompletionState = EPurchaseTransactionState::NotAllowed;
			break;
	}
	
	self.OnRestoreTransactionsComplete.Broadcast(CompletionState);
}

-(void)makePurchase:(NSArray*)products WithUserId: (const FString&) userId SimulateAskToBuy: (bool) bAskToBuy;
{
	UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::makePurchase by SKProduct with UserId"));
	
	for (SKProduct* Product in products)
	{
		SKMutablePayment* Payment = [SKMutablePayment paymentWithProduct:Product];
		Payment.quantity = 1;
		Payment.simulatesAskToBuyInSandbox = bAskToBuy;

		if (!userId.IsEmpty())
		{
			// hash of username to detect irregular activity
			Payment.applicationUsername = [NSString stringWithFString: userId];
		}
		[[SKPaymentQueue defaultQueue] addPayment:Payment];
	}
}

-(void)makePurchase:(NSArray*)products SimulateAskToBuy: (bool) bAskToBuy
{
	UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::makePurchase by SKProduct"));
	FString EmptyString;
	[self makePurchase: products WithUserId: EmptyString SimulateAskToBuy: bAskToBuy];
}

-(void)requestProductData: (NSMutableSet*)productIDs WithDelegate : (const FOnQueryOnlineStoreOffersComplete&)delegate
{
	UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::requestProductData"));
	
	FSKProductsRequestHelper* TempRequest = [[FSKProductsRequestHelper alloc] initWithProductIdentifiers:productIDs];
	TempRequest.OfferDelegate = delegate;
	TempRequest.delegate = self;
	self.Request = TempRequest;
	
	[self.Request start];
}

-(void)productsRequest: (SKProductsRequest *)request didReceiveResponse : (SKProductsResponse *)response
{
	if (request == self.Request)
	{
		UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::didReceiveResponse"));
		
		if ([request isKindOfClass : [FSKProductsRequestHelper class]])
		{
			FSKProductsRequestHelper* Helper = (FSKProductsRequestHelper*)request;

			[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
			{
				// Notify listeners of the request completion
				self.OnProductRequestResponse.Broadcast(response, Helper.OfferDelegate);
				return true;
			}];
		}
		else
		{
			UE_LOG_ONLINE_STOREV2(Warning, TEXT("Wrong class associated with product request"));
		}
		
		[request autorelease];
	}
}

-(void)completeTransaction: (SKPaymentTransaction *)transaction
{
	UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::completeTransaction"));
	
	EPurchaseTransactionState Result = EPurchaseTransactionState::Failed;
	
	SKPayment* Payment = transaction.payment;
	if (Payment)
	{
		Result = EPurchaseTransactionState::Purchased;
	}
	
	// Notify listeners of the request completion
	self.OnTransactionCompleteResponse.Broadcast(Result, FStoreKitTransactionData(transaction));

	// Transaction must be finalized before removed from the queue
	[self.PendingTransactions addObject:transaction];
}

-(void)failedTransaction: (SKPaymentTransaction *)transaction
{
	EPurchaseTransactionState CompletionState = EPurchaseTransactionState::Failed;
	switch (transaction.error.code)
	{
		case SKErrorPaymentCancelled:
			CompletionState = EPurchaseTransactionState::Canceled;
			break;
		case SKErrorClientInvalid:
		case SKErrorStoreProductNotAvailable:
		case SKErrorPaymentInvalid:
			CompletionState = EPurchaseTransactionState::Invalid;
			break;
		case SKErrorPaymentNotAllowed:
			CompletionState = EPurchaseTransactionState::NotAllowed;
			break;
	}

	UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::failedTransaction State=%s"), LexToString(CompletionState));

	// Notify listeners of the request completion
	self.OnTransactionCompleteResponse.Broadcast(CompletionState, FStoreKitTransactionData(transaction));
	
	// Remove the transaction from the payment queue.
	[[SKPaymentQueue defaultQueue] finishTransaction:transaction];
}

-(void)restoreTransaction: (SKPaymentTransaction *)transaction
{
	UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::restoreTransaction"));

	self.OnTransactionRestored.Broadcast(FStoreKitTransactionData(transaction));
	
	// @todo Transaction must be finalized before removed from the queue?
	//[self.PendingTransactions addObject:transaction];
	// Remove the transaction from the payment queue.
	[[SKPaymentQueue defaultQueue] finishTransaction:transaction];
}

-(void)purchaseInProgress: (SKPaymentTransaction *)transaction
{
	UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::purchaseInProgress"));
	
	// Notify listeners a purchase is in progress
	self.OnTransactionPurchaseInProgress.Broadcast(FStoreKitTransactionData(transaction));
}

-(void)purchaseDeferred: (SKPaymentTransaction *)transaction
{
	UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::purchaseDeferred"));
	
	// Notify listeners a purchase has been deferred
	self.OnTransactionDeferred.Broadcast(FStoreKitTransactionData(transaction));
}

-(void)finalizeTransaction: (const FString&) receiptId
{
	UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::finalizeTransaction - %s"), *receiptId);
	for (SKPaymentTransaction* pendingTransaction in self.PendingTransactions)
	{
		if (pendingTransaction)
		{
			const FString transId = pendingTransaction.transactionIdentifier;
			const FString originalTransId = GetOriginalTransactionId(pendingTransaction);
			UE_LOG_ONLINE_STOREV2(Verbose, TEXT("FStoreKitHelperV2::checking - id: %s origId: %s"), *transId,  *originalTransId);

			if ((!originalTransId.IsEmpty()) && (originalTransId == receiptId))
			{
				UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitHelperV2::finalizeTransaction - %s"), *receiptId);

				// Remove the transaction from the payment queue.
				[[SKPaymentQueue defaultQueue] finishTransaction:pendingTransaction];
				[self.PendingTransactions removeObject:pendingTransaction];
				break;
			}
		}
		else
		{
			UE_LOG_ONLINE_STOREV2(Verbose, TEXT("null transaction"));
		}
	}
}

-(void)dumpAppReceipt
{
	FString receiptData = convertReceiptToString(nullptr);
	UE_LOG_ONLINE_STOREV2(Verbose, TEXT("FStoreKitHelper::dumpAppReceipt"));
	UE_LOG_ONLINE_STOREV2(Verbose, TEXT("%s"), *receiptData);
}

-(FOnProductsRequestResponse&)OnProductRequestResponse
{
	return _OnProductRequestResponse;
}

-(FDelegateHandle)AddOnProductRequestResponse: (const FOnProductsRequestResponseDelegate&) Delegate
{
	_OnProductRequestResponse.Add(Delegate);
	return Delegate.GetHandle();
}

-(FOnTransactionCompleteIOS&)OnTransactionCompleteResponse
{
	return _OnTransactionCompleteResponse;
}

-(FDelegateHandle)AddOnTransactionComplete: (const FOnTransactionCompleteIOSDelegate&) Delegate
{
	_OnTransactionCompleteResponse.Add(Delegate);
	return Delegate.GetHandle();
}

-(FOnTransactionRestoredIOS&)OnTransactionRestored
{
	return _OnTransactionRestored;
}

-(FDelegateHandle)AddOnTransactionRestored: (const FOnTransactionRestoredIOSDelegate&) Delegate
{
	_OnTransactionRestored.Add(Delegate);
	return Delegate.GetHandle();
}

-(FOnRestoreTransactionsCompleteIOS&)OnRestoreTransactionsComplete
{
	return _OnRestoreTransactionsComplete;
}

-(FDelegateHandle)AddOnRestoreTransactionsComplete: (const FOnRestoreTransactionsCompleteIOSDelegate&) Delegate
{
	_OnRestoreTransactionsComplete.Add(Delegate);
	return Delegate.GetHandle();
}

-(FOnTransactionProgress&)OnTransactionPurchaseInProgress
{
	return _OnTransactionPurchaseInProgress;
}

-(FDelegateHandle)AddOnPurchaseInProgress: (const FOnTransactionProgressDelegate&) Delegate
{
	_OnTransactionPurchaseInProgress.Add(Delegate);
	return Delegate.GetHandle();
}

-(FOnTransactionProgress&)OnTransactionDeferred
{
	return _OnTransactionDeferred;
}

-(FDelegateHandle)AddOnTransactionDeferred: (const FOnTransactionProgressDelegate&) Delegate
{
	_OnTransactionDeferred.Add(Delegate);
	return Delegate.GetHandle();
}

@end

