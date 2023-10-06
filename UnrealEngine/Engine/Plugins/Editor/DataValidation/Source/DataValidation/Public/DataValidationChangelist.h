// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once



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

	static void GatherDependencies(const FName& InPackageName, TSet<FName>& OutDependencies);
	static FString GetPrettyPackageName(const FName& InPackageName);
public:
	/** Changelist to validate */
	FSourceControlChangelistPtr Changelist;

	/** Assets contained in the changelist */
	//TArray<FAssetData> AssetsInChangelist;

	/** Change description */
	//FText Description;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "EditorSubsystem.h"
#include "Engine/EngineTypes.h"
#include "UObject/Interface.h"
#endif
