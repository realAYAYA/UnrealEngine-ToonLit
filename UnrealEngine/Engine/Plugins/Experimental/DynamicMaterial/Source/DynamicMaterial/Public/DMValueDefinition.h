// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMDefs.h"
#include "Engine/EngineTypes.h"
#include "Internationalization/Text.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DMValueDefinition.generated.h"

USTRUCT(BlueprintType)
struct DYNAMICMATERIAL_API FDMValueDefinition
{
	GENERATED_BODY()

	FDMValueDefinition()
		: FDMValueDefinition(EDMValueType::VT_None, 0, FText::GetEmpty(), {})
	{		
	}

	FDMValueDefinition(EDMValueType InType, uint8 InFloatCount, const FText& InDisplayName, const TArray<FText>& InChannelNames)
		: Type(InType)
		, FloatCount(InFloatCount)
		, DisplayName(InDisplayName)
		, ChannelNames(InChannelNames)
	{
	}

	FDMValueDefinition(EDMValueType InType, uint8 InFloatCount, FText&& InDisplayName, TArray<FText>&& InChannelNames)
		: Type(InType)
		, FloatCount(InFloatCount)
		, DisplayName(MoveTemp(InDisplayName))
		, ChannelNames(MoveTemp(InChannelNames))
	{
	}

	EDMValueType GetType() const { return Type; }

	uint8 GetFloatCount() const { return FloatCount; }

	const FText& GetDisplayName() const { return DisplayName; }

	const TArray<FText>& GetChannelNames() const { return ChannelNames; }

	bool IsFloatType() const;

	bool IsFloat3Type() const;

	/** To be consistent without OutputChannel, 1 is the first channel, not 0. */
	const FText& GetChannelName(int32 InChannel) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Material Designer")
	EDMValueType Type;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Material Designer")
	uint8 FloatCount;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Material Designer")
	FText DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Material Designer")
	TArray<FText> ChannelNames;
};

UCLASS(BlueprintType)
class DYNAMICMATERIAL_API UDMValueDefinitionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, CallInEditor, Category = "Material Designer")
	static const TArray<EDMValueType>& GetValueTypes();

	UFUNCTION(BlueprintPure, CallInEditor, Category = "Material Designer")
	static const FDMValueDefinition& GetValueDefinition(EDMValueType InValueType);

	UFUNCTION(BlueprintPure, CallInEditor, Category = "Material Designer", Meta = (DisplayName = "Are Types Compatible"))
	static bool BP_AreTypesCompatible(EDMValueType A, EDMValueType B, int32 AChannel, int32 BChannel)
	{
		return AreTypesCompatible(A, B, AChannel, BChannel);
	}

	static bool AreTypesCompatible(EDMValueType A, EDMValueType B,
		int32 AChannel = FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
		int32 BChannel = FDMMaterialStageConnectorChannel::WHOLE_CHANNEL);

	/** Converts a number of floats into the value type */
	static const FDMValueDefinition& GetTypeForFloatCount(uint8 FloatCount);

	/** Converts a number of floats into the value type */
	UFUNCTION(BlueprintPure, CallInEditor, Category = "Material Designer")
	static const FDMValueDefinition& GetTypeForFloatCount(int32 FloatCount);
};
