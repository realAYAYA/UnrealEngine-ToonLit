// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "Output/VCamOutputProviderBase.h"
#include "IVCamOutputProviderCreatedWidget.generated.h"

class UVCamComponent;

USTRUCT(BlueprintType)
struct FVCamReceiveOutputProviderData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Virtual Camera")
	TObjectPtr<UVCamOutputProviderBase> OutputProvider;
};

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class UVCamOutputProviderCreatedWidget : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by widgets that are either directly or indirectly created via an output provider's UMGClass.
 * Receives important events, such as when a widget has been created by an output provider.
 */
class VCAMCORE_API IVCamOutputProviderCreatedWidget
{
	GENERATED_BODY()
public:
	
	/** This is called to let widgets know of the output provider that created them. */
	UFUNCTION(BlueprintNativeEvent, Category = "Virtual Camera")
	void ReceiveOutputProvider(const FVCamReceiveOutputProviderData& Data);
};
