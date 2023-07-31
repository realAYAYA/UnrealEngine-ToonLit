// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlinePurchaseGooglePlay.h"
#include "OnlineSubsystemGooglePlay.h"
#include "OnlineError.h"
#include <jni.h>
#include "Android/AndroidJavaEnv.h"

#define LOCTEXT_NAMESPACE "OnlineSubsystemGooglePlay"

FGoogleTransactionData::FGoogleTransactionData(const FString& InOfferId, const FString& InPurchaseToken, const FString& InReceiptData, const FString& InSignature, EGooglePlayPurchaseState InPurchaseState)
	: OfferId(InOfferId)
	, PurchaseToken(InPurchaseToken)
	, CombinedTransactionData(InReceiptData, InSignature)
	, PurchaseState(InPurchaseState)
{
	if (PurchaseToken.IsEmpty())
	{
		ErrorStr = TEXT("Receipt does not contain purchase token");
	}
	else if (CombinedTransactionData.ReceiptData.IsEmpty())
	{
		ErrorStr = TEXT("Receipt does not contain receipt data");
	}
	else if (CombinedTransactionData.Signature.IsEmpty())
	{
		ErrorStr = TEXT("Receipt does not contain signature data");
	}
}

FString FGoogleTransactionData::ToDebugString() const
{
	return FString::Printf(TEXT("OfferId: %s PurchaseState: %s TransactionId: %s ReceiptData: %s%s"),
		*OfferId,
		LexToString(PurchaseState),
		*PurchaseToken,
		*CombinedTransactionData.ToJson(),
		ErrorStr.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" Error: %s"), *ErrorStr));
}

FOnlinePurchaseGooglePlay::FOnlinePurchaseGooglePlay(FOnlineSubsystemGooglePlay* InSubsystem)
	: bQueryingReceipts(false)
	, Subsystem(InSubsystem)
{
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT( "FOnlinePurchaseGooglePlay::FOnlinePurchaseGooglePlay" ));
}

/**
 * Info used to cache and track the order being currently purchased.
 */
class FOnlinePurchaseGooglePlay::FOnlinePurchaseInProgressTransaction
{
public:
	FOnlinePurchaseInProgressTransaction(
		const FPurchaseCheckoutRequest& InCheckoutRequest,
		const FUniqueNetId& InUserId,
		const EPurchaseTransactionState InPendingTransactionState,
		const FOnPurchaseCheckoutComplete& InCheckoutCompleteDelegate
		);
	
	/**
	 * Generate a final receipt for all purchases made in this single transaction
	 */
	TSharedRef<FPurchaseReceipt> GenerateReceipt();
	
	/** Generate one off receipts for transactions initiated outside the current run of the application */
	static TSharedRef<FPurchaseReceipt> GenerateReceipt(const FGoogleTransactionData& Transaction);
	
	/** Add the single completed transaction to this transaction */
	bool AddCompletedOffer(EPurchaseTransactionState Result, const FGoogleTransactionData& Transaction);

public:
	
	/** Checkout info for the in progress order */
	FPurchaseCheckoutRequest CheckoutRequest;
	/** User for the order in progress */
	FUniqueNetIdGooglePlayRef UserId;
	/** Delegate to call on completion */
	FOnPurchaseCheckoutComplete CheckoutCompleteDelegate;
	/** Tracks the current state of the order */
	FPurchaseReceipt PurchaseInfo;
};


FOnlinePurchaseGooglePlay::FOnlinePurchaseInProgressTransaction::FOnlinePurchaseInProgressTransaction(
	const FPurchaseCheckoutRequest& InCheckoutRequest,
	const FUniqueNetId& InUserId,
	const EPurchaseTransactionState InPendingTransactionState,
	const FOnPurchaseCheckoutComplete& InCheckoutCompleteDelegate
	)
	: CheckoutRequest(InCheckoutRequest)
	, UserId(FUniqueNetIdGooglePlay::Cast(InUserId.AsShared()))
	, CheckoutCompleteDelegate(InCheckoutCompleteDelegate)
{
	PurchaseInfo.TransactionState = InPendingTransactionState;
}

TSharedRef<FPurchaseReceipt> FOnlinePurchaseGooglePlay::FOnlinePurchaseInProgressTransaction::GenerateReceipt()
{
	TSharedRef<FPurchaseReceipt> Receipt = MakeShared<FPurchaseReceipt>();
	
	Receipt->TransactionState = PurchaseInfo.TransactionState;
	Receipt->TransactionId = PurchaseInfo.TransactionId;
	
	if(PurchaseInfo.TransactionState == EPurchaseTransactionState::Purchased ||
	   PurchaseInfo.TransactionState == EPurchaseTransactionState::Deferred)
	{
		Receipt->ReceiptOffers = PurchaseInfo.ReceiptOffers;
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

TSharedRef<FPurchaseReceipt> FOnlinePurchaseGooglePlay::FOnlinePurchaseInProgressTransaction::GenerateReceipt(const FGoogleTransactionData& Transaction)
{
	TSharedRef<FPurchaseReceipt> Receipt = MakeShared<FPurchaseReceipt>();
	
	Receipt->TransactionState = Transaction.GetErrorStr().IsEmpty() ? EPurchaseTransactionState::Purchased : EPurchaseTransactionState::Failed;
	Receipt->TransactionId = Transaction.GetTransactionIdentifier();
	
	if (Receipt->TransactionState == EPurchaseTransactionState::Purchased)
	{
		FPurchaseReceipt::FReceiptOfferEntry ReceiptEntry(TEXT(""), Transaction.GetOfferId(), 1);

		FPurchaseReceipt::FLineItemInfo& LineItem = ReceiptEntry.LineItems.Emplace_GetRef();

		LineItem.ItemName = Transaction.GetOfferId();
		LineItem.UniqueId = Transaction.GetTransactionIdentifier();
		if(Transaction.GetPurchaseState() == EGooglePlayPurchaseState::Purchased)
		{
			// Pending purchases should not be redeemable
			LineItem.ValidationInfo = Transaction.GetCombinedReceiptData();
		}
		Receipt->AddReceiptOffer(MoveTemp(ReceiptEntry));
	}
	
	return Receipt;
}

bool FOnlinePurchaseGooglePlay::FOnlinePurchaseInProgressTransaction::AddCompletedOffer(EPurchaseTransactionState Result, const FGoogleTransactionData& Transaction)
{
	bool bIsFromThisRequest = CheckoutRequest.PurchaseOffers.ContainsByPredicate([&Transaction](const FPurchaseCheckoutRequest::FPurchaseOfferEntry& Entry)
	{
		return (Transaction.GetOfferId() == Entry.OfferId);
	});
	
	if(!bIsFromThisRequest)
	{
		return false;
	}

	FPurchaseReceipt::FReceiptOfferEntry Receipt(TEXT(""), Transaction.GetOfferId(), 1);

	FPurchaseReceipt::FLineItemInfo& LineItem = Receipt.LineItems.Emplace_GetRef();
	
	LineItem.ItemName = Transaction.GetOfferId();
	LineItem.UniqueId = Transaction.GetTransactionIdentifier();
	if(Transaction.GetPurchaseState() == EGooglePlayPurchaseState::Purchased)
	{
		// Pending purchases should not be redeemable
		LineItem.ValidationInfo = Transaction.GetCombinedReceiptData();
	}
	PurchaseInfo.AddReceiptOffer(MoveTemp(Receipt));
	return true;
}

FOnlinePurchaseGooglePlay::~FOnlinePurchaseGooglePlay()
{
}

void FOnlinePurchaseGooglePlay::Init()
{
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("FOnlinePurchaseGooglePlay::Init"));
}

bool FOnlinePurchaseGooglePlay::IsAllowedToPurchase(const FUniqueNetId& UserId)
{
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("FOnlinePurchaseGooglePlay::IsAllowedToPurchase"));
	extern bool AndroidThunkCpp_Iap_IsAllowedToMakePurchases();
	return AndroidThunkCpp_Iap_IsAllowedToMakePurchases();
}

void FOnlinePurchaseGooglePlay::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate)
{
	bool bStarted = false;
	FText ErrorMessage;

	if (InProgressTransaction != nullptr)
	{
		UE_LOG_ONLINE_PURCHASE(Warning, TEXT("Previous purchase finished without notification"));			
		InProgressTransaction.Reset();
	}

	TSharedPtr<FOnlinePurchaseInProgressTransaction> RequestedTransaction = MakeShared<FOnlinePurchaseInProgressTransaction>(CheckoutRequest, UserId, EPurchaseTransactionState::NotStarted, Delegate);

	if (IsAllowedToPurchase(UserId))
	{
		int32 NumOffers = CheckoutRequest.PurchaseOffers.Num();
		if (NumOffers > 0)
		{
			const FPurchaseCheckoutRequest::FPurchaseOfferEntry& Offer = CheckoutRequest.PurchaseOffers[0];

			extern bool AndroidThunkCpp_Iap_BeginPurchase(const FString&, const FString&);
			FString AccountId;
			if (CheckoutRequest.AccountId.IsSet())
			{
				AccountId = CheckoutRequest.AccountId.GetValue();
			}
			bStarted = AndroidThunkCpp_Iap_BeginPurchase(Offer.OfferId, AccountId);
			if(bStarted)
			{
				UE_LOG_ONLINE_PURCHASE(Display, TEXT("Created a transaction."));
				RequestedTransaction->PurchaseInfo.TransactionState = EPurchaseTransactionState::Processing;
			}
			else
			{
				UE_LOG_ONLINE_PURCHASE(Display, TEXT("Failed to create a transaction."));
				RequestedTransaction->PurchaseInfo.TransactionState = EPurchaseTransactionState::Failed;
			}
			if (NumOffers > 1)
			{
				UE_LOG_ONLINE_PURCHASE(Warning, TEXT("Only supported purchasing one offer at a time, %d were requested and ignored"), NumOffers - 1);
			}
		}
		else
		{
			ErrorMessage = NSLOCTEXT("GooglePlayPurchase", "ErrorNoOffersSpecified", "Failed to checkout, no offers given.");
			RequestedTransaction->PurchaseInfo.TransactionState = EPurchaseTransactionState::Failed;
		}
	}
	else
	{
		ErrorMessage = NSLOCTEXT("GooglePlayPurchase", "ErrorPurchaseNotAllowed", "Failed to checkout, user not allowed to purchase.");
		RequestedTransaction->PurchaseInfo.TransactionState = EPurchaseTransactionState::Failed;
	}

	if(bStarted)
	{
		InProgressTransaction = MoveTemp(RequestedTransaction);
	}
	else
	{
		TSharedRef<FPurchaseReceipt> FailReceipt = RequestedTransaction->GenerateReceipt();

		Subsystem->ExecuteNextTick([ErrorMessage, FailReceipt, Delegate]()
		{
			FOnlineError Error(ErrorMessage);
			Delegate.ExecuteIfBound(Error, FailReceipt);
		});
	}
}

void FOnlinePurchaseGooglePlay::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseReceiptlessCheckoutComplete& Delegate)
{
	// Not Supported
	UE_LOG_ONLINE(Error, TEXT("FOnlinePurchaseGooglePlay::Checkout (receiptless) is not currently supported"));
	Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::NotImplemented));
}

void FOnlinePurchaseGooglePlay::FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId)
{
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("FOnlinePurchaseGooglePlay::FinalizePurchase %s %s"), *UserId.ToString(), *ReceiptId);
	extern void AndroidThunkCpp_Iap_ConsumePurchase(const FString&);
	AndroidThunkCpp_Iap_ConsumePurchase(ReceiptId);
}

void FOnlinePurchaseGooglePlay::RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate)
{
	// Not supported
	FOnlineError Result;
	Delegate.ExecuteIfBound(Result, MakeShared<FPurchaseReceipt>());
}

void FOnlinePurchaseGooglePlay::QueryReceipts(const FUniqueNetId& UserId, bool /*bRestoreReceipts*/, const FOnQueryReceiptsComplete& Delegate)
{
	if (!bQueryingReceipts)
	{
		/**
		 * bRestoreReceipts is irrelevant because there is no concept of restore here, any purchase query will reveal any non consumed purchases. 
		 * They will remain in the list until the the game consumes it via FinalizePurchase().
		 */
		bQueryingReceipts = true;
		QueryReceiptsComplete = Delegate;

		extern bool AndroidThunkCpp_Iap_QueryExistingPurchases();
		if (!AndroidThunkCpp_Iap_QueryExistingPurchases())
		{
			UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("FOnlinePurchaseGooglePlay::QueryReceipts failed to start query"));
			
			Subsystem->ExecuteNextTick([this]() {
				FOnlineError Result(false);
				QueryReceiptsComplete.ExecuteIfBound(Result);
				QueryReceiptsComplete.Unbind();
				bQueryingReceipts = false;
			});
		}
	}
	else
	{
		UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("FOnlinePurchaseGooglePlay::QueryReceipts already in progress."));
		
		Subsystem->ExecuteNextTick([Delegate = Delegate]() mutable {
			FOnlineError Result(false);
			Delegate.ExecuteIfBound(Result);
		});
	}
}

void FOnlinePurchaseGooglePlay::GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const
{
	OutReceipts.Empty();

	for(const TSharedRef<FPurchaseReceipt>& Transaction: KnownTransactions)
	{
		OutReceipts.Add(*Transaction);
	}
	
	UE_LOG_ONLINE_PURCHASE(Log, TEXT("FOnlinePurchaseGooglePlay::GetReceipts - Final Number of OutReceipts - %d"), OutReceipts.Num());
}

void FOnlinePurchaseGooglePlay::OnTransactionCompleteResponse(EGooglePlayBillingResponseCode InResponseCode, const FGoogleTransactionData& InTransactionData)
{
	UE_LOG_ONLINE_PURCHASE(Log, TEXT("FOnlinePurchaseGooglePlay::OnTransactionCompleteResponse %s. Transaction: %s"), LexToString(InResponseCode), *InTransactionData.ToDebugString());
	EPurchaseTransactionState Result = ConvertGPResponseToPurchaseTransactionState(InResponseCode, InTransactionData.GetPurchaseState());

	bool bIsInProgressTransaction = false;
	
	if(InProgressTransaction != nullptr)
	{
		// Only supported purchasing one offer at a time
		const FPurchaseCheckoutRequest::FPurchaseOfferEntry& Offer = InProgressTransaction->CheckoutRequest.PurchaseOffers[0];
		bool bWasInProgressTransactionCanceled = (Result == EPurchaseTransactionState::Canceled && InTransactionData.GetOfferId().IsEmpty());
		bIsInProgressTransaction = bWasInProgressTransactionCanceled || (Offer.OfferId == InTransactionData.GetOfferId());
	}

	if(bIsInProgressTransaction)
	{
		const FString& ErrorStr = InTransactionData.GetErrorStr();

		FOnlineError FinalResult;
		switch (Result)
		{
			case EPurchaseTransactionState::Failed:
				FinalResult.SetFromErrorCode(TEXT("com.epicgames.purchase.failure"));
				FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("GooglePlayTransactionFailed", "Transaction Failed");
				break;
			case EPurchaseTransactionState::Canceled:
				FinalResult.SetFromErrorCode(TEXT("com.epicgames.catalog_helper.user_cancelled"));
				FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("GooglePlayTransactionCancel", "Transaction Canceled");
				break;
			case EPurchaseTransactionState::Purchased:
				FinalResult.bSucceeded = true;
				break;
			case EPurchaseTransactionState::Deferred:
				FinalResult.SetFromErrorCode(TEXT("com.epicgames.purchase.deferred"));
				FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("GooglePlayTransactionDeferred", "Transaction Deferred");
				break;
			case EPurchaseTransactionState::Invalid:
				FinalResult.SetFromErrorCode(TEXT("com.epicgames.purchase.invalid"));
				FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("GooglePlayInvalidState", "Invalid purchase result");
				break;
			default:
				UE_LOG_ONLINE_PURCHASE(Warning, TEXT("Unexpected EPurchaseTransactionState %d"), (int)Result);
				FinalResult.SetFromErrorCode(TEXT("com.epicgames.purchase.unexpected_state"));
				FinalResult.ErrorMessage = !ErrorStr.IsEmpty() ? FText::FromString(ErrorStr) : LOCTEXT("GooglePlayUnexpectedState", "Unexpected purchase result");
				break;
		}

		InProgressTransaction->PurchaseInfo.TransactionState = Result;
		InProgressTransaction->PurchaseInfo.TransactionId = InTransactionData.GetTransactionIdentifier();

		if (Result == EPurchaseTransactionState::Purchased ||
			Result == EPurchaseTransactionState::Deferred )
		{
			InProgressTransaction->AddCompletedOffer(Result, InTransactionData);
		}

		TSharedRef<FPurchaseReceipt> FinalReceipt = InProgressTransaction->GenerateReceipt();

		if (Result == EPurchaseTransactionState::Purchased ||
			Result == EPurchaseTransactionState::Deferred )
		{
			KnownTransactions.Add(FinalReceipt);
		}

		InProgressTransaction->CheckoutCompleteDelegate.ExecuteIfBound(FinalResult, FinalReceipt);
		InProgressTransaction.Reset();
	}
	else
	{
		if (Result == EPurchaseTransactionState::Purchased)
		{
			TSharedRef<FPurchaseReceipt> NewReceipt = FOnlinePurchaseInProgressTransaction::GenerateReceipt(InTransactionData);;
			TSharedRef<FPurchaseReceipt>* ExistingReceipt = KnownTransactions.FindByPredicate([&InTransactionData](const TSharedRef<FPurchaseReceipt>& Receipt)
			{
				return Receipt->TransactionId == InTransactionData.GetTransactionIdentifier();
			});
			
			if(ExistingReceipt)
			{
				if((*ExistingReceipt)->TransactionState == EPurchaseTransactionState::Deferred)
				{
					UE_LOG_ONLINE_PURCHASE(Log, TEXT("Pending transaction completed"));
				}
				else
				{
					UE_LOG_ONLINE_PURCHASE(Log, TEXT("Unexpected transaction completed (a possible pending transaction finished from previous session?)"));
				}
				*ExistingReceipt = NewReceipt;
			}
			else
			{
				KnownTransactions.Add(NewReceipt);
			}
			
			TriggerOnUnexpectedPurchaseReceiptDelegates(*FUniqueNetIdGooglePlay::EmptyId());
		}
	}
}

void FOnlinePurchaseGooglePlay::OnQueryExistingPurchasesComplete(EGooglePlayBillingResponseCode InResponseCode, const TArray<FGoogleTransactionData>& InExistingPurchases)
{
	UE_LOG_ONLINE_PURCHASE(Log, TEXT("FOnlinePurchaseGooglePlay::OnQueryExistingPurchasesComplete Response: %s Num: %d"), LexToString(InResponseCode), InExistingPurchases.Num());

	if (bQueryingReceipts)
	{
		bool bSuccess = (InResponseCode == EGooglePlayBillingResponseCode::Ok);
		if (bSuccess)
		{
			KnownTransactions.Empty();

			for(const FGoogleTransactionData& Purchase: InExistingPurchases)
			{
				UE_LOG_ONLINE_PURCHASE(Log, TEXT("Adding existing receipt %s"), *Purchase.ToDebugString());
				TSharedRef<FPurchaseReceipt> Receipt = FOnlinePurchaseInProgressTransaction::GenerateReceipt(Purchase); 
				KnownTransactions.Add(Receipt);
			}			
		}
		else
		{
			UE_LOG_ONLINE_PURCHASE(Log, TEXT("OnQueryExistingPurchasesComplete failed"));
		}

		FOnlineError Result(bSuccess);
		QueryReceiptsComplete.ExecuteIfBound(Result);
		QueryReceiptsComplete.Unbind();
		bQueryingReceipts = false;
	}
	else
	{
		UE_LOG_ONLINE_PURCHASE(Warning, TEXT("FOnlinePurchaseGooglePlay::OnQueryExistingPurchasesComplete unexpected call"));
	}
}

void FOnlinePurchaseGooglePlay::OnConsumePurchaseComplete(EGooglePlayBillingResponseCode InResponseCode, const FString& TransactionIdentifier)
{
	UE_LOG_ONLINE_PURCHASE(Log, TEXT("FOnlinePurchaseGooglePlay::OnConsumePurchaseComplete Response: %s PurchaseToken: %s"), LexToString(InResponseCode), *TransactionIdentifier);

	if (InResponseCode == EGooglePlayBillingResponseCode::Ok || InResponseCode == EGooglePlayBillingResponseCode::ItemNotOwned)
	{
		KnownTransactions.RemoveAll([&TransactionIdentifier](const TSharedRef<FPurchaseReceipt>& Receipt) 
		{
			return Receipt->TransactionId == TransactionIdentifier;
		});		
	}
	else
	{
		UE_LOG_ONLINE_PURCHASE(Log, TEXT("OnConsumePurchaseComplete failed"));
	}
}

void FOnlinePurchaseGooglePlay::FinalizeReceiptValidationInfo(const FUniqueNetId& UserId, FString& InReceiptValidationInfo, const FOnFinalizeReceiptValidationInfoComplete& Delegate)
{
	FOnlineError DefaultSuccess(true);
	Delegate.ExecuteIfBound(DefaultSuccess, InReceiptValidationInfo);
}

JNI_METHOD void Java_com_epicgames_unreal_GooglePlayStoreHelper_NativeQueryExistingPurchasesComplete(JNIEnv* jenv, jobject /*Thiz*/, jint ResponseCode, jobjectArray ProductIDs, jintArray PurchaseStatesArray, jobjectArray PurchaseTokens, jobjectArray ReceiptsData, jobjectArray Signatures)
{
	TArray<FGoogleTransactionData> ExistingPurchaseInfo;

	EGooglePlayBillingResponseCode EGPResponse = (EGooglePlayBillingResponseCode)ResponseCode;

	bool bWasSuccessful = (EGPResponse == EGooglePlayBillingResponseCode::Ok);
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("NativeQueryExistingPurchasesComplete received response code: %s\n"), LexToString(EGPResponse));

	if (bWasSuccessful)
	{
		jsize NumProducts = jenv->GetArrayLength(ProductIDs);
		jsize NumTransactionStates = jenv->GetArrayLength(PurchaseStatesArray);
		jsize NumPurchaseTokens = jenv->GetArrayLength(PurchaseTokens);
		jsize NumReceipts = jenv->GetArrayLength(ReceiptsData);
		jsize NumSignatures = jenv->GetArrayLength(Signatures);

		ensure((NumProducts == NumPurchaseTokens) && (NumProducts == NumTransactionStates) && (NumProducts == NumReceipts) && (NumProducts == NumSignatures));

		jint* PurchaseStates = jenv->GetIntArrayElements(PurchaseStatesArray, NULL);
		
		for (jsize Idx = 0; Idx < NumProducts; Idx++)
		{
			// Build the product information strings.
			const auto OfferId = FJavaHelper::FStringFromLocalRef(jenv, (jstring)jenv->GetObjectArrayElement(ProductIDs, Idx));
			const auto PurchaseToken = FJavaHelper::FStringFromLocalRef(jenv, (jstring)jenv->GetObjectArrayElement(PurchaseTokens, Idx));
			const auto ReceiptData = FJavaHelper::FStringFromLocalRef(jenv, (jstring)jenv->GetObjectArrayElement(ReceiptsData, Idx));
			const auto SignatureData = FJavaHelper::FStringFromLocalRef(jenv, (jstring)jenv->GetObjectArrayElement(Signatures, Idx));
			const auto PurchaseState = (EGooglePlayPurchaseState)PurchaseStates[Idx];

			FGoogleTransactionData ExistingPurchase(OfferId, PurchaseToken, ReceiptData, SignatureData, PurchaseState);
			ExistingPurchaseInfo.Add(ExistingPurchase);

			UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("\Existing Product Identifier: %s"), *ExistingPurchase.ToDebugString());
		}
		jenv->ReleaseIntArrayElements(PurchaseStatesArray, PurchaseStates, JNI_ABORT);
	}
	
	if (auto OnlineSubGP = static_cast<FOnlineSubsystemGooglePlay* const>(IOnlineSubsystem::Get(GOOGLEPLAY_SUBSYSTEM)))
	{
		OnlineSubGP->ExecuteNextTick([OnlineSubGP, EGPResponse, ExistingPurchaseInfo = MoveTemp(ExistingPurchaseInfo)]()
		{
			TSharedPtr<FOnlinePurchaseGooglePlay> PurchaseInt = StaticCastSharedPtr<FOnlinePurchaseGooglePlay>(OnlineSubGP->GetPurchaseInterface());
			PurchaseInt->OnQueryExistingPurchasesComplete(EGPResponse, ExistingPurchaseInfo);
		});
	}
}

JNI_METHOD void Java_com_epicgames_unreal_GooglePlayStoreHelper_NativePurchaseComplete(JNIEnv* jenv, jobject /*Thiz*/, jint JavaResponseCode, jstring JavaProductId, jint JavaPurchaseState, jstring JavaPurchaseToken, jstring JavaReceiptData, jstring JavaSignature)
{
	FString ProductId, PurchaseToken, ReceiptData, Signature;
	EGooglePlayBillingResponseCode EGPResponse = (EGooglePlayBillingResponseCode)JavaResponseCode;
	EGooglePlayPurchaseState PurchaseState = (EGooglePlayPurchaseState)JavaPurchaseState;

	ProductId = FJavaHelper::FStringFromParam(jenv, JavaProductId);
	PurchaseToken = FJavaHelper::FStringFromParam(jenv, JavaPurchaseToken);
	ReceiptData = FJavaHelper::FStringFromParam(jenv, JavaReceiptData);
	Signature = FJavaHelper::FStringFromParam(jenv, JavaSignature);

	FGoogleTransactionData TransactionData(ProductId, PurchaseToken, ReceiptData, Signature, PurchaseState);

	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("NativePurchaseComplete received response code: %s. Transaction: %s\n"), LexToString(EGPResponse), *TransactionData.ToDebugString());

	if (auto OnlineSubGP = static_cast<FOnlineSubsystemGooglePlay* const>(IOnlineSubsystem::Get(GOOGLEPLAY_SUBSYSTEM)))
	{
		OnlineSubGP->ExecuteNextTick([OnlineSubGP, EGPResponse, TransactionData = MoveTemp(TransactionData)]()
		{
			TSharedPtr<FOnlinePurchaseGooglePlay> PurchaseInt = StaticCastSharedPtr<FOnlinePurchaseGooglePlay>(OnlineSubGP->GetPurchaseInterface());
			PurchaseInt->OnTransactionCompleteResponse(EGPResponse, TransactionData);
		});
	}
}

JNI_METHOD void Java_com_epicgames_unreal_GooglePlayStoreHelper_NativeConsumeComplete(JNIEnv* jenv, jobject /*Thiz*/, jint JavaResponseCode, jstring JavaPurchaseToken)
{
	FString PurchaseToken;
	EGooglePlayBillingResponseCode EGPResponse = (EGooglePlayBillingResponseCode)JavaResponseCode;

	bool bWasSuccessful = (EGPResponse == EGooglePlayBillingResponseCode::Ok);
	UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("NativeConsumeComplete received response code: %s. PurchaseToken: %s\n"), LexToString(EGPResponse), *PurchaseToken);

	PurchaseToken = FJavaHelper::FStringFromParam(jenv, JavaPurchaseToken);

	if (auto OnlineSubGP = static_cast<FOnlineSubsystemGooglePlay* const>(IOnlineSubsystem::Get(GOOGLEPLAY_SUBSYSTEM)))
	{
		OnlineSubGP->ExecuteNextTick([OnlineSubGP, EGPResponse, PurchaseToken]()
		{
			TSharedPtr<FOnlinePurchaseGooglePlay> PurchaseInt = StaticCastSharedPtr<FOnlinePurchaseGooglePlay>(OnlineSubGP->GetPurchaseInterface());
			PurchaseInt->OnConsumePurchaseComplete(EGPResponse, PurchaseToken);
		});
	}
}

#undef LOCTEXT_NAMESPACE

