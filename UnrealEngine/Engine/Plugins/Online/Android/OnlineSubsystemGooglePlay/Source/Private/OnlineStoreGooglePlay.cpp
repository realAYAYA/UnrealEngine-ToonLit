// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineStoreGooglePlay.h"
#include "OnlineSubsystemGooglePlay.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Internationalization/FastDecimalFormat.h"
#include "Misc/ConfigCacheIni.h"
#include <jni.h>
#include "Android/AndroidJavaEnv.h"

FOnlineStoreGooglePlayV2::FOnlineStoreGooglePlayV2(FOnlineSubsystemGooglePlay* InSubsystem)
	: bIsQueryInFlight(false)
	, Subsystem(InSubsystem)
{
	UE_LOG_ONLINE_STOREV2(Verbose, TEXT( "FOnlineStoreGooglePlayV2::FOnlineStoreGooglePlayV2" ));
}

FOnlineStoreGooglePlayV2::FOnlineStoreGooglePlayV2()
	: bIsQueryInFlight(false)
	, Subsystem(nullptr)
{
	UE_LOG_ONLINE_STOREV2(Verbose, TEXT( "FOnlineStoreGooglePlayV2::FOnlineStoreGooglePlayV2 empty" ));
}

FOnlineStoreGooglePlayV2::~FOnlineStoreGooglePlayV2()
{
}

void FOnlineStoreGooglePlayV2::Init()
{
	UE_LOG_ONLINE_STOREV2(Verbose, TEXT("FOnlineStoreGooglePlayV2::Init"));

	FString GooglePlayLicenseKey;
	if (!GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("GooglePlayLicenseKey"), GooglePlayLicenseKey, GEngineIni) || GooglePlayLicenseKey.IsEmpty())
	{
		UE_LOG_ONLINE_STOREV2(Warning, TEXT("Missing GooglePlayLicenseKey key in /Script/AndroidRuntimeSettings.AndroidRuntimeSettings of DefaultEngine.ini"));
	}

	extern void AndroidThunkCpp_Iap_SetupIapService(const FString&);
	AndroidThunkCpp_Iap_SetupIapService(GooglePlayLicenseKey);
}

TSharedRef<FOnlineStoreOffer> ConvertProductToStoreOffer(const FOnlineStoreOffer& Product)
{
	return MakeShared<FOnlineStoreOffer>(Product);
}

void FOnlineStoreGooglePlayV2::OnGooglePlayAvailableIAPQueryComplete(EGooglePlayBillingResponseCode InResponseCode, const TArray<FProvidedProductInformation>& InProvidedProductInformation)
{ 
	UE_LOG_ONLINE_STOREV2(Verbose, TEXT("OnGooglePlayAvailableIAPQueryComplete Response: %s NumProucts: %d"), LexToString(InResponseCode), InProvidedProductInformation.Num());

	bool bSuccess = (InResponseCode == EGooglePlayBillingResponseCode::Ok);
	TArray<FUniqueOfferId> OfferIds;
	FString ErrorStr;
	
	if(!bIsQueryInFlight)
	{
		UE_LOG_ONLINE_STOREV2(Log, TEXT("OnGooglePlayAvailableIAPQueryComplete: No IAP query in flight"));
	}

	if (bSuccess)
	{
		for (const FProvidedProductInformation& Product : InProvidedProductInformation)
		{
			TSharedRef<FOnlineStoreOffer> NewProductOffer = ConvertProductToStoreOffer(Product);

			AddOffer(NewProductOffer);
			OfferIds.Add(NewProductOffer->OfferId);

			UE_LOG_ONLINE_STOREV2(Log, TEXT("Product Identifier: %s, Name: %s, Desc: %s, Long Desc: %s, Price: %s IntPrice: %d"),
				*NewProductOffer->OfferId,
				*NewProductOffer->Title.ToString(),
				*NewProductOffer->Description.ToString(),
				*NewProductOffer->LongDescription.ToString(),
				*NewProductOffer->PriceText.ToString(),
				NewProductOffer->NumericPrice);
		}
	}
	else
	{
		ErrorStr = LexToString(InResponseCode);
	}
	
	QueryOnlineStoreOffersCompleteDelegate.ExecuteIfBound(bSuccess, OfferIds, ErrorStr);
	QueryOnlineStoreOffersCompleteDelegate.Unbind();
	bIsQueryInFlight = false;
}

void FOnlineStoreGooglePlayV2::QueryCategories(const FUniqueNetId& UserId, const FOnQueryOnlineStoreCategoriesComplete& Delegate)
{
	Delegate.ExecuteIfBound(false, TEXT("No CatalogService"));
}

void FOnlineStoreGooglePlayV2::GetCategories(TArray<FOnlineStoreCategory>& OutCategories) const
{
	OutCategories.Empty();
}

void FOnlineStoreGooglePlayV2::QueryOffersByFilter(const FUniqueNetId& UserId, const FOnlineStoreFilter& Filter, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
	Delegate.ExecuteIfBound(false, TArray<FUniqueOfferId>(), TEXT("No CatalogService"));
}

void FOnlineStoreGooglePlayV2::QueryOffersById(const FUniqueNetId& UserId, const TArray<FUniqueOfferId>& OfferIds, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
	UE_LOG_ONLINE_STOREV2(Verbose, TEXT("FOnlineStoreGooglePlayV2::QueryOffersById"));

	if (bIsQueryInFlight)
	{
		Delegate.ExecuteIfBound(false, OfferIds, TEXT("Request already in flight"));
	}
	else if (OfferIds.Num() == 0)
	{
		Delegate.ExecuteIfBound(false, OfferIds, TEXT("No offers to query for"));
	}
	else
	{
		bIsQueryInFlight = true;
		QueryOnlineStoreOffersCompleteDelegate = Delegate;
		extern bool AndroidThunkCpp_Iap_QueryInAppPurchases(const TArray<FString>&);
		AndroidThunkCpp_Iap_QueryInAppPurchases(OfferIds);
	}
}

void FOnlineStoreGooglePlayV2::AddOffer(const TSharedRef<FOnlineStoreOffer>& NewOffer)
{
	TSharedRef<FOnlineStoreOffer>* Existing = CachedOffers.Find(NewOffer->OfferId);
	if (Existing != nullptr)
	{
		// Replace existing offer
		*Existing = NewOffer;
	}
	else
	{
		CachedOffers.Add(NewOffer->OfferId, NewOffer);
	}
}

void FOnlineStoreGooglePlayV2::GetOffers(TArray<FOnlineStoreOfferRef>& OutOffers) const
{
	for (const auto& CachedEntry : CachedOffers)
	{
		const TSharedRef<FOnlineStoreOffer>& CachedOffer = CachedEntry.Value;
		OutOffers.Add(CachedOffer);
	}
}

TSharedPtr<FOnlineStoreOffer> FOnlineStoreGooglePlayV2::GetOffer(const FUniqueOfferId& OfferId) const
{
	TSharedPtr<FOnlineStoreOffer> Result;

	const TSharedRef<FOnlineStoreOffer>* Existing = CachedOffers.Find(OfferId);
	if (Existing != nullptr)
	{
		Result = (*Existing);
	}

	return Result;
}

JNI_METHOD void Java_com_epicgames_unreal_GooglePlayStoreHelper_NativeQueryComplete(JNIEnv* jenv, jobject /*Thiz*/, jint ResponseCode, jobjectArray ProductIDs, jobjectArray Titles, jobjectArray Descriptions, jobjectArray Prices, jfloatArray PriceValuesRaw, jobjectArray CurrencyCodes)
{
	TArray<FOnlineStoreOffer> ProvidedProductInformation;
	EGooglePlayBillingResponseCode EGPResponse = (EGooglePlayBillingResponseCode)ResponseCode;
	bool bWasSuccessful = (EGPResponse == EGooglePlayBillingResponseCode::Ok);

	if (jenv && bWasSuccessful)
	{
		jsize NumProducts = jenv->GetArrayLength(ProductIDs);
		jsize NumTitles = jenv->GetArrayLength(Titles);
		jsize NumDescriptions = jenv->GetArrayLength(Descriptions);
		jsize NumPrices = jenv->GetArrayLength(Prices);
		jsize NumPricesRaw = jenv->GetArrayLength(PriceValuesRaw);
		jsize NumCurrencyCodes = jenv->GetArrayLength(CurrencyCodes);

		ensure((NumProducts == NumTitles) && (NumProducts == NumDescriptions) && (NumProducts == NumPrices) && (NumProducts == NumPricesRaw) && (NumProducts == NumCurrencyCodes));

		jfloat* PriceValues = jenv->GetFloatArrayElements(PriceValuesRaw, 0);

		for (jsize Idx = 0; Idx < NumProducts; Idx++)
		{
			FOnlineStoreOffer NewProductInfo;

			NewProductInfo.OfferId = FJavaHelper::FStringFromLocalRef(jenv, (jstring)jenv->GetObjectArrayElement(ProductIDs, Idx));

			int32 OpenParenIdx = -1;
			int32 CloseParenIdx = -1;
			FString Title = FJavaHelper::FStringFromLocalRef(jenv, (jstring)jenv->GetObjectArrayElement(Titles, Idx));
			if (Title.FindLastChar(TEXT(')'), CloseParenIdx) && Title.FindLastChar(TEXT('('), OpenParenIdx) && (OpenParenIdx < CloseParenIdx))
			{
				Title = Title.Left(OpenParenIdx).TrimEnd();
			}
			NewProductInfo.Title = FText::FromString(Title);

			NewProductInfo.Description = FText::FromString(FJavaHelper::FStringFromLocalRef(jenv, (jstring)jenv->GetObjectArrayElement(Descriptions, Idx)));
			NewProductInfo.PriceText = FText::FromString(FJavaHelper::FStringFromLocalRef(jenv, (jstring)jenv->GetObjectArrayElement(Prices, Idx)));
			NewProductInfo.CurrencyCode = FJavaHelper::FStringFromLocalRef(jenv, (jstring)jenv->GetObjectArrayElement(CurrencyCodes, Idx));

			// Convert the backend stated price into its base units
			FInternationalization& I18N = FInternationalization::Get();
			const FCulture& Culture = *I18N.GetCurrentCulture();

			const FDecimalNumberFormattingRules& FormattingRules = Culture.GetCurrencyFormattingRules(NewProductInfo.CurrencyCode);
			const FNumberFormattingOptions& FormattingOptions = FormattingRules.CultureDefaultFormattingOptions;
			double Val = static_cast<double>(PriceValues[Idx]) * static_cast<double>(FMath::Pow(10.0f, FormattingOptions.MaximumFractionalDigits));

			NewProductInfo.NumericPrice = FMath::TruncToInt(Val + 0.5);
			NewProductInfo.ReleaseDate = FDateTime::MinValue();
			NewProductInfo.ExpirationDate = FDateTime::MaxValue();

			ProvidedProductInformation.Add(NewProductInfo);

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\nProduct Identifier: %s, Name: %s, Description: %s, Price: %s, Price Raw: %d, Currency Code: %s\n"),
				*NewProductInfo.OfferId,
				*NewProductInfo.Title.ToString(),
				*NewProductInfo.Description.ToString(),
				*NewProductInfo.GetDisplayPrice().ToString(),
				NewProductInfo.NumericPrice,
				*NewProductInfo.CurrencyCode);
		}
		jenv->ReleaseFloatArrayElements(PriceValuesRaw, PriceValues, JNI_ABORT);
	}

	UE_LOG_ONLINE_STOREV2(Verbose, TEXT("QueryOffersById result Success: %d Response: %s"), bWasSuccessful, LexToString(EGPResponse));

	if (auto OnlineSubGP = static_cast<FOnlineSubsystemGooglePlay* const>(IOnlineSubsystem::Get(GOOGLEPLAY_SUBSYSTEM)))
	{
		OnlineSubGP->ExecuteNextTick([OnlineSubGP, EGPResponse, Response = MoveTemp(ProvidedProductInformation)]()
		{
			TSharedPtr<FOnlineStoreGooglePlayV2> StoreInt = StaticCastSharedPtr<FOnlineStoreGooglePlayV2>(OnlineSubGP->GetStoreV2Interface());
			StoreInt->OnGooglePlayAvailableIAPQueryComplete(EGPResponse, Response);
		});
	}
}
