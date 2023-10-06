// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "OnlineSubsystem.h"
#include "InAppPurchaseFinalizeProxy.generated.h"

struct FInAppPurchaseReceiptInfo2;

UCLASS(MinimalAPI)
class UInAppPurchaseFinalizeProxy : public UObject
{
	GENERATED_UCLASS_BODY()

	// Finalizes a transaction for the provided transaction identifier
	UFUNCTION(BlueprintCallable, meta = (DisplayName="Finalize In-App Purchase Transaction"), Category="Online|InAppPurchase")
	static UInAppPurchaseFinalizeProxy* CreateProxyObjectForInAppPurchaseFinalize(const FInAppPurchaseReceiptInfo2& InAppPurchaseReceipt, class APlayerController* PlayerController);

private:

	/** Triggers the In-App Purchase Finalize Transaction for the specifed user */
	void Trigger(const FInAppPurchaseReceiptInfo2& InAppPurchaseReceipt, class APlayerController* PlayerController);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "InAppPurchaseCallbackProxy2.h"
#include "Interfaces/OnlinePurchaseInterface.h"
#endif
