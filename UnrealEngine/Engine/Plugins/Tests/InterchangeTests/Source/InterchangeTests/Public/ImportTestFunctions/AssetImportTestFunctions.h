// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImportTestFunctionsBase.h"
#include "InterchangeTestFunction.h"
#include "AssetImportTestFunctions.generated.h"

struct FInterchangeTestFunctionResult;
class UObject;


UCLASS()
class INTERCHANGETESTS_API UAssetImportTestFunctions : public UImportTestFunctionsBase
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;


	/** Check whether the expected number of metadata for the object are imported */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckImportedMetadataCount(const UObject* Object, const int32 ExpectedNumberOfMetadataForThisObject);

	/** 
	 * Check whether the expected object metadata key exist.
	 * @Param ExpectedMetadataKey - The object metadata key to pass to the package to retrieve the metadata value
	 */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckMetadataExist(const UObject* Object, const FString& ExpectedMetadataKey);

	/**
	 * Check whether the expected object metadata value is imported.
	 * @Param ExpectedMetadataKey - The object metadata key to pass to the package to retrieve the metadata value
	 * @Param ExpectedMetadataValue - The value to compare the object metadata query with the metadata key
	 */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckMetadataValue(const UObject* Object, const FString& ExpectedMetadataKey, const FString& ExpectedMetadataValue);

};
