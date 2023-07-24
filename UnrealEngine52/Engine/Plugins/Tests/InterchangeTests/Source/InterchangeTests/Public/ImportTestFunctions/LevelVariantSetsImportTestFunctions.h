// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImportTestFunctionsBase.h"
#include "LevelVariantSetsImportTestFunctions.generated.h"

class ULevelVariantSets;

struct FInterchangeTestFunctionResult;


UCLASS()
class INTERCHANGETESTS_API ULevelVariantSetsImportTestFunctions : public UImportTestFunctionsBase
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;

	/** Check whether the expected number of LevelVariantSets are imported */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckLevelVariantSetsCount(const TArray<ULevelVariantSets*>& LevelVariantSetsAssets, int32 ExpectedNumberOfLevelVariantSets);

	/** Check whether the imported LevelVariantSets has the expected number of variant sets */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckVariantSetsCount(ULevelVariantSets* LevelVariantSets, int32 ExpectedNumberOfVariantSets);

	/** Check whether the imported LevelVariantSets has the expected number of variants for the given variant set */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckVariantsCount(ULevelVariantSets* LevelVariantSets, const FString& VariantSetName, int32 ExpectedNumberOfVariants);

	/** Check whether the imported LevelVariantSets has the expected number of bindings for the given variant in the given set */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckBindingsCount(ULevelVariantSets* LevelVariantSets, const FString& VariantSetName, const FString& VariantName, int32 ExpectedNumberOfBindings);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "InterchangeTestFunction.h"
#endif
