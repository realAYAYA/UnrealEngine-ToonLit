// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlinePurchaseIOS.h"
#include "OnlineError.h"
#include "OnlineSubsystemIOS.h"
#include "Stats/Stats.h"
#import "OnlineStoreKitHelper.h"

/** Take successful transactions and route them through deferred pipeline */
#define TEST_DEFERRED_TRANSACTIONS 0
#define TEST_DEFERRED_DELAY 25.0f
#if TEST_DEFERRED_TRANSACTIONS
#include "Containers/Ticker.h"
#endif // TEST_DEFERRED_TRANSACTIONS

namespace OSSConsoleVariables
{
	// CVars
	TAutoConsoleVariable<int32> CVarSimluateAskToBuy(
															 TEXT("OSS.AskToBuy"),
															 0,
															 TEXT("Simulate ask to buy in iOS\n")
															 TEXT("1 enable, 0 disable"),
															 ECVF_Default);
}

#define LOCTEXT_NAMESPACE "OnlineSubsystemIOS"
#define IOSUSER TEXT("IOSUser")

FOnlinePurchaseIOS::FOnlinePurchaseIOS(FOnlineSubsystemIOS* InSubsystem)
	: StoreHelper(nullptr)
	, bRestoringTransactions(false)
	, Subsystem(InSubsystem)
{
	UE_LOG_ONLINE_PURCHASE(Log, TEXT( "FOnlinePurchaseIOS::FOnlinePurchaseIOS" ));
}

FOnlinePurchaseIOS::FOnlinePurchaseIOS()
{
	UE_LOG_ONLINE_PURCHASE(Log, TEXT( "FOnlinePurchaseIOS::FOnlinePurchaseIOS" ));
}

FOnlinePurchaseIOS::~FOnlinePurchaseIOS()
{
}

void FOnlinePurchaseIOS::InitStoreKit(FStoreKitHelperV2* InStoreKit)
{
	StoreHelper = InStoreKit;

	FOnProductsRequestResponseDelegate OnProductsRequestResponseDelegate = FOnProductsRequestResponseDelegate::CreateThreadSafeSP(this, &FOnlinePurchaseIOS::OnProductPurchaseRequestResponse);
	[StoreHelper AddOnProductRequestResponse: OnProductsRequestResponseDelegate];
	
	FOnTransactionCompleteIOSDelegate OnTransactionCompleteResponseDelegate = FOnTransactionCompleteIOSDelegate::CreateThreadSafeSP(this, &FOnlinePurchaseIOS::OnTransactionCompleteResponse);
	[StoreHelper AddOnTransactionComplete: OnTransactionCompleteResponseDelegate];
	
	FOnTransactionRestoredIOSDelegate OnTransactionRestoredDelegate = FOnTransactionRestoredIOSDelegate::CreateThreadSafeSP(this, &FOnlinePurchaseIOS::OnTransactionRestored);
	[StoreHelper AddOnTransactionRestored: OnTransactionRestoredDelegate];
	
	FOnRestoreTransactionsCompleteIOSDelegate OnRestoreTransactionsCompleteDelegate = FOnRestoreTransactionsCompleteIOSDelegate::CreateThreadSafeSP(this, &FOnlinePurchaseIOS::OnRestoreTransactionsComplete);
	[StoreHelper AddOnRestoreTransactionsComplete: OnRestoreTransactionsCompleteDelegate];
	
	FOnTransactionProgressDelegate OnTransactionPurchasingDelegate = FOnTransactionProgressDelegate::CreateThreadSafeSP(this, &FOnlinePurchaseIOS::OnTransactionInProgress);
	[StoreHelper AddOnPurchaseInProgress: OnTransactionPurchasingDelegate];
	
	FOnTransactionProgressDelegate OnTransactionDeferredDelegate = FOnTransactionProgressDelegate::CreateThreadSafeSP(this, &FOnlinePurchaseIOS::OnTransactionDeferred);
	[StoreHelper AddOnTransactionDeferred: OnTransactionDeferredDelegate];
}

bool FOnlinePurchaseIOS::IsAllowedToPurchase(const FUniqueNetId& UserId)
{
	bool bCanMakePurchases = [SKPaymentQueue canMakePayments];
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("FOnlinePurchaseIOS::IsAllowedToPurchase %s"), *LexToString(bCanMakePurchases));
	return bCanMakePurchases;
}

void FOnlinePurchaseIOS::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate)
{
	bool bStarted = false;
	FText ErrorMessage;

	TSharedRef<FOnlinePurchasePendingTransactionIOS> RequestedTransaction = MakeShareable(new FOnlinePurchasePendingTransactionIOS(CheckoutRequest, UserId, EPurchaseTransactionState::NotStarted, Delegate));

	if (IsAllowedToPurchase(UserId))
	{
		const FString UserIdStr = IOSUSER;
		const TSharedRef<FOnlinePurchasePendingTransactionIOS>* UserPendingTransaction = PendingTransactions.Find(UserIdStr);
		if (UserPendingTransaction == nullptr)
		{
			FOnlineStoreIOSPtr StoreInterface = StaticCastSharedPtr<FOnlineStoreIOS>(Subsystem->GetStoreV2Interface());
			if (StoreInterface.IsValid())
			{
				TSharedRef<FOnlinePurchasePendingTransactionIOS> PendingTransaction = PendingTransactions.Add(UserIdStr, RequestedTransaction);
				PendingTransaction->StartProcessing();
				
				// autoreleased NSMutableArray to hold products
				int32 NumOffers = CheckoutRequest.PurchaseOffers.Num();

				NSMutableArray<SKProduct*>* ProductSet = [NSMutableArray arrayWithCapacity: NumOffers];
				for (int32 OfferIdx = 0; OfferIdx < NumOffers; OfferIdx++)
				{
					const FPurchaseCheckoutRequest::FPurchaseOfferEntry& Offer = CheckoutRequest.PurchaseOffers[OfferIdx];

					SKProduct* Product = StoreInterface->GetSKProductByOfferId(Offer.OfferId);
					if (Product)
					{
						[ProductSet addObject:Product];
					}
				}
				
				bool bAskToBuy = false;
#if !UE_BUILD_SHIPPING
				bAskToBuy = OSSConsoleVariables::CVarSimluateAskToBuy.GetValueOnGameThread() == 1;
#endif
				
				if ([ProductSet count] > 0)
				{
					dispatch_async(dispatch_get_main_queue(), ^
					{
					    // Purchase the product through the StoreKit framework
						[StoreHelper makePurchase:ProductSet SimulateAskToBuy:bAskToBuy];
					});
					bStarted = true;
				}
				else
				{
					ErrorMessage = NSLOCTEXT("IOSPurchase", "ErrorNoOffersSpecified", "Failed to checkout, no offers given.");
					RequestedTransaction->PendingPurchaseInfo.TransactionState = EPurchaseTransactionState::Failed;
				}
			}
		}
		else
		{
			ErrorMessage = NSLOCTEXT("IOSPurchase", "ErrorTransactionInProgress", "Failed to checkout, user has in progress transaction.");
			RequestedTransaction->PendingPurchaseInfo.TransactionState = EPurchaseTransactionState::Failed;
		}
	}
	else
	{
		ErrorMessage = NSLOCTEXT("IOSPurchase", "ErrorPurchaseNotAllowed", "Failed to checkout, user not allowed to purchase.");
		RequestedTransaction->PendingPurchaseInfo.TransactionState = EPurchaseTransactionState::Failed;
	}

	if (!bStarted)
	{
		TSharedRef<FPurchaseReceipt> FailReceipt = RequestedTransaction->GenerateReceipt();
		
		Subsystem->ExecuteNextTick([ErrorMessage, FailReceipt, Delegate]()
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

	const FString ReceiptIdCopy(ReceiptId);
	dispatch_async(dispatch_get_main_queue(), ^
	{
		// Purchase the product through the StoreKit framework
		[StoreHelper finalizeTransaction:ReceiptIdCopy];
	});
}

void FOnlinePurchaseIOS::RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate)
{
	FOnlineError Result;
	Delegate.ExecuteIfBound(Result, MakeShareable(new FPurchaseReceipt()));
}

void FOnlinePurchaseIOS::QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate)
{
	bool bSuccess = true;
	bool bTriggerDelegate = true;
	if (bRestoreReceipts)
	{
		if (!bRestoringTransactions)
		{
			// Restore purchases, adding them to the offline receipts array for later redemption
			if (StoreHelper)
			{
				QueryReceiptsComplete = Delegate;
				bTriggerDelegate = false;
				bRestoringTransactions = true;
				dispatch_async(dispatch_get_main_queue(), ^
				{
					[StoreHelper restorePurchases];
				});
			}
			else
			{
				bSuccess = false;
			}
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

	// Add the cached list of user purchases
    const FString UserIdStr = IOSUSER;
	const TArray<TSharedRef<FPurchaseReceipt>>* UserCompletedTransactions = CompletedTransactions.Find(UserIdStr);
	if (UserCompletedTransactions != nullptr)
	{
		for (int32 Idx = 0; Idx < UserCompletedTransactions->Num(); Idx++)
		{
			OutReceipts.Add(*(*UserCompletedTransactions)[Idx]);
		}
	}
	
	// Add purchases completed while "offline"
	for (int32 Idx = 0; Idx < OfflineTransactions.Num(); Idx++)
	{
		OutReceipts.Add(*OfflineTransactions[Idx]);
	}
}

void FOnlinePurchaseIOS::OnProductPurchaseRequestResponse(SKProductsResponse* Response, const FOnQueryOnlineStoreOffersComplete& CompletionDelegate)
{
	UE_LOG_ONLINE_PURCHASE(Log, TEXT("FOnlinePurchaseIOS::OnProductPurchaseRequestResponse"));
}

void FOnlinePurchaseIOS::OnTransactionCompleteResponse(EPurchaseTransactionState Result, const FStoreKitTransactionData& TransactionData)
{
	UE_LOG_ONLINE_PURCHASE(Log, TEXT("FOnlinePurchaseIOS::OnTransactionCompleteResponse Result=%s TransactionData=[%s]"), LexToString(Result), *TransactionData.ToDebugString());
	
	FString UserIdStr = IOSUSER;
	const TSharedRef<FOnlinePurchasePendingTransactionIOS>* UserPendingTransactionPtr = PendingTransactions.Find(UserIdStr);
	if (UserPendingTransactionPtr != nullptr)
	{
#if TEST_DEFERRED_TRANSACTIONS
		if (Result == EPurchaseTransactionState::Restored || Result == EPurchaseTransactionState::Purchased)
		{
			// Route a successful purchase to the deferred code
			OnTransactionDeferred(TransactionData);
			return;
		}
#endif // TEST_DEFERRED_TRANSACTIONS

		const TSharedRef<FOnlinePurchasePendingTransactionIOS> UserPendingTransaction = *UserPendingTransactionPtr;
		
		UserPendingTransaction->AddCompletedOffer(Result, TransactionData);
		if (UserPendingTransaction->AreAllOffersComplete())
		{
			FOnlineError FinalResult;
			EPurchaseTransactionState FinalState = UserPendingTransaction->GetFinalTransactionState();
			
			UserPendingTransaction->PendingPurchaseInfo.TransactionState = FinalState;
			// UserPendingTransaction->PendingPurchaseInfo.TransactionId; purposefully blank
			
			const FString& ErrorStr = TransactionData.GetErrorStr();
			switch (FinalState)
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
				default:
					UE_LOG_ONLINE_PURCHASE(Warning, TEXT("Unexpected state after purchase %d"), (int)FinalState);
					FinalResult.SetFromErrorCode(TEXT("com.epicgames.purchase.unexpected_state"));
					FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("UnexpectedState", "Unexpected purchase result");
					UserPendingTransaction->PendingPurchaseInfo.TransactionState = EPurchaseTransactionState::Failed;
					break;
			}
			
			TSharedRef<FPurchaseReceipt> FinalReceipt = UserPendingTransaction->GenerateReceipt();
			
			TArray< TSharedRef<FPurchaseReceipt> >& UserCompletedTransactions = CompletedTransactions.FindOrAdd(UserIdStr);
			
			PendingTransactions.Remove(UserIdStr);
			UserCompletedTransactions.Add(FinalReceipt);
			
			UserPendingTransaction->CheckoutCompleteDelegate.ExecuteIfBound(FinalResult, FinalReceipt);
		}
	}
	else
	{
		// Transactions that come in during login or other non explicit purchase moments are added to a receipts list for later redemption
		UE_LOG_ONLINE_PURCHASE(Log, TEXT("Pending transaction completed offline"));
		if (Result == EPurchaseTransactionState::Restored || Result == EPurchaseTransactionState::Purchased)
		{
			TSharedRef<FPurchaseReceipt> OfflineReceipt = FOnlinePurchasePendingTransactionIOS::GenerateReceipt(Result, TransactionData);
			OfflineTransactions.Add(OfflineReceipt);

			// Queue this user to be updated about this next-tick on the game-thread, if they're not mid-purchase
			TWeakPtr<FOnlinePurchaseIOS, ESPMode::ThreadSafe> WeakThis(AsShared());
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
	}
}

void FOnlinePurchaseIOS::OnTransactionRestored(const FStoreKitTransactionData& TransactionData)
{
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("FOnlinePurchaseIOS::OnTransactionRestored TransactionData=[%s]"), *TransactionData.ToDebugString());

	// Single item restored amongst a group of items
	TSharedRef<FPurchaseReceipt> OfflineReceipt = FOnlinePurchasePendingTransactionIOS::GenerateReceipt(EPurchaseTransactionState::Restored, TransactionData);
	
#if 0
	bool bFound = false;
	for (TSharedRef<FPurchaseReceipt>& OtherReceipt : OfflineTransactions)
	{
		// If redundant, replace the entry
		if (OtherReceipt->TransactionId == OfflineReceipt->TransactionId)
		{
			*OtherReceipt = *OfflineReceipt;
			bFound = true;
			break;
		}
	}
	
	if (!bFound)
#endif
	{
		OfflineTransactions.Add(OfflineReceipt);
	}
}

void FOnlinePurchaseIOS::OnRestoreTransactionsComplete(EPurchaseTransactionState Result)
{
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("FOnlinePurchaseIOS::OnRestoreTransactionsComplete %d"), (int32)Result);
	
	// Full restore is complete
	bRestoringTransactions = false;
	bool bSuccess = (Result == EPurchaseTransactionState::Restored) || (Result == EPurchaseTransactionState::Purchased);
	Subsystem->ExecuteNextTick([this, bSuccess]() {
		FOnlineError FinalResult(bSuccess);
		QueryReceiptsComplete.ExecuteIfBound(FinalResult);
		QueryReceiptsComplete.Unbind();
	});
}

void FOnlinePurchaseIOS::OnTransactionInProgress(const FStoreKitTransactionData& TransactionData)
{
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("FOnlinePurchaseIOS::OnTransactionInProgress TransactionData=[%s]"), *TransactionData.ToDebugString());
}

void FOnlinePurchaseIOS::OnTransactionDeferred(const FStoreKitTransactionData& TransactionData)
{
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("FOnlinePurchaseIOS::OnTransactionDeferred TransactionData=[%s]"), *TransactionData.ToDebugString());
	
	FString UserIdStr = IOSUSER;
	const TSharedRef<FOnlinePurchasePendingTransactionIOS>* UserPendingTransactionPtr = PendingTransactions.Find(UserIdStr);
	if (UserPendingTransactionPtr != nullptr)
	{
		const TSharedRef<FOnlinePurchasePendingTransactionIOS> UserPendingTransaction = *UserPendingTransactionPtr;
		
		const FString& ErrorStr = TransactionData.GetErrorStr();
		
		FOnlineError FinalResult;
		FinalResult.SetFromErrorCode(TEXT("com.epicgames.purchase.deferred"));
		FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("IOSTransactionDeferred", "Transaction awaiting approval.");
		
		TSharedRef<FPurchaseReceipt> DeferredReceipt = FOnlinePurchasePendingTransactionIOS::GenerateReceipt(EPurchaseTransactionState::Deferred, TransactionData);
		
		// Clear out the deferred transaction, it should get picked up in "offline" receipts
		PendingTransactions.Remove(UserIdStr);
		UserPendingTransaction->CheckoutCompleteDelegate.ExecuteIfBound(FinalResult, DeferredReceipt);

#if TEST_DEFERRED_TRANSACTIONS
		// Existing transactions emulating deferment will trigger as success shortly
		TWeakPtr<FOnlinePurchaseIOS, ESPMode::ThreadSafe> WeakThis(AsShared());
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, TransactionData](float) -> bool
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FOnlinePurchaseIOS_TestDeferredTransactions);
			FOnlinePurchaseIOSPtr StrongThis = WeakThis.Pin();
			if (StrongThis.IsValid())
			{
				// Emulate redemption in a fixed time period
				StrongThis->OnTransactionCompleteResponse(EPurchaseTransactionState::Purchased, TransactionData);
			}
			return false;
		}), TEST_DEFERRED_DELAY);
#endif // TEST_DEFERRED_TRANSACTIONS
	}
	else
	{
		UE_LOG_ONLINE_PURCHASE(Log, TEXT("Offline deferred transaction"));
	}
}

void FOnlinePurchaseIOS::FinalizeReceiptValidationInfo(const FUniqueNetId& UserId, FString& InReceiptValidationInfo, const FOnFinalizeReceiptValidationInfoComplete& Delegate)
{
	FOnlineError DefaultSuccess(true);
	Delegate.ExecuteIfBound(DefaultSuccess, InReceiptValidationInfo);
}

TSharedRef<FPurchaseReceipt> FOnlinePurchasePendingTransactionIOS::GenerateReceipt()
{
	TSharedRef<FPurchaseReceipt> Receipt = MakeShareable(new FPurchaseReceipt());
	
	Receipt->TransactionState = PendingPurchaseInfo.TransactionState;
	Receipt->TransactionId = PendingPurchaseInfo.TransactionId;
	
	if(PendingPurchaseInfo.TransactionState == EPurchaseTransactionState::Purchased ||
	   PendingPurchaseInfo.TransactionState == EPurchaseTransactionState::Restored)
	{
		Receipt->ReceiptOffers = PendingPurchaseInfo.ReceiptOffers;
	}
	else
	{
		// Add the requested offers to the receipt in the event of an incomplete purchase.
		for(const auto& RequestedOffer : CheckoutRequest.PurchaseOffers)
		{
			Receipt->AddReceiptOffer(RequestedOffer.OfferNamespace, RequestedOffer.OfferId, RequestedOffer.Quantity);
		}
	}
	
	return Receipt;
}

TSharedRef<FPurchaseReceipt> FOnlinePurchasePendingTransactionIOS::GenerateReceipt(EPurchaseTransactionState Result, const FStoreKitTransactionData& Transaction)
{
	TSharedRef<FPurchaseReceipt> Receipt = MakeShareable(new FPurchaseReceipt());
	
	Receipt->TransactionState = Result;
	Receipt->TransactionId = Transaction.GetTransactionIdentifier();
	
	if (Result == EPurchaseTransactionState::Purchased ||
	    Result == EPurchaseTransactionState::Restored)
	{
		FPurchaseReceipt::FReceiptOfferEntry ReceiptEntry(TEXT(""), Transaction.GetOfferId(), 1);
		
		int32 Idx = ReceiptEntry.LineItems.AddZeroed();
		
		FPurchaseReceipt::FLineItemInfo& LineItem = ReceiptEntry.LineItems[Idx];
		
		LineItem.ItemName = Transaction.GetOfferId();
		LineItem.UniqueId = Transaction.GetTransactionIdentifier();
		LineItem.ValidationInfo = Transaction.GetReceiptData();

		Receipt->AddReceiptOffer(ReceiptEntry);
	}
	
	return Receipt;
}

void FOnlinePurchasePendingTransactionIOS::StartProcessing()
{
	PendingPurchaseInfo.TransactionState = EPurchaseTransactionState::Processing;
	for (int32 OfferIdx = 0; OfferIdx < CheckoutRequest.PurchaseOffers.Num(); ++OfferIdx)
	{
		OfferPurchaseStates[OfferIdx] = EPurchaseTransactionState::Processing;
	}
}

bool FOnlinePurchasePendingTransactionIOS::AddCompletedOffer(EPurchaseTransactionState Result, const FStoreKitTransactionData& Transaction)
{
	for (int32 OfferIdx = 0; OfferIdx < CheckoutRequest.PurchaseOffers.Num(); ++OfferIdx)
	{
		const FPurchaseCheckoutRequest::FPurchaseOfferEntry& Offer = CheckoutRequest.PurchaseOffers[OfferIdx];
		if (Transaction.GetOfferId() == Offer.OfferId)
		{
			OfferPurchaseStates[OfferIdx] = Result;
			FPurchaseReceipt::FReceiptOfferEntry Receipt(TEXT(""), Transaction.GetOfferId(), 1);

			int32 Idx = Receipt.LineItems.AddZeroed();
			
			FPurchaseReceipt::FLineItemInfo& LineItem = Receipt.LineItems[Idx];
			
			LineItem.ItemName = Transaction.GetOfferId();
			LineItem.UniqueId = Transaction.GetTransactionIdentifier();
			LineItem.ValidationInfo = Transaction.GetReceiptData();
			
			PendingPurchaseInfo.AddReceiptOffer(Receipt);
			return true;
		}
	}
	
	return false;
}

bool FOnlinePurchasePendingTransactionIOS::AreAllOffersComplete() const
{
	for (const EPurchaseTransactionState State : OfferPurchaseStates)
	{
		if (State == EPurchaseTransactionState::NotStarted ||
			State == EPurchaseTransactionState::Processing)
		{
			return false;
		}
	}
	
	return true;
}

EPurchaseTransactionState FOnlinePurchasePendingTransactionIOS::GetFinalTransactionState() const
{
	bool bAnyFailures = false;
	bool bAnyCancels = false;
	for (const EPurchaseTransactionState State : OfferPurchaseStates)
	{
		if (State == EPurchaseTransactionState::NotStarted ||
			State == EPurchaseTransactionState::Processing ||
			State == EPurchaseTransactionState::Failed)
		{
			bAnyFailures = true;
		}
		else if (State == EPurchaseTransactionState::Canceled)
		{
			bAnyCancels = true;
		}
	}
	
	if (bAnyFailures)
	{
		return EPurchaseTransactionState::Failed;
	}
	else if (bAnyCancels)
	{
		return EPurchaseTransactionState::Canceled;
	}
	
	return EPurchaseTransactionState::Purchased;
}

#undef LOCTEXT_NAMESPACE
