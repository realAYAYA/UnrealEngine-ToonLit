// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextParameterBlockEntry.h"
#include "AnimNextParameterBlockProcessor.generated.h"

/** Block entry that performs arbitrary logic at the point the block is applied */
UCLASS(MinimalAPI)
class UAnimNextParameterBlockProcessor : public UAnimNextParameterBlockEntry
{
	GENERATED_BODY()

	// UAnimNextParameterBlockEntry interface
	virtual FText GetDisplayName() const override { return FText::GetEmpty(); }
	virtual FText GetDisplayNameTooltip() const override { return FText::GetEmpty(); }
};