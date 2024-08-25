// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"
#include "Containers/UnrealString.h"
#include "ConnectableValue.generated.h"

USTRUCT()
struct FChaosClothAssetConnectableStringValue
{
	GENERATED_BODY()

	/** The value for this property. */
	UPROPERTY(EditAnywhere, Category = "Value")
	FString StringValue = TEXT("StringValue");

	/** The string value override value for when the StringValue has a connection that replaces the provided string value. */
	UPROPERTY(VisibleAnywhere, Category = "Value", Transient)
	mutable FString StringValue_Override;  // _Override has a special meaning to the property customization, mutable because this property is set while getting the original value
};

USTRUCT()
struct FChaosClothAssetConnectableIStringValue
{
	GENERATED_BODY()

	/** The value for this property. */
	UPROPERTY(EditAnywhere, Category = "Value", Meta = (DataflowInput))
	FString StringValue = TEXT("StringValue");

	/**
	  * Whether the property could import fabrics datas or not
	  */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Value")
	bool bCouldUseFabrics = false;

	/** The string value override value for when the StringValue has a connection that replaces the provided string value. */
	UPROPERTY(VisibleAnywhere, Category = "Value", Transient)
	mutable FString StringValue_Override;  // _Override has a special meaning to the property customization, mutable because this property is set while getting the original value
	
	/**
	 * Whether the property can override the weight map based on the imported fabrics
	 */
	UPROPERTY(EditAnywhere, Category = "Value", Meta = (EditCondition = "bCouldUseFabrics", EditConditionHides))
	bool bBuildFabricMaps = false;
};

USTRUCT()
struct FChaosClothAssetConnectableIOStringValue
{
	GENERATED_BODY()

	/** The value for this property. */
	UPROPERTY(EditAnywhere, Category = "Value", Meta = (DataflowInput, DataflowOutput, DataflowPassthrough = "StringValue"))
	FString StringValue = TEXT("StringValue");

	/** The string value override value for when the StringValue has a connection that replaces the provided string value. */
	UPROPERTY(VisibleAnywhere, Category = "Value", Transient)
	mutable FString StringValue_Override;  // _Override has a special meaning to the property customization, mutable because this property is set while getting the original value
};
