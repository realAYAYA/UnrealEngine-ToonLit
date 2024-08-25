// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGControlFlow.generated.h"

USTRUCT(BlueprintType, meta = (Hidden))
struct PCG_API FEnumSelector
{
	GENERATED_BODY()

	UPROPERTY(DisplayName="Enum Class", meta=(PCG_NotOverridable))
	UEnum* Class = nullptr;

	UPROPERTY(DisplayName="Enum Value")
	int64 Value = 0;

	FText GetDisplayName() const;
	FString GetCultureInvariantDisplayName() const;
};

UENUM()
enum class EPCGControlFlowSelectionMode : uint8
{
	Integer,
	Enum,
	String
};

namespace PCGControlFlowConstants
{
	inline const FText SubtitleInt = NSLOCTEXT("FPCGControlFlow", "SubtitleInt", "Integer Selection");
	inline const FText SubtitleEnum = NSLOCTEXT("FPCGControlFlow", "SubtitleEnum", "Enum Selection");
	inline const FText SubtitleString = NSLOCTEXT("FPCGControlFlow", "SubtitleString", "String Selection");
	inline const FName DefaultPathPinLabel = TEXT("Default");
}
