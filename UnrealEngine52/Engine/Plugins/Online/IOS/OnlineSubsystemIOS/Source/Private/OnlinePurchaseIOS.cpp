// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlinePurchaseIOS.h"
#include "OnlineError.h"
#include "OnlineSubsystemIOS.h"
#include "Misc/CommandLine.h"
#include "Stats/Stats.h"
#include "IOS/ProxyPaymentTransactionObserver.h"

#import <StoreKit/SKError.h>
#import <StoreKit/SKReceiptRefreshRequest.h>
#import <StoreKit/SKPaymentTransaction.h>
#import <StoreKit/SKPayment.h>
#import <StoreKit/SKPaymentQueue.h>

#define LOCTEXT_NAMESPACE "OnlineSubsystemIOS"

namespace OnlinePurchaseIOSPrivate
{
    TAutoConsoleVariable<int32> CVarSimluateAskToBuy(TEXT("OSS.AskToBuy"),
                                                     0,
                                                     TEXT("Simulate ask to buy in iOS\n")
                                                     TEXT("1 enable, 0 disable"),
                                                     ECVF_Default);

    /**
     * Convert an Apple SKPaymentTransaction receipt into a string
     *
     * @return hex encoded string with opaque data representing a completed transaction
     */
    FString ConvertReceiptToString()
    {
        FString ReceiptData;
        
        NSURL* nsReceiptUrl = [[NSBundle mainBundle] appStoreReceiptURL];
        NSData* nsReceiptData = [NSData dataWithContentsOfURL : nsReceiptUrl];
        if (nsReceiptData)
        {
            NSString* nsEncodedReceiptData = [nsReceiptData base64EncodedStringWithOptions : NSDataBase64EncodingEndLineWithLineFeed];
            ReceiptData = nsEncodedReceiptData;
            UE_LOG_ONLINE_STOREV2(VeryVerbose, TEXT("ConvertReceiptToString %s"), *ReceiptData);
        }
        else
        {
            UE_LOG_ONLINE_STOREV2(Log, TEXT("ConvertReceiptToString: No receipt data found for transaction"));
        }
        return ReceiptData;
    }
}

/**
 * Intermediate type to communicate SKPaymentTransaction information from Objective-C to
 * to C++ implementation
 */
struct FOnlinePurchaseTransactionIOS
{
    explicit FOnlinePurchaseTransactionIOS(SKPaymentTransaction* Transaction);
    
    /** @return a string that prints useful debug information about this transaction */
    FString ToDebugString() const
    {
        return FString::Printf(TEXT("OfferId: %s TransactionId: %s ReceiptData: %s Error:%s"),
                        *OfferId,
                        *TransactionIdentifier,
                        *ReceiptData,
                        *ErrorStr);
    }
    
    /** @return offer id for this transaction */
    const FString& GetOfferId() const { return OfferId; }
    /** @return receipt data for this transaction */
    const FString& GetReceiptData() const { return ReceiptData; }
    /** @return error string for this transaction, if applicable */
    const FString& GetErrorStr() const { return ErrorStr; }
    /** @return quantity of items requested of the same type */
    int GetQuantity() const { return Quantity; }
    /** @return transaction identifier for this purchase */
    const FString& GetTransactionIdentifier() const { return TransactionIdentifier; }
    /** @return the SKPaymentTransaction object so we can finish the transaction */
    SKPaymentTransaction* GetPaymentTransaction() const { return static_cast<SKPaymentTransaction*>(PaymentTransaction); }
private:
    
    /** iTunesConnect offer id */
    FString OfferId;
    /** Opaque store receipt data */
    FString ReceiptData;
    /** Error on the transaction, if applicable */
    FString ErrorStr;
    /** Unique transaction identifier */
    FString TransactionIdentifier;
    /** Quantity of items requested*/
    int Quantity;
    /** Platform transaction object*/
    TRetainedObjCInstance<SKPaymentTransaction*> PaymentTransaction;
};

FOnlinePurchaseTransactionIOS::FOnlinePurchaseTransactionIOS(SKPaymentTransaction* Transaction)
    : ReceiptData(OnlinePurchaseIOSPrivate::ConvertReceiptToString())
    , TransactionIdentifier(Transaction.transactionIdentifier)
    , Quantity(0)
    , PaymentTransaction(Transaction)
{
    if(Transaction.error)
    {
        FString ErrorRaw([Transaction.error localizedDescription]);
        if (!ErrorRaw.IsEmpty())
        {
            int32 ErrorCode([Transaction.error code]);
            FString ErrorDomain([Transaction.error domain]);
            ErrorStr = FString::Printf(TEXT("%s [%s:%d]"), *ErrorRaw, *ErrorDomain, ErrorCode);
        }
    }
    
    OfferId = Transaction.payment.productIdentifier;
    Quantity = Transaction.payment.quantity;
}

/**
 * Info used to cache and track order in progress.
 */
struct FOnlinePurchaseInProgressTransactionIOS
{
    FOnlinePurchaseInProgressTransactionIOS(const FPurchaseCheckoutRequest& InCheckoutRequest, const FOnPurchaseCheckoutComplete InCheckoutCompleteDelegate)
        : CheckoutRequest(InCheckoutRequest)
        , CheckoutCompleteDelegate(InCheckoutCompleteDelegate)
    {
    }
    
    bool HasRequestForProduct(const FString& ProductId) const
    {
        const TArray<FPurchaseCheckoutRequest::FPurchaseOfferEntry>& Offers = CheckoutRequest.PurchaseOffers;
        return (Offers.Num() > 0 ? Offers[0].OfferId == ProductId : false);
    }
    
    /** Checkout info for the order */
    FPurchaseCheckoutRequest CheckoutRequest;
    /** Delegate to call on completion */
    FOnPurchaseCheckoutComplete CheckoutCompleteDelegate;
};

/**
 * Proxy class that notifies updates from SKPaymentQueue to FOnlinePurchaseIOS on the game 
 * thread and invokes SKPaymentQueue methods on main thread
 */
@interface FStoreKitPurchaseProxy : NSObject<SKPaymentTransactionObserver>
{
    FOnlinePurchaseIOS* _PurchaseReceiver;
};
@end

@implementation FStoreKitPurchaseProxy

////////////////////////////////////////////////////////////////////
/// SKPaymentTransactionObserver implementation
-(void)paymentQueue: (SKPaymentQueue*)Queue updatedTransactions: (NSArray*)Transactions
{
    UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::updatedTransactions"));
    for(SKPaymentTransaction* Transaction in Transactions)
    {
        [self UpdatedTransaction : Transaction];
    }
}

-(void)paymentQueue: (SKPaymentQueue*)Queue removedTransactions: (NSArray*)Transactions
{
    UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::removedTransaction"));
}

-(void)paymentQueue: (SKPaymentQueue*)Queue restoreCompletedTransactionsFailedWithError: (NSError*)Error
{
    UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::restoreCompletedTransactionsFailedWithError"));

    EPurchaseTransactionState CompletionState = [FStoreKitPurchaseProxy TranslateError:Error];
    
    // Notifications from SKPaymentQueue are received on main thread. Use a task to notify from game thread
    [FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
    {
        if(_PurchaseReceiver)
        {
            _PurchaseReceiver->OnRestoreTransactionsComplete(CompletionState);
        }
        return true;
    }];
}

-(void)paymentQueueRestoreCompletedTransactionsFinished: (SKPaymentQueue*)Queue
{
    UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::paymentQueueRestoreCompletedTransactionsFinished"));

    // Notifications from SKPaymentQueue are received on main thread. Use a task to notify from game thread
    [FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
    {
        if(_PurchaseReceiver)
        {
            _PurchaseReceiver->OnRestoreTransactionsComplete(EPurchaseTransactionState::Restored);
        }
        return true;
    }];
}

////////////////////////////////////////////////////////////////////
/// FStoreKitPurchaseProxy methods

- (id)initWithReceiver: (FOnlinePurchaseIOS*)PurchaseReceiver
{
    _PurchaseReceiver = PurchaseReceiver;
    [FProxyPaymentTransactionObserver sharedInstance].ossObserver = self;

    // Notifications from SKPaymentQueue are received on main thread. Use dispatch_async on main queue to serialize operations
    dispatch_async(dispatch_get_main_queue(), ^
    {
        SKPaymentQueue* queue = [SKPaymentQueue defaultQueue];
        [self paymentQueue: queue updatedTransactions: [queue transactions]];
    });
    return self;
}

-(void)Shutdown
{
    _PurchaseReceiver = nullptr;
    [FProxyPaymentTransactionObserver sharedInstance].ossObserver = nil;
}

- (bool)CanMakePayments
{
    return [SKPaymentQueue canMakePayments] != FALSE;
}

+(EPurchaseTransactionState)TranslateError: (NSError*)Error
{
    EPurchaseTransactionState TranslatedError = EPurchaseTransactionState::Failed;
    switch (Error.code)
    {
        case SKErrorPaymentCancelled:
            TranslatedError = EPurchaseTransactionState::Canceled;
            break;
        case SKErrorClientInvalid:
        case SKErrorStoreProductNotAvailable:
        case SKErrorPaymentInvalid:
            TranslatedError = EPurchaseTransactionState::Invalid;
            break;
        case SKErrorPaymentNotAllowed:
            TranslatedError = EPurchaseTransactionState::NotAllowed;
            break;
    }
    return TranslatedError;
}

-(void)UpdatedTransaction : (SKPaymentTransaction*)Transaction
{
    TOptional<EPurchaseTransactionState> TranslatedState;
    SKPaymentTransaction* ActualTransaction = Transaction;
    SKPaymentTransaction* FinishTransaction = nil;
    
    switch ([Transaction transactionState])
    {
    case SKPaymentTransactionStatePurchased:
        UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::updatedTransaction SKPaymentTransactionStatePurchased"));

        if (FParse::Param(FCommandLine::Get(), TEXT("disableiosredeem")))
        {
            UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::completedTransaction (disabled)"));
        }
        else
        {
            TranslatedState = EPurchaseTransactionState::Purchased;
        }
        break;
    case SKPaymentTransactionStateFailed:
        UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::updatedTransaction SKPaymentTransactionStateFailed"));
        TranslatedState = [FStoreKitPurchaseProxy TranslateError:ActualTransaction.error];
        FinishTransaction = ActualTransaction;
        break;
    case SKPaymentTransactionStateRestored:
        UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::updatedTransaction SKPaymentTransactionStateRestored"));
        TranslatedState = EPurchaseTransactionState::Restored;
        FinishTransaction = ActualTransaction;
        ActualTransaction = ActualTransaction.originalTransaction;
        break;
    case SKPaymentTransactionStatePurchasing:
        UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::updatedTransaction SKPaymentTransactionStatePurchasing"));
        break;
    case SKPaymentTransactionStateDeferred:
        UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::updatedTransaction SKPaymentTransactionStateDeferred"));
        TranslatedState = EPurchaseTransactionState::Deferred;
        FinishTransaction = ActualTransaction;
        break;
    default:
        UE_LOG_ONLINE_STOREV2(Warning, TEXT("FStoreKitPurchaseProxy::updatedTransaction unhandled state: %d"), [Transaction transactionState]);
        break;
    }

    if(TranslatedState)
    {
        FOnlinePurchaseTransactionIOS TransactionData(ActualTransaction);
        // Notifications from SKPaymentQueue are received on main thread. Use a task to notify from game thread
        [FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
        {
            if(_PurchaseReceiver)
            {
                _PurchaseReceiver->OnTransactionComplete(*TranslatedState, TransactionData);
            }
            return true;
        }];
    }
    
    if(FinishTransaction)
    {
        UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::updatedTransaction transaction finished"));
        [[SKPaymentQueue defaultQueue] finishTransaction:FinishTransaction];
    }
}

-(void)RestorePurchases
{
    // Notifications from SKPaymentQueue are received on main thread. Use dispatch_async on main queue to serialize operations
    dispatch_async(dispatch_get_main_queue(), ^
    {
        [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
    });
}

-(void)MakePurchase: (SKProduct*)Product withQuantity: (int)Quantity simulateAskToBuy: (bool) bAskToBuy
{
    UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::makePurchase by SKProduct"));

    // Notifications from SKPaymentQueue are received on main thread. Use dispatch_async on main queue to serialize operations
    dispatch_async(dispatch_get_main_queue(), ^
    {
        SKMutablePayment* Payment = [SKMutablePayment paymentWithProduct:Product];
        Payment.quantity = Quantity;
        // simulatesAskToBuyInSandbox is only effective while Sandbox testing
        Payment.simulatesAskToBuyInSandbox = bAskToBuy;

        [[SKPaymentQueue defaultQueue] addPayment:Payment];
    });
}

-(void)FinalizeTransaction: (SKPaymentTransaction*)Transaction
{
    UE_LOG_ONLINE_STOREV2(Log, TEXT("FStoreKitPurchaseProxy::finalizeTransaction"));

    // Notifications from SKPaymentQueue are received on main thread. Use dispatch_async on main queue to serialize operations
    dispatch_async(dispatch_get_main_queue(), ^
    {
        [[SKPaymentQueue defaultQueue] finishTransaction:Transaction];
    });
}

@end

FOnlinePurchaseIOS::FOnlinePurchaseIOS(FOnlineSubsystemIOS* InSubsystem)
	: bRestoringTransactions(false)
	, Subsystem(InSubsystem)
{
	UE_LOG_ONLINE_PURCHASE(Log, TEXT( "FOnlinePurchaseIOS::FOnlinePurchaseIOS" ));
    StoreKitProxy = [[[FStoreKitPurchaseProxy alloc] initWithReceiver:this] autorelease];
}

////////////////////////////////////////////////////////////////////
/// FOnlinePurchaseIOS methods

FOnlinePurchaseIOS::~FOnlinePurchaseIOS()
{
    [StoreKitProxy Shutdown];
}

void FOnlinePurchaseIOS::DumpAppReceipt()
{
    FString receiptData = OnlinePurchaseIOSPrivate::ConvertReceiptToString();
    UE_LOG_ONLINE_STOREV2(Verbose, TEXT("FOnlinePurchaseIOS::DumpAppReceipt"));
    UE_LOG_ONLINE_STOREV2(Verbose, TEXT("%s"), *receiptData);
}

bool FOnlinePurchaseIOS::IsAllowedToPurchase(const FUniqueNetId& /*UserId*/)
{
	bool bCanMakePurchases = [StoreKitProxy CanMakePayments];
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("FOnlinePurchaseIOS::IsAllowedToPurchase %s"), *LexToString(bCanMakePurchases));
	return bCanMakePurchases;
}

void FOnlinePurchaseIOS::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate)
{
	bool bStarted = false;
	FText ErrorMessage;

    if (IsAllowedToPurchase(UserId))
	{
		if (InProgressTransaction == nullptr)
		{
			FOnlineStoreIOSPtr StoreInterface = StaticCastSharedPtr<FOnlineStoreIOS>(Subsystem->GetStoreV2Interface());
			if (StoreInterface.IsValid())
			{
                int32 NumOffers = CheckoutRequest.PurchaseOffers.Num();
                
                SKProduct* Product = nil;
                if(NumOffers > 0)
                {
                    const FPurchaseCheckoutRequest::FPurchaseOfferEntry& Offer = CheckoutRequest.PurchaseOffers[0];
                    if (NumOffers > 1)
                    {
                        UE_LOG_ONLINE_PURCHASE(Warning, TEXT("StoreKit does not support multiple different products in one transaction. Only %s will be requested"), *Offer.OfferId);
                    }
                    Product = StoreInterface->GetSKProductByOfferId(Offer.OfferId);
                
                    if (Product)
                    {
                        bool bAskToBuy = false;
#if !UE_BUILD_SHIPPING
                        bAskToBuy = OnlinePurchaseIOSPrivate::CVarSimluateAskToBuy.GetValueOnGameThread() == 1;
#endif
                        
                        [StoreKitProxy MakePurchase:Product withQuantity:Offer.Quantity simulateAskToBuy:bAskToBuy];
                        InProgressTransaction = MakeShared<const FOnlinePurchaseInProgressTransactionIOS>(CheckoutRequest, Delegate);
                        bStarted = true;
                    }
                }
                if(!bStarted)
                {
                    ErrorMessage = NSLOCTEXT("IOSPurchase", "ErrorNoOffersSpecified", "Failed to checkout, no valid offers given.");
                }
			}
            else
            {
                ErrorMessage = NSLOCTEXT("IOSPurchase", "ErrorPurchaseNotAllowed", "Failed to checkout, invalid FOnlineStoreIOS instance.");
            }		
        }
		else
		{
			ErrorMessage = NSLOCTEXT("IOSPurchase", "ErrorTransactionInProgress", "Failed to checkout, user has in progress transaction.");
		}
	}
	else
	{
		ErrorMessage = NSLOCTEXT("IOSPurchase", "ErrorPurchaseNotAllowed", "Failed to checkout, user not allowed to purchase.");
	}

	if (!bStarted)
	{
		TSharedRef<FPurchaseReceipt> FailReceipt = GenerateFailReceipt(CheckoutRequest);
		
        // Notify failure on next tick
		Subsystem->ExecuteNextTick([ErrorMessage, FailReceipt = FailReceipt, Delegate]()
		{
			FOnlineError Error(ErrorMessage);
			Delegate.ExecuteIfBound(Error, FailReceipt);
		});
	}
}

void FOnlinePurchaseIOS::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseReceiptlessCheckoutComplete& Delegate)
{
	UE_LOG_ONLINE(Error, TEXT("FOnlinePurchaseIOS::Checkout (receiptless) is not currently supported"));
	Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::NotImplemented));
}

void FOnlinePurchaseIOS::FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId)
{
	UE_LOG_ONLINE_PURCHASE(Log, TEXT("FOnlinePurchaseIOS::FinalizePurchase %s %s"), *UserId.ToString(), *ReceiptId);

    int IndexToRemove = Transactions.IndexOfByPredicate([&ReceiptId](const FKnownTransaction& Entry) { return Entry.Receipt->TransactionId == ReceiptId; });
    if(IndexToRemove != INDEX_NONE)
    {
        const FKnownTransaction& Transaction = Transactions[IndexToRemove];
        
        // Restored transactions are already marked finalized when received but kept as known transactions so user code can process them
        if(Transaction.Receipt->TransactionState != EPurchaseTransactionState::Restored)
        {
            [StoreKitProxy FinalizeTransaction:Transaction.PaymentTransaction];
        }
        Transactions.RemoveAt(IndexToRemove);
    }
}

void FOnlinePurchaseIOS::RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate)
{
	FOnlineError Result;
	Delegate.ExecuteIfBound(Result, MakeShared<FPurchaseReceipt>());
}

void FOnlinePurchaseIOS::QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate)
{
	bool bSuccess = true;
	bool bTriggerDelegate = true;
	if (bRestoreReceipts)
	{
		if (!bRestoringTransactions)
		{
            QueryReceiptsComplete = Delegate;
            bTriggerDelegate = false;
            bRestoringTransactions = true;
            [StoreKitProxy RestorePurchases];
		}
		else
		{
			UE_LOG_ONLINE_PURCHASE(Log, TEXT("FOnlinePurchaseIOS::QueryReceipts already restoring transactions"));
			bSuccess = false;
		}
	}
	
	if (bTriggerDelegate)
	{
		// Query receipts comes dynamically from the StoreKit observer
		Subsystem->ExecuteNextTick([Delegate, bSuccess]() {
			FOnlineError Result(bSuccess);
			Delegate.ExecuteIfBound(Result);
		});
	}
}

void FOnlinePurchaseIOS::GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const
{
	OutReceipts.Empty();

	for (const FKnownTransaction& Transaction: Transactions)
	{
		OutReceipts.Add(*Transaction.Receipt);
	}
}

void FOnlinePurchaseIOS::OnTransactionComplete(EPurchaseTransactionState Result, const FOnlinePurchaseTransactionIOS& TransactionData)
{
	UE_LOG_ONLINE_PURCHASE(Log, TEXT("FOnlinePurchaseIOS::OnTransactionCompleteResponse Result=%s TransactionData=[%s]"), LexToString(Result), *TransactionData.ToDebugString());
	
    bool bIsInProgressTransaction = InProgressTransaction != nullptr && InProgressTransaction->HasRequestForProduct(TransactionData.GetOfferId());
    bool bIsResoredTransaction = (Result == EPurchaseTransactionState::Restored);
    
	if (bIsInProgressTransaction && !bIsResoredTransaction)
	{
        FOnlineError FinalResult;
        const FString& ErrorStr = TransactionData.GetErrorStr();

        switch (Result)
        {
            case EPurchaseTransactionState::Failed:
                FinalResult.SetFromErrorCode(TEXT("com.epicgames.purchase.failure"));
                FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("IOSTransactionFailed", "TransactionFailed");
                break;
            case EPurchaseTransactionState::Canceled:
                FinalResult.SetFromErrorCode(TEXT("com.epicgames.catalog_helper.user_cancelled"));
                FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("IOSTransactionCancel", "TransactionCanceled");
                break;
            case EPurchaseTransactionState::Purchased:
                FinalResult.bSucceeded = true;
                break;
            case EPurchaseTransactionState::Deferred:
                FinalResult.SetFromErrorCode(TEXT("com.epicgames.purchase.deferred"));
                FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("IOSTransactionDeferred", "Transaction awaiting approval.");
                break;
            default:
                UE_LOG_ONLINE_PURCHASE(Warning, TEXT("Unexpected state after purchase %d"), (int)Result);
                FinalResult.SetFromErrorCode(TEXT("com.epicgames.purchase.unexpected_state"));
                FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("UnexpectedState", "Unexpected purchase result");
                break;
        }
        
        TSharedRef<FPurchaseReceipt> Receipt = GenerateReceipt(Result, InProgressTransaction->CheckoutRequest, TransactionData);
        
        if(FinalResult.bSucceeded)
        {
            Receipt = TryStoreTransactionAndGetReceipt(Receipt, TransactionData);
        }
        InProgressTransaction->CheckoutCompleteDelegate.ExecuteIfBound(FinalResult, Receipt);
        InProgressTransaction.Reset();
	}
	else
	{
		// Transactions other than the in progress one are stored for later redemption. Those can be received at login or when restoring transactions
        // We have no interest on other failure states because they won't store a redeemable receipt
		if (Result == EPurchaseTransactionState::Restored || Result == EPurchaseTransactionState::Purchased)
		{
            TSharedRef<FPurchaseReceipt> Receipt = GenerateOfflineReceipt(Result, TransactionData);
            TryStoreTransactionAndGetReceipt(Receipt, TransactionData);
        }
        
        if(Result == EPurchaseTransactionState::Purchased)
        {
		    UE_LOG_ONLINE_PURCHASE(Log, TEXT("Deferred transaction finished or unfinished transaction was received"));
            TWeakPtr<FOnlinePurchaseIOS> WeakThis = AsShared();
            Subsystem->ExecuteNextTick([WeakThis]()
			{
				FOnlinePurchaseIOSPtr StrongThis = WeakThis.Pin();
				if (StrongThis.IsValid())
				{
					// No user id for app store
					StrongThis->TriggerOnUnexpectedPurchaseReceiptDelegates(*FUniqueNetIdIOS::EmptyId());
				}
			});
		}
        else if(Result == EPurchaseTransactionState::Restored)
        {
            UE_LOG_ONLINE_PURCHASE(Log, TEXT("Restored transaction"));
        }
	}
}

TSharedRef<FPurchaseReceipt> FOnlinePurchaseIOS::TryStoreTransactionAndGetReceipt(const TSharedRef<FPurchaseReceipt>& Receipt, const FOnlinePurchaseTransactionIOS& Transaction)
{
    FKnownTransaction* CachedTransaction = Transactions.FindByPredicate([&Transaction](const FKnownTransaction& KnownTransaction) {
        return KnownTransaction.Receipt->TransactionId == Transaction.GetTransactionIdentifier();
    });
    if(CachedTransaction)
    {
        // In case the transaction already exists as Purchased don't make it Restored
        if(CachedTransaction->Receipt->TransactionState == EPurchaseTransactionState::Purchased &&
           Receipt->TransactionState == EPurchaseTransactionState::Restored)
        {
            return CachedTransaction->Receipt;
        }
        else
        {
            CachedTransaction->Receipt = Receipt;
            return Receipt;
        }
    }
    else
    {
        Transactions.Emplace(Receipt, Transaction.GetPaymentTransaction());
        return Receipt;
    }
}

void FOnlinePurchaseIOS::OnRestoreTransactionsComplete(EPurchaseTransactionState Result)
{
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("FOnlinePurchaseIOS::OnRestoreTransactionsComplete %d"), (int32)Result);
	
	// Full restore is complete
	bRestoringTransactions = false;
	bool bSuccess = (Result == EPurchaseTransactionState::Restored);
	Subsystem->ExecuteNextTick([this, bSuccess]() {
		FOnlineError FinalResult(bSuccess);
		QueryReceiptsComplete.ExecuteIfBound(FinalResult);
		QueryReceiptsComplete.Unbind();
	});
}

void FOnlinePurchaseIOS::FinalizeReceiptValidationInfo(const FUniqueNetId& UserId, FString& InReceiptValidationInfo, const FOnFinalizeReceiptValidationInfoComplete& Delegate)
{
	FOnlineError DefaultSuccess(true);
	Delegate.ExecuteIfBound(DefaultSuccess, InReceiptValidationInfo);
}

TSharedRef<FPurchaseReceipt> FOnlinePurchaseIOS::GenerateFailReceipt(const FPurchaseCheckoutRequest& CheckoutRequest)
{
    TSharedRef<FPurchaseReceipt> Receipt = MakeShared<FPurchaseReceipt>();
    
    Receipt->TransactionState = EPurchaseTransactionState::Failed;
    
    // Add the requested offers to the receipt in the event of an incomplete purchase.
    for(const auto& RequestedOffer : CheckoutRequest.PurchaseOffers)
    {
        Receipt->AddReceiptOffer(RequestedOffer.OfferNamespace, RequestedOffer.OfferId, RequestedOffer.Quantity);
    }
    return Receipt;
}

TSharedRef<FPurchaseReceipt> FOnlinePurchaseIOS::GenerateOfflineReceipt(EPurchaseTransactionState Result, const FOnlinePurchaseTransactionIOS& Transaction)
{
    TSharedRef<FPurchaseReceipt> Receipt = MakeShared<FPurchaseReceipt>();

	Receipt->TransactionState = Result;
	Receipt->TransactionId = Transaction.GetTransactionIdentifier();
	
	if (Result == EPurchaseTransactionState::Purchased ||
	    Result == EPurchaseTransactionState::Restored)
	{
		FPurchaseReceipt::FReceiptOfferEntry ReceiptEntry(TEXT(""), Transaction.GetOfferId(), Transaction.GetQuantity());
		
		FPurchaseReceipt::FLineItemInfo& LineItem = ReceiptEntry.LineItems.Emplace_GetRef();
		
		LineItem.ItemName = Transaction.GetOfferId();
		LineItem.UniqueId = Transaction.GetTransactionIdentifier();
		LineItem.ValidationInfo = Transaction.GetReceiptData();

		Receipt->AddReceiptOffer(ReceiptEntry);
	}
	
	return Receipt;
}

TSharedRef<FPurchaseReceipt> FOnlinePurchaseIOS::GenerateReceipt(EPurchaseTransactionState Result, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnlinePurchaseTransactionIOS& Transaction)
{
    TSharedRef<FPurchaseReceipt> Receipt = GenerateOfflineReceipt(Result, Transaction);

	if(Result != EPurchaseTransactionState::Purchased &&
       Result != EPurchaseTransactionState::Restored)
	{
        // Add the requested offers to the receipt in the event of an incomplete purchase.
        for(const auto& RequestedOffer : CheckoutRequest.PurchaseOffers)
        {
            Receipt->AddReceiptOffer(RequestedOffer.OfferNamespace, RequestedOffer.OfferId, RequestedOffer.Quantity);
        }
	}
	
	return Receipt;
}

#undef LOCTEXT_NAMESPACE

