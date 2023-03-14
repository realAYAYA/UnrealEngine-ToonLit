// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"

#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"

#include "ISourceControlChangelist.h"

#include "DataValidationChangelist.generated.h"

/**
 * Changelist abstraction to allow changelist-level data validation
 */
UCLASS(config = Editor)
class DATAVALIDATION_API UDataValidationChangelist : public UObject
{
	GENERATED_BODY()

public:
	/** Default constructor with nothing */
	UDataValidationChangelist() = default;

	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) override;

	/** Initializes internal state so the validation can be done */
	void Initialize(FSourceControlChangelistPtr InChangelist);

public:
	/** Changelist to validate */
	FSourceControlChangelistPtr Changelist;

	/** Assets contained in the changelist */
	//TArray<FAssetData> AssetsInChangelist;

	/** Change description */
	//FText Description;
};